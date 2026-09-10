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

#pragma once

#include <unordered_map>
#include <utility>
#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include <cstring>
#include <set>
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "DBCStructure.h"
#include "DBCStores.h"

#include <map>
#include "Cast/Recipe/RecipeBook.h"

class Player;
class Spell;
class Unit;
struct SpellModifier;

enum SpellCategories
{
    SPELLCATEGORY_HEALTH_MANA_POTIONS = 4,
    SPELLCATEGORY_FOOD                = 11,
    SPELLCATEGORY_DEVOUR_MAGIC        = 12,
    SPELLCATEGORY_DRINK               = 59
};

enum SpellSpecific
{
    SPELL_NORMAL            = 0,
    SPELL_SEAL              = 1,
    SPELL_BLESSING          = 2,
    SPELL_AURA              = 3,
    SPELL_STING             = 4,
    SPELL_CURSE             = 5,
    SPELL_ASPECT            = 6,
    SPELL_TRACKER           = 7,
    SPELL_WARLOCK_ARMOR     = 8,
    SPELL_MAGE_ARMOR        = 9,
    SPELL_ELEMENTAL_SHIELD  = 10,
    SPELL_MAGE_POLYMORPH    = 11,
    SPELL_POSITIVE_SHOUT    = 12,
    SPELL_JUDGEMENT         = 13,
    SPELL_BATTLE_ELIXIR     = 14,
    SPELL_GUARDIAN_ELIXIR   = 15,
    SPELL_FLASK_ELIXIR      = 16,

    SPELL_WELL_FED          = 19,
    SPELL_FOOD              = 20,
    SPELL_DRINK             = 21,
    SPELL_FOOD_AND_DRINK    = 22,
};

SpellSpecific GetSpellSpecific(uint32 spellId);

inline float GetSpellRadius(SpellRadiusEntry const* radius) { return (radius ? radius->Radius : 0); }

uint32 GetSpellCastTime(SpellEntry const* spellInfo, Spell const* spell = nullptr);

inline float GetSpellMinRange(SpellRangeEntry const* range)
{
    return (range ? range->RangeMin : 0);
}
inline float GetSpellMaxRange(SpellRangeEntry const* range)
{
    return (range ? range->RangeMax : 0);
}
inline uint32 GetSpellRecoveryTime(SpellEntry const* spellInfo) { return spellInfo->RecoveryTime > spellInfo->CategoryRecoveryTime ? spellInfo->RecoveryTime : spellInfo->CategoryRecoveryTime; }

int32 GetSpellDuration(SpellEntry const* spellInfo);

int32 GetSpellMaxDuration(SpellEntry const* spellInfo);

int32 CalculateSpellDuration(SpellEntry const* spellInfo, Unit const* caster = nullptr);

inline bool IsSpellHaveEffect(SpellEntry const* spellInfo, SpellEffects effect)
{
    return spellInfo ? spellInfo->HasSpellEffect(effect) : false;
}

inline bool IsAuraApplyEffect(SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    switch (spellInfo->Effect[effecIdx])
    {
        case SPELL_EFFECT_APPLY_AURA:
        case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
        case SPELL_EFFECT_APPLY_AREA_AURA_PET:
            return true;
    }
    return false;
}

inline bool IsSpellAppliesAura(SpellEntry const* spellInfo, uint32 effectMask = ((1 << EFFECT_INDEX_0) | (1 << EFFECT_INDEX_1) | (1 << EFFECT_INDEX_2)))
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (effectMask & (1 << i))
        {
            if (IsAuraApplyEffect(spellInfo, SpellEffectIndex(i)))
            {
                return true;
            }
        }
    }
    return false;
}

inline bool IsEffectHandledOnDelayedSpellLaunch(SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    switch (spellInfo->Effect[effecIdx])
    {
        case SPELL_EFFECT_SCHOOL_DAMAGE:
        case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
        case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
        case SPELL_EFFECT_WEAPON_DAMAGE:
        case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
            return true;
        default:
            return false;
    }
}

inline bool IsPeriodicRegenerateEffect(SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    switch (AuraType(spellInfo->EffectAura[effecIdx]))
    {
        case SPELL_AURA_PERIODIC_ENERGIZE:
        case SPELL_AURA_PERIODIC_HEAL:
        case SPELL_AURA_PERIODIC_HEALTH_FUNNEL:
            return true;
        default:
            return false;
    }
}

inline bool IsSpellHaveAura(SpellEntry const* spellInfo, AuraType aura, uint32 effectMask = (1 << EFFECT_INDEX_0) | (1 << EFFECT_INDEX_1) | (1 << EFFECT_INDEX_2))
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (effectMask & (1 << i))
        {
            if (AuraType(spellInfo->EffectAura[i]) == aura)
            {
                return true;
            }
        }
    }
    return false;
}

inline bool IsSpellLastAuraEffect(SpellEntry const* spellInfo, SpellEffectIndex effecIdx)
{
    for (int i = effecIdx + 1; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo->EffectAura[i])
        {
            return false;
        }
    }
    return true;
}

bool IsNoStackAuraDueToAura(uint32 spellId_1, uint32 spellId_2);

inline bool IsSealSpell(SpellEntry const* spellInfo)
{

    return spellInfo->IsFitToFamily(SPELLFAMILY_PALADIN, UI64LIT(0x0000000008000200));
}

inline bool IsElementalShield(SpellEntry const* spellInfo)
{

    return (spellInfo->SpellClassMask & UI64LIT(0x00000000400)) || spellInfo->ID == 23552;
}

