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

#include "Utilities/Errors.h"
#include <algorithm>
#include "Pet.h"
#include "TransportMap.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"
#include "Mint.h"
#include "SpellMgr.h"
#include "Formulas.h"
#include "SpellAuras.h"
#include "Unit.h"
#include "Transports.h"
#include "Movement/Spline/MoveSpline.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "Cast/Recipe/RecipeBook.h"

uint32 const LevelUpLoyalty[6] =
{
    5500,
    11500,
    17000,
    23500,
    31000,
    39500,
};

uint32 const LevelStartLoyalty[6] =
{
    2000,
    4500,
    7000,
    10000,
    13500,
    17500,
};

Pet::Pet(PetType type) : Creature(CREATURE_SUBTYPE_PET),
    m_TrainingPoints(0), m_resetTalentsCost(0), m_resetTalentsTime(0),
    m_removed(false), m_happinessTimer(7500), m_loyaltyTimer(12000), m_petType(type),
    m_loyaltyPoints(0), m_bonusdamage(0), m_auraUpdateMask(0), m_loading(false),
    m_petModeFlags(PET_MODE_DEFAULT), m_sheet(*this), m_pace(*this)
{
    m_name = "Pet";
    m_recovery.NextIn(4000);

    CharmInfo& charmInfo = InitCharmInfo();

    if (type == MINI_PET)
    {
        charmInfo.SetReactState(REACT_PASSIVE);
    }
    else if (type == GUARDIAN_PET)
    {
        charmInfo.SetReactState(REACT_AGGRESSIVE);
    }
}

Pet::~Pet()
{
}

void Pet::AddToWorld()
{

    if (!IsInWorld())
    {
        GetMap()->GetObjectsStore().insert<Pet>(GetObjectGuid(), (Pet*)this);
    }

    Unit::AddToWorld();
}

void Pet::RemoveFromWorld()
{

    if (IsInWorld())
    {
        GetMap()->GetObjectsStore().erase<Pet>(GetObjectGuid(), (Pet*)nullptr);
    }

    Unit::RemoveFromWorld();
}

void Pet::SetDeathState(DeathState s)
{
    Creature::SetDeathState(s);
    if (GetDeathState() == CORPSE)
    {

        if (getPetType() != SUMMON_PET)
        {

            SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
            RemoveUnitFlag(UNIT_FLAG_SKINNABLE);

            MapEntry const* mapEntry = sMapStore.LookupEntry(GetMapId());
            if (!mapEntry || (mapEntry->InstanceType != MAP_BATTLEGROUND))
            {
                ModifyPower(POWER_HAPPINESS, -HAPPINESS_LEVEL_SIZE);
            }

            SetUnitFlag(UNIT_FLAG_STUNNED);
        }
    }
    else if (GetDeathState() == ALIVE)
    {
        RemoveUnitFlag(UNIT_FLAG_STUNNED);
        CastPetAuras(true);
    }
    CastOwnerTalentAuras();
}

void Pet::Update(uint32 update_diff, uint32 diff)
{
    if (m_removed)
    {
        return;
    }

    switch (m_deathState)
    {
        case CORPSE:
        {
            if (getPetType() != HUNTER_PET || Watch().CorpseGoesAt() <= time(nullptr))
            {
                Unsummon(PET_SAVE_NOT_IN_SLOT);
                return;
            }
            break;
        }
        case ALIVE:
        {

            Unit* owner = GetOwner();

            const bool crossingDeck = owner && !Where().ShareFrame(owner->Where()) &&
                                      ((FindMap() && FindMap()->AsTransport()) ||
                                       (owner->FindMap() && owner->FindMap()->AsTransport()));

            if (!owner ||
                (!crossingDeck && !InReach(*this, *owner, GetMap()->GetVisibilityDistance()) && (owner->GetCharmGuid() && (owner->GetCharmGuid() != GetObjectGuid()))) ||
                (isControlled() && !owner->GetPetGuid()))
            {
                Unsummon(PET_SAVE_REAGENTS);
                return;
            }

            if (isControlled())
            {
                if (owner->GetPetGuid() != GetObjectGuid())
                {
                    Unsummon(getPetType() == HUNTER_PET ? PET_SAVE_AS_DELETED : PET_SAVE_NOT_IN_SLOT, owner);
                    return;
                }
            }

            if (Term().Bounded() && Term().RunsOut(update_diff, tenure::BodyOf(*this)))
            {
                Unsummon(getPetType() != SUMMON_PET ? PET_SAVE_AS_DELETED : PET_SAVE_NOT_IN_SLOT, owner);
                return;
            }

            break;
        }
        default:
            break;
    }

    Creature::Update(update_diff, diff);
}

