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

#include <random>
#include "Summoning.h"
#include "Platform/Define.h"
#include "Utilities/MathDefines.h"
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
#include "Geometry/Vector3.h"
#include "Corpse.h"
#include "Cast/Recipe/RecipeBook.h"

pEffect SpellEffects[TOTAL_SPELL_EFFECTS] =
{
    &Spell::EffectNULL,
    &Spell::EffectInstaKill,
    &Spell::EffectSchoolDMG,
    &Spell::EffectDummy,
    &Spell::EffectUnused,
    &Spell::EffectTeleportUnits,
    &Spell::EffectApplyAura,
    &Spell::EffectEnvironmentalDMG,
    &Spell::EffectPowerDrain,
    &Spell::EffectHealthLeech,
    &Spell::EffectHeal,
    &Spell::EffectBind,
    &Spell::EffectUnused,
    &Spell::EffectUnused,
    &Spell::EffectUnused,
    &Spell::EffectUnused,
    &Spell::EffectQuestComplete,
    &Spell::EffectWeaponDmg,
    &Spell::EffectResurrect,
    &Spell::EffectAddExtraAttacks,
    &Spell::EffectEmpty,
    &Spell::EffectEmpty,
    &Spell::EffectParry,
    &Spell::EffectBlock,
    &Spell::EffectCreateItem,
    &Spell::EffectEmpty,
    &Spell::EffectEmpty,
    &Spell::EffectPersistentAA,
    &Spell::EffectSummon,
    &Spell::EffectLeapForward,
    &Spell::EffectEnergize,
    &Spell::EffectWeaponDmg,
    &Spell::EffectTriggerMissileSpell,
    &Spell::EffectOpenLock,
    &Spell::EffectSummonChangeItem,
    &Spell::EffectApplyAreaAura,
    &Spell::EffectLearnSpell,
    &Spell::EffectEmpty,
    &Spell::EffectDispel,
    &Spell::EffectEmpty,
    &Spell::EffectDualWield,
    &Spell::EffectSummonWild,
    &Spell::EffectSummonGuardian,
    &Spell::EffectTeleUnitsFaceCaster,
    &Spell::EffectLearnSkill,
    &Spell::EffectAddHonor,
    &Spell::EffectNULL,
    &Spell::EffectTradeSkill,
    &Spell::EffectUnused,
    &Spell::EffectUnused,
    &Spell::EffectTransmitted,
    &Spell::EffectUnused,
    &Spell::EffectUnused,
    &Spell::EffectEnchantItemPerm,
    &Spell::EffectEnchantItemTmp,
    &Spell::EffectTameCreature,
    &Spell::EffectSummonPet,
    &Spell::EffectLearnPetSpell,
    &Spell::EffectWeaponDmg,
    &Spell::EffectOpenLock,
    &Spell::EffectProficiency,
    &Spell::EffectSendEvent,
    &Spell::EffectPowerBurn,
    &Spell::EffectThreat,
    &Spell::EffectTriggerSpell,
    &Spell::EffectUnused,
    &Spell::EffectUnused,
    &Spell::EffectHealMaxHealth,
    &Spell::EffectInterruptCast,
    &Spell::EffectDistract,
    &Spell::EffectPull,
    &Spell::EffectPickPocket,
    &Spell::EffectAddFarsight,
    &Spell::EffectSummonPossessed,
    &Spell::EffectSummonTotem,
    &Spell::EffectHealMechanical,
    &Spell::EffectSummonObjectWild,
    &Spell::EffectScriptEffect,
    &Spell::EffectUnused,
    &Spell::EffectSanctuary,
    &Spell::EffectAddComboPoints,
    &Spell::EffectUnused,
    &Spell::EffectNULL,
    &Spell::EffectDuel,
    &Spell::EffectStuck,
    &Spell::EffectSummonPlayer,
    &Spell::EffectActivateObject,
    &Spell::EffectSummonTotem,
    &Spell::EffectSummonTotem,
    &Spell::EffectSummonTotem,
    &Spell::EffectSummonTotem,
    &Spell::EffectUnused,
    &Spell::EffectEnchantHeldItem,
    &Spell::EffectUnused,
    &Spell::EffectSelfResurrect,
    &Spell::EffectSkinning,
    &Spell::EffectCharge,
    &Spell::EffectSummonCritter,
    &Spell::EffectKnockBack,
    &Spell::EffectDisEnchant,
    &Spell::EffectInebriate,
    &Spell::EffectFeedPet,
    &Spell::EffectDismissPet,
    &Spell::EffectReputation,
    &Spell::EffectSummonObject,
    &Spell::EffectSummonObject,
    &Spell::EffectSummonObject,
    &Spell::EffectSummonObject,
    &Spell::EffectDispelMechanic,
    &Spell::EffectSummonDeadPet,
    &Spell::EffectDestroyAllTotems,
    &Spell::EffectDurabilityDamage,
    &Spell::EffectSummonDemon,
    &Spell::EffectResurrectNew,
    &Spell::EffectTaunt,
    &Spell::EffectDurabilityDamagePCT,
    &Spell::EffectSkinPlayerCorpse,
    &Spell::EffectSpiritHeal,
    &Spell::EffectSkill,
    &Spell::EffectApplyAreaAura,
    &Spell::EffectTeleportGraveyard,
    &Spell::EffectWeaponDmg,
    &Spell::EffectUnused,
    &Spell::EffectSendTaxi,
    &Spell::EffectPlayerPull,
    &Spell::EffectModifyThreatPercent,
    &Spell::EffectUnused,
    &Spell::EffectUnused,
    &Spell::EffectUnused,
    &Spell::EffectUnused,
};

