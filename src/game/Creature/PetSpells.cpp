/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the 1.12.x client.
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Pet.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Formulas.h"
#include "SpellAuras.h"
#include "CreatureAI.h"
#include "Unit.h"
#include "Util.h"
#include "Transports.h"
#include "Movement/Spline/MoveSpline.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "Cast/Recipe/RecipeBook.h"

void Pet::_LoadSpellCooldowns()
{
    Knowing().Clear();

    QueryResult* result = CharacterDatabase.PQuery("SELECT `spell`,`time` FROM `pet_spell_cooldown` WHERE `guid` = '%u'", m_charmInfo->GetPetNumber());

    if (result)
    {
        time_t curTime = time(nullptr);

        WorldPacket data(SMSG_SPELL_COOLDOWN, (8 + size_t(result->GetRowCount()) * 8));
        data << static_cast<ObjectGuid>(GetObjectGuid());

        do
        {
            Field* fields = result->Fetch();

            uint32 spell_id = fields[0].GetUInt32();
            time_t db_time  = (time_t)fields[1].GetUInt64();

            if (!sSpellStore.LookupEntry(spell_id))
            {
                sLog.outError("Pet %u have unknown spell %u in `pet_spell_cooldown`, skipping.", m_charmInfo->GetPetNumber(), spell_id);
                continue;
            }

            if (db_time <= curTime)
            {
                continue;
            }

            data << uint32(spell_id);
            data << uint32(uint32(db_time - curTime)*IN_MILLISECONDS);

            _AddCreatureSpellCooldown(spell_id, db_time);

            DEBUG_LOG("Pet (Number: %u) spell %u cooldown loaded (%u secs).", m_charmInfo->GetPetNumber(), spell_id, uint32(db_time - curTime));
        }
        while (result->NextRow());

        delete result;

        if (!Knowing().NothingDown() && GetOwner())
        {
            ((Player*)GetOwner())->GetSession()->SendPacket(&data);
        }
    }
}

void Pet::_SaveSpellCooldowns()
{
    static SqlStatementID delSpellCD ;
    static SqlStatementID insSpellCD ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delSpellCD, "DELETE FROM `pet_spell_cooldown` WHERE `guid` = ?");
    stmt.PExecute(m_charmInfo->GetPetNumber());

    time_t curTime = time(nullptr);

    Knowing().ForgetExpired(curTime);

    for (auto const& down : Knowing().StillDown())
    {
        stmt = CharacterDatabase.CreateStatement(insSpellCD, "INSERT INTO `pet_spell_cooldown` (`guid`,`spell`,`time`) VALUES (?, ?, ?)");
        stmt.PExecute(m_charmInfo->GetPetNumber(), down.first, uint64(down.second));
    }
}

void Pet::_LoadSpells()
{
    QueryResult* result = CharacterDatabase.PQuery("SELECT `spell`,`active` FROM `pet_spell` WHERE `guid` = '%u'", m_charmInfo->GetPetNumber());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            addSpell(fields[0].GetUInt32(), ActiveStates(fields[1].GetUInt8()), PETSPELL_UNCHANGED);
        }
        while (result->NextRow());

        delete result;
    }
}

void Pet::_SaveSpells()
{
    static SqlStatementID delSpell ;
    static SqlStatementID insSpell ;

    for (PetSpellMap::iterator itr = m_spells.begin(), next = m_spells.begin(); itr != m_spells.end(); itr = next)
    {
        ++next;

        if (itr->second.type == PETSPELL_FAMILY)
        {
            continue;
        }

        switch (itr->second.state)
        {
            case PETSPELL_REMOVED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(delSpell, "DELETE FROM `pet_spell` WHERE `guid` = ? AND `spell` = ?");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first);
                m_spells.erase(itr);
            }
            continue;
            case PETSPELL_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(delSpell, "DELETE FROM `pet_spell` WHERE `guid` = ? AND `spell` = ?");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first);

                stmt = CharacterDatabase.CreateStatement(insSpell, "INSERT INTO `pet_spell` (`guid`,`spell`,`active`) VALUES (?, ?, ?)");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first, uint32(itr->second.active));
            }
            break;
            case PETSPELL_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insSpell, "INSERT INTO `pet_spell` (`guid`,`spell`,`active`) VALUES (?, ?, ?)");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first, uint32(itr->second.active));
            }
            break;
            case PETSPELL_UNCHANGED:
                continue;
        }

        itr->second.state = PETSPELL_UNCHANGED;
    }
}

