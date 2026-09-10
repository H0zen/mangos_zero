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

#include "DBCEnums.h"
#include "Path.h"
#include "Platform/Define.h"
#include "SharedDefines.h"

#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct AreaTableEntry
{
    uint32  ID;
    uint32  ContinentID;
    uint32  ParentAreaID;
    uint32  AreaBit;
    uint32  Flags;

    int32   ExplorationLevel;
    char*   AreaName_lang[8];

    uint32  FactionGroupMask;

    uint32  LiquidTypeID_3;
};

struct AreaTriggerEntry
{
    uint32    ID;
    uint32    mapid;
    float     x;
    float     y;
    float     z;
    float     radius;
    float     box_length;
    float     box_width;
    float     box_height;
    float     box_yaw;
};

struct AuctionHouseEntry
{
    uint32    houseId;
    uint32    faction;
    uint32    depositPercent;
    uint32    cutPercent;

};

struct BankBagSlotPricesEntry
{
    uint32  ID;
    uint32  Price;
};

#define MAX_OUTFIT_ITEMS 12

struct CharStartOutfitEntry
{

    uint8  RaceID;
    uint8  ClassID;
    uint8  SexID;
    uint8  OutfitID;
    int32  ItemID[MAX_OUTFIT_ITEMS];

};

struct ChatChannelsEntry
{
    uint32  ID;
    uint32  Flags;

    char const*   Name_lang[8];

};

struct ChrClassesEntry
{
    uint32  ID;

    uint32  DisplayPower;

    char const* Name_lang[8];

    uint32  SpellClassSet;

};

struct ChrRacesEntry
{
    uint32      ID;

    uint32      FactionID;

    uint32      MaleDisplayID;
    uint32      FemaleDisplayID;

    uint32      BaseLanguage;

    uint32      StartingTaxiNodes;

    uint32      CinematicSequence;
    char*       Name_lang[8];

};

struct CinematicSequencesEntry
{
    uint32      ID;

};

struct CreatureDisplayInfoEntry
{
    uint32      ID;

    uint32      ExtendedDisplayInfoID;
    float       CreatureModelScale;

};

struct CreatureDisplayInfoExtraEntry
{
    uint32      DisplayExtraId;
    uint32      Race;

};

struct CreatureFamilyEntry
{
    uint32    ID;
    float     MinScale;
    uint32    MinScaleLevel;
    float     MaxScale;
    uint32    MaxScaleLevel;
    uint32    SkillLine[2];
    uint32    PetFoodMask;
    char*     Name_lang[8];

};

#define MAX_CREATURE_SPELL_DATA_SLOT 4

struct CreatureSpellDataEntry
{
    uint32    ID;
    uint32    SpellId[MAX_CREATURE_SPELL_DATA_SLOT];

};

struct CreatureTypeEntry
{
    uint32    ID;

};

struct DurabilityCostsEntry
{
    uint32    ID;
    uint32    WeaponSubClassCost[29];
};

struct DurabilityQualityEntry
{
    uint32    Id;
    float     quality_mod;
};

struct EmotesEntry
{
    uint32  Id;

    uint32  EmoteType;

};

struct EmotesTextEntry
{
    uint32  ID;

    uint32  EmoteID;

};

struct FactionEntry
{
    uint32      ID;
    int32       ReputationIndex;
    uint32      ReputationRaceMask[4];
    uint32      ReputationClassMask[4];
    int32       ReputationBase[4];
    uint32      ReputationFlags[4];
    uint32      ParentFactionID;
    char*       Name_lang[8];

    int GetIndexFitTo(uint32 raceMask, uint32 classMask) const
    {
        for (int i = 0; i < 4; ++i)
        {
            if ((ReputationRaceMask[i] == 0 || (ReputationRaceMask[i] & raceMask)) &&
                (ReputationClassMask[i] == 0 || (ReputationClassMask[i] & classMask)))
            {
                return i;
            }
        }

        return -1;
    }
};