void Spell::EffectEmpty(const cast::Operation& )
{

}

void Spell::EffectNULL(const cast::Operation& )
{
    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: Spell Effect DUMMY");
}

void Spell::EffectUnused(const cast::Operation& )
{

}

void Spell::EffectSummon(const cast::Operation& operation)
{
    if (m_caster->GetPetGuid())
    {
        return;
    }

    if (!unitTarget)
    {
        return;
    }

    uint32 pet_entry = operation.miscValue;
    if (!pet_entry)
    {
        return;
    }

    CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(pet_entry);
    if (!cInfo)
    {
        sLog.outErrorDb("Spell::DoSummon: creature entry %u not found for spell %u.", pet_entry, m_spellInfo->ID);
        return;
    }

    uint32 level = m_caster->getLevel();
    Pet* spawnCreature = new Pet(SUMMON_PET);

    if (IsPlayer(m_caster) && spawnCreature->LoadPetFromDB((Player*)m_caster, pet_entry))
    {

        if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
        {
            spawnCreature->Place().MoveTo(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, m_caster->Where().Facing() + M_PI_F);
        }

        if (m_duration > 0)
        {
            spawnCreature->SetDuration(m_duration);
        }

        return;
    }

    CreatureCreatePos pos(m_caster->GetMap(), m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, m_caster->Where().Facing() + M_PI_F);

    if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
    {
        pos = CreatureCreatePos(m_caster, m_caster->Where().Facing() + M_PI_F);
    }

    Map* map = m_caster->GetMap();
    uint32 pet_number = sMint.PetNumbers().Next();
    if (!spawnCreature->Create(map->GenerateLocalLowGuid(HIGHGUID_PET), pos, cInfo, pet_number))
    {
        sLog.outErrorDb("Spell::EffectSummon: can't create creature with entry %u for spell %u", cInfo->Entry, m_spellInfo->ID);
        delete spawnCreature;
        return;
    }

    spawnCreature->SetSpawn(pos);

    if (m_duration > 0)
    {
        spawnCreature->SetDuration(m_duration);
    }

    spawnCreature->SetOwnerGuid(m_caster->GetObjectGuid());
    spawnCreature->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
    spawnCreature->SetPowerType(POWER_MANA);
    spawnCreature->setFaction(m_caster->getFaction());
    spawnCreature->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, 0);
    spawnCreature->SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
    spawnCreature->SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, 1000);
    spawnCreature->SetCreatorGuid(m_caster->GetObjectGuid());
    spawnCreature->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->ID);

    spawnCreature->InitStatsForLevel(level, m_caster);

    spawnCreature->GetCharmInfo()->SetPetNumber(pet_number, false);

    spawnCreature->SetHealth(spawnCreature->GetMaxHealth());
    spawnCreature->SetPower(POWER_MANA, spawnCreature->GetMaxPower(POWER_MANA));

    map->Add((Creature*)spawnCreature);

    spawnCreature->AIM_Initialize();
    spawnCreature->InitPetCreateSpells();

    m_caster->SetPet(spawnCreature);

    if (IsPlayer(m_caster))
    {
        spawnCreature->GetCharmInfo()->SetReactState(REACT_DEFENSIVE);
        spawnCreature->SavePetToDB(PET_SAVE_AS_CURRENT);
        ((Player*)m_caster)->PetSpellInitialize();
    }

    if (IsCreature(m_caster) && ((Creature*)m_caster)->AI())
    {
        ((Creature*)m_caster)->AI()->JustSummoned((Creature*)spawnCreature);
    }
    if (m_originalCaster && m_originalCaster != m_caster &&IsCreature(m_originalCaster) && ((Creature*)m_originalCaster)->AI())
    {
        ((Creature*)m_originalCaster)->AI()->JustSummoned((Creature*)spawnCreature);
    }
}