void Pet::RegenerateAll(uint32 update_diff)
{

    m_recovery.Run(update_diff);

    if (m_recovery.Due())
    {
        if (!IsInCombat() || IsPolymorphed())
        {
            RegenerateHealth();
        }

        RegeneratePower();

        m_recovery.NextIn(4000);
    }

    if (getPetType() != HUNTER_PET)
    {
        return;
    }

    if (m_happinessTimer <= update_diff)
    {
        LooseHappiness();
        m_happinessTimer = 7500;
    }
    else
    {
        m_happinessTimer -= update_diff;
    }

    if (m_loyaltyTimer <= update_diff)
    {
        TickLoyaltyChange();
        m_loyaltyTimer = 12000;
    }
    else
    {
        m_loyaltyTimer -= update_diff;
    }
}

void Pet::LooseHappiness()
{
    uint32 curValue = GetPower(POWER_HAPPINESS);
    if (curValue <= 0)
    {
        return;
    }
    int32 addvalue = (140 >> GetLoyaltyLevel()) * 125;
    if (IsInCombat())
    {
        addvalue = int32(addvalue * 1.5);
    }
    ModifyPower(POWER_HAPPINESS, -addvalue);
}

void Pet::ModifyLoyalty(int32 addvalue)
{
    uint32 loyaltylevel = GetLoyaltyLevel();

    if (addvalue > 0)
    {
        addvalue = int32((float)addvalue * sWorld.getConfig(CONFIG_FLOAT_RATE_LOYALTY));
    }

    if (loyaltylevel >= BEST_FRIEND && (addvalue + m_loyaltyPoints) > int32(GetMaxLoyaltyPoints(loyaltylevel)))
    {
        return;
    }

    m_loyaltyPoints += addvalue;

    if (m_loyaltyPoints < 0)
    {
        if (loyaltylevel > REBELLIOUS)
        {

            --loyaltylevel;
            SetLoyaltyLevel(LoyaltyLevel(loyaltylevel));
            m_loyaltyPoints = GetStartLoyaltyPoints(loyaltylevel);
            SetTP(m_TrainingPoints - int32(getLevel()));
        }
        else
        {
            m_loyaltyPoints = 0;
            Unit* owner = GetOwner();
            if (owner &&IsPlayer(owner))
            {
                WorldPacket data(SMSG_PET_BROKEN, 0);
                ((Player*)owner)->GetSession()->SendPacket(&data);

                Unsummon(PET_SAVE_AS_DELETED, owner);
            }
        }
    }

    else if (m_loyaltyPoints > int32(GetMaxLoyaltyPoints(loyaltylevel)))
    {
        ++loyaltylevel;
        SetLoyaltyLevel(LoyaltyLevel(loyaltylevel));
        m_loyaltyPoints = GetStartLoyaltyPoints(loyaltylevel);
        SetTP(m_TrainingPoints + getLevel());
    }
}

void Pet::TickLoyaltyChange()
{
    int32 addvalue;

    switch (GetHappinessState())
    {
        case HAPPY:   addvalue =  20; break;
        case CONTENT: addvalue =  10; break;
        case UNHAPPY: addvalue = -20; break;
        default:
            return;
    }
    ModifyLoyalty(addvalue);
}

