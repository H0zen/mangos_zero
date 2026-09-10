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

#include <iterator>
#include <random>
#include <utility>
#include "Reaction.h"
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include <string>
#include <list>
#include <algorithm>
#include "Utilities/MathDefines.h"
#include <ctime>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Mint.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "SpellAuras.h"
#include "Group.h"
#include "UpdateData.h"
#include "SharedDefines.h"
#include "Pet.h"
#include "GameObject.h"
#include "GossipDef.h"
#include "Creature.h"
#include "Totem.h"
#include "CreatureAI.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundWS.h"
#include "Language.h"
#include "SocialMgr.h"
#include "Util.h"
#include "TemporarySummon.h"
#include "ScriptMgr.h"
#include "Formulas.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Geometry/Vector3.h"
#include "Cast/Recipe/RecipeBook.h"

void Spell::EffectLearnSpell(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!unitTarget)
    {
        return;
    }

    if (!IsPlayer(unitTarget))
    {
        if (IsPlayer(m_caster))
        {
            EffectLearnPetSpell(operation);
        }

        return;
    }

    Player* player = (Player*)unitTarget;
    uint32 spellToLearn = operation.triggerSpell;

    player->learnSpell(spellToLearn, false);

    if (Occupant const* caster = GetCastingObject())
    {
        DEBUG_LOG("Spell: %s has learned spell %u from %s", player->GetGuidStr().c_str(), spellToLearn, caster->GetGuidStr().c_str());
    }
}

void Spell::EffectDispel(const cast::Operation& operation)
{
    if (!unitTarget)
    {
        return;
    }

    if (m_spellInfo->SpellClassSet == SPELLFAMILY_WARRIOR && (m_spellInfo->SpellClassMask & UI64LIT(0x0000000100000000)) &&
        !roll_chance_i(50))
    {
        return;
    }

    std::list <std::pair<SpellAuraHolder* , uint32> > dispel_list;

    uint32 dispel_type = operation.miscValue;
    uint32 dispelMask  = GetDispellMask(DispelType(dispel_type));
    Unit::SpellAuraHolderMap const& auras = unitTarget->GetSpellAuraHolderMap();
    for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;
        if ((1 << holder->GetSpellProto()->DispelType) & dispelMask)
        {
            if (holder->GetSpellProto()->DispelType == DISPEL_MAGIC)
            {
                bool positive = true;
                if (!holder->IsPositive())
                {
                    positive = false;
                }
                else
                {
                    positive = (holder->GetSpellProto()->AttributesEx & SPELL_ATTR_EX_CANT_BE_REFLECTED) == 0;
                }

                if (positive == IsFriendly(*unitTarget, *m_caster))
                {
                    continue;
                }
            }
            dispel_list.push_back(std::pair<SpellAuraHolder* , uint32>(holder, holder->GetStackAmount()));
        }
    }

    if (!dispel_list.empty())
    {
        std::list<std::pair<SpellAuraHolder* , uint32> > success_list;
        std::list < uint32 > fail_list;

        if (!damage)
        {
            damage = 1;
        }

        for (int32 count = 0; count < damage && !dispel_list.empty(); ++count)
        {

            std::list<std::pair<SpellAuraHolder* , uint32> >::iterator dispel_itr = dispel_list.begin();
            std::advance(dispel_itr, urand(0, dispel_list.size() - 1));

            SpellAuraHolder* holder = dispel_itr->first;

            dispel_itr->second -= 1;

            if (dispel_itr->second == 0)
            {
                dispel_list.erase(dispel_itr);
            }

            SpellEntry const* spellInfo = holder->GetSpellProto();

            int32 miss_chance = 0;

            if (Unit* caster = holder->GetCaster())
            {
                if (Player* modOwner = caster->GetSpellModOwner())
                {
                    modOwner->SpellMods().Apply(spellInfo->ID, SPELLMOD_RESIST_DISPEL_CHANCE, miss_chance, this);
                }
            }

            if (roll_chance_i(miss_chance))
            {
                fail_list.push_back(spellInfo->ID);
            }
            else
            {
                bool foundDispelled = false;
                for (std::list<std::pair<SpellAuraHolder* , uint32> >::iterator success_iter = success_list.begin(); success_iter != success_list.end(); ++success_iter)
                {
                    if (success_iter->first->GetId() == holder->GetId() && success_iter->first->GetCasterGuid() == holder->GetCasterGuid())
                    {
                        success_iter->second += 1;
                        foundDispelled = true;
                        break;
                    }
                }
                if (!foundDispelled)
                {
                    success_list.push_back(std::pair<SpellAuraHolder* , uint32>(holder, 1));
                }
            }
        }

        if (!success_list.empty())
        {
            int32 count = success_list.size();
            WorldPacket data(SMSG_SPELLDISPELLOG, 8 + 8 + 4 + 1 + 4 + count * 5);
            data << unitTarget->GetPackGUID();
            data << m_caster->GetPackGUID();
            data << uint32(m_spellInfo->ID);

            data << uint32(count);
            for (std::list<std::pair<SpellAuraHolder* , uint32> >::iterator j = success_list.begin(); j != success_list.end(); ++j)
            {
                SpellAuraHolder* dispelledHolder = j->first;
                data << uint32(dispelledHolder->GetId());

                unitTarget->RemoveStacks(dispelledHolder->GetId(), j->second, dispelledHolder->GetCasterGuid(), AURA_REMOVE_BY_DISPEL);
            }
            Deliver(Audience::Around(*m_caster).AndSubject(), &data);

            if (m_spellInfo->SpellClassSet == SPELLFAMILY_WARLOCK && m_spellInfo->Category == SPELLCATEGORY_DEVOUR_MAGIC)
            {
                uint32 heal_spell = 0;
                switch (m_spellInfo->ID)
                {
                    case 19505: heal_spell = 19658; break;
                    case 19731: heal_spell = 19732; break;
                    case 19734: heal_spell = 19733; break;
                    case 19736: heal_spell = 19735; break;
                    default:
                        DEBUG_LOG("Spell for Devour Magic %d not handled in Spell::EffectDispel", m_spellInfo->ID);
                        break;
                }
                if (heal_spell)
                {
                    m_caster->CastSpell(m_caster, heal_spell, true);
                }
            }
        }

        if (!fail_list.empty())
        {

            WorldPacket data(SMSG_DISPEL_FAILED, 8 + 8 + 4 + 4 * fail_list.size());
            data << m_caster->GetObjectGuid();
            data << unitTarget->GetObjectGuid();
            data << uint32(m_spellInfo->ID);
            for (std::list< uint32 >::iterator j = fail_list.begin(); j != fail_list.end(); ++j)
            {
                data << uint32(*j);
            }
            Deliver(Audience::Around(*m_caster).AndSubject(), &data);
        }
    }
}