void Spell::EffectSummonWild(const cast::Operation& operation)
{
    uint32 creature_entry = operation.miscValue;
    if (!creature_entry)
    {
        return;
    }

    uint32 level = m_caster->getLevel();

    if (IsPlayer(m_caster) && m_CastItem)
    {
        ItemPrototype const* proto = m_CastItem->GetProto();
        if (proto && proto->RequiredSkill == SKILL_ENGINEERING)
        {
            uint16 skill202 = ((Player*)m_caster)->GetSkillValue(SKILL_ENGINEERING);
            if (skill202)
            {
                level = skill202 / 5;
            }
        }
    }

    float center_x = m_targets.m_destX;
    float center_y = m_targets.m_destY;
    float center_z = m_targets.m_destZ;

    float radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(operation.radiusIndex));
    int32 duration = Recipe().DurationMs();
    TempSpawnType summonType = (duration == 0) ? TEMPSPAWN_DEAD_DESPAWN : TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN;

    int32 amount = damage > 0 ? damage : 1;

    for (int32 count = 0; count < amount; ++count)
    {
        float px, py, pz;

        if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
        {

            if (count == 0)
            {
                px = m_targets.m_destX;
                py = m_targets.m_destY;
                pz = m_targets.m_destZ;
            }

            else
            {
                const Geometry::Vector3 spot = RandomGroundPointNear(*m_caster,
                    Geometry::Vector3(center_x, center_y, center_z), radius);
                px = spot.x;
                py = spot.y;
                pz = spot.z;
            }
        }

        else
        {
            if (radius > 0.0f)
            {

                ClosePointNear(*m_caster, px, py, pz, 0.0f, radius);
            }
            else
            {

                px = m_caster->Where().X();
                py = m_caster->Where().Y();
                pz = m_caster->Where().Z();
            }
        }

        if (Creature* summon = SummonCreature(*m_caster, creature_entry, px, py, pz, m_caster->Where().Facing(), summonType, duration))
        {
            summon->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->ID);

            if (m_originalCaster && m_originalCaster != m_caster &&IsCreature(m_originalCaster) && ((Creature*)m_originalCaster)->AI())
            {
                ((Creature*)m_originalCaster)->AI()->JustSummoned(summon);
            }
        }
    }
}