void Pet::KillLoyaltyBonus(uint32 level)
{
    if (level > 100)
    {
        return;
    }

    uint32 bonus = uint32(((100 - level) / 10) + (6 - GetLoyaltyLevel()));
    ModifyLoyalty(bonus);
}

HappinessState Pet::GetHappinessState()
{
    if (GetPower(POWER_HAPPINESS) < HAPPINESS_LEVEL_SIZE)
    {
        return UNHAPPY;
    }
    else if (GetPower(POWER_HAPPINESS) >= HAPPINESS_LEVEL_SIZE * 2)
    {
        return HAPPY;
    }
    else
    {
        return CONTENT;
    }
}

void Pet::SetLoyaltyLevel(LoyaltyLevel level)
{
    SetLoyaltyByte(level);
}

bool Pet::CanTakeMoreActiveSpells(uint32 spellid)
{
    uint8  activecount = 1;
    uint32 chainstartstore[ACTIVE_SPELLS_MAX];

    if (cast::Recipes().StartsAs(spellid, cast::Start::Passive))
    {
        return true;
    }

    chainstartstore[0] = sSpellMgr.GetFirstSpellInChain(spellid);

    for (PetSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PETSPELL_REMOVED)
        {
            continue;
        }

        if (cast::Recipes().StartsAs(itr->first, cast::Start::Passive))
        {
            continue;
        }

        uint32 chainstart = sSpellMgr.GetFirstSpellInChain(itr->first);

        uint8 x;

        for (x = 0; x < activecount; ++x)
        {
            if (chainstart == chainstartstore[x])
            {
                break;
            }
        }

        if (x == activecount)
        {
            ++activecount;
            if (activecount > ACTIVE_SPELLS_MAX)
            {
                return false;
            }
            chainstartstore[x] = chainstart;
        }
    }
    return true;
}

bool Pet::HasTPForSpell(uint32 spellid)
{
    int32 neededtrainp = GetTPForSpell(spellid);
    if ((m_TrainingPoints - neededtrainp < 0 || neededtrainp < 0) && neededtrainp != 0)
    {
        return false;
    }
    return true;
}

int32 Pet::GetTPForSpell(uint32 spellid)
{
    uint32 basetrainp = 0;

    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(spellid);
    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        if (!_spell_idx->second->ReqTrainPoints)
        {
            return 0;
        }

        basetrainp = _spell_idx->second->ReqTrainPoints;
        break;
    }

    uint32 spenttrainp = 0;
    uint32 chainstart = sSpellMgr.GetFirstSpellInChain(spellid);

    for (PetSpellMap::iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PETSPELL_REMOVED)
        {
            continue;
        }

        if (sSpellMgr.GetFirstSpellInChain(itr->first) == chainstart)
        {
            SkillLineAbilityMapBounds _bounds = sSpellMgr.GetSkillLineAbilityMapBounds(itr->first);

            for (SkillLineAbilityMap::const_iterator _spell_idx2 = _bounds.first; _spell_idx2 != _bounds.second; ++_spell_idx2)
            {
                if (_spell_idx2->second->ReqTrainPoints > spenttrainp)
                {
                    spenttrainp = _spell_idx2->second->ReqTrainPoints;
                    break;
                }
            }
        }
    }

    return int32(basetrainp) - int32(spenttrainp);
}

uint32 Pet::GetMaxLoyaltyPoints(uint32 level)
{
    if (level < 1)
    {
        level = 1;
    }
    if (level > 6)
    {
        level = 6;
    }
    return LevelUpLoyalty[level - 1];
}

uint32 Pet::GetStartLoyaltyPoints(uint32 level)
{
    if (level < 1)
    {
        level = 1;
    }
    if (level > 6)
    {
        level = 6;
    }
    return LevelStartLoyalty[level - 1];
}

void Pet::SetTP(int32 TP)
{
    m_TrainingPoints = TP;

    SetUInt16Value(UNIT_TRAINING_POINTS, 1, static_cast<uint16>(TP > 0 ? TP : 0));
    SetUInt16Value(UNIT_TRAINING_POINTS, 0, static_cast<uint16>(TP < 0 ? -TP : 0));
}

