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

/**
 * @file Creature.h
 * @brief Creature (NPC) class definition and related structures.
 *
 * This file defines the Creature class which represents non-player characters (NPCs)
 * in the game world. It extends the Unit class with creature-specific functionality
 * including:
 * - Creature data storage and initialization
 * - Loot management and drop handling
 * - Movement AI and pathfinding
 * - Respawn and despawn mechanics
 * - Ability and skill management
 * - Faction and reputation systems
 *
 * The file also contains CreatureInfo struct for storing static creature template data
 * from the database, and various creature-related flags and enumerations.
 *
 * @see Creature for the main creature implementation
 * @see Unit for the base unit class
 * @see CreatureInfo for creature template data
 */

#pragma once

#include <unordered_map>
#include "Platform/Define.h"
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include "LootClaim.h"
#include "Unit.h"
#include "CreatureLinks.h"
#include "Stats/CreatureSheet.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "DBCEnums.h"
#include "Cell.h"

#include <list>

struct SpellEntry;

class CreatureAI;
class Group;
class Quest;
class Player;
class WorldSession;

struct GameEventCreatureData;

/**
 * @brief Creature extra flags enumeration
 *
 * Additional flags that modify creature behavior.
 */
enum CreatureFlagsExtra
{
    CREATURE_FLAG_EXTRA_INSTANCE_BIND = 0x00000001,         ///< Creature kill binds instance with killer and killer's group
    CREATURE_FLAG_EXTRA_NO_AGGRO = 0x00000002,              ///< Not aggro (ignore faction/reputation hostility)
    CREATURE_FLAG_EXTRA_NO_PARRY = 0x00000004,              ///< Creature can't parry
    CREATURE_FLAG_EXTRA_NO_PARRY_HASTEN = 0x00000008,       ///< Creature can't counter-attack at parry
    CREATURE_FLAG_EXTRA_NO_BLOCK = 0x00000010,              ///< Creature can't block
    CREATURE_FLAG_EXTRA_NO_CRUSH = 0x00000020,              ///< Creature can't do crush attacks
    CREATURE_FLAG_EXTRA_NO_XP_AT_KILL = 0x00000040,         ///< Creature kill doesn't provide XP
    CREATURE_FLAG_EXTRA_INVISIBLE = 0x00000080,             ///< Creature is always invisible for player (mostly trigger creatures)
    CREATURE_FLAG_EXTRA_NOT_TAUNTABLE = 0x00000100,         ///< Creature is immune to taunt auras and effect attack me
    CREATURE_FLAG_EXTRA_AGGRO_ZONE = 0x00000200,            ///< Creature sets itself in combat with zone on aggro
    CREATURE_FLAG_EXTRA_GUARD = 0x00000400,                 ///< Creature is a guard
    CREATURE_FLAG_EXTRA_NO_CALL_ASSIST = 0x00000800,        ///< Creature shouldn't call for assistance on aggro
    CREATURE_FLAG_EXTRA_ACTIVE = 0x00001000,                ///< Creature is active object (grid will be loaded and creature set as active)
    CREATURE_FLAG_EXTRA_MMAP_FORCE_ENABLE = 0x00002000,     ///< Creature is forced to use MMaps
    CREATURE_FLAG_EXTRA_MMAP_FORCE_DISABLE = 0x00004000,    ///< Creature is forced to NOT use MMaps
    CREATURE_FLAG_EXTRA_WALK_IN_WATER = 0x00008000,         ///< Creature is forced to walk in water even if it can swim
    CREATURE_FLAG_EXTRA_HAVE_NO_SWIM_ANIMATION = 0x00010000 ///< Creature has no swim animation (or creature will have "no animation")
};

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

#define MAX_KILL_CREDIT 2
#define MAX_CREATURE_MODEL 4                                // only single send to client in static data
#define USE_DEFAULT_DATABASE_LEVEL  0                       // just used to show we don't want to force the new creature level and use the level stored in db

/**
 * @brief Creature information structure
 *
 * Data from `creature_template` table.
 */