void Spell::EffectSummonGuardian(const cast::Operation& operation)
{
    uint32 pet_entry = operation.miscValue;
    if (!pet_entry)
    {
        return;
    }

    CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(pet_entry);
    if (!cInfo)
    {
        sLog.outErrorDb("Spell::DoSummonGuardian: creature entry %u not found for spell %u.", pet_entry, m_spellInfo->ID);
        return;
    }

    int32 duration = CalculateSpellDuration(m_spellInfo, m_caster);

    if (IsPlayer(m_caster) && (duration <= 0 || GetSpellRecoveryTime(m_spellInfo) == 0))
    {
        if (m_caster->Retainers().GuardianOfEntry(pet_entry))
        {
            return;
        }
    }

    uint32 level = m_caster->getLevel();

    if (IsPlayer(m_caster) && m_CastItem)
    {
        ItemPrototype const* proto = m_CastItem->GetProto();
        if (proto && proto->RequiredSkill == SKILL_ENGINEERING)
        {
            uint16 skill202 = ((Player*)m_caster)->GetSkillValue(SKILL_ENGINEERING);
            if (skill202)
            {
                level = skill202 / 5;
            }
        }
    }

    float center_x = m_targets.m_destX;
    float center_y = m_targets.m_destY;
    float center_z = m_targets.m_destZ;

    float radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(operation.radiusIndex));

    int32 amount = damage > 0 ? damage : 1;

    for (int32 count = 0; count < amount; ++count)
    {
        Pet* spawnCreature = new Pet(GUARDIAN_PET);

        CreatureCreatePos pos(m_caster->GetMap(), m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, m_caster->Where().Facing() + M_PI_F);

        if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
        {

            if (count > 0)
            {
                float x, y, z;
                const Geometry::Vector3 spot = RandomGroundPointNear(*m_caster,
                    Geometry::Vector3(center_x, center_y, center_z), radius);
                x = spot.x;
                y = spot.y;
                z = spot.z;
                pos = CreatureCreatePos(m_caster->GetMap(), x, y, z, m_caster->Where().Facing());
            }
        }

        else
        {
            pos = CreatureCreatePos(m_caster, m_caster->Where().Facing());
        }

        Map* map = m_caster->GetMap();
        uint32 pet_number = sMint.PetNumbers().Next();
        if (!spawnCreature->Create(map->GenerateLocalLowGuid(HIGHGUID_PET), pos, cInfo, pet_number))
        {
            sLog.outError("Spell::DoSummonGuardian: can't create creature entry %u for spell %u.", pet_entry, m_spellInfo->ID);
            delete spawnCreature;
            return;
        }

        spawnCreature->SetSpawn(pos);

        if (m_duration > 0)
        {
            spawnCreature->SetDuration(m_duration);
        }

        spawnCreature->SetOwnerGuid(m_caster->GetObjectGuid());
        spawnCreature->SetPowerType(POWER_MANA);
        spawnCreature->SetUInt32Value(UNIT_NPC_FLAGS, spawnCreature->GetCreatureInfo()->NpcFlags);
        spawnCreature->setFaction(m_caster->getFaction());
        spawnCreature->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, 0);
        spawnCreature->SetCreatorGuid(m_caster->GetObjectGuid());
        spawnCreature->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->ID);

        spawnCreature->InitStatsForLevel(level, m_caster);
        spawnCreature->GetCharmInfo()->SetPetNumber(pet_number, false);

        m_caster->Retainers().AddGuardian(*spawnCreature);

        map->Add((Creature*)spawnCreature);

        spawnCreature->AIM_Initialize();

        if (IsCreature(m_caster) && ((Creature*)m_caster)->AI())
        {
            ((Creature*)m_caster)->AI()->JustSummoned(spawnCreature);
        }
        if (m_originalCaster && m_originalCaster != m_caster &&IsCreature(m_originalCaster) && ((Creature*)m_originalCaster)->AI())
        {
            ((Creature*)m_originalCaster)->AI()->JustSummoned(spawnCreature);
        }
    }
}

void Spell::EffectAddHonor(const cast::Operation& )
{
    if (!IsPlayer(unitTarget))
    {
        return;
    }

    ((Player*)unitTarget)->AddHonorCP(float(damage), HONORABLE, 0, 0);
    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "SpellEffect::AddHonor (spell_id %u) rewards %u honor points (non scale) for player: %u", m_spellInfo->ID, damage, ((Player*)unitTarget)->GetGUIDLow());
}