void Pet::Unsummon(PetSaveMode mode, Unit* owner )
{
    if (!owner)
    {
        owner = GetOwner();
    }

    CombatStop();

    if (owner)
    {
        if (GetOwnerGuid() != owner->GetObjectGuid())
        {
            return;
        }

        Player* p_owner =IsPlayer(owner) ? (Player*)owner : nullptr;

        if (p_owner)
        {

            if (mode == PET_SAVE_AS_CURRENT && p_owner->GetTemporaryUnsummonedPetNumber() &&
                p_owner->GetTemporaryUnsummonedPetNumber() != GetCharmInfo()->GetPetNumber())
            {
                mode = PET_SAVE_NOT_IN_SLOT;
            }

            if (mode == PET_SAVE_REAGENTS)
            {

                uint32 spellId = GetUInt32Value(UNIT_CREATED_BY_SPELL);
                SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

                if (spellInfo)
                {
                    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
                    {
                        if (spellInfo->Reagent[i] > 0)
                        {
                            ItemPosCountVec dest;
                            uint8 msg = p_owner->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, spellInfo->Reagent[i], spellInfo->ReagentCount[i]);
                            if (msg == EQUIP_ERR_OK)
                            {
                                Item* item = p_owner->StoreNewItem(dest, spellInfo->Reagent[i], true);
                                if (p_owner->IsInWorld())
                                {
                                    p_owner->SendNewItem(item, spellInfo->ReagentCount[i], true, false);
                                }
                            }
                        }
                    }
                }
            }

            if (isControlled())
            {
                p_owner->RemovePetActionBar();

                if (p_owner->GetGroup())
                {
                    p_owner->SetGroupUpdateFlag(GROUP_UPDATE_PET);
                }
            }
        }

        switch (getPetType())
        {
            case MINI_PET:
                if (p_owner)
                {
                    p_owner->_SetMiniPet(nullptr);
                }
                break;
            case GUARDIAN_PET:
                owner->Retainers().RemoveGuardian(*this);
                break;
            default:
                if (owner->GetPetGuid() == GetObjectGuid())
                {
                    owner->SetPet(nullptr);
                }
                break;
        }
    }

    SavePetToDB(mode);
    AddObjectToRemoveList();
    m_removed = true;
}

void Pet::GivePetXP(uint32 xp)
{
    xp = uint32(xp * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_PETKILL));
    if (getPetType() != HUNTER_PET)
    {
        return;
    }

    if (xp < 1)
    {
        return;
    }

    if (!IsAlive())
    {
        return;
    }

    uint32 level = getLevel();
    uint32 maxlevel = std::min(sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL), GetOwner()->getLevel());

    if (level >= maxlevel)
    {
        return;
    }

    uint32 nextLvlXP = GetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP);
    uint32 curXP = GetUInt32Value(UNIT_FIELD_PETEXPERIENCE);
    uint32 newXP = curXP + xp;

    while (newXP >= nextLvlXP && level < maxlevel)
    {
        newXP -= nextLvlXP;
        ++level;

        GivePetLevel(level);

        nextLvlXP = GetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP);
    }

    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, level < maxlevel ? newXP : 0);

    if (getPetType() == HUNTER_PET)
    {
        KillLoyaltyBonus(level);
    }
}

void Pet::GivePetLevel(uint32 level)
{
    if (!level || level == getLevel())
    {
        return;
    }

    if (getPetType() == HUNTER_PET)
    {
        SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
        SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(level));
    }

    InitStatsForLevel(level);
    SetTP(m_TrainingPoints + (GetLoyaltyLevel() - 1));
}