int32 CompareAuraRanks(uint32 spellId_1, uint32 spellId_2);

bool IsSingleFromSpellSpecificPerTargetPerCaster(SpellSpecific spellSpec1, SpellSpecific spellSpec2);

bool IsSingleFromSpellSpecificSpellRanksPerTarget(SpellSpecific spellSpec1, SpellSpecific spellSpec2);

bool IsSingleFromSpellSpecificPerTarget(SpellSpecific spellSpec1, SpellSpecific spellSpec2);

inline bool IsPassiveSpellStackableWithRanks(SpellEntry const* spellProto)
{
    if (!(cast::RecipeOf(*spellProto).Starts() == cast::Start::Passive))
    {
        return false;
    }

    return !spellProto->HasSpellEffect(SPELL_EFFECT_APPLY_AURA);
}

inline bool IsDeathOnlySpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX3_CAST_ON_DEAD) || spellInfo->ID == 2584;
}

inline bool IsDeathPersistentSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX3_DEATH_PERSISTENT);
}

inline bool IsNonCombatSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_CANT_USED_IN_COMBAT);
}

bool IsPositiveEffect(SpellEntry const* spellInfo, SpellEffectIndex effIndex);

bool IsPositiveTarget(uint32 targetA, uint32 targetB);

bool IsExplicitPositiveTarget(uint32 targetA);

bool IsExplicitNegativeTarget(uint32 targetA);

bool IsSingleTargetSpell(SpellEntry const* spellInfo);

bool IsSingleTargetSpells(SpellEntry const* spellInfo1, SpellEntry const* spellInfo2);

inline bool IsCasterSourceTarget(uint32 target)
{
    switch (target)
    {
        case TARGET_SELF:
        case TARGET_PET:
        case TARGET_ALL_PARTY_AROUND_CASTER:
        case TARGET_IN_FRONT_OF_CASTER:
        case TARGET_MASTER:
        case TARGET_MINION:
        case TARGET_ALL_PARTY:
        case TARGET_ALL_PARTY_AROUND_CASTER_2:
        case TARGET_SELF_FISHING:
        case TARGET_TOTEM_EARTH:
        case TARGET_TOTEM_WATER:
        case TARGET_TOTEM_AIR:
        case TARGET_TOTEM_FIRE:
        case TARGET_AREAEFFECT_GO_AROUND_DEST:
            return true;
        default:
            break;
    }
    return false;
}

inline bool IsSpellWithCasterSourceTargetsOnly(SpellEntry const* spellInfo)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!spellInfo->Effect[i])
        {
            continue;
        }

        uint32 targetA = spellInfo->ImplicitTargetA[i];
        if (targetA && !IsCasterSourceTarget(targetA))
        {
            return false;
        }

        uint32 targetB = spellInfo->ImplicitTargetB[i];
        if (targetB && !IsCasterSourceTarget(targetB))
        {
            return false;
        }

        if (!targetA && !targetB)
        {
            return false;
        }
    }
    return true;
}

inline bool IsPointEffectTarget(Targets target)
{
    switch (target)
    {
        case TARGET_INNKEEPER_COORDINATES:
        case TARGET_TABLE_X_Y_Z_COORDINATES:
        case TARGET_CASTER_COORDINATES:
        case TARGET_SCRIPT_COORDINATES:
        case TARGET_CURRENT_ENEMY_COORDINATES:
        case TARGET_DUELVSPLAYER_COORDINATES:
            return true;
        default:
            break;
    }
    return false;
}

inline bool IsAreaEffectPossitiveTarget(Targets target)
{
    switch (target)
    {
        case TARGET_ALL_PARTY_AROUND_CASTER:
        case TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER:
        case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
        case TARGET_ALL_PARTY:
        case TARGET_ALL_PARTY_AROUND_CASTER_2:
        case TARGET_AREAEFFECT_PARTY:
        case TARGET_ALL_RAID_AROUND_CASTER:
        case TARGET_AREAEFFECT_PARTY_AND_CLASS:
            return true;
        default:
            break;
    }
    return false;
}

inline bool IsAreaEffectTarget(Targets target)
{
    switch (target)
    {
        case TARGET_AREAEFFECT_INSTANT:
        case TARGET_AREAEFFECT_CUSTOM:
        case TARGET_ALL_ENEMY_IN_AREA:
        case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
        case TARGET_ALL_PARTY_AROUND_CASTER:
        case TARGET_IN_FRONT_OF_CASTER:
        case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
        case TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER:
        case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
        case TARGET_ALL_PARTY:
        case TARGET_ALL_PARTY_AROUND_CASTER_2:
        case TARGET_AREAEFFECT_PARTY:
        case TARGET_AREAEFFECT_GO_AROUND_DEST:
        case TARGET_ALL_RAID_AROUND_CASTER:
        case TARGET_AREAEFFECT_PARTY_AND_CLASS:
            return true;
        default:
            break;
    }
    return false;
}