void Spell::EffectSummonTotem(const cast::Operation& operation)
{
    int slot = 0;
    switch (operation.verb)
    {
        case SPELL_EFFECT_SUMMON_TOTEM:       slot = TOTEM_SLOT_NONE;  break;
        case SPELL_EFFECT_SUMMON_TOTEM_SLOT1: slot = TOTEM_SLOT_FIRE;  break;
        case SPELL_EFFECT_SUMMON_TOTEM_SLOT2: slot = TOTEM_SLOT_EARTH; break;
        case SPELL_EFFECT_SUMMON_TOTEM_SLOT3: slot = TOTEM_SLOT_WATER; break;
        case SPELL_EFFECT_SUMMON_TOTEM_SLOT4: slot = TOTEM_SLOT_AIR;   break;
        default: return;
    }

    if (slot < MAX_TOTEM_SLOT)
    {
        if (Totem* OldTotem = m_caster->Retainers().TotemIn(TotemSlot(slot)))
        {
            OldTotem->UnSummon();
        }
    }

    float angle = slot < MAX_TOTEM_SLOT ? M_PI_F / MAX_TOTEM_SLOT - (slot * 2 * M_PI_F / MAX_TOTEM_SLOT) : 0;

    CreatureCreatePos pos(m_caster, m_caster->Where().Facing(), 2.0f, angle);

    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(operation.miscValue);
    if (!cinfo)
    {
        sLog.outErrorDb("Creature entry %u does not exist but used in spell %u totem summon.", m_spellInfo->ID, operation.miscValue);
        return;
    }

    Totem* pTotem = new Totem;

    if (!pTotem->Create(m_caster->GetMap()->GenerateLocalLowGuid(HIGHGUID_UNIT), pos, cinfo, m_caster))
    {
        delete pTotem;
        return;
    }

    pTotem->SetSpawn(pos);

    if (slot < MAX_TOTEM_SLOT)
    {
        m_caster->Retainers().PutTotem(TotemSlot(slot), *pTotem);
    }

    pTotem->SetOwner(m_caster);
    pTotem->SetTypeBySummonSpell(m_spellInfo);

    pTotem->SetDuration(m_duration);

    if (damage)
    {
        pTotem->SetMaxHealth(damage);
        pTotem->SetHealth(damage);
    }

    pTotem->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->ID);

    if (IsPlayer(m_caster))
    {
        pTotem->SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);
    }

    if (m_caster->IsPvP())
    {
        pTotem->SetPvP(true);
    }

    pTotem->Summon(m_caster);
}

void Spell::EffectSummonPossessed(const cast::Operation& operation)
{
    uint32 creatureEntry = operation.miscValue;
    CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(creatureEntry);
    if (!cInfo)
    {
        sLog.outErrorDb("Spell::DoSummonPossessed: creature entry %u not found for spell %u.", creatureEntry, m_spellInfo->ID);
        return;
    }

    Creature* spawnCreature = SummonCreature(*m_caster, creatureEntry, m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, m_caster->Where().Facing(), TEMPSPAWN_CORPSE_DESPAWN, 0);
    if (!spawnCreature)
    {
        sLog.outError("Spell::DoSummonPossessed: creature entry %u for spell %u could not be summoned.", creatureEntry, m_spellInfo->ID);
        return;
    }

    spawnCreature->SetCharmerGuid(m_caster->GetObjectGuid());
    spawnCreature->SetCreatorGuid(m_caster->GetObjectGuid());
    spawnCreature->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->ID);
    spawnCreature->SetUnitFlag(UNIT_FLAG_POSSESSED);

    spawnCreature->SetLevel(m_caster->getLevel());

    spawnCreature->SetWalk(m_caster->IsWalking());

    spawnCreature->addUnitState(UNIT_STAT_CONTROLLED);

    if (IsPlayer(m_caster))
    {
        Player* player = (Player*)m_caster;

        player->GetCamera().SetView(spawnCreature);

        player->SetCharm(spawnCreature);
        player->SetClientControl(spawnCreature, 1);
        player->SetMover(spawnCreature);

        spawnCreature->InitCharmInfo().InitPossessCreateSpells();
        player->PossessSpellInitialize();
    }

    if (m_originalCaster && m_originalCaster != m_caster &&IsCreature(m_originalCaster) && ((Creature*)m_originalCaster)->AI())
    {
        ((Creature*)m_originalCaster)->AI()->JustSummoned(spawnCreature);
    }
}

