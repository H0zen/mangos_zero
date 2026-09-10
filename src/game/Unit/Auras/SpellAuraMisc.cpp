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

#include "SpellAuras.h"
#include "Platform/Define.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "ObjectLookup.h"
#include "Policies/Singleton.h"
#include "Totem.h"
#include "Creature.h"
#include "Formulas.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "CreatureAI.h"
#include "ScriptMgr.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Language.h"
#include "TemporarySummon.h"

void Aura::HandleShapeshiftBoosts(bool apply)
{
    uint32 spellId1 = 0;
    uint32 spellId2 = 0;
    uint32 HotWSpellId = 0;

    ShapeshiftForm form = ShapeshiftForm(GetModifier()->m_miscvalue);

    Unit* target = GetTarget();

    switch (form)
    {
        case FORM_CAT:
            spellId1 = 3025;
            HotWSpellId = 24900;
            break;
        case FORM_TREE:
            spellId1 = 5420;
            break;
        case FORM_TRAVEL:
            spellId1 = 5419;
            break;
        case FORM_AQUA:
            spellId1 = 5421;
            break;
        case FORM_BEAR:
            spellId1 = 1178;
            spellId2 = 21178;
            HotWSpellId = 24899;
            break;
        case FORM_DIREBEAR:
            spellId1 = 9635;
            spellId2 = 21178;
            HotWSpellId = 24899;
            break;
        case FORM_BATTLESTANCE:
            spellId1 = 21156;
            break;
        case FORM_DEFENSIVESTANCE:
            spellId1 = 7376;
            break;
        case FORM_BERSERKERSTANCE:
            spellId1 = 7381;
            break;
        case FORM_MOONKIN:
            spellId1 = 24905;
            break;
        case FORM_SPIRITOFREDEMPTION:
            spellId1 = 27792;
            spellId2 = 27795;
            break;
        case FORM_GHOSTWOLF:
        case FORM_AMBIENT:
        case FORM_GHOUL:
        case FORM_SHADOW:
        case FORM_STEALTH:
        case FORM_CREATURECAT:
        case FORM_CREATUREBEAR:
            break;
        default:
            break;
    }

    if (apply)
    {
        if (spellId1)
        {
            target->CastSpell(target, spellId1, true, nullptr, this);
        }
        if (spellId2)
        {
            target->CastSpell(target, spellId2, true, nullptr, this);
        }

        if (IsPlayer(target))
        {
            const PlayerSpellMap& sp_list = ((Player*)target)->GetSpellMap();
            for (PlayerSpellMap::const_iterator itr = sp_list.begin(); itr != sp_list.end(); ++itr)
            {
                if (itr->second.state == PLAYERSPELL_REMOVED)
                {
                    continue;
                }
                if (itr->first == spellId1 || itr->first == spellId2)
                {
                    continue;
                }
                SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);
                if (!spellInfo || !IsNeedCastSpellAtFormApply(spellInfo, form))
                {
                    continue;
                }
                target->CastSpell(target, itr->first, true, nullptr, this);
            }

            if (((Player*)target)->HasSpell(17007))
            {
                SpellEntry const* spellInfo = sSpellStore.LookupEntry(24932);
                if (spellInfo && spellInfo->ShapeshiftMask & (1 << (form - 1)))
                {
                    target->CastSpell(target, 24932, true, nullptr, this);
                }
            }

            if (HotWSpellId)
            {
                const auto mModTotalStatPct = target->GetAurasByType(SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE);
                for (auto* aura : mModTotalStatPct)
                {
                    if (aura->GetSpellProto()->SpellIconID == 240 && aura->GetModifier()->m_miscvalue == 3)
                    {
                        int32 HotWMod = aura->GetModifier()->m_amount;
                        target->CastCustomSpell(target, HotWSpellId, &HotWMod, nullptr, nullptr, true, nullptr, this);
                        break;
                    }
                }
            }
        }
    }
    else
    {
        if (spellId1)
        {
            target->RemoveAuras(spellId1);
        }
        if (spellId2)
        {
            target->RemoveAuras(spellId2);
        }

        Unit::SpellAuraHolderMap& tAuras = target->GetSpellAuraHolderMap();
        for (Unit::SpellAuraHolderMap::iterator itr = tAuras.begin(); itr != tAuras.end();)
        {
            if ((itr->second->IsRemovedOnShapeLost() && itr->second->GetSpellProto()->ID != 12292) || itr->second->GetSpellProto()->ID == 24864)
            {
                target->RemoveAuras(itr->second->GetId());
                itr = tAuras.begin();
            }
            else
            {
                ++itr;
            }
        }
    }
}