struct FactionTemplateEntry
{

    uint32      ID;

    uint32      Faction;

    uint32      Flags;

    uint32      FactionGroup;

    uint32      FriendGroup;

    uint32      EnemyGroup;

    uint32      Enemies[4];

    uint32      Friend[4];

    bool IsFriendlyTo(FactionTemplateEntry const& entry) const
    {
        if (entry.Faction)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (Enemies[i]  == entry.Faction)
                {
                    return false;
                }
            }

            for (int i = 0; i < 4; ++i)
            {
                if (Friend[i] == entry.Faction)
                {
                    return true;
                }
            }
        }
        return (FriendGroup & entry.FactionGroup) || (FactionGroup & entry.FriendGroup);
    }

    bool IsHostileTo(FactionTemplateEntry const& entry) const
    {
        if (entry.Faction)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (Enemies[i]  == entry.Faction)
                {
                    return true;
                }
            }

            for (int i = 0; i < 4; ++i)
            {
                if (Friend[i] == entry.Faction)
                {
                    return false;
                }
            }
        }
        return (EnemyGroup & entry.FactionGroup) != 0;
    }

    bool IsHostileToPlayers() const { return (EnemyGroup & FACTION_MASK_PLAYER) != 0; }
    bool IsNeutralToAll() const
    {
        for (int i = 0; i < 4; ++i)
        {
            if (Enemies[i] != 0)
            {
                return false;
            }
        }
        return EnemyGroup == 0 && FriendGroup == 0;
    }

    bool IsContestedGuardFaction() const { return (Flags & FACTION_TEMPLATE_FLAG_CONTESTED_GUARD) != 0; }
};

struct GameObjectDisplayInfoEntry
{
    uint32      ID;
    char*       ModelName;

};

#define GT_MAX_LEVEL    100

struct ItemBagFamilyEntry
{
    uint32   ID;

};

struct ItemClassEntry
{
    uint32   ID;

    char*    ClassName_lang[8];

};

struct ItemRandomPropertiesEntry
{
    uint32    ID;

    uint32    Enchantment[3];

};

struct ItemSetEntry
{

    char*     Name_lang[8];

    uint32    SetSpellID[8];
    uint32    SetThreshold[8];
    uint32    RequiredSkill;
    uint32    RequiredSkillRank;
};

struct LiquidTypeEntry
{
    uint32 ID;
    char*  Name;
    uint32 Type;
    uint32 SpellID;
};

#define MAX_LOCK_CASE 8

struct LockEntry
{
    uint32      ID;
    uint32      Type[MAX_LOCK_CASE];
    uint32      Index[MAX_LOCK_CASE];
    uint32      Skill[MAX_LOCK_CASE];

};

struct MailTemplateEntry
{
    uint32      ID;

};

struct MapEntry
{
    uint32  MapID;

    uint32  InstanceType;

    char*   MapName_lang[8];

    uint32  AreaTableID;

    uint32  LoadingScreenID;

    bool IsDungeon() const { return InstanceType == MAP_INSTANCE || InstanceType == MAP_RAID; }
    bool IsNonRaidDungeon() const { return InstanceType == MAP_INSTANCE; }
    bool Instanceable() const { return InstanceType == MAP_INSTANCE || InstanceType == MAP_RAID || InstanceType == MAP_BATTLEGROUND; }
    bool IsRaid() const { return InstanceType == MAP_RAID; }
    bool IsBattleGround() const { return InstanceType == MAP_BATTLEGROUND; }

    bool IsMountAllowed() const
    {
        return !IsDungeon() ||
            MapID == 309 || MapID == 209 || MapID == 509 || MapID == 269;
    }

    bool IsContinent() const
    {
        return MapID == 0 || MapID == 1;
    }
};

struct MovieEntry
{
    uint32      Id;

};

struct QuestSortEntry
{
    uint32      ID;

};

struct SkillRaceClassInfoEntry
{