void Spell::EffectDualWield(const cast::Operation& )
{
    if (unitTarget &&IsPlayer(unitTarget))
    {
        ((Player*)unitTarget)->Arms().CanDualWield(true);
    }
}

void Spell::EffectPull(const cast::Operation& )
{

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: Spell Effect DUMMY");
}

void Spell::EffectDistract(const cast::Operation& )
{

    if (!unitTarget || unitTarget->IsInCombat())
    {
        return;
    }

    if (unitTarget->hasUnitState(UNIT_STAT_CAN_NOT_REACT))
    {
        return;
    }

    float angle = unitTarget->Where().BearingTo(Geometry::Vector2(m_targets.m_destX, m_targets.m_destY));
    unitTarget->SetFacingTo(angle);
    unitTarget->clearUnitState(UNIT_STAT_MOVING);
    unitTarget->Place().Face(angle);

    if (IsCreature(unitTarget))
    {
        unitTarget->GetMotionMaster()->MoveDistract(damage * IN_MILLISECONDS);
    }
}

void Spell::EffectPickPocket(const cast::Operation& )
{
    if (!IsPlayer(m_caster))
    {
        return;
    }

    if (!unitTarget || !IsCreature(unitTarget) || IsFriendly(*m_caster, *unitTarget))
    {
        return;
    }

    if (unitTarget->IsAlive() && (unitTarget->GetCreatureTypeMask() & CREATURE_TYPEMASK_HUMANOID_OR_UNDEAD) != 0)
    {
        int32 chance = 10 + int32(m_caster->getLevel()) - int32(unitTarget->getLevel());

        if (chance > irand(0, 19))
        {

            ((Player*)m_caster)->SendLoot(unitTarget->GetObjectGuid(), LOOT_PICKPOCKETING);
        }
        else
        {

            m_caster->SendSpellMiss(unitTarget, m_spellInfo->ID, SPELL_MISS_RESIST);
            m_caster->RemoveAurasOfType(SPELL_AURA_MOD_STEALTH);
            unitTarget->AttackedBy(m_caster);
        }
    }
}