void Spell::EffectSummonCritter(const cast::Operation& operation)
{
    if (!IsPlayer(m_caster))
    {
        return;
    }
    Player* player = (Player*)m_caster;

    uint32 pet_entry = operation.miscValue;
    if (!pet_entry)
    {
        return;
    }

    CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(pet_entry);
    if (!cInfo)
    {
        sLog.outErrorDb("Spell::DoSummonCritter: creature entry %u not found for spell %u.", pet_entry, m_spellInfo->ID);
        return;
    }

    Pet* old_critter = player->GetMiniPet();

    if (old_critter && old_critter->GetEntry() == pet_entry)
    {
        player->RemoveMiniPet();
        return;
    }

    if (old_critter)
    {
        player->RemoveMiniPet();
    }

    CreatureCreatePos pos(m_caster->GetMap(), m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, m_caster->Where().Facing());
    if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
    {
        pos = CreatureCreatePos(m_caster, m_caster->Where().Facing());
    }

    Pet* critter = new Pet(MINI_PET);

    Map* map = m_caster->GetMap();
    uint32 pet_number = sMint.PetNumbers().Next();
    if (!critter->Create(map->GenerateLocalLowGuid(HIGHGUID_PET), pos, cInfo, pet_number))
    {
        sLog.outError("Spell::EffectSummonCritter, spellid %u: no such creature entry %u", m_spellInfo->ID, pet_entry);
        delete critter;
        return;
    }

    critter->SetSpawn(pos);

    critter->SetOwnerGuid(m_caster->GetObjectGuid());
    critter->SetCreatorGuid(m_caster->GetObjectGuid());
    critter->setFaction(m_caster->getFaction());
    critter->SetUInt32Value(UNIT_CREATED_BY_SPELL, m_spellInfo->ID);
    critter->SelectLevel();
    critter->SetUInt32Value(UNIT_NPC_FLAGS, critter->GetCreatureInfo()->NpcFlags);

    int32 duration = Recipe().DurationMs();
    if (duration > 0)
    {
        critter->SetDuration(duration);
    }

    player->_SetMiniPet(critter);

    map->Add((Creature*)critter);

    critter->AIM_Initialize();
    critter->InitPetCreateSpells();

    if (IsCreature(m_caster) && ((Creature*)m_caster)->AI())
    {
        ((Creature*)m_caster)->AI()->JustSummoned(critter);
    }
    if (m_originalCaster && m_originalCaster != m_caster &&IsCreature(m_originalCaster) && ((Creature*)m_originalCaster)->AI())
    {
        ((Creature*)m_originalCaster)->AI()->JustSummoned(critter);
    }
}

void Spell::EffectSummonDemon(const cast::Operation& operation)
{
    float px = m_targets.m_destX;
    float py = m_targets.m_destY;
    float pz = m_targets.m_destZ;

    Creature* Charmed = SummonCreature(*m_caster, operation.miscValue, px, py, pz, m_caster->Where().Facing(), TEMPSPAWN_TIMED_OR_DEAD_DESPAWN, 3600000);
    if (!Charmed)
    {
        return;
    }

    Charmed->SetLevel(m_caster->getLevel());

    if (operation.miscValue == 89)
    {

        m_caster->CastSpell(Charmed, 20882, true);

        Charmed->CastSpell(Charmed, 22703, true, 0);
    }
}

void Spell::EffectTeleportGraveyard(const cast::Operation& )
{
    if (!unitTarget || !IsPlayer(unitTarget) || !unitTarget->GetMap()->IsBattleGround())
    {
        return;
    }

    Player* player = static_cast<Player*>(unitTarget);
    player->RepopAtGraveyard();
}