bool Pet::CreateBaseAtCreature(Creature* creature)
{
    if (!creature)
    {
        sLog.outError("CRITICAL: nullptr pointer passed into CreateBaseAtCreature()");
        return false;
    }

    CreatureCreatePos pos(creature, creature->Where().Facing());

    uint32 guid = creature->GetMap()->GenerateLocalLowGuid(HIGHGUID_PET);

    uint32 pet_number = sMint.PetNumbers().Next();
    if (!Create(guid, pos, creature->GetCreatureInfo(), pet_number))
    {
        return false;
    }

    CreatureInfo const* cinfo = GetCreatureInfo();
    if (!cinfo)
    {
        sLog.outError("CreateBaseAtCreature() failed, creatureInfo is missing!");
        return false;
    }

    if (cinfo->CreatureType == CREATURE_TYPE_CRITTER)
    {
        setPetType(MINI_PET);
        return true;
    }
    SetDisplayId(creature->GetDisplayId());
    SetNativeDisplayId(creature->GetNativeDisplayId());
    SetMaxPower(POWER_HAPPINESS, GetCreatePowers(POWER_HAPPINESS));
    SetPower(POWER_HAPPINESS, 166500);
    SetPowerType(POWER_FOCUS);
    SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, 0);
    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
    SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(creature->getLevel()));
    SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

    if (CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cinfo->Family))
    {
        SetName(cFamily->Name_lang[sWorld.GetDefaultDbcLocale()]);
    }
    else
    {
        SetName(creature->GetNameForLocaleIdx(sObjectMgr.GetDBCLocaleIndex()));
    }

    m_loyaltyPoints = 1000;
    if (cinfo->CreatureType == CREATURE_TYPE_BEAST)
    {
        SetClass(CLASS_WARRIOR);
        SetGender(GENDER_NONE);
        SetPowerKind(POWER_FOCUS);
        SetSheath(SHEATH_STATE_MELEE);
        SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED | UNIT_FLAG_ABANDON | UNIT_FLAG_RENAME);

        SetCastSpeedMod(creature->GetCastSpeedMod());
        SetLoyaltyLevel(REBELLIOUS);
    }
    return true;
}