void Spell::EffectAddFarsight(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!IsPlayer(m_caster))
    {
        return;
    }

    int32 duration = Recipe().DurationMs();
    DynamicObject* dynObj = new DynamicObject;

    if (!dynObj->Create(m_caster->GetMap()->GenerateLocalLowGuid(HIGHGUID_DYNAMICOBJECT), m_caster,
        m_spellInfo->ID, eff_idx, m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, duration, 0, DYNAMIC_OBJECT_FARSIGHT_FOCUS))
    {
        delete dynObj;
        return;
    }

    m_caster->Conjured().AddArea(dynObj);
    m_caster->GetMap()->Add(dynObj);

    ((Player*)m_caster)->GetCamera().SetView(dynObj);
}

void Spell::EffectTeleUnitsFaceCaster(const cast::Operation& operation)
{
    if (!unitTarget)
    {
        return;
    }

    if (unitTarget->IsTaxiFlying())
    {
        return;
    }

    float fx, fy, fz;
    if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        m_targets.getDestination(fx, fy, fz);
    }
    else
    {
        float dis = GetSpellRadius(sSpellRadiusStore.LookupEntry(operation.radiusIndex));
        ClosePointNear(*m_caster, fx, fy, fz, unitTarget->Where().Extent(), dis);
    }

    unitTarget->NearTeleportTo(fx, fy, fz, m_caster->Where().Facing() + M_PI_F, unitTarget == m_caster);
}

void Spell::EffectLearnSkill(const cast::Operation& operation)
{
    if (!IsPlayer(unitTarget))
    {
        return;
    }

    if (damage < 0)
    {
        return;
    }

    uint32 skillid =  operation.miscValue;
    uint16 skillval = ((Player*)unitTarget)->GetPureSkillValue(skillid);
    ((Player*)unitTarget)->SetSkill(skillid, skillval ? skillval : 1, damage * 75, damage);

    if (Occupant const* caster = GetCastingObject())
    {
        DEBUG_LOG("Spell: %s has learned skill %u (to maxlevel %u) from %s", unitTarget->GetGuidStr().c_str(), skillid, damage * 75, caster->GetGuidStr().c_str());
    }
}

void Spell::EffectTradeSkill(const cast::Operation& )
{
    if (!IsPlayer(unitTarget))
    {
        return;
    }

}