struct CreatureInfo
{
    uint32 Entry; ///< Creature entry ID
    char* Name; ///< Creature name
    char* SubName; ///< Creature sub-name
    uint32 MinLevel; ///< Minimum level
    uint32 MaxLevel; ///< Maximum level
    uint32 ModelId[MAX_CREATURE_MODEL]; ///< Model IDs
    uint32 FactionAlliance; ///< Alliance faction
    uint32 FactionHorde; ///< Horde faction
    float Scale; ///< Scale factor
    uint32 Family; ///< Creature family (enum CreatureFamily values, optional)
    uint32 CreatureType; ///< Creature type (enum CreatureType values)
    uint32 InhabitType; ///< Inhabit type
    uint32 RegenerateStats; ///< Regenerate stats
    bool RacialLeader; ///< Is racial leader
    uint32 NpcFlags; ///< NPC flags
    uint32 UnitFlags; ///< Unit flags (enum UnitFlags mask values)
    uint32 DynamicFlags; ///< Dynamic flags
    uint32 ExtraFlags; ///< Extra flags
    uint32 CreatureTypeFlags; ///< Creature type flags (enum CreatureTypeFlags mask values)
    float SpeedWalk; ///< Walk speed
    float SpeedRun; ///< Run speed
    uint32 UnitClass; ///< Unit class (enum Classes, note only 4 classes are known for creatures)
    uint32 Rank; ///< Creature rank
    float HealthMultiplier; ///< Health multiplier
    float PowerMultiplier; ///< Power multiplier
    float DamageMultiplier; ///< Damage multiplier
    float DamageVariance; ///< Damage variance
    float ArmorMultiplier; ///< Armor multiplier
    float ExperienceMultiplier; ///< Experience multiplier
    uint32  MinLevelHealth;
    uint32  MaxLevelHealth;
    uint32  MinLevelMana;
    uint32  MaxLevelMana;
    float   MinMeleeDmg;                                      ///< Minimum melee damage
    float   MaxMeleeDmg;                                      ///< Maximum melee damage
    float   MinRangedDmg;                                     ///< Minimum ranged damage
    float   MaxRangedDmg;                                     ///< Maximum ranged damage
    uint32  Armor;                                            ///< Armor value
    uint32  MeleeAttackPower;                                 ///< Melee attack power
    uint32  RangedAttackPower;                                ///< Ranged attack power
    uint32  MeleeBaseAttackTime;                              ///< Melee attack base time (milliseconds)
    uint32  RangedBaseAttackTime;                             ///< Ranged attack base time (milliseconds)
    uint32  DamageSchool;                                     ///< Primary damage school (enum SpellSchools)
    uint32  MinLootGold;                                      ///< Minimum loot gold dropped
    uint32  MaxLootGold;                                      ///< Maximum loot gold dropped
    uint32  LootId;                                           ///< Loot table ID
    uint32  PickpocketLootId;                                 ///< Pickpocket loot table ID
    uint32  SkinningLootId;                                   ///< Skinning loot table ID
    uint32  KillCredit[MAX_KILL_CREDIT];                      ///< Kill credit IDs for quest tracking
    uint32  MechanicImmuneMask;                               ///< Mechanic immunity mask (enum Mechanics)
    uint32  SchoolImmuneMask;                                 ///< School immunity mask (enum SpellSchools)
    int32   ResistanceHoly;                                   ///< Holy resistance
    int32   ResistanceFire;                                   ///< Fire resistance
    int32   ResistanceNature;                                 ///< Nature resistance
    int32   ResistanceFrost;                                  ///< Frost resistance
    int32   ResistanceShadow;                                 ///< Shadow resistance
    int32   ResistanceArcane;                                 ///< Arcane resistance
    uint32  SpellListId;                                      ///< Creature spells list ID
    uint32  PetSpellDataId;                                   ///< Pet spell data ID
    uint32  MovementType;                                     ///< Movement type (enum MovementGeneratorType)
    uint32  TrainerType;                                      ///< Trainer type (enum TrainerType)
    uint32  TrainerSpell;                                     ///< Trainer spell ID
    uint32  TrainerClass;                                     ///< Trainer class (enum Classes)
    uint32  TrainerRace;                                      ///< Trainer race (enum Races)
    uint32  TrainerTemplateId;                                ///< Trainer template ID
    uint32  VendorTemplateId;                                 ///< Vendor template ID
    uint32  GossipMenuId;                                     ///< Gossip menu ID
    uint32  EquipmentTemplateId;                              ///< Equipment template ID
    uint32  civilian;                                         ///< Civilian flag (2 = civilian npc)
    char const* AIName;                                       ///< Custom AI name for special behavior
    //uint32  ScriptID;

    /// @brief Helper methods for CreatureInfo

    /// @brief Get the high GUID type for creatures.
    /// @return Always returns HIGHGUID_UNIT for creatures
    static HighGuid GetHighGuid()
    {
        return HIGHGUID_UNIT;                               // in pre-3.x always HIGHGUID_UNIT
    }

    /// @brief Create a full ObjectGuid for this creature template.
    /// @param lowguid The low GUID (unique creature instance identifier)
    /// @return ObjectGuid combining creature entry and low GUID
    ObjectGuid GetObjectGuid(uint32 lowguid) const { return ObjectGuid(GetHighGuid(), Entry, lowguid); }

};

/// @brief Creature spell list structure.
///
/// Stores spell IDs for creatures to cast during combat or special events.
struct CreatureTemplateSpells
{
    uint32 entry;                          ///> Creature entry ID
    uint32 spells[CREATURE_MAX_SPELLS];    ///> Spell IDs creature can cast (up to CREATURE_MAX_SPELLS)
};

/// @brief Equipment template structure.
///
/// Defines which items a creature should equip on spawn.
struct EquipmentInfo
{
    uint32  entry;            ///> Creature entry ID
    uint32  equipentry[3];    ///> Equipment entry IDs (main hand, off-hand, ranged)
};

/// @brief Equipment item information structure.
///
/// Detailed information for a specific piece of equipment.
struct EquipmentInfoItem
{
    uint32  entry;
    uint32  Class;
    uint32  SubClass;
    uint32  Material;
    uint32  DisplayID;
    uint32  InventoryType;
    uint32  Sheath;
};

// depricated old way
struct EquipmentInfoRaw
{
    uint32  entry;
    uint32  equipmodel[3];
    uint32  equipinfo[3];
    uint32  equipslot[3];
};