bool Pet::InitStatsForLevel(uint32 petlevel, Unit* owner)
{
    CreatureInfo const* cinfo = GetCreatureInfo();
    MANGOS_ASSERT(cinfo);

    if (!owner)
    {
        owner = GetOwner();
        if (!owner)
        {
            sLog.outError("attempt to summon pet (Entry %u) without owner! Attempt terminated.", cinfo->Entry);
            return false;
        }
    }

    uint32 creature_ID = cinfo->Entry;

    switch (getPetType())
    {
        case SUMMON_PET:
            SetClass(CLASS_MAGE);

            SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
            break;
        case HUNTER_PET:
            SetClass(CLASS_WARRIOR);
            SetGender(GENDER_NONE);
            SetSheath(SHEATH_STATE_MELEE);

            SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED | UNIT_FLAG_ABANDON);
            break;
        case GUARDIAN_PET:
        case MINI_PET:
        default:
            break;
    }

    SetLevel(petlevel);

    SetMeleeDamageSchool(SpellSchools(cinfo->DamageSchool));

    Tallied().Value(UNIT_MOD_ARMOR, BASE_VALUE, float(petlevel * 50));

    SetAttackTime(BASE_ATTACK, cinfo->MeleeBaseAttackTime);
    SetAttackTime(OFF_ATTACK, cinfo->MeleeBaseAttackTime);
    SetAttackTime(RANGED_ATTACK, cinfo->RangedBaseAttackTime);

    SetCastSpeedMod(1.0);

    CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cinfo->Family);
    if (cFamily && cFamily->MinScale > 0.0f && getPetType() == HUNTER_PET)
    {
        float scale;
        if (getLevel() >= cFamily->MaxScaleLevel)
        {
            scale = cFamily->MaxScale;
        }
        else if (getLevel() <= cFamily->MinScaleLevel)
        {
            scale = cFamily->MinScale;
        }
        else
        {
            scale = cFamily->MinScale + float(getLevel() - cFamily->MinScaleLevel) / cFamily->MaxScaleLevel * (cFamily->MaxScale - cFamily->MinScale);
        }

        SetObjectScale(scale);
        UpdateModelData();
    }
    m_bonusdamage = 0;

    int32 createResistance[MAX_SPELL_SCHOOL] = {0, 0, 0, 0, 0, 0, 0};

    if (getPetType() != HUNTER_PET)
    {
        createResistance[SPELL_SCHOOL_HOLY]   = cinfo->ResistanceHoly;
        createResistance[SPELL_SCHOOL_FIRE]   = cinfo->ResistanceFire;
        createResistance[SPELL_SCHOOL_NATURE] = cinfo->ResistanceNature;
        createResistance[SPELL_SCHOOL_FROST]  = cinfo->ResistanceFrost;
        createResistance[SPELL_SCHOOL_SHADOW] = cinfo->ResistanceShadow;
        createResistance[SPELL_SCHOOL_ARCANE] = cinfo->ResistanceArcane;
    }

    switch (getPetType())
    {
        case SUMMON_PET:
        {
            if (IsPlayer(owner))
            {
                switch (owner->getClass())
                {
                    case CLASS_WARLOCK:
                    {

                        uint32 fire  = owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FIRE);
                        uint32 shadow = owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_SHADOW);
                        uint32 val  = (fire > shadow) ? fire : shadow;

                        SetBonusDamage(int32(val * 0.15f));

                        break;
                    }
                    case CLASS_MAGE:
                    {

                        float val = owner->GetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + SPELL_SCHOOL_FROST) * 0.4f;
                        if (val < 0)
                        {
                            val = 0;
                        }
                        SetBonusDamage(int32(val));
                        break;
                    }
                    default:
                        break;
                }
            }

            SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(petlevel - (petlevel / 4)));
            SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(petlevel + (petlevel / 4)));

            PetLevelInfo const* pInfo = sObjectMgr.GetPetLevelInfo(creature_ID, petlevel);
            if (pInfo)
            {
                SetCreateHealth(pInfo->health);
                SetCreateMana(pInfo->mana);

                if (pInfo->armor > 0)
                {
                    Tallied().Value(UNIT_MOD_ARMOR, BASE_VALUE, float(pInfo->armor));
                }

                for (int stat = 0; stat < MAX_STATS; ++stat)
                {
                    Tallied().Made(Stats(stat), float(pInfo->stats[stat]));
                }
            }
            else
            {
                sLog.outErrorDb("Summoned pet (Entry: %u) not have pet stats data in DB", cinfo->Entry);

                SetCreateHealth(uint32(((float(cinfo->MaxLevelHealth) / cinfo->MaxLevel) / (1 + 2 * cinfo->Rank)) * petlevel));
                SetCreateMana(uint32(((float(cinfo->MaxLevelMana)   / cinfo->MaxLevel) / (1 + 2 * cinfo->Rank)) * petlevel));

                Tallied().Made(STAT_STRENGTH, 22);
                Tallied().Made(STAT_AGILITY, 22);
                Tallied().Made(STAT_STAMINA, 25);
                Tallied().Made(STAT_INTELLECT, 28);
                Tallied().Made(STAT_SPIRIT, 27);
            }
            break;
        }
        case HUNTER_PET:
        {
            SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(petlevel));

            SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(petlevel - (petlevel / 4)));
            SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(petlevel + (petlevel / 4)));

            PetLevelInfo const* pInfo = sObjectMgr.GetPetLevelInfo(creature_ID, petlevel);
            if (pInfo)
            {
                SetCreateHealth(pInfo->health);
                Tallied().Value(UNIT_MOD_ARMOR, BASE_VALUE, float(pInfo->armor));

                for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
                {
                    Tallied().Made(Stats(i),  float(pInfo->stats[i]));
                }
            }
            else
            {
                sLog.outErrorDb("Hunter pet levelstats missing in DB");

                SetCreateHealth(uint32(((float(cinfo->MaxLevelHealth) / cinfo->MaxLevel) / (1 + 2 * cinfo->Rank)) * petlevel));

                Tallied().Made(STAT_STRENGTH, 22);
                Tallied().Made(STAT_AGILITY, 22);
                Tallied().Made(STAT_STAMINA, 25);
                Tallied().Made(STAT_INTELLECT, 28);
                Tallied().Made(STAT_SPIRIT, 27);
            }
            break;
        }
        case GUARDIAN_PET:
            SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
            SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, 1000);

            SetCreateMana(28 + 10 * petlevel);
            SetCreateHealth(28 + 30 * petlevel);

            SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(petlevel - (petlevel / 4)));

            SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(petlevel + (petlevel / 4)));
            break;
        default:
            sLog.outError("Pet have incorrect type (%u) for levelup.", getPetType());
            break;
    }

    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
    {
        Tallied().Value(UnitMods(UNIT_MOD_RESISTANCE_START + i), BASE_VALUE, float(createResistance[i]));
    }

    Sheet().Everything();

    SetHealth(GetMaxHealth());
    SetPower(GetPowerType(), GetMaxPower(GetPowerType()));

    return true;
}

