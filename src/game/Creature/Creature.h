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

#include "Loot/Spoilable.h"
#include "Position.h"
#include <unordered_map>
#include "Platform/Define.h"
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include "LootClaim.h"
#include "VendorStock.h"
#include "Unit.h"
#include "Tenure.h"
#include "CreatureLinks.h"
#include "Stats/CreatureNumbers.h"
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

enum CreatureFlagsExtra
{
    CREATURE_FLAG_EXTRA_INSTANCE_BIND = 0x00000001,
    CREATURE_FLAG_EXTRA_NO_AGGRO = 0x00000002,
    CREATURE_FLAG_EXTRA_NO_PARRY = 0x00000004,
    CREATURE_FLAG_EXTRA_NO_PARRY_HASTEN = 0x00000008,
    CREATURE_FLAG_EXTRA_NO_BLOCK = 0x00000010,
    CREATURE_FLAG_EXTRA_NO_CRUSH = 0x00000020,
    CREATURE_FLAG_EXTRA_NO_XP_AT_KILL = 0x00000040,
    CREATURE_FLAG_EXTRA_INVISIBLE = 0x00000080,
    CREATURE_FLAG_EXTRA_NOT_TAUNTABLE = 0x00000100,
    CREATURE_FLAG_EXTRA_AGGRO_ZONE = 0x00000200,
    CREATURE_FLAG_EXTRA_GUARD = 0x00000400,
    CREATURE_FLAG_EXTRA_NO_CALL_ASSIST = 0x00000800,
    CREATURE_FLAG_EXTRA_ACTIVE = 0x00001000,
    CREATURE_FLAG_EXTRA_MMAP_FORCE_ENABLE = 0x00002000,
    CREATURE_FLAG_EXTRA_MMAP_FORCE_DISABLE = 0x00004000,
    CREATURE_FLAG_EXTRA_WALK_IN_WATER = 0x00008000,
    CREATURE_FLAG_EXTRA_HAVE_NO_SWIM_ANIMATION = 0x00010000
};

#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

#define MAX_KILL_CREDIT 2
#define MAX_CREATURE_MODEL 4
#define USE_DEFAULT_DATABASE_LEVEL  0

struct CreatureInfo
{
    uint32 Entry;
    char* Name;
    char* SubName;
    uint32 MinLevel;
    uint32 MaxLevel;
    uint32 ModelId[MAX_CREATURE_MODEL];
    uint32 FactionAlliance;
    uint32 FactionHorde;
    float Scale;
    uint32 Family;
    uint32 CreatureType;
    uint32 InhabitType;
    uint32 RegenerateStats;
    bool RacialLeader;
    uint32 NpcFlags;
    uint32 UnitFlags;
    uint32 DynamicFlags;
    uint32 ExtraFlags;
    uint32 CreatureTypeFlags;
    float SpeedWalk;
    float SpeedRun;
    uint32 UnitClass;
    uint32 Rank;
    float HealthMultiplier;
    float PowerMultiplier;
    float DamageMultiplier;
    float DamageVariance;
    float ArmorMultiplier;
    float ExperienceMultiplier;
    uint32  MinLevelHealth;
    uint32  MaxLevelHealth;
    uint32  MinLevelMana;
    uint32  MaxLevelMana;
    float   MinMeleeDmg;
    float   MaxMeleeDmg;
    float   MinRangedDmg;
    float   MaxRangedDmg;
    uint32  Armor;
    uint32  MeleeAttackPower;
    uint32  RangedAttackPower;
    uint32  MeleeBaseAttackTime;
    uint32  RangedBaseAttackTime;
    uint32  DamageSchool;
    uint32  MinLootGold;
    uint32  MaxLootGold;
    uint32  LootId;
    uint32  PickpocketLootId;
    uint32  SkinningLootId;
    uint32  KillCredit[MAX_KILL_CREDIT];
    uint32  MechanicImmuneMask;
    uint32  SchoolImmuneMask;
    int32   ResistanceHoly;
    int32   ResistanceFire;
    int32   ResistanceNature;
    int32   ResistanceFrost;
    int32   ResistanceShadow;
    int32   ResistanceArcane;
    uint32  SpellListId;
    uint32  PetSpellDataId;
    uint32  MovementType;
    uint32  TrainerType;
    uint32  TrainerSpell;
    uint32  TrainerClass;
    uint32  TrainerRace;
    uint32  TrainerTemplateId;
    uint32  VendorTemplateId;
    uint32  GossipMenuId;
    uint32  EquipmentTemplateId;
    uint32  civilian;
    char const* AIName;