inline bool IsSpellRemovedOnEvade(SpellEntry const* spellInfo)
{

    switch (spellInfo->ID)
    {
        case 588:
        case 3235:
        case 3284:
        case 3417:
        case 3418:
        case 3616:
        case 3637:
        case 5111:
        case 5301:
        case 5680:
        case 6718:
        case 6752:
        case 6947:
        case 7090:
        case 7165:
        case 7276:
        case 8247:
        case 8279:
        case 8393:
        case 8599:
        case 8601:
        case 8876:
        case 9205:
        case 9460:
        case 9941:
        case 10022:
        case 10072:
        case 10074:
        case 10095:
        case 11838:
        case 11919:
        case 11966:
        case 11984:
        case 12099:
        case 12246:
        case 12529:
        case 12539:
        case 12546:
        case 12556:
        case 12627:
        case 12787:
        case 12898:
        case 13299:
        case 13616:
        case 13767:
        case 14178:
        case 15088:
        case 15097:
        case 15876:
        case 16140:
        case 16563:
        case 16577:
        case 16592:
        case 17327:
        case 17467:
        case 18148:
        case 18268:
        case 18943:
        case 18968:
        case 19030:
        case 18950:
        case 19194:
        case 19195:
        case 19396:
        case 19626:
        case 19640:
        case 19817:
        case 19818:
        case 20514:
        case 21061:
        case 21857:
        case 22128:
        case 22578:
        case 22735:
        case 22781:
        case 22788:
        case 22856:
        case 25592:
        case 26341:
        case 27987:
        case 28126:
        case 29526:
            return false;
        default:
            return true;
    }
}

inline bool IsAreaOfEffectSpell(SpellEntry const* spellInfo)
{
    if (IsAreaEffectTarget(Targets(spellInfo->ImplicitTargetA[EFFECT_INDEX_0])) || IsAreaEffectTarget(Targets(spellInfo->ImplicitTargetB[EFFECT_INDEX_0])))
    {
        return true;
    }
    if (IsAreaEffectTarget(Targets(spellInfo->ImplicitTargetA[EFFECT_INDEX_1])) || IsAreaEffectTarget(Targets(spellInfo->ImplicitTargetB[EFFECT_INDEX_1])))
    {
        return true;
    }
    if (IsAreaEffectTarget(Targets(spellInfo->ImplicitTargetA[EFFECT_INDEX_2])) || IsAreaEffectTarget(Targets(spellInfo->ImplicitTargetB[EFFECT_INDEX_2])))
    {
        return true;
    }
    return false;
}

inline bool IsAreaAuraEffect(uint32 effect)
{
    if (effect == SPELL_EFFECT_APPLY_AREA_AURA_PARTY    ||
        effect == SPELL_EFFECT_APPLY_AREA_AURA_PET)
    {
        return true;
    }
    return false;
}

inline bool HasAreaAuraEffect(SpellEntry const* spellInfo)
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (IsAreaAuraEffect(spellInfo->Effect[i]))
        {
            return true;
        }
    }
    return false;
}

inline bool HasAuraWithTriggerEffect(SpellEntry const* spellInfo)
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (spellInfo->Effect[i])
        {
            case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
            case SPELL_AURA_PROC_TRIGGER_SPELL:
            case SPELL_AURA_PROC_TRIGGER_DAMAGE:
                return true;
        }
    }
    return false;
}

inline bool IsOnlySelfTargeting(SpellEntry const* spellInfo)
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!spellInfo->Effect[i])
        {
            return true;
        }

        switch (spellInfo->ImplicitTargetA[i])
        {
            case TARGET_SELF:
                break;
            default:
                return false;
        }
        switch (spellInfo->ImplicitTargetB[i])
        {
            case TARGET_SELF:
            case TARGET_NONE:
                break;
            default:
                return false;
        }
    }
    return true;
}

inline bool IsDismountSpell(SpellEntry const* spellInfo)
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if ((spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AURA) && (spellInfo->EffectAura[i] == SPELL_AURA_MECHANIC_IMMUNITY) && (spellInfo->EffectMiscValue[i] == MECHANIC_MOUNT))
        {
            return true;
        }
    }
    return false;
}

inline bool IsDispelSpell(SpellEntry const* spellInfo)
{
    return spellInfo->HasSpellEffect(SPELL_EFFECT_DISPEL);
}

inline bool isSpellBreakStealth(SpellEntry const* spellInfo)
{
    return !spellInfo->HasAttribute(SPELL_ATTR_EX_NOT_BREAK_STEALTH);
}

inline bool IsSpellRequiresRangedAP(SpellEntry const* spellInfo)
{
    return (spellInfo->SpellClassSet == SPELLFAMILY_HUNTER && spellInfo->DefenseType != SPELL_DAMAGE_CLASS_MELEE);
}

SpellCastResult GetErrorAtShapeshiftedCast(SpellEntry const* spellInfo, uint32 form);

inline bool IsNeedCastSpellAtFormApply(SpellEntry const* spellInfo, ShapeshiftForm form)
{
    if ((!spellInfo->HasAttribute(SPELL_ATTR_PASSIVE) && !spellInfo->HasAttribute(SPELL_ATTR_HIDDEN_CLIENTSIDE)) || !form)
    {
        return false;
    }

    return ((spellInfo->ShapeshiftMask & (1 << (form - 1))  || (spellInfo->ID == 24864 && form == FORM_CAT)) &&
        !spellInfo->HasAttribute(SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT));
}

inline bool IsNeedCastSpellAtOutdoor(SpellEntry const* spellInfo)
{
    return (spellInfo->HasAttribute(SPELL_ATTR_OUTDOORS_ONLY) && spellInfo->HasAttribute(SPELL_ATTR_PASSIVE));
}