// from `creature` table
struct CreatureData
{
    uint32 id;                                              // entry in creature_template
    // uint32, NOT uint16: a vessel's deck map has a minted id above 65535, and crew are
    // ordinary `creature` rows on it. GameObjectData::mapid has always been uint32.
    uint32 mapid;
    uint32 modelid_override;                                // overrides any model defined in creature_template
    int32 equipmentId;
    float posX;
    float posY;
    float posZ;
    float orientation;
    uint32 spawntimesecs;
    float spawndist;
    uint32 currentwaypoint;
    uint32 curhealth;
    uint32 curmana;
    bool  is_dead;
    uint8 movementType;

    // helper function
    ObjectGuid GetObjectGuid(uint32 lowguid) const
    {
        return ObjectGuid(CreatureInfo::GetHighGuid(), id, lowguid);
    }
};

enum SplineFlags
{
    SPLINEFLAG_WALKMODE     = 0x0000100,
    SPLINEFLAG_FLYING       = 0x0000200,
};

// from `creature_addon` and `creature_template_addon`tables
struct CreatureDataAddon
{
    uint32 guidOrEntry;
    uint32 mount;
    uint32 bytes1;
    uint8  sheath_state;                                    // SheathState
    uint8  flags;                                           // unread: the client never looks at this byte
    uint32 emote;
    uint32 move_flags;
    uint32 const* auras;                                    // loaded as char* "spell1 spell2 ... "
};

// Bases values for given Level and UnitClass
struct CreatureClassLvlStats
{
    uint32  BaseHealth;
    uint32  BaseMana;
    float   BaseDamage;
    float   BaseMeleeAttackPower;
    float   BaseRangedAttackPower;
    uint32  BaseArmor;
};

struct CreatureModelInfo
{
    uint32 modelid;
    float bounding_radius;
    float combat_reach;
    uint8 gender;
    uint32 modelid_other_gender;                            // The opposite gender for this modelid (male/female)
    uint32 modelid_other_team;                              // The opposite team. Generally for alliance totem
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

struct CreatureLocale
{
    std::vector<std::string> Name;
    std::vector<std::string> SubName;
};

struct GossipMenuItemsLocale
{
    std::vector<std::string> OptionText;
    std::vector<std::string> BoxText;
};

struct PointOfInterestLocale
{
    std::vector<std::string> IconName;
};

enum InhabitTypeValues
{
    INHABIT_GROUND = 1,
    INHABIT_WATER  = 2,
    INHABIT_AIR    = 4,
    INHABIT_ANYWHERE = INHABIT_GROUND | INHABIT_WATER | INHABIT_AIR
};

// Enums used by StringTextData::Type (CreatureEventAI)
enum ChatType
{
    CHAT_TYPE_SAY               = 0,
    CHAT_TYPE_YELL              = 1,
    CHAT_TYPE_TEXT_EMOTE        = 2,
    CHAT_TYPE_BOSS_EMOTE        = 3,
    CHAT_TYPE_WHISPER           = 4,
    CHAT_TYPE_BOSS_WHISPER      = 5,
    CHAT_TYPE_ZONE_YELL         = 6
};

// Selection method used by SelectAttackingTarget
enum AttackingTarget
{
    ATTACKING_TARGET_RANDOM = 0,                            // Just selects a random target
    ATTACKING_TARGET_TOPAGGRO,                              // Selects targes from top aggro to bottom
    ATTACKING_TARGET_BOTTOMAGGRO,                           // Selects targets from bottom aggro to top
};

enum SelectFlags
{
    SELECT_FLAG_IN_LOS              = 0x001,                // Default Selection Requirement for Spell-targets
    SELECT_FLAG_PLAYER              = 0x002,
    SELECT_FLAG_POWER_MANA          = 0x004,                // For Energy based spells, like manaburn
    SELECT_FLAG_POWER_RAGE          = 0x008,
    SELECT_FLAG_POWER_ENERGY        = 0x010,
    SELECT_FLAG_IN_MELEE_RANGE      = 0x040,
    SELECT_FLAG_NOT_IN_MELEE_RANGE  = 0x080,
};

enum RegenStatsFlags
{
    REGEN_FLAG_HEALTH               = 0x001,
    REGEN_FLAG_POWER                = 0x002,
};

// Vendors
struct VendorItem
{
    VendorItem(uint32 _item, uint32 _maxcount, uint32 _incrtime, uint16 _conditionId)
        : item(_item), maxcount(_maxcount), incrtime(_incrtime), conditionId(_conditionId) {}

    uint32 item;
    uint32 maxcount;                                        // 0 for infinity item amount
    uint32 incrtime;                                        // time for restore items amount if maxcount != 0
    uint16 conditionId;                                     // condition to check for this item
};
typedef std::vector<VendorItem*> VendorItemList;

struct VendorItemData
{
    VendorItemList m_items;

    VendorItem* GetItem(uint32 slot) const
    {
        if (slot >= m_items.size())
        {
            return nullptr;
        }
        return m_items[slot];
    }
    bool Empty() const { return m_items.empty(); }
    uint8 GetItemCount() const { return m_items.size(); }
    void AddItem(uint32 item, uint32 maxcount, uint32 ptime, uint16 conditonId)
    {
        m_items.push_back(new VendorItem(item, maxcount, ptime, conditonId));
    }
    bool RemoveItem(uint32 item_id);
    VendorItem const* FindItem(uint32 item_id) const;
    size_t FindItemSlot(uint32 item_id) const;