void Pet::_LoadAuras(uint32 timediff)
{
    RemoveAllAuras();

    for (int i = UNIT_FIELD_AURA; i <= UNIT_FIELD_AURASTATE; ++i)
    {
        SetUInt32Value(i, 0);
    }

    QueryResult* result = CharacterDatabase.PQuery("SELECT `caster_guid`,`item_guid`,`spell`,`stackcount`,`remaincharges`,`basepoints0`,`basepoints1`,`basepoints2`,`periodictime0`,`periodictime1`,`periodictime2`,`maxduration`,`remaintime`,`effIndexMask` FROM `pet_aura` WHERE `guid` = '%u'", m_charmInfo->GetPetNumber());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            ObjectGuid casterGuid = static_cast<ObjectGuid>(fields[0].GetUInt64());
            uint32 item_lowguid = fields[1].GetUInt32();
            uint32 spellid = fields[2].GetUInt32();
            uint32 stackcount = fields[3].GetUInt32();
            uint32 remaincharges = fields[4].GetUInt32();
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = fields[i + 5].GetInt32();
                periodicTime[i] = fields[i + 8].GetUInt32();
            }

            int32 maxduration = fields[11].GetInt32();
            int32 remaintime = fields[12].GetInt32();
            uint32 effIndexMask = fields[13].GetUInt32();

            SpellEntry const* spellproto = sSpellStore.LookupEntry(spellid);
            if (!spellproto)
            {
                sLog.outError("Unknown spell (spellid %u), ignore.", spellid);
                continue;
            }

            if (casterGuid != GetObjectGuid() && IsSingleTargetSpell(spellproto))
            {
                continue;
            }

            if (remaintime != -1 && !cast::RecipeOf(*spellproto).IsPositive())
            {
                if (remaintime / IN_MILLISECONDS <= int32(timediff))
                {
                    continue;
                }

                remaintime -= timediff * IN_MILLISECONDS;
            }

            if (spellproto->ProcCharges == 0)
            {
                remaincharges = 0;
            }

            if (!spellproto->CumulativeAura)
            {
                stackcount = 1;
            }
            else if (spellproto->CumulativeAura < stackcount)
            {
                stackcount = spellproto->CumulativeAura;
            }
            else if (!stackcount)
            {
                stackcount = 1;
            }

            SpellAuraHolder* holder = CreateSpellAuraHolder(spellproto, this, nullptr);
            holder->SetLoadedState(casterGuid, MakeGuid(HIGHGUID_ITEM, item_lowguid), stackcount, remaincharges, maxduration, remaintime);

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if ((effIndexMask & (1 << i)) == 0)
                {
                    continue;
                }

                Aura* aura = CreateAura(spellproto, SpellEffectIndex(i), nullptr, holder, this);
                if (!damage[i])
                {
                    damage[i] = aura->GetModifier()->m_amount;
                }

                aura->SetLoadedState(damage[i], periodicTime[i]);
                holder->AddAura(aura, SpellEffectIndex(i));
            }

            if (!holder->IsEmptyHolder())
            {
                AddSpellAuraHolder(holder);
            }
            else
            {
                delete holder;
            }
        }
        while (result->NextRow());

        delete result;
    }
}

void Pet::_SaveAuras()
{
    static SqlStatementID delAuras ;
    static SqlStatementID insAuras ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delAuras, "DELETE FROM `pet_aura` WHERE `guid` = ?");
    stmt.PExecute(m_charmInfo->GetPetNumber());

    SpellAuraHolderMap const& auraHolders = GetSpellAuraHolderMap();

    if (auraHolders.empty())
    {
        return;
    }

    stmt = CharacterDatabase.CreateStatement(insAuras, "INSERT INTO `pet_aura` (`guid`, `caster_guid`, `item_guid`, `spell`, `stackcount`, `remaincharges`, "
        "`basepoints0`, `basepoints1`, `basepoints2`, `periodictime0`, `periodictime1`, `periodictime2`, `maxduration`, `remaintime`, `effIndexMask`) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    for (SpellAuraHolderMap::const_iterator itr = auraHolders.begin(); itr != auraHolders.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;

        bool save = true;
        for (int32 j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            SpellEntry const* spellInfo = holder->GetSpellProto();
            if (spellInfo->EffectAura[j] == SPELL_AURA_MOD_STEALTH ||
                spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AREA_AURA_PET)
            {
                save = false;
                break;
            }
        }

        if (save && !holder->IsPassive() && !(cast::RecipeOf(*holder->GetSpellProto()).Starts() == cast::Start::Channelled) && (holder->GetCasterGuid() == GetObjectGuid() || holder->GetTrackedAuraType() != TRACK_AURA_TYPE_NOT_TRACKED))
        {
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];
            uint32 effIndexMask = 0;

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = 0;
                periodicTime[i] = 0;

                if (Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                {

                    if (aur->IsAreaAura() && holder->GetCasterGuid() != GetObjectGuid())
                    {
                        continue;
                    }

                    damage[i] = aur->GetModifier()->m_amount;
                    periodicTime[i] = aur->GetModifier()->periodictime;
                    effIndexMask |= (1 << i);
                }
            }

            if (!effIndexMask)
            {
                continue;
            }

            stmt.addUInt32(m_charmInfo->GetPetNumber());
            stmt.addUInt64(holder->GetCasterGuid());
            stmt.addUInt32(GuidCounter(holder->GetCastItemGuid()));
            stmt.addUInt32(holder->GetId());
            stmt.addUInt32(holder->GetStackAmount());
            stmt.addUInt8(holder->GetAuraCharges());

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                stmt.addInt32(damage[i]);
            }

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                stmt.addUInt32(periodicTime[i]);
            }

            stmt.addInt32(holder->GetAuraMaxDuration());
            stmt.addInt32(holder->GetAuraDuration());
            stmt.addUInt32(effIndexMask);
            stmt.Execute();
        }
    }
}