inline bool IsTargetPowerTypeValid(SpellEntry const* spellInfo, Powers powerType)
{
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo->Effect[i] == SPELL_EFFECT_NONE)
        {
            continue;
        }

        if ((spellInfo->Effect[i] == SPELL_EFFECT_POWER_BURN ||
            spellInfo->Effect[i] == SPELL_EFFECT_POWER_DRAIN ||
            spellInfo->EffectAura[i] == SPELL_AURA_PERIODIC_MANA_LEECH ||
            spellInfo->EffectAura[i] == SPELL_AURA_POWER_BURN_MANA) &&
            int32(powerType) != spellInfo->EffectMiscValue[i])
        {
            continue;
        }
        return true;
    }
    return false;
}

inline bool NeedsComboPoints(SpellEntry const* spellInfo)
{
    return spellInfo->HasAttribute(SPELL_ATTR_EX_REQ_TARGET_COMBO_POINTS) || spellInfo->HasAttribute(SPELL_ATTR_EX_REQ_COMBO_POINTS);
}

inline SpellSchoolMask GetSpellSchoolMask(SpellEntry const* spellInfo)
{
    return GetSchoolMask(spellInfo->School);
}

inline uint32 GetSpellMechanicMask(SpellEntry const* spellInfo, uint32 effectMask)
{
    uint32 mask = 0;
    if (spellInfo->Mechanic)
    {
        mask |= 1 << (spellInfo->Mechanic - 1);
    }

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!(effectMask & (1 << i)))
        {
            continue;
        }

        if (spellInfo->EffectMechanic[i])
        {
            mask |= 1 << (spellInfo->EffectMechanic[i] - 1);
        }
    }

    return mask;
}

inline uint32 GetAllSpellMechanicMask(SpellEntry const* spellInfo)
{
    uint32 mask = 0;
    if (spellInfo->Mechanic)
    {
        mask |= 1 << (spellInfo->Mechanic - 1);
    }

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo->EffectMechanic[i])
        {
            mask |= 1 << (spellInfo->EffectMechanic[i] - 1);
        }
    }
    return mask;
}

inline Mechanics GetEffectMechanic(SpellEntry const* spellInfo, SpellEffectIndex effect)
{
    if (spellInfo->EffectMechanic[effect])
    {
        return Mechanics(spellInfo->EffectMechanic[effect]);
    }
    if (spellInfo->Mechanic)
    {
        return Mechanics(spellInfo->Mechanic);
    }
    return MECHANIC_NONE;
}

inline uint32 GetDispellMask(DispelType dispel)
{

    if (dispel == DISPEL_ALL)
    {
        return DISPEL_ALL_MASK;
    }
    else
    {
        return (1 << dispel);
    }
}

inline bool IsAuraAddedBySpell(uint32 auraType, uint32 spellId)
{
    SpellEntry const* spellproto = sSpellStore.LookupEntry(spellId);
    if (!spellproto)
    {
        return false;
    }

    for (int i = 0; i < 3; i++)
    {
        if (spellproto->EffectAura[i] == auraType)
        {
            return true;
        }
    }
    return false;
}

DiminishingGroup GetDiminishingReturnsGroupForSpell(SpellEntry const* spellproto, bool triggered);

bool IsDiminishingReturnsGroupDurationLimited(DiminishingGroup group);

DiminishingReturnsType GetDiminishingReturnsGroupType(DiminishingGroup group);

typedef std::map<uint32, uint64> SpellAffectMap;

enum ProcFlags
{
    PROC_FLAG_NONE                          = 0x00000000,

    PROC_FLAG_KILLED                        = 0x00000001,
    PROC_FLAG_KILL                          = 0x00000002,

    PROC_FLAG_SUCCESSFUL_MELEE_HIT          = 0x00000004,
    PROC_FLAG_TAKEN_MELEE_HIT               = 0x00000008,

    PROC_FLAG_SUCCESSFUL_MELEE_SPELL_HIT    = 0x00000010,
    PROC_FLAG_TAKEN_MELEE_SPELL_HIT         = 0x00000020,

    PROC_FLAG_SUCCESSFUL_RANGED_HIT         = 0x00000040,
    PROC_FLAG_TAKEN_RANGED_HIT              = 0x00000080,

    PROC_FLAG_SUCCESSFUL_RANGED_SPELL_HIT   = 0x00000100,
    PROC_FLAG_TAKEN_RANGED_SPELL_HIT        = 0x00000200,

    PROC_FLAG_SUCCESSFUL_POSITIVE_AOE_HIT   = 0x00000400,
    PROC_FLAG_TAKEN_POSITIVE_AOE            = 0x00000800,

    PROC_FLAG_SUCCESSFUL_AOE_SPELL_HIT      = 0x00001000,
    PROC_FLAG_TAKEN_AOE_SPELL_HIT           = 0x00002000,

    PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL     = 0x00004000,
    PROC_FLAG_TAKEN_POSITIVE_SPELL          = 0x00008000,

    PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT = 0x00010000,
    PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT      = 0x00020000,

    PROC_FLAG_ON_DO_PERIODIC                = 0x00040000,
    PROC_FLAG_ON_TAKE_PERIODIC              = 0x00080000,

    PROC_FLAG_TAKEN_ANY_DAMAGE              = 0x00100000,
    PROC_FLAG_ON_TRAP_ACTIVATION            = 0x00200000,

    PROC_FLAG_TAKEN_OFFHAND_HIT             = 0x00400000,
    PROC_FLAG_SUCCESSFUL_OFFHAND_HIT        = 0x00800000
};