    void Clear()
    {
        for (VendorItemList::const_iterator itr = m_items.begin(); itr != m_items.end(); ++itr)
        {
            delete(*itr);
        }
        m_items.clear();
    }
};

struct VendorItemCount
{
    explicit VendorItemCount(uint32 _item, uint32 _count)
        : itemId(_item), count(_count), lastIncrementTime(time(nullptr)) {}

    uint32 itemId;
    uint32 count;
    time_t lastIncrementTime;
};

typedef std::list<VendorItemCount> VendorItemCounts;

struct TrainerSpell
{
    TrainerSpell() : spell(0), spellCost(0), reqSkill(0), reqSkillValue(0), reqLevel(0), isProvidedReqLevel(false) {}

    TrainerSpell(uint32 _spell, uint32 _spellCost, uint32 _reqSkill, uint32 _reqSkillValue, uint32 _reqLevel, bool _isProvidedReqLevel)
        : spell(_spell), spellCost(_spellCost), reqSkill(_reqSkill), reqSkillValue(_reqSkillValue), reqLevel(_reqLevel), isProvidedReqLevel(_isProvidedReqLevel)
    {}

    uint32 spell;
    uint32 spellCost;
    uint32 reqSkill;
    uint32 reqSkillValue;
    uint32 reqLevel;
    bool isProvidedReqLevel;
};

typedef std::unordered_map < uint32 /*spellid*/, TrainerSpell > TrainerSpellMap;

struct TrainerSpellData
{
    TrainerSpellData() : trainerType(0) {}

    TrainerSpellMap spellList;
    uint32 trainerType;                                     // trainer type based at trainer spells, can be different from creature_template value.
    // req. for correct show non-prof. trainers like weaponmaster, allowed values 0 and 2.
    TrainerSpell const* Find(uint32 spell_id) const;
    void Clear()
    {
        spellList.clear();
    }
};

typedef std::map<uint32, time_t> CreatureSpellCooldowns;

// max different by z coordinate for creature aggro reaction
#define CREATURE_Z_ATTACK_RANGE 3

#define MAX_VENDOR_ITEMS 255                                // Limitation in item count field size in SMSG_LIST_INVENTORY

enum VirtualItemSlot
{
    VIRTUAL_ITEM_SLOT_0 = 0,
    VIRTUAL_ITEM_SLOT_1 = 1,
    VIRTUAL_ITEM_SLOT_2 = 2,
};

#define MAX_VIRTUAL_ITEM_SLOT 3

enum VirtualItemInfoByteOffset
{
    VIRTUAL_ITEM_INFO_0_OFFSET_CLASS         = 0,
    VIRTUAL_ITEM_INFO_0_OFFSET_SUBCLASS      = 1,
    VIRTUAL_ITEM_INFO_0_OFFSET_MATERIAL      = 2,
    VIRTUAL_ITEM_INFO_0_OFFSET_INVENTORYTYPE = 3,

    VIRTUAL_ITEM_INFO_1_OFFSET_SHEATH        = 0,
};

struct CreatureCreatePos
{
    public:
        // exactly coordinates used
        CreatureCreatePos(Map* map, float x, float y, float z, float o)
            : m_map(map), m_closeObject(nullptr), m_angle(0.0f), m_dist(0.0f) { m_pos.x = x; m_pos.y = y; m_pos.z = z; m_pos.o = o; }
        // if dist == 0.0f -> exactly object coordinates used, in other case close point to object (CONTACT_DIST can be used as minimal distances)
        CreatureCreatePos(Occupant* closeObject, float ori, float dist = 0.0f, float angle = 0.0f)
            : m_map(closeObject->GetMap()),
            m_closeObject(closeObject), m_angle(angle), m_dist(dist) { m_pos.o = ori; }
    public:
        Map* GetMap() const { return m_map; }
        void SelectFinalPoint(Creature* cr);
        bool PlaceOn(Creature* cr) const;

        // read only after SelectFinalPoint
        Position m_pos;
    private:
        Map* m_map;
        Occupant* m_closeObject;
        float m_angle;
        float m_dist;
};

enum TemporaryFactionFlags                                  // Used at real faction changes
{
    TEMPFACTION_NONE                    = 0x00,             // When no flag is used in temporary faction change, faction will be persistent. It will then require manual change back to default/another faction when changed once
    TEMPFACTION_RESTORE_RESPAWN         = 0x01,             // Default faction will be restored at respawn
    TEMPFACTION_RESTORE_COMBAT_STOP     = 0x02,             // ... at CombatStop() (happens at creature death, at evade or custom scripte among others)
    TEMPFACTION_RESTORE_REACH_HOME      = 0x04,             // ... at reaching home in home movement (evade), if not already done at CombatStop()
    TEMPFACTION_TOGGLE_NON_ATTACKABLE   = 0x08,             // Remove UNIT_FLAG_NON_ATTACKABLE(0x02) when faction is changed (reapply when temp-faction is removed)
    TEMPFACTION_TOGGLE_OOC_NOT_ATTACK   = 0x10,             // Remove UNIT_FLAG_OOC_NOT_ATTACKABLE(0x100) when faction is changed (reapply when temp-faction is removed)
    TEMPFACTION_TOGGLE_PASSIVE          = 0x20,             // Remove UNIT_FLAG_PASSIVE(0x200) when faction is changed (reapply when temp-faction is removed)
    TEMPFACTION_TOGGLE_PACIFIED         = 0x40,             // Remove UNIT_FLAG_PACIFIED(0x20000) when faction is changed (reapply when temp-faction is removed)
    TEMPFACTION_TOGGLE_NOT_SELECTABLE   = 0x80,             // Remove UNIT_FLAG_NOT_SELECTABLE(0x2000000) when faction is changed (reapply when temp-faction is removed)
    TEMPFACTION_ALL,
};

class Creature : public Unit
{
    CreatureAI* i_AI;