    uint32    SkillID;
    uint32    RaceMask;
    uint32    ClassMask;
    uint32    Flags;
    uint32    MinLevel;

};

struct SkillLineEntry
{
    uint32    ID;
    int32     CategoryID;

    char*     DisplayName_lang[8];

};

struct SkillLineAbilityEntry
{
    uint32    ID;
    uint32    SkillLine;
    uint32    Spell;
    uint32    RaceMask;
    uint32    ClassMask;

    uint32    MinSkillLineRank;
    uint32    SupercededBySpell;
    uint32    AcquireMethod;
    uint32    TrivialSkillLineRankHigh;
    uint32    TrivialSkillLineRankLow;

    uint32    ReqTrainPoints;
};

struct SoundEntriesEntry
{
    uint32    Id;

};

struct ClassFamilyMask
{

    uint64 Flags;

    ClassFamilyMask() : Flags(0) {}

    explicit ClassFamilyMask(uint64 familyFlags) : Flags(familyFlags) {}

    bool Empty() const { return Flags == 0; }

    bool operator!() const { return Empty(); }

    operator void const* () const { return Empty() ? nullptr : this; }

    bool IsFitToFamilyMask(uint64 familyFlags) const
    {
        return Flags & familyFlags;
    }

    bool IsFitToFamilyMask(ClassFamilyMask const& mask) const
    {
        return Flags & mask.Flags;
    }

    uint64 operator& (uint64 mask) const
    {
        return Flags & mask;
    }

    ClassFamilyMask& operator|= (ClassFamilyMask const& mask)
    {
        Flags |= mask.Flags;
        return *this;
    }
};

#define MAX_SPELL_REAGENTS 8
#define MAX_SPELL_TOTEMS 2

struct SpellEntry
{
    uint32    ID;
    uint32    School;
    uint32    Category;

    uint32    DispelType;
    uint32    Mechanic;
    uint32    Attributes;
    uint32    AttributesEx;
    uint32    AttributesExB;
    uint32    AttributesExC;
    uint32    AttributesExD;
    uint32    ShapeshiftMask;
    uint32    ShapeshiftExclude;
    uint32    Targets;
    uint32    TargetCreatureType;
    uint32    RequiresSpellFocus;
    uint32    CasterAuraState;
    uint32    TargetAuraState;
    uint32    CastingTimeIndex;
    uint32    RecoveryTime;
    uint32    CategoryRecoveryTime;
    uint32    InterruptFlags;
    uint32    AuraInterruptFlags;
    uint32    ChannelInterruptFlags;
    uint32    ProcFlags;
    uint32    ProcChance;
    uint32    ProcCharges;
    uint32    MaxLevel;
    uint32    BaseLevel;
    uint32    SpellLevel;
    uint32    DurationIndex;
    uint32    PowerType;
    uint32    ManaCost;
    uint32    ManaCostPerLevel;
    uint32    ManaPerSecond;
    uint32    ManaPerSecondPerLevel;
    uint32    RangeIndex;
    float     Speed;
    uint32    ModalNextSpell;
    uint32    CumulativeAura;
    uint32    Totem[MAX_SPELL_TOTEMS];
    int32     Reagent[MAX_SPELL_REAGENTS];
    uint32    ReagentCount[MAX_SPELL_REAGENTS];
    int32     EquippedItemClass;
    int32     EquippedItemSubclass;
    int32     EquippedItemInvTypes;
    uint32    Effect[MAX_EFFECT_INDEX];
    int32     EffectDieSides[MAX_EFFECT_INDEX];
    uint32    EffectBaseDice[MAX_EFFECT_INDEX];
    float     EffectDicePerLevel[MAX_EFFECT_INDEX];
    float     EffectRealPointsPerLevel[MAX_EFFECT_INDEX];
    int32     EffectBasePoints[MAX_EFFECT_INDEX];
    uint32    EffectMechanic[MAX_EFFECT_INDEX];
    uint32    ImplicitTargetA[MAX_EFFECT_INDEX];
    uint32    ImplicitTargetB[MAX_EFFECT_INDEX];
    uint32    EffectRadiusIndex[MAX_EFFECT_INDEX];
    uint32    EffectAura[MAX_EFFECT_INDEX];
    uint32    EffectAuraPeriod[MAX_EFFECT_INDEX];
    float     EffectAmplitude[MAX_EFFECT_INDEX];
    uint32    EffectChainTargets[MAX_EFFECT_INDEX];
    uint32    EffectItemType[MAX_EFFECT_INDEX];
    int32     EffectMiscValue[MAX_EFFECT_INDEX];
    uint32    EffectTriggerSpell[MAX_EFFECT_INDEX];
    float     EffectPointsPerCombo[MAX_EFFECT_INDEX];
    uint32    SpellVisualID;