void Spell::EffectEnchantItemPerm(const cast::Operation& operation)
{
    if (!IsPlayer(m_caster))
    {
        return;
    }
    if (!itemTarget)
    {
        return;
    }

    Player* p_caster = (Player*)m_caster;

    p_caster->UpdateCraftSkill(m_spellInfo->ID);

    uint32 enchant_id = operation.miscValue;
    if (!enchant_id)
    {
        return;
    }

    SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
    if (!pEnchant)
    {
        return;
    }

    Player* item_owner = itemTarget->GetOwner();
    if (!item_owner)
    {
        return;
    }

    if (item_owner != p_caster && p_caster->GetSession()->GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
    {
        sLog.outCommand(p_caster->GetSession()->GetAccountId(), "GM %s (Account: %u) enchanting(perm): %s (Entry: %d) for player: %s (Account: %u)",
            p_caster->GetName(), p_caster->GetSession()->GetAccountId(),
            itemTarget->GetProto()->Name1, itemTarget->GetEntry(),
            item_owner->GetName(), item_owner->GetSession()->GetAccountId());
    }

    item_owner->ApplyEnchantment(itemTarget, PERM_ENCHANTMENT_SLOT, false);

    itemTarget->SetEnchantment(PERM_ENCHANTMENT_SLOT, enchant_id, 0, 0);

    item_owner->ApplyEnchantment(itemTarget, PERM_ENCHANTMENT_SLOT, true);
}

void Spell::EffectEnchantItemTmp(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!IsPlayer(m_caster))
    {
        return;
    }
    if (!itemTarget)
    {
        return;
    }

    Player* p_caster = (Player*)m_caster;

    uint32 enchant_id = operation.miscValue;
    if (!enchant_id)
    {
        sLog.outError("Spell %u Effect %u (SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY) have 0 as enchanting id", m_spellInfo->ID, eff_idx);
        return;
    }

    SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
    if (!pEnchant)
    {
        sLog.outError("Spell %u Effect %u (SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY) have nonexistent enchanting id %u ", m_spellInfo->ID, eff_idx, enchant_id);
        return;
    }

    uint32 duration;

    if (m_spellInfo->Attributes == (SPELL_ATTR_TARGET_MAINHAND_ITEM | SPELL_ATTR_NOT_SHAPESHIFT | SPELL_ATTR_DONT_AFFECT_SHEATH_STATE))
    {
        duration = 300;
    }

    else if (m_spellInfo->SpellIconID == 241 && m_spellInfo->ID != 7434)
    {
        duration = 3600;
    }

    else if (m_spellInfo->ID == 28891 || m_spellInfo->ID == 28898)
    {
        duration = 3600;
    }

    else if (Recipe().Says().hiddenFromClient)
    {
        duration = 600;
    }

    else
    {
        duration = 1800;
    }

    Player* item_owner = itemTarget->GetOwner();
    if (!item_owner)
    {
        return;
    }

    if (item_owner != p_caster && p_caster->GetSession()->GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
    {
        sLog.outCommand(p_caster->GetSession()->GetAccountId(), "GM %s (Account: %u) enchanting(temp): %s (Entry: %d) for player: %s (Account: %u)",
            p_caster->GetName(), p_caster->GetSession()->GetAccountId(),
            itemTarget->GetProto()->Name1, itemTarget->GetEntry(),
            item_owner->GetName(), item_owner->GetSession()->GetAccountId());
    }

    item_owner->ApplyEnchantment(itemTarget, TEMP_ENCHANTMENT_SLOT, false);

    itemTarget->SetEnchantment(TEMP_ENCHANTMENT_SLOT, enchant_id, duration * 1000, 0);

    item_owner->ApplyEnchantment(itemTarget, TEMP_ENCHANTMENT_SLOT, true);
}

void Spell::EffectTameCreature(const cast::Operation& )
{

    Player* plr = (Player*)GetAffectiveCaster();

    Creature* creatureTarget = (Creature*)unitTarget;

    finish();

    Pet* pet = new Pet(HUNTER_PET);

    if (!pet->CreateBaseAtCreature(creatureTarget))
    {
        delete pet;
        return;
    }

    pet->SetOwnerGuid(plr->GetObjectGuid());
    pet->SetCreatorGuid(plr->GetObjectGuid());
    pet->setFaction(plr->getFaction());
    pet->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->ID);

    if (plr->IsPvP())
    {
        pet->SetPvP(true);
    }

    if (!pet->InitStatsForLevel(creatureTarget->getLevel()))
    {
        sLog.outError("Pet::InitStatsForLevel() failed for creature (Entry: %u)!", creatureTarget->GetEntry());
        delete pet;
        return;
    }

    pet->GetCharmInfo()->SetPetNumber(sMint.PetNumbers().Next(), true);

    pet->SetHealth(pet->GetMaxHealth());

    creatureTarget->ForcedDespawn();

    pet->SetUInt32Value(UNIT_FIELD_LEVEL, creatureTarget->getLevel() - 1);

    pet->GetMap()->Add((Creature*)pet);

    pet->AIM_Initialize();
    pet->InitPetCreateSpells();

    pet->SetUInt32Value(UNIT_FIELD_LEVEL, creatureTarget->getLevel());

    plr->SetPet(pet);

    pet->SavePetToDB(PET_SAVE_AS_CURRENT);
    plr->PetSpellInitialize();
}