    public:

        /* Loot Variables */
        bool hasBeenLootedOnce;
        uint32 assignedLooter;

        explicit Creature(CreatureSubtype subtype = CREATURE_SUBTYPE_GENERIC);
        virtual ~Creature();

        void AddToWorld() override;
        void RemoveFromWorld() override;
        void CleanupsBeforeDelete() override;

        bool Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, Team team = TEAM_NONE, const CreatureData* data = nullptr, GameEventCreatureData const* eventData = nullptr);
        bool LoadCreatureAddon(bool reload);
        void SelectLevel(uint32 forcedLevel = USE_DEFAULT_DATABASE_LEVEL);
        void LoadEquipment(uint32 equip_entry, bool force = false);

        bool HasStaticDBSpawnData() const;                  // listed in `creature` table and have fixed in DB guid

        char const* GetSubName() const { return GetCreatureInfo()->SubName; }

        void Update(uint32 update_diff, uint32 time) override;  // overwrite Unit::Update

        virtual void RegenerateAll(uint32 update_diff);
        uint32 GetEquipmentId() const { return m_equipmentId; }


        bool IsCorpse() const { return GetDeathState() ==  CORPSE; }
        bool IsDespawned() const { return GetDeathState() ==  DEAD; }
        bool IsRacialLeader() const { return Record().IsRacialLeader(); }
        bool IsCivilian() const { return Record().IsCivilian(); }
        bool IsGuard() const { return GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_GUARD; }

        bool CanWalk() const { return GetCreatureInfo()->InhabitType & INHABIT_GROUND; }
        bool CanSwim() const override { return GetCreatureInfo()->InhabitType & INHABIT_WATER; }
        bool IsSwimming() const { return (m_movementInfo.HasMovementFlag((MovementFlags)(MOVEFLAG_SWIMMING))); }
        bool CanFly() const override { return (GetCreatureInfo()->InhabitType & INHABIT_AIR) || m_movementInfo.HasMovementFlag((MovementFlags)(MOVEFLAG_LEVITATING | MOVEFLAG_CAN_FLY)); }
        bool IsFlying() const { return (m_movementInfo.HasMovementFlag((MovementFlags)(MOVEFLAG_FLYING | MOVEFLAG_LEVITATING))); }

        bool IsTrainerOf(Player* player, bool msg) const;
        bool CanInteractWithBattleMaster(Player* player, bool msg) const;
        bool CanTrainAndResetTalentsOf(Player* pPlayer) const;

        bool IsOutOfThreatArea(Unit* pVictim) const;
        void FillGuidsListFromThreatList(GuidVector& guids, uint32 maxamount = 0);

        bool IsImmuneToSpell(SpellEntry const* spellInfo, bool castOnSelf) override;
        bool IsImmuneToDamage(SpellSchoolMask meleeSchoolMask) override;
        bool IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const override;

        bool IsElite() const
        {
            if (IsPet())
            {
                return false;
            }

            uint32 rank = GetCreatureInfo()->Rank;
            return rank != CREATURE_ELITE_NORMAL && rank != CREATURE_ELITE_RARE;
        }

        bool IsWorldBoss() const
        {
            if (IsPet())
            {
                return false;
            }

            return GetCreatureInfo()->Rank == CREATURE_ELITE_WORLDBOSS;
        }

        uint32 GetLevelForTarget(Unit const* target) const override; // overwrite Unit::GetLevelForTarget for boss level support

        bool IsInEvadeMode() const;

        bool AIM_Initialize();

        CreatureAI* AI()
        {
            return i_AI;
        }

        void SetWalk(bool enable, bool asDefault = true);
        void SetLevitate(bool enable) override;
        void SetSwim(bool enable) override;
        void SetCanFly(bool enable) override;
        void SetFeatherFall(bool enable) override;
        void SetHover(bool enable) override;
        void SetRoot(bool enable) override;
        void SetWaterWalk(bool enable) override;

        SpellSchoolMask GetMeleeDamageSchoolMask() const override { return m_meleeDamageSchoolMask; }
        void SetMeleeDamageSchool(SpellSchools school) { m_meleeDamageSchoolMask = GetSchoolMask(school); }

        void _AddCreatureSpellCooldown(uint32 spell_id, time_t end_time);
        void _AddCreatureCategoryCooldown(uint32 category, time_t apply_time);
        void AddCreatureSpellCooldown(uint32 spellid);
        bool HasSpellCooldown(uint32 spell_id) const;
        bool HasCategoryCooldown(uint32 spell_id) const;
        uint32 GetCreatureSpellCooldownDelay(uint32 spellId) const;