void Aura::HandleAuraEmpathy(bool apply, bool )
{
    Unit* target = GetTarget();

    CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(target->GetEntry());
    if (IsPlayer(target) || (IsCreature(target) && ci && ci->CreatureType == CREATURE_TYPE_BEAST))
    {
        target->ApplyModUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_SPECIALINFO, apply);
    }
}

void Aura::HandleAuraUntrackable(bool apply, bool )
{
    if (apply)
    {
        GetTarget()->SetUntrackable(true);
    }
    else
    {
        GetTarget()->SetUntrackable(false);
    }
}

void Aura::HandleAuraModPacify(bool apply, bool )
{
    if (apply)
    {
        GetTarget()->SetUnitFlag(UNIT_FLAG_PACIFIED);
    }
    else
    {
        GetTarget()->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
    }
}

void Aura::HandleAuraModPacifyAndSilence(bool apply, bool Real)
{
    HandleAuraModPacify(apply, Real);
    HandleAuraModSilence(apply, Real);
}

void Aura::HandleAuraGhost(bool apply, bool )
{
    if (Player* player = static_cast<Player*>(GetTarget()))
    {
        player->ApplyPlayerFlag(PLAYER_FLAGS_GHOST, apply);
    }
}

void Aura::HandleAuraRetainComboPoints(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    Player* target = (Player*)GetTarget();

    if (!apply && m_removeMode == AURA_REMOVE_BY_EXPIRE && target->GetComboTargetGuid())
    {
        if (Unit* unit = ObjectLookup::GetUnit(*GetTarget(), target->GetComboTargetGuid()))
        {
            target->AddComboPoints(unit, -m_modifier.m_amount);
        }
    }
}

void Aura::HandleModUnattackable(bool Apply, bool Real)
{
    if (Real && Apply)
    {
        GetTarget()->CombatStop();
        GetTarget()->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);
    }
    GetTarget()->ApplyUnitFlag(UNIT_FLAG_NON_ATTACKABLE, Apply);
}

void Aura::HandleSpiritOfRedemption(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {
        if (IsPlayer(target))
        {

            ((Player*)target)->StopMirrorTimers();

            if (!target->IsStandState())
            {
                target->SetStandState(UNIT_STAND_STATE_STAND);
            }
        }

        if (target->IsNonMeleeSpellCasted(false))
        {
            target->InterruptNonMeleeSpells(false);
        }

        target->SetHealth(target->GetMaxHealth());
        target->SetPower(POWER_MANA, target->GetMaxPower(POWER_MANA));
    }

    else
    {
        target->DealDamage(target, target->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, GetSpellProto(), false);
    }
}

void Aura::HandleSchoolAbsorb(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Unit* caster = GetCaster();
    if (!caster)
    {
        return;
    }

    Unit* target = GetTarget();
    SpellEntry const* spellProto = GetSpellProto();
    if (apply)
    {

        if (!IsPlayer(target) || !((Player*)target)->GetSession()->PlayerLoading())
        {
            float DoneActualBenefit = 0.0f;
            switch (spellProto->SpellClassSet)
            {
                case SPELLFAMILY_PRIEST:

                    if (spellProto->SpellClassMask & UI64LIT(0x0000000000000001))
                    {

                        DoneActualBenefit = caster->SpellBaseHealingBonusDone(GetSpellSchoolMask(spellProto)) * 0.3f;
                        break;
                    }
                    break;
                case SPELLFAMILY_MAGE:

                    if (spellProto->IsFitToFamilyMask(UI64LIT(0x0000000100080108)))

                    {
                        DoneActualBenefit = caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(spellProto)) * 0.1f;
                    }
                    break;
                case SPELLFAMILY_WARLOCK:

                    if (!spellProto->SpellClassMask)

                    {
                        DoneActualBenefit = caster->SpellBaseDamageBonusDone(GetSpellSchoolMask(spellProto)) * 0.1f;
                    }
                    break;
                default:
                    break;
            }

            DoneActualBenefit *= caster->CalculateLevelPenalty(GetSpellProto());

            m_modifier.m_amount += (int32)DoneActualBenefit;
        }
    }
}