#define MELEE_BASED_TRIGGER_MASK (PROC_FLAG_SUCCESSFUL_MELEE_HIT          | \
PROC_FLAG_TAKEN_MELEE_HIT             | \
PROC_FLAG_SUCCESSFUL_MELEE_SPELL_HIT  | \
PROC_FLAG_TAKEN_MELEE_SPELL_HIT       | \
PROC_FLAG_SUCCESSFUL_RANGED_HIT       | \
PROC_FLAG_TAKEN_RANGED_HIT            | \
PROC_FLAG_SUCCESSFUL_RANGED_SPELL_HIT | \
PROC_FLAG_TAKEN_RANGED_SPELL_HIT)

#define NEGATIVE_TRIGGER_MASK (MELEE_BASED_TRIGGER_MASK                 | \
PROC_FLAG_SUCCESSFUL_AOE_SPELL_HIT      | \
PROC_FLAG_TAKEN_AOE_SPELL_HIT           | \
PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT | \
PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT)

enum ProcFlagsEx
{

    PROC_EX_NONE                = 0x0000000,

    PROC_EX_NORMAL_HIT          = 0x0000001,
    PROC_EX_CRITICAL_HIT        = 0x0000002,
    PROC_EX_MISS                = 0x0000004,
    PROC_EX_RESIST              = 0x0000008,
    PROC_EX_DODGE               = 0x0000010,
    PROC_EX_PARRY               = 0x0000020,
    PROC_EX_BLOCK               = 0x0000040,
    PROC_EX_EVADE               = 0x0000080,
    PROC_EX_IMMUNE              = 0x0000100,
    PROC_EX_DEFLECT             = 0x0000200,
    PROC_EX_ABSORB              = 0x0000400,
    PROC_EX_REFLECT             = 0x0000800,

    PROC_EX_INTERRUPT           = 0x0001000,
    PROC_EX_RESERVED1           = 0x0002000,
    PROC_EX_RESERVED2           = 0x0004000,
    PROC_EX_RESERVED3           = 0x0008000,

    PROC_EX_EX_TRIGGER_ALWAYS   = 0x0010000,

    PROC_EX_EX_ONE_TIME_TRIGGER = 0x0020000,

    PROC_EX_PERIODIC_POSITIVE   = 0x0040000,
};

struct SpellProcEventEntry
{
    uint32      schoolMask;
    uint32      spellFamilyName;
    ClassFamilyMask spellFamilyMask[MAX_EFFECT_INDEX];
    uint32      procFlags;
    uint32      procEx;
    float       ppmRate;
    float       customChance;
    uint32      cooldown;
};

struct SpellBonusEntry
{
    float  direct_damage;
    float  one_hand_direct_damage;
    float  two_hand_direct_damage;
    float  direct_damage_done;
    float  one_hand_direct_damage_done;
    float  two_hand_direct_damage_done;
    float  direct_damage_taken;
    float  one_hand_direct_damage_taken;
    float  two_hand_direct_damage_taken;
    float  dot_damage;
    float  ap_bonus;
    float  ap_dot_bonus;
};

typedef std::unordered_map<uint32, SpellProcEventEntry> SpellProcEventMap;
typedef std::unordered_map<uint32, SpellBonusEntry>     SpellBonusMap;

#define ELIXIR_FLASK_MASK     0x03
#define ELIXIR_WELL_FED       0x10

struct SpellThreatEntry
{
    uint16 threat;
    float multiplier;
    float ap_bonus;
};

typedef std::map<uint32, uint8> SpellElixirMap;
typedef std::map<uint32, float> SpellProcItemEnchantMap;
typedef std::map<uint32, SpellThreatEntry> SpellThreatMap;

enum SpellTargetType
{
    SPELL_TARGET_TYPE_GAMEOBJECT = 0,
    SPELL_TARGET_TYPE_CREATURE   = 1,
    SPELL_TARGET_TYPE_DEAD       = 2
};

#define MAX_SPELL_TARGET_TYPE 3

struct SpellTargetEntry
{
    uint32 spellId;
    uint32 type;
    uint32 targetEntry;
    uint32 inverseEffectMask;

    bool CanNotHitWithSpellEffect(SpellEffectIndex effect) const { return inverseEffectMask & (1 << effect); }
};

struct SpellTargetPosition
{
    uint32 target_mapId;
    float  target_X;
    float  target_Y;
    float  target_Z;
    float  target_Orientation;
};

typedef std::unordered_map<uint32, SpellTargetPosition> SpellTargetPositionMap;

enum SpellLinkedType
{
    SPELL_LINKED_TYPE_NONE              = 0,
    SPELL_LINKED_TYPE_BOOST             = 1,
    SPELL_LINKED_TYPE_PRECAST           = 2,
    SPELL_LINKED_TYPE_TRIGGERED         = 3,
    SPELL_LINKED_TYPE_PROC              = 4,
    SPELL_LINKED_TYPE_REMOVEONCAST      = 5,
    SPELL_LINKED_TYPE_REMOVEONREMOVE    = 6,
    SPELL_LINKED_TYPE_CASTONREMOVE      = 7,
    SPELL_LINKED_TYPE_MAX,
};

struct SpellLinkedEntry
{
    uint32 spellId;
    uint32 linkedId;
    uint32 type;
    uint32 effectMask;
};