        bool HasSpell(uint32 spellID) const override;

        bool UpdateEntry(uint32 entry, Team team = ALLIANCE, const CreatureData* data = nullptr, GameEventCreatureData const* eventData = nullptr, bool preserveHPAndPower = true);

        void ApplyGameEventSpells(GameEventCreatureData const* eventData, bool activated);
        StatSheet& Sheet() override { return m_sheet; }
        StatSheet const& Sheet() const override { return m_sheet; }

        Pace& Pacing() override { return m_pace; }
        Pace const& Pacing() const override { return m_pace; }

        /// What its fortunes do to the creatures tied to it.
        CreatureLinks& Links() { return m_links; }
        CreatureLinks const& Links() const { return m_links; }
        uint32 GetCurrentEquipmentId() const { return m_equipmentId; }

        static float _GetHealthMod(int32 Rank);             ///< Get custom factor to scale health (default 1, CONFIG_FLOAT_RATE_CREATURE_*_HP)
        static float _GetDamageMod(int32 Rank);             ///< Get custom factor to scale damage (default 1, CONFIG_FLOAT_RATE_*_DAMAGE)
        static float _GetSpellDamageMod(int32 Rank);        ///< Get custom factor to scale spell damage (default 1, CONFIG_FLOAT_RATE_*_SPELLDAMAGE)

        VendorItemData const* GetVendorItems() const;
        VendorItemData const* GetVendorTemplateItems() const;
        uint32 GetVendorItemCurrentCount(VendorItem const* vItem);
        uint32 UpdateVendorItemCurrentCount(VendorItem const* vItem, uint32 used_count);

        TrainerSpellData const* GetTrainerTemplateSpells() const;
        TrainerSpellData const* GetTrainerSpells() const;

        CreatureDataAddon const* GetCreatureAddon() const;

        static uint32 ChooseDisplayId(const CreatureInfo* cinfo, const CreatureData* data = nullptr, GameEventCreatureData const* eventData = nullptr);

        std::string GetAIName() const;
        std::string GetScriptName() const;
        uint32 GetScriptId() const;

        // overwrite Occupant function for proper name localization
        const char* GetNameForLocaleIdx(int32 locale_idx) const override;

        void SetDeathState(DeathState s) override;          // overwrite virtual Unit::SetDeathState

        bool LoadFromDB(uint32 guid, Map* map);
        virtual void SaveToDB();
        // overwrited in Pet
        virtual void SaveToDB(uint32 mapid);
        virtual void DeleteFromDB();                        // overwrited in Pet
        static void DeleteFromDB(uint32 lowguid, CreatureData const* data);

        /// Represent the loots available on the creature.
        Loot loot;

        /// Indicates whether the creature has has been pickpocked.
        bool lootForPickPocketed;

        /// Indicates whether the creature has been checked.
        bool lootForBody;

        /// Indicates whether the creature has been skinned.
        bool lootForSkin;

        /**
         * Method preparing the creature for the loot state. Based on the previous loot state, the loot ID provided in the database and the creature's type,
         * this method updates the state of the creature for loots.
         *
         * At the end of this method, the creature loot state may be:
         * Lootable: UNIT_DYNFLAG_LOOTABLE
         * Skinnable: UNIT_FLAG_SKINNABLE
         * Not lootable: No flag
         */
        void PrepareBodyLootState();

        /**
         * function returning the GUID of the loot recipient (a player GUID).
         *
         * \return ObjectGuid Player GUID.
         */

        /**
         * function returning the group recipient ID.
         *
         * \return uint32 Group ID.
         */
        bool IsTappedBy(Player const* player) const;
        bool IsDamageEnoughForLootingAndReward() const { return m_PlayerDamageReq == 0; }
        void LowerPlayerDamageReq(uint32 unDamage);
        void ResetPlayerDamageReq()
        {
            m_PlayerDamageReq = GetHealth() / 2;
        }

        /**
         * function indicating whether the whether the creature has a looter recipient defined (either a group ID, either a player GUID).
         *
         * \return boolean true if the creature has a recipient defined, false otherwise.
         */

        /**
         * function indicating whether the recipient is a group.
         *
         * \return boolean true if the creature's recipient is a group, false otherwise.
         */
        /**
         * Stake the claim on this body for whoever is behind `taker`, and grey it
         * out for everyone else. The two are one event: a body goes grey exactly
         * because somebody else has the right to it.
         */
        void TappedBy(Unit* taker)
        {
            if (m_claim.StakedBy(taker))
            {
                SetDynFlag(UNIT_DYNFLAG_TAPPED);
            }
        }

        // <<TODO: the flag is stored, and it says exactly what Claim().IsClaimed()
        // says, so it could be derived where it is sent instead -- Fields::Project
        // already rewrites this same flag per observer. Both places that drop it
        // now drop the claim with it, so nothing stands in the way of deriving it;
        // what is left is to decide whether UNIT_DYNFLAG_TAPPED_BY_PLAYER should
        // be set for the holder rather than TAPPED being cleared for him, which is
        // the same picture drawn the other way round and the way retail draws it.

        /* ****************** What this NPC will do for you ******************* */
        //
        // UNIT_NPC_FLAGS is written on creatures and pets and on nothing else, so
        // every one of these is a creature's question. A player is a unit too and
        // has no answer to any of them.