bool Pet::addSpell(uint32 spell_id, ActiveStates active , PetSpellState state , PetSpellType type )
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
    {

        if (state == PETSPELL_UNCHANGED)
        {
            sLog.outError("Pet::addSpell: nonexistent in SpellStore spell #%u request, deleting for all pets in `pet_spell`.", spell_id);
            CharacterDatabase.PExecute("DELETE FROM `pet_spell` WHERE `spell` = '%u'", spell_id);
        }
        else
        {
            sLog.outError("Pet::addSpell: nonexistent in SpellStore spell #%u request.", spell_id);
        }

        return false;
    }

    PetSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr != m_spells.end())
    {
        if (itr->second.state == PETSPELL_REMOVED)
        {
            m_spells.erase(itr);
            state = PETSPELL_CHANGED;
        }
        else if (state == PETSPELL_UNCHANGED && itr->second.state != PETSPELL_UNCHANGED)
        {

            itr->second.state = PETSPELL_UNCHANGED;

            if (active == ACT_ENABLED)
            {
                ToggleAutocast(spell_id, true);
            }
            else if (active == ACT_DISABLED)
            {
                ToggleAutocast(spell_id, false);
            }

            return false;
        }
        else
        {
            return false;
        }
    }

    PetSpell newspell;
    newspell.state = state;
    newspell.type = type;

    if (active == ACT_DECIDE)
    {
        if ((cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive))
        {
            newspell.active = ACT_PASSIVE;
        }
        else
        {
            newspell.active = ACT_DISABLED;
        }
    }
    else
    {
        newspell.active = active;
    }

    if (sSpellMgr.GetSpellRank(spell_id) != 0)
    {
        for (PetSpellMap::const_iterator itr2 = m_spells.begin(); itr2 != m_spells.end(); ++itr2)
        {
            if (itr2->second.state == PETSPELL_REMOVED)
            {
                continue;
            }

            uint32 const oldspell_id = itr2->first;

            if (sSpellMgr.IsRankSpellDueToSpell(spellInfo, oldspell_id))
            {

                if (sSpellMgr.IsHighRankOfSpell(spell_id, oldspell_id))
                {
                    newspell.active = itr2->second.active;

                    if (newspell.active == ACT_ENABLED)
                    {
                        ToggleAutocast(oldspell_id, false);
                    }

                    unlearnSpell(oldspell_id, false, false);
                    break;
                }

                else if (sSpellMgr.IsHighRankOfSpell(oldspell_id, spell_id))
                {
                    return false;
                }
            }
        }
    }

    m_spells[spell_id] = newspell;

    if ((cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive))
    {
        CastSpell(this, spell_id, true);
    }
    else
    {
        m_charmInfo->AddSpellToActionBar(spell_id, ActiveStates(newspell.active));
    }

    if (newspell.active == ACT_ENABLED)
    {
        ToggleAutocast(spell_id, true);
    }

    return true;
}

bool Pet::learnSpell(uint32 spell_id)
{

    if (!addSpell(spell_id))
    {
        return false;
    }

    if (!m_loading)
    {
        Unit* owner = GetOwner();
        if (owner &&IsPlayer(owner))
        {
            ((Player*)owner)->PetSpellInitialize();
        }
    }
    return true;
}

bool Pet::unlearnSpell(uint32 spell_id, bool learn_prev, bool clear_ab)
{
    if (removeSpell(spell_id, learn_prev, clear_ab))
    {
        return true;
    }
    return false;
}

bool Pet::removeSpell(uint32 spell_id, bool learn_prev, bool clear_ab)
{
    PetSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
    {
        return false;
    }

    if (itr->second.state == PETSPELL_REMOVED)
    {
        return false;
    }

    if (itr->second.state == PETSPELL_NEW)
    {
        m_spells.erase(itr);
    }
    else
    {
        itr->second.state = PETSPELL_REMOVED;
    }

    RemoveAuras(spell_id);

    if (learn_prev)
    {
        if (uint32 prev_id = sSpellMgr.GetPrevSpellInChain(spell_id))
        {
            learnSpell(prev_id);
        }
        else
        {
            learn_prev = false;
        }
    }

    if (clear_ab && !learn_prev && m_charmInfo->RemoveSpellFromActionBar(spell_id))
    {
        if (!m_loading)
        {

            if (Unit* owner = GetOwner())
            {
                if (IsPlayer(owner))
                {
                    ((Player*)owner)->PetSpellInitialize();
                }
            }
        }
    }

    return true;
}