bool Pet::HaveInDiet(ItemPrototype const* item) const
{
    if (!item->FoodType)
    {
        return false;
    }

    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
    {
        return false;
    }

    CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family);
    if (!cFamily)
    {
        return false;
    }

    uint32 diet = cFamily->PetFoodMask;
    uint32 FoodMask = 1 << (item->FoodType - 1);
    return diet & FoodMask;
}

uint32 Pet::GetCurrentFoodBenefitLevel(uint32 itemlevel)
{

    if (getLevel() <= itemlevel + 5)
    {
        return 35000;
    }

    else if (getLevel() <= itemlevel + 10)
    {
        return 17000;
    }

    else if (getLevel() <= itemlevel + 14)
    {
        return 8000;
    }

    else
    {
        return 0;
    }
}

void Pet::CheckLearning(uint32 spellid)
{

    if (IsPlayer(this) || getPetType() != HUNTER_PET)
    {
        return;
    }

    Unit* owner = GetOwner();

    if (m_teachspells.empty() || !owner || !IsPlayer(owner))
    {
        return;
    }

    TeachSpellMap::iterator itr = m_teachspells.find(spellid);
    if (itr == m_teachspells.end())
    {
        return;
    }

    if (urand(0, 100) < 10)
    {
        ((Player*)owner)->learnSpell(itr->second, false);
        m_teachspells.erase(itr);
    }
}

bool Pet::IsPermanentPetFor(Player* owner)
{
    switch (getPetType())
    {
        case SUMMON_PET:
            switch (owner->getClass())
            {

                case CLASS_WARLOCK:
                    return GetCreatureInfo()->CreatureType == CREATURE_TYPE_DEMON;
                default:
                    return false;
            }
        case HUNTER_PET:
            return true;
        default:
            return false;
    }
}

bool Pet::Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, uint32 pet_number)
{
    SetMap(cPos.GetMap());

    Object::_Create(guidlow, pet_number, HIGHGUID_PET);

    m_originalEntry = cinfo->Entry;

    if (!InitEntry(cinfo->Entry))
    {
        return false;
    }

    cPos.SelectFinalPoint(this);

    if (!cPos.PlaceOn(this))
    {
        return false;
    }

    SetSheath(SHEATH_STATE_MELEE);

    if (getPetType() == MINI_PET)
    {
        SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
    }

    return true;
}

bool Pet::HasSpell(uint32 spell) const
{
    PetSpellMap::const_iterator itr = m_spells.find(spell);
    return (itr != m_spells.end() && itr->second.state != PETSPELL_REMOVED);
}

void Pet::LearnPetPassives()
{
    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
    {
        return;
    }

    CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family);
    if (!cFamily)
    {
        return;
    }

    PetFamilySpellsStore::const_iterator petStore = sPetFamilySpellsStore.find(cFamily->ID);
    if (petStore != sPetFamilySpellsStore.end())
    {
        for (PetFamilySpellsSet::const_iterator petSet = petStore->second.begin(); petSet != petStore->second.end(); ++petSet)
        {
            addSpell(*petSet, ACT_DECIDE, PETSPELL_NEW, PETSPELL_FAMILY);
        }
    }
}