typedef std::multimap<uint32, SpellLinkedEntry>  SpellLinkedMap;
typedef std::pair<SpellLinkedMap::const_iterator, SpellLinkedMap::const_iterator> SpellLinkedMapBounds;
typedef std::set<uint32>  SpellLinkedSet;

class PetAura
{
    public:
        PetAura() : removeOnChangePet(false),
            damage(0)
        {}

        PetAura(uint32 petEntry, uint32 aura, bool _removeOnChangePet, int _damage)
            : removeOnChangePet(_removeOnChangePet), damage(_damage)
        {
            auras[petEntry] = aura;
        }

        uint32 GetAura(uint32 petEntry) const
        {
            std::map<uint32, uint32>::const_iterator itr = auras.find(petEntry);
            if (itr != auras.end())
            {
                return itr->second;
            }
            else
            {
                std::map<uint32, uint32>::const_iterator itr2 = auras.find(0);
                if (itr2 != auras.end())
                {
                    return itr2->second;
                }
                else
                {
                    return 0;
                }
            }
        }

        void AddAura(uint32 petEntry, uint32 aura)
        {
            auras[petEntry] = aura;
        }

        bool IsRemovedOnChangePet() const
        {
            return removeOnChangePet;
        }

        int32 GetDamage() const
        {
            return damage;
        }

    private:
        std::map<uint32, uint32> auras;
        bool removeOnChangePet;
        int32 damage;
};
typedef std::map<uint16, PetAura> SpellPetAuraMap;

struct SpellArea
{
    uint32 spellId;
    uint32 areaId;
    uint32 questStart;
    uint32 questEnd;
    uint16 conditionId;
    int32  auraSpell;
    uint32 raceMask;
    Gender gender;
    bool questStartCanActive;
    bool autocast;

    bool IsFitToRequirements(Player const* player, uint32 newZone, uint32 newArea) const;
    void ApplyOrRemoveSpellIfCan(Player* player, uint32 newZone, uint32 newArea, bool onlyApply) const;
};

typedef std::multimap < uint32 , SpellArea > SpellAreaMap;
typedef std::multimap < uint32 , SpellArea const* > SpellAreaForAuraMap;
typedef std::multimap < uint32 , SpellArea const* > SpellAreaForAreaMap;
typedef std::pair<SpellAreaMap::const_iterator, SpellAreaMap::const_iterator> SpellAreaMapBounds;
typedef std::pair<SpellAreaForAuraMap::const_iterator, SpellAreaForAuraMap::const_iterator>  SpellAreaForAuraMapBounds;
typedef std::pair<SpellAreaForAreaMap::const_iterator, SpellAreaForAreaMap::const_iterator>  SpellAreaForAreaMapBounds;

struct SpellChainNode
{
    uint32 prev;
    uint32 first;
    uint32 req;
    uint8  rank;
};

typedef std::unordered_map<uint32, SpellChainNode> SpellChainMap;
typedef std::multimap<uint32, uint32> SpellChainMapNext;

struct SpellLearnSkillNode
{
    uint16 skill;
    uint16 step;
    uint16 value;
    uint16 maxvalue;
};

typedef std::map<uint32, SpellLearnSkillNode> SpellLearnSkillMap;

struct SpellLearnSpellNode
{
    uint32 spell;
    bool active;
    bool autoLearned;
};

typedef std::multimap<uint32, SpellLearnSpellNode> SpellLearnSpellMap;
typedef std::pair<SpellLearnSpellMap::const_iterator, SpellLearnSpellMap::const_iterator> SpellLearnSpellMapBounds;

typedef std::multimap<uint32, SkillLineAbilityEntry const*> SkillLineAbilityMap;
typedef std::pair<SkillLineAbilityMap::const_iterator, SkillLineAbilityMap::const_iterator> SkillLineAbilityMapBounds;

typedef std::multimap<uint32, SkillRaceClassInfoEntry const*> SkillRaceClassInfoMap;
typedef std::pair<SkillRaceClassInfoMap::const_iterator, SkillRaceClassInfoMap::const_iterator> SkillRaceClassInfoMapBounds;

bool IsPrimaryProfessionSkill(uint32 skill);

inline bool IsProfessionSkill(uint32 skill)
{
    return  IsPrimaryProfessionSkill(skill) || skill == SKILL_FISHING || skill == SKILL_COOKING || skill == SKILL_FIRST_AID;
}

inline bool IsProfessionOrRidingSkill(uint32 skill)
{
    return  IsProfessionSkill(skill) || skill == SKILL_RIDING;
}

typedef std::map<uint32, uint32> SpellFacingFlagMap;

class SpellMgr
{
    friend struct DoSpellBonuses;
    friend struct DoSpellProcEvent;
    friend struct DoSpellProcItemEnchant;

    public:
        SpellMgr();
        ~SpellMgr();

    public:

        ClassFamilyMask GetSpellAffectMask(uint32 spellId, SpellEffectIndex effectId) const
        {
            SpellAffectMap::const_iterator itr = mSpellAffectMap.find((spellId << 8) + effectId);
            if (itr != mSpellAffectMap.end())
            {
                return ClassFamilyMask(itr->second);
            }
            if (SpellEntry const* spellEntry = sSpellStore.LookupEntry(spellId))
            {
                return ClassFamilyMask(spellEntry->EffectItemType[effectId]);
            }
            return ClassFamilyMask();
        }

        SpellElixirMap const& GetSpellElixirMap() const { return mSpellElixirs; }