void Pet::CleanupActionBar()
{
    for (int i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (UnitActionBarEntry const* ab = m_charmInfo->GetActionBarEntry(i))
        {
            if (uint32 action = ab->GetAction())
            {
                if (ab->IsActionBarForSpell() && !HasSpell(action))
                {
                    m_charmInfo->SetActionBar(i, 0, ACT_DISABLED);
                }
            }
        }
    }
}

void Pet::InitPetCreateSpells()
{
    m_charmInfo->InitPetActionBar();
    m_spells.clear();

    int32 usedtrainpoints = 0;

    uint32 petspellid;
    PetCreateSpellEntry const* CreateSpells = sObjectMgr.GetPetCreateSpellEntry(GetEntry());
    if (CreateSpells)
    {
        Unit* owner = GetOwner();
        Player* p_owner = owner &&IsPlayer(owner) ? (Player*)owner : nullptr;

        for (uint8 i = 0; i < 4; ++i)
        {
            if (!CreateSpells->spellid[i])
            {
                break;
            }

            SpellEntry const* learn_spellproto = sSpellStore.LookupEntry(CreateSpells->spellid[i]);
            if (!learn_spellproto)
            {
                continue;
            }

            if (learn_spellproto->Effect[0] == SPELL_EFFECT_LEARN_SPELL || learn_spellproto->Effect[0] == SPELL_EFFECT_LEARN_PET_SPELL)
            {
                petspellid = learn_spellproto->EffectTriggerSpell[0];
                if (p_owner && !p_owner->HasSpell(learn_spellproto->ID))
                {
                    if (cast::Recipes().StartsAs(petspellid, cast::Start::Passive))
                    {
                        p_owner->learnSpell(learn_spellproto->ID, false);
                    }
                    else
                    {
                        AddTeachSpell(learn_spellproto->EffectTriggerSpell[0], learn_spellproto->ID);
                    }
                }
            }
            else
            {
                petspellid = learn_spellproto->ID;
            }

            addSpell(petspellid);

            SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(learn_spellproto->EffectTriggerSpell[0]);

            for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
            {
                usedtrainpoints += _spell_idx->second->ReqTrainPoints;
                break;
            }
        }
    }

    LearnPetPassives();

    CastPetAuras(false);

    SetTP(-usedtrainpoints);
}

uint32 Pet::resetTalentsCost() const
{
    uint32 days = uint32(sWorld.GetGameTime() - m_resetTalentsTime) / DAY;

    if (m_resetTalentsCost < 10 * SILVER || days > 0)
    {
        return 10 * SILVER;
    }

    else if (m_resetTalentsCost < 50 * SILVER)
    {
        return 50 * SILVER;
    }

    else if (m_resetTalentsCost < 1 * GOLD)
    {
        return 1 * GOLD;
    }

    else
    {
        return (m_resetTalentsCost + 1 * GOLD > 10 * GOLD ? 10 * GOLD : m_resetTalentsCost + 1 * GOLD);
    }
}

void Pet::ToggleAutocast(uint32 spellid, bool apply)
{
    if (cast::Recipes().StartsAs(spellid, cast::Start::Passive))
    {
        return;
    }

    PetSpellMap::iterator itr = m_spells.find(spellid);
    PetSpell &petSpell = itr->second;

    uint32 i;

    if (apply)
    {
        for (i = 0; i < m_autospells.size() && m_autospells[i] != spellid; ++i)
        {
            ;
        }

        if (i == m_autospells.size())
        {
            m_autospells.push_back(spellid);

            if (petSpell.active != ACT_ENABLED)
            {
                petSpell.active = ACT_ENABLED;
                if (petSpell.state != PETSPELL_NEW)
                {
                    petSpell.state = PETSPELL_CHANGED;
                }
            }
        }
    }
    else
    {
        AutoSpellList::iterator itr2 = m_autospells.begin();
        for (i = 0; i < m_autospells.size() && m_autospells[i] != spellid; ++i, ++itr2)
        {
            ;
        }

        if (i < m_autospells.size())
        {
            m_autospells.erase(itr2);
            if (petSpell.active != ACT_DISABLED)
            {
                petSpell.active = ACT_DISABLED;
                if (petSpell.state != PETSPELL_NEW)
                {
                    petSpell.state = PETSPELL_CHANGED;
                }
            }
        }
    }
}