        /**
         * @return true if this unit is a vendor, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsVendor()       const { return HasNpcFlag(UNIT_NPC_FLAG_VENDOR); }
        /**
         * @return true if this unit is a trainer, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsTrainer()      const { return HasNpcFlag(UNIT_NPC_FLAG_TRAINER); }
        /**
         * @return true if this unit is a QuestGiver, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsQuestGiver()   const { return HasNpcFlag(UNIT_NPC_FLAG_QUESTGIVER); }
        /**
         * @return true if this unit is a gossip, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsGossip()       const { return HasNpcFlag(UNIT_NPC_FLAG_GOSSIP); }
        /**
         * @return true if this unit is a taxi, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsTaxi()         const { return HasNpcFlag(UNIT_NPC_FLAG_FLIGHTMASTER); }
        /**
         * @return true if this unit is a banker, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsBanker()       const { return HasNpcFlag(UNIT_NPC_FLAG_BANKER); }
        /**
         * @return true if this unit is a innkeeper, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsInnkeeper()    const { return HasNpcFlag(UNIT_NPC_FLAG_INNKEEPER); }
        /**
         * @return true if this unit is a SpiritGuide, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsSpiritGuide()  const { return HasNpcFlag(UNIT_NPC_FLAG_SPIRITGUIDE); }
        /**
         * @return true if this unit is a TabardDesigner, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsTabardDesigner()const { return HasNpcFlag(UNIT_NPC_FLAG_TABARDDESIGNER); }
        /**
         * Returns if this is a service provider or not, a service provider has one of the
         * following flags:
         * - \ref UNIT_NPC_FLAG_VENDOR
         * - \ref UNIT_NPC_FLAG_TRAINER
         * - \ref UNIT_NPC_FLAG_FLIGHTMASTER
         * - \ref UNIT_NPC_FLAG_PETITIONER
         * - \ref UNIT_NPC_FLAG_BATTLEMASTER
         * - \ref UNIT_NPC_FLAG_BANKER
         * - \ref UNIT_NPC_FLAG_INNKEEPER
         * - \ref UNIT_NPC_FLAG_SPIRITHEALER
         * - \ref UNIT_NPC_FLAG_SPIRITGUIDE
         * - \ref UNIT_NPC_FLAG_TABARDDESIGNER
         * - \ref UNIT_NPC_FLAG_AUCTIONEER
         *
         * @return true if this unit is a ServiceProvider, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsServiceProvider() const
        {
            return HasNpcFlag(UNIT_NPC_FLAG_VENDOR | UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_FLIGHTMASTER |
                UNIT_NPC_FLAG_PETITIONER | UNIT_NPC_FLAG_BATTLEMASTER | UNIT_NPC_FLAG_BANKER |
                UNIT_NPC_FLAG_INNKEEPER | UNIT_NPC_FLAG_SPIRITHEALER |
                UNIT_NPC_FLAG_SPIRITGUIDE | UNIT_NPC_FLAG_TABARDDESIGNER | UNIT_NPC_FLAG_AUCTIONEER);
        }
        /**
         * Returns if this is a spirit service or not, a spirit service has one of the
         * following flags:
         * - \ref UNIT_NPC_FLAG_SPIRITHEALER
         * - \ref UNIT_NPC_FLAG_SPIRITGUIDE
         * @return true if this unit is a spirit service, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsSpiritService() const { return HasNpcFlag(UNIT_NPC_FLAG_SPIRITHEALER | UNIT_NPC_FLAG_SPIRITGUIDE); }
        /**
         * @return true if this unit is a GuildMaster, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsGuildMaster()  const { return HasNpcFlag(UNIT_NPC_FLAG_PETITIONER); }
        /**
         * @return true if this unit is a BattleMaster, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsBattleMaster() const { return HasNpcFlag(UNIT_NPC_FLAG_BATTLEMASTER); }
        /**
         * @return true if this unit is a armorer, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsArmorer()      const { return HasNpcFlag(UNIT_NPC_FLAG_REPAIR); }
        /**
         * @return true if this unit is a SpiritHealer, false otherwise
         * \see Object::HasFlag
         * \see EUnitFields
         * \see NPCFlags
         */
        bool IsSpiritHealer() const { return HasNpcFlag(UNIT_NPC_FLAG_SPIRITHEALER); }

        /// Who may take what is on this body, and whether a roll is running.
        LootClaim& Claim() { return m_claim; }
        LootClaim const& Claim() const { return m_claim; }
        void AllLootRemovedFromCorpse();

        SpellEntry const* ReachWithSpellAttack(Unit* pVictim);
        SpellEntry const* ReachWithSpellCure(Unit* pVictim);

        uint32 m_spells[CREATURE_MAX_SPELLS];
        CreatureSpellCooldowns m_CreatureSpellCooldowns;
        CreatureSpellCooldowns m_CreatureCategoryCooldowns;

        // Used by Creature Spells system to always know result of cast
        SpellCastResult TryToCast(Unit* pTarget, uint32 uiSpell, uint32 uiCastFlags, uint8 uiChance);
        SpellCastResult TryToCast(Unit* pTarget, SpellEntry const* pSpellInfo, uint32 uiCastFlags, uint8 uiChance);

        float GetAttackDistance(Unit const* pl) const;