void Spell::EffectSummonPet(const cast::Operation& operation)
{
    uint32 petentry = operation.miscValue;

    Pet* OldSummon = m_caster->GetPet();

    if (OldSummon)
    {
        if ((petentry == 0 || OldSummon->GetEntry() == petentry) && OldSummon->getPetType() != SUMMON_PET)
        {

            if (OldSummon->IsDead())
            {
                return;
            }

            OldSummon->GetMap()->Remove((Creature*)OldSummon, false);

            float px, py, pz;
            ClosePointNear(*m_caster, px, py, pz, OldSummon->Where().Extent());

            OldSummon->Place().MoveTo(px, py, pz, OldSummon->Where().Facing());
            m_caster->GetMap()->Add((Creature*)OldSummon);

            if (IsPlayer(m_caster) && OldSummon->isControlled())
            {
                ((Player*)m_caster)->PetSpellInitialize();
            }
            return;
        }

        if (IsPlayer(m_caster))
        {
            OldSummon->Unsummon(OldSummon->getPetType() == HUNTER_PET ? PET_SAVE_AS_DELETED : PET_SAVE_NOT_IN_SLOT, m_caster);
        }
        else
        {
            return;
        }
    }

    CreatureInfo const* cInfo = petentry ? ObjectMgr::GetCreatureTemplate(petentry) : nullptr;

    if (petentry && !cInfo)
    {
        sLog.outErrorDb("EffectSummonPet: creature entry %u not found for spell %u.", petentry, m_spellInfo->ID);
        return;
    }

    Pet* NewSummon = new Pet;

    if (IsPlayer(m_caster) && NewSummon->LoadPetFromDB((Player*)m_caster, petentry))
    {
        if (NewSummon->getPetType() == SUMMON_PET)
        {

            const auto auraClassScripts = m_caster->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
            for (const auto* script : auraClassScripts)
            {
                if (script->GetModifier()->m_miscvalue == 2228)
                {
                    m_caster->RemoveAuras(script->GetId());
                }
            }
        }

        return;
    }

    if (!petentry)
    {
        delete NewSummon;
        return;
    }

    CreatureCreatePos pos(m_caster, m_caster->Where().Facing());

    Map* map = m_caster->GetMap();
    uint32 pet_number = sMint.PetNumbers().Next();
    if (!NewSummon->Create(map->GenerateLocalLowGuid(HIGHGUID_PET), pos, cInfo, pet_number))
    {
        delete NewSummon;
        return;
    }

    NewSummon->SetSpawn(pos);

    uint32 petlevel = std::max(m_caster->getLevel() + operation.amplitude, 1.0f);
    NewSummon->setPetType(SUMMON_PET);

    uint32 faction = m_caster->getFaction();
    if (IsCreature(m_caster))
    {
        if (((Creature*)m_caster)->IsTotem())
        {
            NewSummon->GetCharmInfo()->SetReactState(REACT_AGGRESSIVE);
        }
        else
        {
            NewSummon->GetCharmInfo()->SetReactState(REACT_DEFENSIVE);
        }
    }

    NewSummon->SetOwnerGuid(m_caster->GetObjectGuid());
    NewSummon->SetCreatorGuid(m_caster->GetObjectGuid());
    NewSummon->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
    NewSummon->setFaction(faction);
    NewSummon->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(nullptr)));
    NewSummon->SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
    NewSummon->SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, 1000);
    NewSummon->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->ID);

    NewSummon->GetCharmInfo()->SetPetNumber(pet_number, true);

    if (m_caster->IsPvP())
    {
        NewSummon->SetPvP(true);
    }

    NewSummon->InitStatsForLevel(petlevel, m_caster);
    NewSummon->InitPetCreateSpells();

    if (NewSummon->getPetType() == SUMMON_PET)
    {

        const auto auraClassScripts = m_caster->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
        for (const auto* script : auraClassScripts)
        {
            if (script->GetModifier()->m_miscvalue == 2228)
            {
                m_caster->RemoveAuras(script->GetId());
            }
        }
    }

    if (IsPlayer(m_caster) && NewSummon->getPetType() == SUMMON_PET)
    {

        std::string new_name = sObjectMgr.GeneratePetName(petentry);
        if (!new_name.empty())
        {
            NewSummon->SetName(new_name);
        }
    }
    else if (NewSummon->getPetType() == HUNTER_PET)
    {
        NewSummon->SetUnitFlag(UNIT_FLAG_RENAME);
    }

    NewSummon->SetHealth(NewSummon->GetMaxHealth());
    NewSummon->SetPower(POWER_MANA, NewSummon->GetMaxPower(POWER_MANA));

    map->Add((Creature*)NewSummon);

    NewSummon->AIM_Initialize();

    m_caster->SetPet(NewSummon);
    DEBUG_LOG("New Pet has guid %u", NewSummon->GetGUIDLow());

    if (IsPlayer(m_caster))
    {
        NewSummon->SavePetToDB(PET_SAVE_AS_CURRENT);
        ((Player*)m_caster)->PetSpellInitialize();
    }
}

void Spell::EffectLearnPetSpell(const cast::Operation& operation)
{
    if (!IsPlayer(m_caster))
    {
        return;
    }

    Player* _player = (Player*)m_caster;

    Pet* pet = _player->GetPet();
    if (!pet)
    {
        return;
    }
    if (!pet->IsAlive())
    {
        return;
    }

    SpellEntry const* learn_spellproto = sSpellStore.LookupEntry(operation.triggerSpell);
    if (!learn_spellproto)
    {
        return;
    }

    pet->SetTP(pet->m_TrainingPoints - pet->GetTPForSpell(learn_spellproto->ID));
    pet->learnSpell(learn_spellproto->ID);

    pet->SavePetToDB(PET_SAVE_AS_CURRENT);
    _player->PetSpellInitialize();

    if (Occupant const* caster = GetCastingObject())
    {
        DEBUG_LOG("Spell: %s has learned spell %u from %s", pet->GetGuidStr().c_str(), learn_spellproto->ID, caster->GetGuidStr().c_str());
    }
}