        uint32 GetSpellElixirMask(uint32 spellid) const
        {
            SpellElixirMap::const_iterator itr = mSpellElixirs.find(spellid);
            if (itr == mSpellElixirs.end())
            {
                return 0x0;
            }

            return itr->second;
        }

        SpellSpecific GetSpellElixirSpecific(uint32 spellid) const
        {
            uint32 mask = GetSpellElixirMask(spellid);

            if ((mask & ELIXIR_FLASK_MASK) == ELIXIR_FLASK_MASK)
            {
                return SPELL_FLASK_ELIXIR;
            }
            else if (mask & ELIXIR_WELL_FED)
            {
                return SPELL_WELL_FED;
            }
            else
            {
                return SPELL_NORMAL;
            }
        }

        SpellThreatEntry const* GetSpellThreatEntry(uint32 spellid) const
        {
            SpellThreatMap::const_iterator itr = mSpellThreatMap.find(spellid);
            if (itr != mSpellThreatMap.end())
            {
                return &itr->second;
            }

            return nullptr;
        }

        float GetSpellThreatMultiplier(SpellEntry const* spellInfo) const
        {
            if (!spellInfo)
            {
                return 1.0f;
            }

            if (SpellThreatEntry const* entry = GetSpellThreatEntry(spellInfo->ID))
            {
                return entry->multiplier;
            }

            return 1.0f;
        }

        SpellProcEventEntry const* GetSpellProcEvent(uint32 spellId) const
        {
            SpellProcEventMap::const_iterator itr = mSpellProcEventMap.find(spellId);
            if (itr != mSpellProcEventMap.end())
            {
                return &itr->second;
            }
            return nullptr;
        }

        float GetItemEnchantProcChance(uint32 spellid) const
        {
            SpellProcItemEnchantMap::const_iterator itr = mSpellProcItemEnchantMap.find(spellid);
            if (itr == mSpellProcItemEnchantMap.end())
            {
                return 0.0f;
            }

            return itr->second;
        }

        static bool IsSpellProcEventCanTriggeredBy(SpellProcEventEntry const* spellProcEvent, uint32 EventProcFlag, SpellEntry const* procSpell, uint32 procFlags, uint32 procExtra);

        SpellBonusEntry const* GetSpellBonusData(uint32 spellId) const
        {

            SpellBonusMap::const_iterator itr = mSpellBonusMap.find(spellId);
            if (itr != mSpellBonusMap.end())
            {
                return &itr->second;
            }

            return nullptr;
        }

        uint32 GetSpellFacingFlag(uint32 spellId) const
        {
            SpellFacingFlagMap::const_iterator itr =  mSpellFacingFlagMap.find(spellId);
            if (itr != mSpellFacingFlagMap.end())
            {
                return itr->second;
            }
            return 0x0;
        }

        SpellTargetPosition const* GetSpellTargetPosition(uint32 spell_id) const
        {
            SpellTargetPositionMap::const_iterator itr = mSpellTargetPositions.find(spell_id);
            if (itr != mSpellTargetPositions.end())
            {
                return &itr->second;
            }
            return nullptr;
        }

        SpellChainNode const* GetSpellChainNode(uint32 spell_id) const
        {
            SpellChainMap::const_iterator itr = mSpellChains.find(spell_id);
            if (itr == mSpellChains.end())
            {
                return nullptr;
            }

            return &itr->second;
        }

        uint32 GetFirstSpellInChain(uint32 spell_id) const
        {
            if (SpellChainNode const* node = GetSpellChainNode(spell_id))
            {
                return node->first;
            }

            return spell_id;
        }

        uint32 GetPrevSpellInChain(uint32 spell_id) const
        {
            if (SpellChainNode const* node = GetSpellChainNode(spell_id))
            {
                return node->prev;
            }

            return 0;
        }

        SpellChainMapNext const& GetSpellChainNext() const { return mSpellChainsNext; }

        template<typename Worker>
            void doForHighRanks(uint32 spellid, Worker& worker)
        {
            SpellChainMapNext const& nextMap = GetSpellChainNext();
            for (SpellChainMapNext::const_iterator itr = nextMap.lower_bound(spellid); itr != nextMap.upper_bound(spellid); ++itr)
            {
                worker(itr->second);
                doForHighRanks(itr->second, worker);
            }
        }

        uint8 GetSpellRank(uint32 spell_id) const
        {
            if (SpellChainNode const* node = GetSpellChainNode(spell_id))
            {
                return node->rank;
            }

            return 0;
        }

        bool IsHighRankOfSpell(uint32 spell1, uint32 spell2) const
        {
            SpellChainMap::const_iterator itr = mSpellChains.find(spell1);

            uint32 rank2 = GetSpellRank(spell2);

            if (itr == mSpellChains.end() || !rank2 || itr->second.rank <= rank2)
            {
                return false;
            }

            for (; itr != mSpellChains.end(); itr = mSpellChains.find(itr->second.prev))
            {
                if (itr->second.prev == spell2)
                {
                    return true;
                }
            }
            return false;
        }

        bool IsRankSpellDueToSpell(SpellEntry const* spellInfo_1, uint32 spellId_2) const;
        bool IsNoStackSpellDueToSpell(uint32 spellId_1, uint32 spellId_2) const;
        bool canStackSpellRanksInSpellBook(SpellEntry const* spellInfo) const;
        bool IsRankedSpellNonStackableInSpellBook(SpellEntry const* spellInfo) const
        {
            return !canStackSpellRanksInSpellBook(spellInfo) && GetSpellRank(spellInfo->ID) != 0;
        }