    uint32    SpellIconID;
    uint32    ActiveIconID;

    char*     Name_lang[8];

    char*     NameSubtext_lang[8];

    uint32    ManaCostPct;
    uint32    StartRecoveryCategory;
    uint32    StartRecoveryTime;
    uint32    MaxTargetLevel;
    uint32    SpellClassSet;
    ClassFamilyMask SpellClassMask;
    uint32    MaxTargets;
    uint32    DefenseType;
    uint32    PreventionType;

    float     EffectChainAmplitude[MAX_EFFECT_INDEX];

    int32 CalculateSimpleValue(SpellEffectIndex eff) const { return EffectBasePoints[eff] + int32(EffectBaseDice[eff]); }

    bool IsFitToFamilyMask(uint64 familyFlags) const
    {
        return SpellClassMask.IsFitToFamilyMask(familyFlags);
    }

    bool IsFitToFamily(SpellFamily family, uint64 familyFlags) const
    {
        return SpellFamily(SpellClassSet) == family && IsFitToFamilyMask(familyFlags);
    }

    bool IsFitToFamilyMask(ClassFamilyMask const& mask) const
    {
        return SpellClassMask.IsFitToFamilyMask(mask);
    }

    bool IsFitToFamily(SpellFamily family, ClassFamilyMask const& mask) const
    {
        return SpellFamily(SpellClassSet) == family && IsFitToFamilyMask(mask);
    }

    inline bool HasAttribute(SpellAttributes attribute) const { return Attributes & attribute; }

    inline bool HasAttribute(SpellAttributesEx attribute) const { return AttributesEx & attribute; }

    inline bool HasAttribute(SpellAttributesEx2 attribute) const { return AttributesExB & attribute; }

    inline bool HasAttribute(SpellAttributesEx3 attribute) const { return AttributesExC & attribute; }

    inline bool HasAttribute(SpellAttributesEx4 attribute) const { return AttributesExD & attribute; }

    inline bool HasSpellEffect(SpellEffects effect) const
    {
        for (uint8 i = EFFECT_INDEX_0; i <= EFFECT_INDEX_2; ++i)
        {
            if (Effect[i] == effect)
            {
                return true;
            }
        }
        return false;
    }

    private:

        SpellEntry(SpellEntry const&);
};

struct SpellCastTimesEntry
{
    uint32    ID;
    int32     Base;

};

struct SpellRadiusEntry
{
    uint32    ID;
    float     Radius;

};

struct SpellRangeEntry
{
    uint32    ID;
    float     RangeMin;
    float     RangeMax;

};

struct SpellShapeshiftFormEntry
{
    uint32 ID;

    uint32 Flags;
    int32  CreatureType;

};

struct SpellDurationEntry
{
    uint32    ID;
    int32     Duration[3];
};

struct SpellFocusObjectEntry
{
    uint32    ID;

};

struct SpellItemEnchantmentEntry
{
    uint32      ID;
    uint32      Effect[3];
    uint32      EffectPointsMin[3];

    uint32      EffectArg[3];
    char*       Name_lang[8];