void Spell::EffectTaunt(const cast::Operation& )
{
    if (!unitTarget)
    {
        return;
    }

    if (!IsPlayer(unitTarget))
    {
        if (unitTarget->getVictim() == m_caster)
        {
            SendCastResult(SPELL_FAILED_DONT_REPORT);
            return;
        }
    }

    if (unitTarget->CanHaveThreatList() && unitTarget->GetThreatManager().getCurrentVictim())
    {
        unitTarget->GetThreatManager().addThreat(m_caster, unitTarget->GetThreatManager().getCurrentVictim()->getThreat());
    }
}

void Spell::EffectWeaponDmg(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!unitTarget)
    {
        return;
    }
    if (!unitTarget->IsAlive())
    {
        return;
    }

    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        switch (Recipe().At(static_cast<uint8>(j)).verb)
        {
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
                if (j < int(eff_idx))
                {
                    return;
                }
                break;
        }
    }

    bool customBonusDamagePercentMod = false;
    float bonusDamagePercentMod  = 1.0f;
    float weaponDamagePercentMod = 1.0f;
    float totalDamagePercentMod  = 1.0f;
    bool normalized = false;

    int32 spell_bonus = 0;

    switch (m_spellInfo->SpellClassSet)
    {
        case SPELLFAMILY_ROGUE:
        {

            if (m_spellInfo->SpellClassMask & UI64LIT(0x00000200))
            {
                customBonusDamagePercentMod = true;
                bonusDamagePercentMod = 2.5f;
            }
            break;
        }
    }

    int32 fixed_bonus = 0;
    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        switch (Recipe().At(static_cast<uint8>(j)).verb)
        {
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
                fixed_bonus += CalculateDamage(SpellEffectIndex(j), unitTarget);
                break;
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                fixed_bonus += CalculateDamage(SpellEffectIndex(j), unitTarget);
                normalized = true;
                break;
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
                weaponDamagePercentMod *= float(CalculateDamage(SpellEffectIndex(j), unitTarget)) / 100.0f;

                if (m_spellInfo->ID == 20424)
                {
                    const auto mModDamagePercentDone = m_caster->GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
                    for (auto* aura : mModDamagePercentDone)
                    {
                        if ((aura->GetModifier()->m_miscvalue & SPELL_SCHOOL_MASK_HOLY) && (aura->GetModifier()->m_miscvalue & SPELL_SCHOOL_MASK_NORMAL) &&
                            aura->GetSpellProto()->EquippedItemClass == -1 &&

                            aura->GetSpellProto()->EquippedItemInvTypes == 0)

                        {
                            totalDamagePercentMod /= (aura->GetModifier()->m_amount + 100.0f) / 100.0f;
                        }
                    }
                }

                if (customBonusDamagePercentMod)
                {
                    fixed_bonus = int32(fixed_bonus * bonusDamagePercentMod);
                }
                else
                {
                    fixed_bonus = int32(fixed_bonus * weaponDamagePercentMod);
                }
                break;
            default:
                break;
        }
    }

    int32 bonus = spell_bonus + fixed_bonus;

    if (bonus)
    {
        UnitMods unitMod;
        switch (Recipe().Swings())
        {
            default:
            case BASE_ATTACK:   unitMod = UNIT_MOD_DAMAGE_MAINHAND; break;
            case OFF_ATTACK:    unitMod = UNIT_MOD_DAMAGE_OFFHAND;  break;
            case RANGED_ATTACK: unitMod = UNIT_MOD_DAMAGE_RANGED;   break;
        }

        float weapon_total_pct  = m_caster->Tallied().Value(unitMod, TOTAL_PCT);
        bonus = int32(bonus * weapon_total_pct);
    }

    bonus += int32(m_caster->CalculateDamage(Recipe().Swings(), normalized) * weaponDamagePercentMod);

    bonus = int32(bonus * totalDamagePercentMod);

    m_damage += uint32(bonus > 0 ? bonus : 0);

    if (m_spellInfo->IsFitToFamily(SPELLFAMILY_DRUID, UI64LIT(0x0000040000000000)))
    {
        if (IsPlayer(m_caster))
        {
            ((Player*)m_caster)->AddComboPoints(unitTarget, 1);
        }
    }
}