    static HighGuid GetHighGuid()
    {
        return HIGHGUID_UNIT;
    }

    ObjectGuid GetObjectGuid(uint32 lowguid) const { return MakeGuid(GetHighGuid(), Entry, lowguid); }

};

struct CreatureTemplateSpells
{
    uint32 entry;
    uint32 spells[CREATURE_MAX_SPELLS];
};

struct EquipmentInfo
{
    uint32  entry;
    uint32  equipentry[3];
};

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

struct EquipmentInfoRaw
{
    uint32  entry;
    uint32  equipmodel[3];
    uint32  equipinfo[3];
    uint32  equipslot[3];
};

struct CreatureData
{
    uint32 id;

    uint32 mapid;
    uint32 modelid_override;
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

    ObjectGuid GetObjectGuid(uint32 lowguid) const
    {
        return MakeGuid(CreatureInfo::GetHighGuid(), id, lowguid);
    }
};

enum SplineFlags
{
    SPLINEFLAG_WALKMODE     = 0x0000100,
    SPLINEFLAG_FLYING       = 0x0000200,
};

struct CreatureDataAddon
{
    uint32 guidOrEntry;
    uint32 mount;
    uint32 bytes1;
    uint8  sheath_state;
    uint8  flags;
    uint32 emote;
    uint32 move_flags;
    uint32 const* auras;
};

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
    uint32 modelid_other_gender;
    uint32 modelid_other_team;
};

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

enum AttackingTarget
{
    ATTACKING_TARGET_RANDOM = 0,
    ATTACKING_TARGET_TOPAGGRO,
    ATTACKING_TARGET_BOTTOMAGGRO,
};

enum SelectFlags
{
    SELECT_FLAG_IN_LOS              = 0x001,
    SELECT_FLAG_PLAYER              = 0x002,
    SELECT_FLAG_POWER_MANA          = 0x004,
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

typedef std::unordered_map < uint32 , TrainerSpell > TrainerSpellMap;

struct TrainerSpellData
{
    TrainerSpellData() : trainerType(0) {}

    TrainerSpellMap spellList;
    uint32 trainerType;

    TrainerSpell const* Find(uint32 spell_id) const;
    void Clear()
    {
        spellList.clear();
    }
};

#define CREATURE_Z_ATTACK_RANGE 3

#define MAX_VENDOR_ITEMS 255

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

        CreatureCreatePos(Map* map, float x, float y, float z, float o)
            : m_map(map), m_closeObject(nullptr), m_angle(0.0f), m_dist(0.0f) { m_pos.x = x; m_pos.y = y; m_pos.z = z; m_pos.o = o; }

        CreatureCreatePos(Occupant* closeObject, float ori, float dist = 0.0f, float angle = 0.0f)
            : m_map(closeObject->GetMap()),
            m_closeObject(closeObject), m_angle(angle), m_dist(dist) { m_pos.o = ori; }
    public:
        Map* GetMap() const { return m_map; }
        void SelectFinalPoint(Creature* cr);
        bool PlaceOn(Creature* cr) const;

        Position m_pos;
    private:
        Map* m_map;
        Occupant* m_closeObject;
        float m_angle;
        float m_dist;
};

class Creature : public Unit, public Spoilable
{
    CreatureAI* i_AI;

    public:

        explicit Creature(CreatureSubtype subtype = CREATURE_SUBTYPE_GENERIC);
        virtual ~Creature();

        void AddToWorld() override;
        void RemoveFromWorld() override;
        void CleanupsBeforeDelete() override;