    uint32      ItemVisual;
    uint32      Flags;
};

struct StableSlotPricesEntry
{
    uint32 Slot;
    uint32 Price;
};

#define MAX_TALENT_RANK 5

struct TalentEntry
{
    uint32    TalentID;
    uint32    TalentTab;
    uint32    Row;
    uint32    Col;
    uint32    RankID[MAX_TALENT_RANK];

    uint32    DependsOn;

    uint32    DependsOnRank;

    uint32    RequiredSpellID;
};

struct TalentTabEntry
{
    uint32  ID;

    uint32  ClassMask;
    uint32  OrderIndex;

};

struct TaxiNodesEntry
{
    uint32    ID;
    uint32    map_id;
    float     x;
    float     y;
    float     z;
    char*     Name_lang[8];

    uint32    MountCreatureID[2];
};

struct TaxiPathEntry
{
    uint32    ID;
    uint32    from;
    uint32    to;
    uint32    price;
};

struct TaxiPathNodeEntry
{

    uint32    PathID;
    uint32    NodeIndex;
    uint32    ContinentID;
    float     LocX;
    float     LocY;
    float     LocZ;
    uint32    Flags;
    uint32    Delay;
};

struct WMOAreaTableEntry
{
    uint32 ID;
    int32 WMOID;
    int32 NameSetID;
    int32 WMOGroupID;

    uint32 Flags;
    uint32 AreaTableID;

};

struct WorldMapAreaEntry
{

    uint32  MapID;
    uint32  AreaID;

    float   LocLeft;
    float   LocRight;
    float   LocTop;
    float   LocBottom;
};

struct WorldSafeLocsEntry
{
    uint32    ID;
    uint32    map_id;
    float     x;
    float     y;
    float     z;

};

#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

typedef std::set<uint32> SpellCategorySet;
typedef std::map<uint32, SpellCategorySet > SpellCategoryStore;
typedef std::set<uint32> PetFamilySpellsSet;
typedef std::map<uint32, PetFamilySpellsSet > PetFamilySpellsStore;

struct TalentSpellPos
{
    TalentSpellPos() : talent_id(0), rank(0) {}
    TalentSpellPos(uint16 _talent_id, uint8 _rank) : talent_id(_talent_id), rank(_rank) {}

    uint16 talent_id;
    uint8  rank;
};

typedef std::map<uint32, TalentSpellPos> TalentSpellPosMap;

struct TaxiPathBySourceAndDestination
{
    TaxiPathBySourceAndDestination() : ID(0), price(0) {}
    TaxiPathBySourceAndDestination(uint32 _id, uint32 _price) : ID(_id), price(_price) {}

    uint32    ID;
    uint32    price;
};
typedef std::map<uint32, TaxiPathBySourceAndDestination> TaxiPathSetForSource;
typedef std::map<uint32, TaxiPathSetForSource> TaxiPathSetBySource;

struct TaxiPathNodePtr
{
    TaxiPathNodePtr() : i_ptr(nullptr) {}
    TaxiPathNodePtr(TaxiPathNodeEntry const* ptr) : i_ptr(ptr) {}

    TaxiPathNodeEntry const* i_ptr;

    operator TaxiPathNodeEntry const& () const { return *i_ptr; }
};

typedef Path<TaxiPathNodePtr, TaxiPathNodeEntry const> TaxiPathNodeList;
typedef std::vector<TaxiPathNodeList> TaxiPathNodesByPath;

struct TransportAnimationEntry
{

    uint32    TransportID;
    uint32    TimeIndex;
    float     PosX;
    float     PosY;
    float     PosZ;
    uint32    SequenceID;
};

typedef std::vector<TransportAnimationEntry const*> TransportAnimation;

typedef std::unordered_map<uint32, TransportAnimation> TransportAnimationsByEntry;

#define TaxiMaskSize 8
typedef uint32 TaxiMask[TaxiMaskSize];