        void SendAIReaction(AiReaction reactionType);

        void DoFleeToGetAssistance();
        void CallForHelp(float fRadius);
        void CallAssistance();
        void SetNoCallAssistance(bool val) { m_AlreadyCallAssistance = val; }
        void SetNoSearchAssistance(bool val) { m_AlreadySearchedAssistance = val; }
        bool HasSearchedAssistance()
        {
            return m_AlreadySearchedAssistance;
        }

        bool CanAssistTo(const Unit* u, const Unit* enemy, bool checkfaction = true) const;
        bool CanInitiateAttack();


        // for use only in LoadHelper, Map::Add Map::CreatureCellRelocation
        Cell const& GetCurrentCell() const { return m_currentCell; }
        void SetCurrentCell(Cell const& cell) { m_currentCell = cell; }

        bool IsVisibleInGridForPlayer(Player* pl) const override;

        void RemoveCorpse(bool inPlace = false);

        void ForcedDespawn(uint32 timeMSToDespawn = 0);

        void Respawn();
        void SaveRespawnTime();




        // Functions spawn/remove creature with DB guid in all loaded map copies (if point grid loaded in map)
        static void AddToRemoveListInMaps(uint32 db_guid, CreatureData const* data);
        static void SpawnInMaps(uint32 db_guid, CreatureData const* data);

        void SendZoneUnderAttackMessage(Player* attacker);

        void SetInCombatWithZone();

        Unit* SelectAttackingTarget(AttackingTarget target, uint32 position, uint32 uiSpellEntry, uint32 selectFlags = 0) const;
        Unit* SelectAttackingTarget(AttackingTarget target, uint32 position, SpellEntry const* pSpellInfo = nullptr, uint32 selectFlags = 0) const;

        bool OffersQuest(uint32 quest_id) const;
        bool TakesQuest(uint32 quest_id) const;

        GridReference<Creature>& GetGridRef()
        {
            return m_gridRef;
        }

        bool IsRegeneratingHealth()
        {
            return GetCreatureInfo()->RegenerateStats & REGEN_FLAG_HEALTH;
        }

        bool IsRegeneratingPower()
        {
            return GetCreatureInfo()->RegenerateStats & REGEN_FLAG_POWER;
        }

        virtual uint8 GetPetAutoSpellSize() const { return CREATURE_MAX_SPELLS; }
        virtual uint32 GetPetAutoSpellOnPos(uint8 pos) const
        {
            CharmInfo const* bar = GetCharmInfo();
            if (!bar || pos >= CREATURE_MAX_SPELLS)
            {
                return 0;
            }

            CharmSpellEntry const* spell = bar->GetCharmSpell(pos);
            return spell->GetType() == ACT_ENABLED ? spell->GetAction() : 0;
        }


        void SetSpawn(CreatureCreatePos const& pos);
        void SetSpawn(Geometry::Vector3 const& at, float facing);
        void ResetSpawn();


        void SetFactionTemporary(uint32 factionId, uint32 tempFactionFlags = TEMPFACTION_ALL);
        void ClearTemporaryFaction();
        uint32 GetTemporaryFactionFlags()
        {
            return m_temporaryFactionFlags;
        }

        void SendAreaSpiritHealerQueryOpcode(Player* pl);

        void SetVirtualItem(VirtualItemSlot slot, uint32 item_id);
        void SetVirtualItemRaw(VirtualItemSlot slot, uint32 display_id, uint32 info0, uint32 info1);

        void SetDisableReputationGain(bool disable) { DisableReputationGain = disable; }
        bool IsReputationGainDisabled()
        {
            return DisableReputationGain;
        }

    protected:
        bool MeetsSelectAttackingRequirement(Unit* pTarget, SpellEntry const* pSpellInfo, uint32 selectFlags) const;

        bool CreateFromProto(uint32 guidlow, CreatureInfo const* cinfo, Team team, const CreatureData* data = nullptr, GameEventCreatureData const* eventData = nullptr);
        bool InitEntry(uint32 entry, Team team = ALLIANCE, const CreatureData* data = nullptr, GameEventCreatureData const* eventData = nullptr);

        LootClaim m_claim;

        // vendor items
        VendorItemCounts m_vendorItemCounts;

        uint32 m_lootMoney;


        void RegeneratePower();
        void RegenerateHealth();
        Cell m_currentCell;                                 // store current cell where creature listed
        uint32 m_equipmentId;
        uint32 m_PlayerDamageReq;

        // below fields has potential for optimization
        bool m_AlreadyCallAssistance;
        bool m_AlreadySearchedAssistance;
        bool m_AI_locked;
        uint32 m_temporaryFactionFlags;                     // used for real faction changes (not auras etc)

        SpellSchoolMask m_meleeDamageSchoolMask;
        uint32 m_originalEntry;



        bool DisableReputationGain;

    private:
        CreatureSheet m_sheet;
        CreatureLinks m_links;
        Pace m_pace;
        GridReference<Creature> m_gridRef;
};

class ForcedDespawnDelayEvent : public BasicEvent
{
    public:
        ForcedDespawnDelayEvent(Creature& owner) : BasicEvent(), m_owner(owner) {}
        bool Execute(uint64 e_time, uint32 p_time) override;

    private:
        Creature& m_owner;
};