void Pet::CastPetAuras(bool current)
{
    Unit* owner = GetOwner();
    if (!owner || !IsPlayer(owner))
    {
        return;
    }

    if (!IsPermanentPetFor((Player*)owner))
    {
        return;
    }

    PetAuraSet const& granted = owner->Carrying().ForItsPet();
    for (PetAuraSet::const_iterator itr = granted.begin(); itr != granted.end();)
    {
        PetAura const* pa = *itr;
        ++itr;

        if (!current && pa->IsRemovedOnChangePet())
        {
            owner->RemovePetAura(pa);
        }
        else
        {
            CastPetAura(pa);
        }
    }
}

void Pet::CastOwnerTalentAuras()
{
    if (!GetOwner() || !IsPlayer(GetOwner()))
    {
        return;
    }

}

void Pet::CastPetAura(PetAura const* aura)
{
    uint32 auraId = aura->GetAura(GetEntry());
    if (!auraId)
    {
        return;
    }

    if (auraId == 35696)
    {
        int32 basePoints = int32(aura->GetDamage() * (GetStat(STAT_STAMINA) + GetStat(STAT_INTELLECT)) / 100);
        CastCustomSpell(this, auraId, &basePoints, nullptr, nullptr, true);
    }
    else
    {
        CastSpell(this, auraId, true);
    }
}

void Pet::SynchronizeLevelWithOwner()
{
    Unit* owner = GetOwner();
    if (!owner || !IsPlayer(owner))
    {
        return;
    }

    switch (getPetType())
    {

        case SUMMON_PET:
            GivePetLevel(owner->getLevel());
            break;

        case HUNTER_PET:
            if (getLevel() > owner->getLevel())
            {
                GivePetLevel(owner->getLevel());
            }
            break;
        default:
            break;
    }
}

void Pet::ApplyModeFlags(PetModeFlags mode, bool apply)
{
    if (apply)
    {
        m_petModeFlags = PetModeFlags(m_petModeFlags | mode);
    }
    else
    {
        m_petModeFlags = PetModeFlags(m_petModeFlags & ~mode);
    }

    Unit* owner = GetOwner();
    if (!owner || !IsPlayer(owner))
    {
        return;
    }

    WorldPacket data(SMSG_PET_MODE, 12);
    data << GetObjectGuid();
    data << uint32(m_petModeFlags);
    static_cast<Player*>(owner)->SendDirectMessage(&data);
}

PetDatabaseStatus Pet::GetStatusFromDB(Player* owner)
{
    PetDatabaseStatus status = PET_DB_NO_PET;

    uint32 ownerid = owner->GetGUIDLow();

    QueryResult* result;

    result = CharacterDatabase.PQuery("SELECT `id`, `entry`, `owner`, `modelid`, `level`, `exp`, `Reactstate`, `loyaltypoints`, `loyalty`, `trainpoint`, `slot`, `name`, `renamed`, `curhealth`, `curmana`, `curhappiness`, `abdata`, `TeachSpelldata`, `savetime`, `resettalents_cost`, `resettalents_time`, `CreatedBySpell`, `PetType` "
        "FROM `character_pet` WHERE `owner` = %u AND (`slot` = %u OR `slot` > %u)",
        ownerid, PET_SAVE_AS_CURRENT, PET_SAVE_LAST_STABLE_SLOT);
    if (!result)
    {
        return status;
    }

    Field* fields = result->Fetch();

    uint32 petentry = fields[1].GetUInt32();

    if (!petentry)
    {
        delete result;
        return status;
    }

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(petentry);
    if (!creatureInfo)
    {
        delete result;
        return status;
    }

    uint32 savedHP = fields[13].GetUInt32();
    delete result;
    if (savedHP > 0)
    {
        status = PET_DB_ALIVE;
    }
    else
    {
        status = PET_DB_DEAD;
    }

    return status;
}