        SpellEntry const* SelectAuraRankForLevel(SpellEntry const* spellInfo, uint32 Level) const;

        SpellLearnSkillNode const* GetSpellLearnSkill(uint32 spell_id) const
        {
            SpellLearnSkillMap::const_iterator itr = mSpellLearnSkills.find(spell_id);
            if (itr != mSpellLearnSkills.end())
            {
                return &itr->second;
            }
            else
            {
                return nullptr;
            }
        }

        bool IsSpellLearnSpell(uint32 spell_id) const
        {
            return mSpellLearnSpells.find(spell_id) != mSpellLearnSpells.end();
        }

        SpellLearnSpellMapBounds GetSpellLearnSpellMapBounds(uint32 spell_id) const
        {
            return mSpellLearnSpells.equal_range(spell_id);
        }

        bool IsSpellLearnToSpell(uint32 spell_id1, uint32 spell_id2) const
        {
            SpellLearnSpellMapBounds bounds = GetSpellLearnSpellMapBounds(spell_id1);
            for (SpellLearnSpellMap::const_iterator i = bounds.first; i != bounds.second; ++i)
            {
                if (i->second.spell == spell_id2)
                {
                    return true;
                }
            }
            return false;
        }

        static bool IsProfessionOrRidingSpell(uint32 spellId);
        static bool IsProfessionSpell(uint32 spellId);
        static bool IsPrimaryProfessionSpell(uint32 spellId);
        bool IsPrimaryProfessionFirstRankSpell(uint32 spellId) const;

        bool IsSkillBonusSpell(uint32 spellId) const;

        static bool IsSpellValid(SpellEntry const* spellInfo, Player* pl = nullptr, bool msg = true);

        SkillLineAbilityMapBounds GetSkillLineAbilityMapBounds(uint32 spell_id) const
        {
            return mSkillLineAbilityMap.equal_range(spell_id);
        }

        SkillRaceClassInfoMapBounds GetSkillRaceClassInfoMapBounds(uint32 skill_id) const
        {
            return mSkillRaceClassInfoMap.equal_range(skill_id);
        }

        PetAura const* GetPetAura(uint32 spell_id)
        {
            SpellPetAuraMap::const_iterator itr = mSpellPetAuraMap.find(spell_id);
            if (itr != mSpellPetAuraMap.end())
            {
                return &itr->second;
            }
            else
            {
                return nullptr;
            }
        }

        SpellCastResult GetSpellAllowedInLocationError(SpellEntry const* spellInfo, uint32 map_id, uint32 zone_id, uint32 area_id, Player const* player = nullptr);

        SpellAreaMapBounds GetSpellAreaMapBounds(uint32 spell_id) const
        {
            return mSpellAreaMap.equal_range(spell_id);
        }

        SpellAreaForAuraMapBounds GetSpellAreaForAuraMapBounds(uint32 spell_id) const
        {
            return mSpellAreaForAuraMap.equal_range(spell_id);
        }

        SpellAreaForAreaMapBounds GetSpellAreaForAreaMapBounds(uint32 area_id) const
        {
            return mSpellAreaForAreaMap.equal_range(area_id);
        }

        SpellLinkedMapBounds GetSpellLinkedMapBounds(uint32 spell_id) const
        {
            return mSpellLinkedMap.equal_range(spell_id);
        }

        SpellLinkedSet GetSpellLinked(uint32 spell_id, SpellLinkedType type) const;

    public:
        static SpellMgr& Instance();

        void CheckUsedSpells(char const* table);

        void LoadSpellChains();
        void LoadSpellLearnSkills();
        void LoadSpellLearnSpells();
        void LoadSpellScriptTarget();
        void LoadSpellAffects();
        void LoadSpellElixirs();
        void LoadSpellProcEvents();
        void LoadSpellProcItemEnchant();
        void LoadSpellBonuses();
        void LoadSpellLinked();
        void LoadSpellTargetPositions();
        void LoadSpellThreats();
        void LoadSkillLineAbilityMap();
        void LoadSkillRaceClassInfoMap();
        void LoadSpellPetAuras();
        void LoadSpellAreas();
        void LoadFacingCasterFlags();

        void ModDBCSpellAttributes();

    private:
        SpellChainMap      mSpellChains;
        SpellChainMapNext  mSpellChainsNext;
        SpellLearnSkillMap mSpellLearnSkills;
        SpellLearnSpellMap mSpellLearnSpells;
        SpellTargetPositionMap mSpellTargetPositions;
        SpellAffectMap     mSpellAffectMap;
        SpellElixirMap     mSpellElixirs;
        SpellThreatMap     mSpellThreatMap;
        SpellProcEventMap  mSpellProcEventMap;
        SpellProcItemEnchantMap mSpellProcItemEnchantMap;
        SpellBonusMap      mSpellBonusMap;
        SpellLinkedMap     mSpellLinkedMap;
        SkillLineAbilityMap mSkillLineAbilityMap;
        SkillRaceClassInfoMap mSkillRaceClassInfoMap;
        SpellPetAuraMap     mSpellPetAuraMap;
        SpellAreaMap         mSpellAreaMap;
        SpellAreaForAuraMap  mSpellAreaForAuraMap;
        SpellAreaForAreaMap  mSpellAreaForAreaMap;
        SpellFacingFlagMap  mSpellFacingFlagMap;
};

#define sSpellMgr SpellMgr::Instance()