        bool Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, Team team = TEAM_NONE, const CreatureData* data = nullptr, GameEventCreatureData const* eventData = nullptr);
        bool LoadCreatureAddon(bool reload);
        void SelectLevel(uint32 forcedLevel = USE_DEFAULT_DATABASE_LEVEL);
        void LoadEquipment(uint32 equip_entry, bool force = false);

        char const* GetSubName() const { return GetCreatureInfo()->SubName; }

        void Update(uint32 update_diff, uint32 time) override;

        virtual void RegenerateAll(uint32 update_diff);

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

        uint32 GetLevelForTarget(Unit const* target) const override;

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

        bool UpdateEntry(uint32 entry, Team team = ALLIANCE, const CreatureData* data = nullptr, GameEventCreatureData const* eventData = nullptr, bool preserveHPAndPower = true);

        void ApplyGameEventSpells(GameEventCreatureData const* eventData, bool activated);
        StatSheet& Sheet() override { return m_sheet; }
        StatSheet const& Sheet() const override { return m_sheet; }

        Pace& Pacing() override { return m_pace; }
        Pace const& Pacing() const override { return m_pace; }

        CreatureLinks& Links() { return m_links; }
        CreatureLinks const& Links() const { return m_links; }

        Tenure& Term() { return m_tenure; }
        Tenure const& Term() const { return m_tenure; }

        static stats::RankRates RatesFor(int32 rank);

        VendorItemData const* GetVendorItems() const;
        VendorItemData const* GetVendorTemplateItems() const;

        TrainerSpellData const* GetTrainerTemplateSpells() const;
        TrainerSpellData const* GetTrainerSpells() const;

        CreatureDataAddon const* GetCreatureAddon() const;

        static uint32 ChooseDisplayId(const CreatureInfo* cinfo, const CreatureData* data = nullptr, GameEventCreatureData const* eventData = nullptr);

        std::string GetAIName() const;
        std::string GetScriptName() const;
        uint32 GetScriptId() const;

        const char* GetNameForLocaleIdx(int32 locale_idx) const override;

        void SetDeathState(DeathState s) override;

        bool LoadFromDB(uint32 guid, Map* map);

        void MovedTo(float x, float y, float z, float o) override;

        bool MovesItself() const override { return false; }

        Cell const& GetCurrentCell() const { return m_currentCell; }
        void SetCurrentCell(Cell const& cell) { m_currentCell = cell; }

        Loot loot;

        Loot* Spoils() override { return &loot; }

        bool OpenableBy(Player const& who) const override;
        bool FillSpoilsFor(Player& who, LootType& how, PermissionTypes& permission) override;

        void PrepareBodyLootState();

        bool IsTappedBy(Player const* player) const;
        void LowerPlayerDamageReq(uint32 unDamage);

        void TappedBy(Unit* taker)
        {
            if (m_claim.StakedBy(taker))
            {
                SetDynFlag(UNIT_DYNFLAG_TAPPED);
            }
        }

        void AllLootRemovedFromCorpse();

        SpellEntry const* ReachWithSpellAttack(Unit* pVictim);
        SpellEntry const* ReachWithSpellCure(Unit* pVictim);

        SpellCastResult TryToCast(Unit* pTarget, uint32 uiSpell, uint32 uiCastFlags, uint8 uiChance);
        SpellCastResult TryToCast(Unit* pTarget, SpellEntry const* pSpellInfo, uint32 uiCastFlags, uint8 uiChance);

        float GetAttackDistance(Unit const* pl) const;

        void SendAIReaction(AiReaction reactionType);

        void DoFleeToGetAssistance();
        void CallForHelp(float fRadius);
        void CallAssistance();

        bool CanAssistTo(const Unit* u, const Unit* enemy, bool checkfaction = true) const;
        bool CanInitiateAttack();

        bool IsVisibleInGridForPlayer(Player* pl) const override;

        void RemoveCorpse(bool inPlace = false);

        void ForcedDespawn(uint32 timeMSToDespawn = 0);

        void Respawn();

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

        void SendAreaSpiritHealerQueryOpcode(Player* pl);

        void SetVirtualItem(VirtualItemSlot slot, uint32 item_id);
        void SetVirtualItemRaw(VirtualItemSlot slot, uint32 display_id, uint32 info0, uint32 info1);

    protected:
        bool MeetsSelectAttackingRequirement(Unit* pTarget, SpellEntry const* pSpellInfo, uint32 selectFlags) const;

        bool CreateFromProto(uint32 guidlow, CreatureInfo const* cinfo, Team team, const CreatureData* data = nullptr, GameEventCreatureData const* eventData = nullptr);
        bool InitEntry(uint32 entry, Team team = ALLIANCE, const CreatureData* data = nullptr, GameEventCreatureData const* eventData = nullptr);

        void RegeneratePower();
        void RegenerateHealth();

    private:
        CreatureSheet m_sheet;
        CreatureLinks m_links;
        Pace m_pace;
        Tenure m_tenure;
        Cell m_currentCell;
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
