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
#include <deque>
#include <cassert>
#include <utility>
#include <queue>
#include "Common/ServerDefines.h"
#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include <ctime>
#include <map>
#include <set>
#include <list>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include "ItemPrototype.h"
#include "Unit.h"
#include "Item.h"
#include "ItemSaveQueue.h"
#include "Inventory/ItemSlots.h"
#include "Inventory/Inventory.h"
#include "Honor/HonorLedger.h"
#include "Offers/Invitations.h"
#include "Drink.h"
#include "Hearth.h"
#include "Mailbox.h"
#include "PlayedTime.h"
#include "Duel.h"
#include "BattleGroundStay.h"
#include "DungeonBinds.h"
#include "Stats/PlayerSheet.h"
#include "QueueSlots.h"
#include "Trade.h"
#include "SpellModifiers.h"
#include "Weaponry.h"
#include "Rest.h"
#include "Perils/Perils.h"
#include "Offers/PlayerOffers.h"
#include "Teleport/TeleportOrder.h"

#include "Database/DatabaseEnv.h"
#include "QuestDef.h"
#include "Journal/QuestJournal.h"
#include "Group.h"
#include "Bag.h"
#include "WorldSession.h"
#include "Pet.h"
#include "MapReference.h"
#include "Util.h"
#include "ReputationMgr.h"
#include "SpellCooldownMgr.h"
#include "PetMgr.h"
#include "BattleGround.h"
#include "DBCStores.h"
#include "InitialWorldEntry.h"
#include "SharedDefines.h"
#include "Chat.h"
#include "GMTicketMgr.h"

#include<vector>

struct Mail;
class Channel;
class DynamicObject;
class Creature;
class PlayerMenu;
class Transport;
class SpellCastTargets;
class PlayerSocial;
class DungeonPersistentState;
class Spell;
class Item;

struct AreaTrigger;

#include "CinematicFlyover.h"

#define PLAYER_MAX_SKILLS           127
#define PLAYER_EXPLORED_ZONES_SIZE  64

enum SpellModType
{
    SPELLMOD_FLAT = 107,
    SPELLMOD_PCT = 108
};

enum BuyBankSlotResult
{
    ERR_BANKSLOT_FAILED_TOO_MANY = 0,
    ERR_BANKSLOT_INSUFFICIENT_FUNDS = 1,
    ERR_BANKSLOT_NOTBANKER = 2,
    ERR_BANKSLOT_OK = 3
};

enum PlayerSpellState
{
    PLAYERSPELL_UNCHANGED = 0,
    PLAYERSPELL_CHANGED = 1,
    PLAYERSPELL_NEW = 2,
    PLAYERSPELL_REMOVED = 3
};

struct PlayerSpell
{
    PlayerSpellState state : 8;
    bool active : 1;
    bool dependent : 1;
    bool disabled : 1;
};

typedef std::unordered_map<uint32, PlayerSpell> PlayerSpellMap;

struct SpellModifier
{

    SpellModifier() : op(SpellModOp()), type(SPELLMOD_FLAT), charges(0), value(0), spellId(0), lastAffected(nullptr) {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, uint32 _spellId, uint64 _mask, int16 _charges = 0)
        : op(_op), type(_type), charges(_charges), value(_value), mask(_mask), spellId(_spellId), lastAffected(nullptr)
    {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, uint32 _spellId, ClassFamilyMask _mask, int16 _charges = 0)
        : op(_op), type(_type), charges(_charges), value(_value), mask(_mask), spellId(_spellId), lastAffected(nullptr)
    {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, SpellEntry const* spellEntry, SpellEffectIndex eff, int16 _charges = 0);

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, Aura const* aura, int16 _charges = 0);

    bool isAffectedOnSpell(SpellEntry const* spell) const;

    SpellModOp op : 8;
    SpellModType type : 8;
    int16 charges : 16;
    int32 value;
    ClassFamilyMask mask;
    uint32 spellId;
    Spell const* lastAffected;
};

enum TrainerSpellState
{
    TRAINER_SPELL_GREEN = 0,
    TRAINER_SPELL_RED = 1,
    TRAINER_SPELL_GRAY = 2,
    TRAINER_SPELL_GREEN_DISABLED = 10
};

enum ActionButtonUpdateState
{
    ACTIONBUTTON_UNCHANGED = 0,
    ACTIONBUTTON_CHANGED = 1,
    ACTIONBUTTON_NEW = 2,
    ACTIONBUTTON_DELETED        = 3
};

enum ActionButtonType
{
    ACTION_BUTTON_SPELL = 0x00,
    ACTION_BUTTON_C = 0x01,
    ACTION_BUTTON_MACRO = 0x40,
    ACTION_BUTTON_CMACRO = ACTION_BUTTON_C | ACTION_BUTTON_MACRO,
    ACTION_BUTTON_ITEM = 0x80
};

#define ACTION_BUTTON_ACTION(X) (uint32(X) & 0x00FFFFFF)
#define ACTION_BUTTON_TYPE(X)   ((uint32(X) & 0xFF000000) >> 24)
#define MAX_ACTION_BUTTON_ACTION_VALUE (0x00FFFFFF+1)

struct ActionButton
{
    ActionButton() : packedData(0), uState(ACTIONBUTTON_NEW) {}

    uint32 packedData;
    ActionButtonUpdateState uState;

    ActionButtonType GetType() const
    {
        return ActionButtonType(ACTION_BUTTON_TYPE(packedData));
    }
    uint32 GetAction() const
    {
        return ACTION_BUTTON_ACTION(packedData);
    }
    void SetActionAndType(uint32 action, ActionButtonType type)
    {
        uint32 newData = action | (uint32(type) << 24);
        if (newData != packedData || uState == ACTIONBUTTON_DELETED)
        {
            packedData = newData;
            if (uState != ACTIONBUTTON_NEW)
            {
                uState = ACTIONBUTTON_CHANGED;
            }
        }
    }
};

#define  MAX_ACTION_BUTTONS 120

typedef std::map<uint8, ActionButton> ActionButtonList;

struct PlayerCreateInfoItem
{
    PlayerCreateInfoItem(uint32 id, uint32 amount) : item_id(id), item_amount(amount) {}

    uint32 item_id;
    uint32 item_amount;
};

typedef std::list<PlayerCreateInfoItem> PlayerCreateInfoItems;

struct PlayerClassLevelInfo
{
    PlayerClassLevelInfo() : basehealth(0), basemana(0) {}
    uint16 basehealth;
    uint16 basemana;
};

struct PlayerClassInfo
{
    PlayerClassInfo() : levelInfo(nullptr) {}

    PlayerClassLevelInfo* levelInfo;
};

struct PlayerLevelInfo
{
    PlayerLevelInfo()
    {
        for (int i = 0; i < MAX_STATS; ++i)
        {
            stats[i] = 0;
        }
    }

    uint8 stats[MAX_STATS];
};

typedef std::list<uint32> PlayerCreateInfoSpells;

struct PlayerCreateInfoAction
{
    PlayerCreateInfoAction() : button(0), type(0), action(0) {}
    PlayerCreateInfoAction(uint8 _button, uint32 _action, uint8 _type) : button(_button), type(_type), action(_action) {}

    uint8 button;
    uint8 type;
    uint32 action;
};

typedef std::list<PlayerCreateInfoAction> PlayerCreateInfoActions;

struct PlayerInfo
{

    PlayerInfo() : displayId_m(0), displayId_f(0), levelInfo(nullptr), areaId(0), mapId(0), orientation(0.0f), positionX(0.0f), positionY(0.0f), positionZ(0.0f) {}

    uint32 mapId;
    uint32 areaId;
    float positionX;
    float positionY;
    float positionZ;
    float orientation;
    uint16 displayId_m;
    uint16 displayId_f;
    PlayerCreateInfoItems item;
    PlayerCreateInfoSpells spell;
    PlayerCreateInfoActions action;

    PlayerLevelInfo* levelInfo;
};

struct PvPInfo
{
    PvPInfo() : inHostileArea(false), endTimer(0) {}

    bool inHostileArea;
    time_t endTimer;
};

struct Areas
{
    uint32 areaID;
    uint32 areaFlag;
    float x1;
    float x2;
    float y1;
    float y2;
};

enum RaidGroupError
{
    ERR_RAID_GROUP_REQUIRED = 1,
    ERR_RAID_GROUP_FULL     = 2
};

enum PlayerFlags
{
    PLAYER_FLAGS_NONE                   = 0x00000000,
    PLAYER_FLAGS_GROUP_LEADER           = 0x00000001,
    PLAYER_FLAGS_AFK                    = 0x00000002,
    PLAYER_FLAGS_DND                    = 0x00000004,
    PLAYER_FLAGS_GM                     = 0x00000008,
    PLAYER_FLAGS_GHOST                  = 0x00000010,
    PLAYER_FLAGS_RESTING                = 0x00000020,
    PLAYER_FLAGS_UNK7                   = 0x00000040,
    PLAYER_FLAGS_FFA_PVP                = 0x00000080,
    PLAYER_FLAGS_CONTESTED_PVP          = 0x00000100,
    PLAYER_FLAGS_IN_PVP                 = 0x00000200,
    PLAYER_FLAGS_HIDE_HELM              = 0x00000400,
    PLAYER_FLAGS_HIDE_CLOAK             = 0x00000800,
    PLAYER_FLAGS_PARTIAL_PLAY_TIME      = 0x00001000,
    PLAYER_FLAGS_NO_PLAY_TIME           = 0x00002000,
    PLAYER_FLAGS_UNK15                  = 0x00004000,
    PLAYER_FLAGS_UNK16                  = 0x00008000,
    PLAYER_FLAGS_SANCTUARY              = 0x00010000,
    PLAYER_FLAGS_TAXI_BENCHMARK         = 0x00020000,
    PLAYER_FLAGS_PVP_TIMER              = 0x00040000,
    PLAYER_FLAGS_XP_USER_DISABLED       = 0x02000000,
};

enum PlayerFieldByteFlags
{
    PLAYER_FIELD_BYTE_TRACK_STEALTHED   = 0x02,
    PLAYER_FIELD_BYTE_RELEASE_TIMER     = 0x08,
    PLAYER_FIELD_BYTE_NO_RELEASE_WINDOW = 0x10
};

enum AuraVision
{
    AURA_VISION_NONE         = 0x00,
    AURA_VISION_AMORE_0      = 0x02,
    AURA_VISION_AMORE_1      = 0x04,
    AURA_VISION_AMORE_2      = 0x08,
    AURA_VISION_AMORE_3      = 0x10,
    AURA_VISION_STEALTH      = 0x20,
    AURA_VISION_INVISIBILITY = 0x40
};

enum PlayerExtraFlags
{

    PLAYER_EXTRA_GM_ON              = 0x0001,
    PLAYER_EXTRA_GM_ACCEPT_TICKETS  = 0x0002,
    PLAYER_EXTRA_ACCEPT_WHISPERS    = 0x0004,
    PLAYER_EXTRA_TAXICHEAT          = 0x0008,
    PLAYER_EXTRA_GM_INVISIBLE       = 0x0010,
    PLAYER_EXTRA_GM_CHAT            = 0x0020,
    PLAYER_EXTRA_AUCTION_NEUTRAL    = 0x0040,
    PLAYER_EXTRA_AUCTION_ENEMY      = 0x0080,

    PLAYER_EXTRA_PVP_DEATH          = 0x0100
};

enum AtLoginFlags
{
    AT_LOGIN_NONE                 = 0x00,
    AT_LOGIN_RENAME               = 0x01,
    AT_LOGIN_RESET_SPELLS         = 0x02,
    AT_LOGIN_RESET_TALENTS        = 0x04,

    AT_LOGIN_FIRST                = 0x20
};

enum SkillUpdateState
{
    SKILL_UNCHANGED             = 0,
    SKILL_CHANGED               = 1,
    SKILL_NEW                   = 2,
    SKILL_DELETED               = 3
};

struct SkillStatusData
{
    SkillStatusData(uint8 _pos, SkillUpdateState _uState) : pos(_pos), uState(_uState) {}

    uint8 pos;
    SkillUpdateState uState;
};

typedef std::unordered_map<uint32, SkillStatusData> SkillStatusMap;

enum TransferAbortReason
{
    TRANSFER_ABORT_MAX_PLAYERS                  = 0x01,
    TRANSFER_ABORT_NOT_FOUND                    = 0x02,
    TRANSFER_ABORT_TOO_MANY_INSTANCES           = 0x03,
    TRANSFER_ABORT_SILENTLY                     = 0x04,
    TRANSFER_ABORT_ZONE_IN_COMBAT               = 0x05,
};

enum InstanceResetWarningType
{
    RAID_INSTANCE_WARNING_HOURS     = 1,
    RAID_INSTANCE_WARNING_MIN       = 2,
    RAID_INSTANCE_WARNING_MIN_SOON  = 3,
    RAID_INSTANCE_WELCOME           = 4
};

enum TeleportToOptions
{
    TELE_TO_GM_MODE             = 0x01,
    TELE_TO_NOT_LEAVE_TRANSPORT = 0x02,
    TELE_TO_NOT_LEAVE_COMBAT    = 0x04,
    TELE_TO_NOT_UNSUMMON_PET    = 0x08,
    TELE_TO_SPELL               = 0x10
};

enum PlayerLoginQueryIndex
{
    PLAYER_LOGIN_QUERY_LOADFROM,
    PLAYER_LOGIN_QUERY_LOADGROUP,
    PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES,
    PLAYER_LOGIN_QUERY_LOADAURAS,
    PLAYER_LOGIN_QUERY_LOADSPELLS,
    PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADHONORCP,
    PLAYER_LOGIN_QUERY_LOADREPUTATION,
    PLAYER_LOGIN_QUERY_LOADINVENTORY,
    PLAYER_LOGIN_QUERY_LOADITEMLOOT,
    PLAYER_LOGIN_QUERY_LOADACTIONS,
    PLAYER_LOGIN_QUERY_LOADSOCIALLIST,
    PLAYER_LOGIN_QUERY_LOADHOMEBIND,
    PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,
    PLAYER_LOGIN_QUERY_LOADGUILD,
    PLAYER_LOGIN_QUERY_LOADBGDATA,
    PLAYER_LOGIN_QUERY_LOADSKILLS,
    PLAYER_LOGIN_QUERY_LOADMAILS,
    PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,

    MAX_PLAYER_LOGIN_QUERY
};

enum ReputationSource
{
    REPUTATION_SOURCE_KILL,
    REPUTATION_SOURCE_QUEST,
    REPUTATION_SOURCE_SPELL
};

#define MAX_MONEY_AMOUNT        (0x7FFFFFFF-1)

enum PlayerRestState
{
    REST_STATE_RESTED           = 0x01,
    REST_STATE_NORMAL           = 0x02,
    REST_STATE_RAF_LINKED       = 0x04
};

enum PlayerMountResult
{
    MOUNTRESULT_INVALIDMOUNTEE  = 0,
    MOUNTRESULT_TOOFARAWAY      = 1,
    MOUNTRESULT_ALREADYMOUNTED  = 2,
    MOUNTRESULT_NOTMOUNTABLE    = 3,
    MOUNTRESULT_NOTYOURPET      = 4,
    MOUNTRESULT_OTHER           = 5,
    MOUNTRESULT_LOOTING         = 6,
    MOUNTRESULT_RACECANTMOUNT   = 7,
    MOUNTRESULT_SHAPESHIFTED    = 8,
    MOUNTRESULT_FORCEDDISMOUNT  = 9,
    MOUNTRESULT_OK              = 10
};

enum PlayerDismountResult
{
    DISMOUNTRESULT_NOPET        = 0,
    DISMOUNTRESULT_NOTMOUNTED   = 1,
    DISMOUNTRESULT_NOTYOURPET   = 2,
    DISMOUNTRESULT_OK           = 3
};

class PlayerTaxi
{
    public:
        PlayerTaxi();
        ~PlayerTaxi() {}

        void InitTaxiNodes(uint32 race, uint32 level);
        void LoadTaxiMask(const char* data);

        bool IsTaximaskNodeKnown(uint32 nodeidx) const
        {
            uint8  field   = uint8((nodeidx - 1) / 32);
            uint32 submask = 1 << ((nodeidx - 1) % 32);
            return (m_taximask[field] & submask) == submask;
        }

        bool SetTaximaskNode(uint32 nodeidx)
        {
            uint8  field   = uint8((nodeidx - 1) / 32);
            uint32 submask = 1 << ((nodeidx - 1) % 32);
            if ((m_taximask[field] & submask) != submask)
            {
                m_taximask[field] |= submask;
                return true;
            }
            else
            {
                return false;
            }
        }

        void AppendTaximaskTo(ByteBuffer& data, bool all);

        bool LoadTaxiDestinationsFromString(const std::string& values, Team team);

        std::string SaveTaxiDestinationsToString();

        void ClearTaxiDestinations()
        {
            m_TaxiDestinations.clear();
        }

        void AddTaxiDestination(uint32 dest)
        {
            m_TaxiDestinations.push_back(dest);
        }

        uint32 GetTaxiSource() const
        {
            return m_TaxiDestinations.empty() ? 0 : m_TaxiDestinations.front();
        }

        uint32 GetTaxiDestination() const
        {
            return m_TaxiDestinations.size() < 2 ? 0 : m_TaxiDestinations[1];
        }

        uint32 GetCurrentTaxiPath() const;

        uint32 NextTaxiDestination()
        {
            m_TaxiDestinations.pop_front();
            return GetTaxiDestination();
        }

        bool empty() const
        {
            return m_TaxiDestinations.empty();
        }

        friend std::ostringstream& operator<< (std::ostringstream& ss, PlayerTaxi const& taxi);

    private:
        TaxiMask m_taximask;
        std::deque<uint32> m_TaxiDestinations;
};

std::ostringstream& operator<< (std::ostringstream& ss, PlayerTaxi const& taxi);

struct TradeStatusInfo
{
    TradeStatusInfo() : Status(TRADE_STATUS_BUSY), TraderGuid(), Result(EQUIP_ERR_OK),
        IsTargetResult(false), ItemLimitCategoryId(0), Slot(0) {}

    TradeStatus Status;
    ObjectGuid TraderGuid = 0;
    InventoryResult Result;
    bool IsTargetResult;
    uint32 ItemLimitCategoryId;
    uint8 Slot;
};

class Player : public Unit
{
    friend class WorldSession;

    public:
        explicit Player(WorldSession* session);
        ~Player();

        time_t lastTimeLooted;

        void CleanupsBeforeDelete() override;

        void AddToWorld() override;
        void RemoveFromWorld() override;

        bool OutlivesItsGrid() const override { return true; }

        void MovedTo(float x, float y, float z, float o) override { SetPosition(x, y, z, o); }

        bool MovesItself() const override { return true; }

        bool TeleportTo(uint32 mapid, float x, float y, float z, float orientation, uint32 options = 0, bool allowNoDelay = false);

        bool TeleportTo(Geometry::Placement const& loc, uint32 options = 0)
        {
            return TeleportTo(loc.MapId(), loc.X(), loc.Y(), loc.Z(), loc.Facing(), options);
        }

        void SetSummonPoint(uint32 mapid, float x, float y, float z)
        {
            m_summon.Offer(mapid, x, y, z, time(nullptr));
        }
        void SummonIfPossible(bool agree);

        bool Create(uint32 guidlow, const std::string& name, uint8 race, uint8 class_, uint8 gender, uint8 skin, uint8 face, uint8 hairStyle, uint8 hairColor, uint8 facialHair, uint8 outfitId);

        void Update(uint32 update_diff, uint32 time) override;

        bool IsInWater() const override
        {
            return m_perils.InWater();
        }
        bool IsUnderWater() const override;
        bool IsDrowning() const { return m_perils.Drowning(); }

        Perils& Dangers() { return m_perils; }
        Perils const& Dangers() const { return m_perils; }

        Drink& Drinking() { return m_drink; }
        Drink const& Drinking() const { return m_drink; }
        bool IsFalling()
        {
            return Where().Z() < m_lastFallZ;
        }

        void SendInitialPacketsBeforeAddToMap(bool deferLoginTimeSpeed = false);
        void SendInitialPacketsAfterAddToMap(InitialWorldEntryContext const* initialEntry = nullptr);
        void SendLoginTimeSpeed();
        void SendInstanceResetWarning(uint32 mapid, uint32 time);

        Creature* GetNPCIfCanInteractWith(ObjectGuid guid, uint32 npcflagmask);

        GameObject* GetGameObjectIfCanInteractWith(ObjectGuid guid, uint32 gameobject_type = MAX_GAMEOBJECT_TYPE) const;

        void ToggleAFK();
        void ToggleDND();
        bool isAFK() const
        {
            return HasPlayerFlag(PLAYER_FLAGS_AFK);
        }
        bool isDND() const
        {
            return HasPlayerFlag(PLAYER_FLAGS_DND);
        }
        ChatTagFlags GetChatTag() const;
        std::string autoReplyMsg;

        PlayerSocial* GetSocial()
        {
            return m_social;
        }

        void SetCreatedDate(uint32 createdDate)
        {
            m_created_date = createdDate;
        }

        uint32 GetCreatedDate()
        {
            return m_created_date;
        }

        PlayerTaxi m_taxi;
        void InitTaxiNodes()
        {
            m_taxi.InitTaxiNodes(getRace(), getLevel());
        }

        bool ActivateTaxiPathTo(std::vector<uint32> const& nodes, Creature* npc = nullptr, uint32 spellid = 0);

        bool ActivateTaxiPathTo(uint32 taxi_path_id, uint32 spellid = 0);

        void ContinueTaxiFlight();
        void Mount(uint32 mount, uint32 spellId = 0) override;
        void Unmount(bool from_aura = false) override;
        void SendMountResult(PlayerMountResult result);
        void SendDismountResult(PlayerDismountResult result);
        bool isAcceptTickets() const { return GetSession()->GetSecurity() >= SEC_GAMEMASTER && (m_ExtraFlags & PLAYER_EXTRA_GM_ACCEPT_TICKETS); }

        void SetAcceptTicket(bool on) { if (on) { m_ExtraFlags |= PLAYER_EXTRA_GM_ACCEPT_TICKETS; } else { m_ExtraFlags &= ~PLAYER_EXTRA_GM_ACCEPT_TICKETS; } }

        bool isAcceptWhispers() const { return m_ExtraFlags & PLAYER_EXTRA_ACCEPT_WHISPERS; }

        void SetAcceptWhispers(bool on) { if (on) { m_ExtraFlags |= PLAYER_EXTRA_ACCEPT_WHISPERS; } else { m_ExtraFlags &= ~PLAYER_EXTRA_ACCEPT_WHISPERS; } }

        bool isGameMaster() const { return m_ExtraFlags & PLAYER_EXTRA_GM_ON; }

        bool CanSeeAura(uint8 vision) const { return HasByteFlag(PLAYER_FIELD_BYTES2, 1, vision); }
        void ApplyAuraVision(uint8 vision, bool apply)
        {
            ApplyModByteFlag(PLAYER_FIELD_BYTES2, 1, vision, apply);
        }

        uint8 GetActionBars() const { return GetByteValue(PLAYER_FIELD_BYTES, 2); }
        void SetActionBars(uint8 bars) { SetByteValue(PLAYER_FIELD_BYTES, 2, bars); }

        void SetShownComboPoints(uint8 points) { SetByteValue(PLAYER_FIELD_BYTES, 1, points); }

        void ShowReleaseTimer(bool on)
        {
            ApplyModByteFlag(PLAYER_FIELD_BYTES, 0, PLAYER_FIELD_BYTE_RELEASE_TIMER, on);
        }
        void TrackStealthed(bool on)
        {
            ApplyModByteFlag(PLAYER_FIELD_BYTES, 0, PLAYER_FIELD_BYTE_TRACK_STEALTHED, on);
        }

        void SetShownHonorRank(uint8 rank) { SetByteValue(PLAYER_BYTES_3, 3, rank); }
        void SetShownHighestHonorRank(uint8 rank) { SetByteValue(PLAYER_FIELD_BYTES, 3, rank); }
        uint8 GetHonorBar() const { return GetByteValue(PLAYER_FIELD_BYTES2, 0); }
        void SetHonorBar(uint8 filled) { SetByteValue(PLAYER_FIELD_BYTES2, 0, filled); }

        uint16 GetDrunkAndGender() const { return GetUInt16Value(PLAYER_BYTES_3, 0); }
        void SetDrunkAndGender(uint16 drunk, uint8 gender)
        {
            SetUInt16Value(PLAYER_BYTES_3, 0, (drunk & 0xFFFE) | gender);
        }

        enum class Tracked { Creatures, Resources };
        void ApplyTracking(Tracked what, uint32 bit, bool on)
        {
            ApplyModFlag(what == Tracked::Creatures ? PLAYER_TRACK_CREATURES : PLAYER_TRACK_RESOURCES,
                         bit, on);
        }
        void ClearTracking()
        {
            SetUInt32Value(PLAYER_TRACK_CREATURES, 0);
            SetUInt32Value(PLAYER_TRACK_RESOURCES, 0);
        }

        uint32 GetExploredZones(uint16 slot) const
        {
            return GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + slot);
        }
        void SetExploredZones(uint16 slot, uint32 mask)
        {
            SetUInt32Value(PLAYER_EXPLORED_ZONES_1 + slot, mask);
        }

        ObjectGuid GetDuelArbiterGuid() const { return GetGuidValue(PLAYER_DUEL_ARBITER); }
        void SetDuelArbiterGuid(ObjectGuid guid) { SetGuidValue(PLAYER_DUEL_ARBITER, guid); }

        void ApplyDamageDonePercent(uint32 school, float percent, bool apply)
        {
            ApplyModSignedFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + school, percent, apply);
        }
        void SetDamageDonePercent(uint32 school, float percent)
        {
            SetFloatValue(PLAYER_FIELD_MOD_DAMAGE_DONE_PCT + school, percent);
        }

        bool HasPlayerFlag(uint32 flag) const { return HasFlag(PLAYER_FLAGS, flag); }
        void SetPlayerFlag(uint32 flag) { SetFlag(PLAYER_FLAGS, flag); }
        void RemovePlayerFlag(uint32 flag) { RemoveFlag(PLAYER_FLAGS, flag); }
        void ApplyPlayerFlag(uint32 flag, bool apply) { ApplyModFlag(PLAYER_FLAGS, flag, apply); }
        void TogglePlayerFlag(uint32 flag) { ToggleFlag(PLAYER_FLAGS, flag); }
        uint32 GetPlayerFlags() const { return GetUInt32Value(PLAYER_FLAGS); }
        void SetAllPlayerFlags(uint32 flags) { SetUInt32Value(PLAYER_FLAGS, flags); }

        void SetGameMaster(bool on);

        bool isGMChat() const { return GetSession()->GetSecurity() >= SEC_MODERATOR && (m_ExtraFlags & PLAYER_EXTRA_GM_CHAT); }

        void SetGMChat(bool on) { if (on) { m_ExtraFlags |= PLAYER_EXTRA_GM_CHAT; } else { m_ExtraFlags &= ~PLAYER_EXTRA_GM_CHAT; } }

        bool IsTaxiCheater() const { return m_ExtraFlags & PLAYER_EXTRA_TAXICHEAT; }

        void SetTaxiCheater(bool on) { if (on) { m_ExtraFlags |= PLAYER_EXTRA_TAXICHEAT; } else { m_ExtraFlags &= ~PLAYER_EXTRA_TAXICHEAT; } }

        bool isGMVisible() const { return !(m_ExtraFlags & PLAYER_EXTRA_GM_INVISIBLE); }

        void SetGMVisible(bool on);

        void SetPvPDeath(bool on)
        {
            if (on)
            {
                m_ExtraFlags |= PLAYER_EXTRA_PVP_DEATH;
            }
            else
            {
                m_ExtraFlags &= ~PLAYER_EXTRA_PVP_DEATH;
            }
        }

        int GetAuctionAccessMode() const
        {
            return m_ExtraFlags & PLAYER_EXTRA_AUCTION_ENEMY ? -1 : (m_ExtraFlags & PLAYER_EXTRA_AUCTION_NEUTRAL ? 1 : 0);
        }

        void SetAuctionAccessMode(int state)
        {
            m_ExtraFlags &= ~(PLAYER_EXTRA_AUCTION_ENEMY | PLAYER_EXTRA_AUCTION_NEUTRAL);

            if (state < 0)
            {
                m_ExtraFlags |= PLAYER_EXTRA_AUCTION_ENEMY;
            }
            else if (state > 0)
            {
                m_ExtraFlags |= PLAYER_EXTRA_AUCTION_NEUTRAL;
            }
        }

        void GiveXP(uint32 xp, Unit* victim);

        void GiveLevel(uint32 level);

        void InitStatsForLevel(bool reapplyMods = false);

        void SetDeathState(DeathState s) override;

        void RemovePet(PetSaveMode mode) { m_petMgr.Remove(mode); }

        void RemoveMiniPet();
        Pet* GetMiniPet() const override;

        void _SetMiniPet(Pet* pet)
        {
            m_miniPetGuid = pet ? pet->GetObjectGuid() : 0;
        }

        void Say(const std::string& text, const uint32 language);
        void Yell(const std::string& text, const uint32 language);
        void TextEmote(const std::string& text);

        void LogWhisper(const std::string& text, ObjectGuid receiver);
        void Whisper(const std::string& text, const uint32 language, ObjectGuid receiver);

        void SetVirtualItemSlot(uint8 i, Item* item);
        void SetSheath(SheathState sheathed) override;
        bool ViableEquipSlots(ItemPrototype const* proto, uint8 *viable_slots) const;
        uint8 FindEquipSlot(ItemPrototype const* proto, uint32 slot, bool swap) const;

        uint32 GetItemCount(uint32 item, bool inBankAlso = false, Item* skipItem = nullptr) const
        {
            return m_inventory.Count(item, inBankAlso ? SCOPE_EVERYWHERE : SCOPE_TO_HAND, skipItem);
        }

        Item* GetItemByGuid(ObjectGuid guid) const { return m_inventory.ByGuid(guid); }

        Item* GetItemByEntry(uint32 item) const { return m_inventory.ByEntry(item); }

        Item* GetItemByPos(uint16 pos) const { return m_inventory.At(pos); }

        Item* GetItemByPos(uint8 bag, uint8 slot) const { return m_inventory.At(bag, slot); }
        Item* GetWeaponForAttack(WeaponAttackType attackType) const
        {
            return GetWeaponForAttack(attackType, false, false);
        }

        Item* GetWeaponForAttack(WeaponAttackType attackType, bool nonbroken, bool useable) const;

        Item* GetShield(bool useable = false) const;

        Inventory& Owns() { return m_inventory; }
        Inventory const& Owns() const { return m_inventory; }

        ItemSaveQueue& ItemSaves() { return m_inventory.Saves(); }
        ItemSaveQueue const& ItemSaves() const { return m_inventory.Saves(); }

        bool IsValidPos(uint16 pos, bool explicit_pos) const { return m_inventory.Exists(Inventory::Container(pos), Inventory::Slot(pos), explicit_pos); }

        bool IsValidPos(uint8 bag, uint8 slot, bool explicit_pos) const { return m_inventory.Exists(bag, slot, explicit_pos); }

        uint8 GetBankBagSlotCount() const { return GetByteValue(PLAYER_BYTES_2, 2); }

        void SetBankBagSlotCount(uint8 count) { SetByteValue(PLAYER_BYTES_2, 2, count); }

        bool HasItemCount(uint32 item, uint32 count, bool inBankAlso = false) const
        {
            return m_inventory.Holds(item, count, inBankAlso ? SCOPE_EVERYWHERE : SCOPE_TO_HAND);
        }

        bool HasItemFitToSpellReqirements(SpellEntry const* spellInfo, Item const* ignoreItem = nullptr);

        bool CanNoReagentCast(SpellEntry const* spellInfo) const;
        bool HasItemWithIdEquipped(uint32 item, uint32 count, uint8 except_slot = NULL_SLOT) const;
        InventoryResult CanTakeMoreSimilarItems(Item* pItem) const { return m_inventory.RoomForMore(pItem->GetEntry(), pItem->GetCount(), pItem); }

        InventoryResult CanTakeMoreSimilarItems(uint32 entry, uint32 count) const { return m_inventory.RoomForMore(entry, count, nullptr); }

        InventoryResult CanStoreNewItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, uint32 item, uint32 count, uint32* no_space_count = nullptr) const
        {
            return m_inventory.PlanToStore(bag, slot, dest, item, count, nullptr, false, no_space_count);
        }

        InventoryResult CanStoreItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, Item* pItem, bool swap = false) const
        {
            if (!pItem)
            {
                return EQUIP_ERR_ITEM_NOT_FOUND;
            }
            uint32 count = pItem->GetCount();
            return m_inventory.PlanToStore(bag, slot, dest, pItem->GetEntry(), count, pItem, swap, nullptr);
        }

        InventoryResult CanStoreItems(Item** pItem, int count) const
        {
            return m_inventory.PlanForAll(pItem, count);
        }

        InventoryResult CanEquipNewItem(uint8 slot, uint16& dest, uint32 item, bool swap) const;

        InventoryResult CanEquipItem(uint8 slot, uint16& dest, Item* pItem, bool swap, bool direct_action = true) const;

        InventoryResult CanEquipUniqueItem(Item* pItem, uint8 except_slot = NULL_SLOT) const;

        InventoryResult CanEquipUniqueItem(ItemPrototype const* itemProto, uint8 except_slot = NULL_SLOT) const;

        InventoryResult CanUnequipItems(uint32 item, uint32 count) const;

        InventoryResult CanUnequipItem(uint16 src, bool swap) const
        {
            return m_inventory.CanTakeOff(src, swap);
        }

        InventoryResult CanBankItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, Item* pItem, bool swap, bool not_loading = true) const
        {
            return m_inventory.PlanToBank(bag, slot, dest, pItem, swap, not_loading);
        }

        InventoryResult CanUseItem(Item* pItem, bool direct_action = true) const;

        bool HasItemTotemCategory(uint32 TotemCategory) const
        {
            return m_inventory.HasTotem(TotemCategory);
        }
        InventoryResult CanUseItem(ItemPrototype const* pItem, bool direct_action = true) const;
        InventoryResult CanUseAmmo(uint32 item) const;

        InventoryResult CanUseItemEluna(uint32 itemEntry) const;

        Item* StoreNewItem(ItemPosCountVec const& pos, uint32 item, bool update, int32 randomPropertyId = 0);

        Item* StoreItem(ItemPosCountVec const& pos, Item* pItem, bool update)
        {
            return m_inventory.Store(pos, pItem, update);
        }

        Item* EquipNewItem(uint16 pos, uint32 item, bool update);

        Item* EquipItem(uint16 pos, Item* pItem, bool update);

        void AutoUnequipOffhandIfNeed();

        bool StoreNewItemInBestSlots(uint32 item_id, uint32 item_count);

        Item* StoreNewItemInInventorySlot(uint32 itemEntry, uint32 amount);

        void AutoStoreLoot(Occupant const* lootTarget, uint32 loot_id, LootStore const& store, bool broadcast = false, uint8 bag = NULL_BAG, uint8 slot = NULL_SLOT);

        void AutoStoreLoot(Loot& loot, bool broadcast = false, uint8 bag = NULL_BAG, uint8 slot = NULL_SLOT);

        Item* ConvertItem(Item* item, uint32 newItemId);

        void ApplyEquipCooldown(Item* pItem);

        void SetAmmo(uint32 item);

        void RemoveAmmo();

        bool CheckAmmoCompatibility(const ItemPrototype* ammo_proto) const;

        void QuickEquipItem(uint16 pos, Item* pItem) { m_inventory.QuickWear(pos, pItem); }

        Item* BankItem(ItemPosCountVec const& dest, Item* pItem, bool update)
        {
            return StoreItem(dest, pItem, update);
        }

        Item* BankItem(uint16 pos, Item* pItem, bool update);

        void RemoveItem(uint8 bag, uint8 slot, bool update);

        void MoveItemFromInventory(uint8 bag, uint8 slot, bool update);

        void MoveItemToInventory(ItemPosCountVec const& dest, Item* pItem, bool update, bool in_characterInventoryDB = false);

        void RemoveItemDependentAurasAndCasts(Item* pItem);

        void DestroyItem(uint8 bag, uint8 slot, bool update);

        uint32 DestroyItemCount(uint32 item, uint32 count, bool update, bool unequip_check = false, bool delete_from_bank = false, bool delete_from_buyback = false);

        void DestroyItemCount(Item* item, uint32& count, bool update);

        void DestroyConjuredItems(bool update);

        void DestroyZoneLimitedItem(bool update, uint32 new_zone);

        void SplitItem(uint16 src, uint16 dst, uint32 count);

        void SwapItem(uint16 src, uint16 dst);

        bool PourBagInto(Item* pSrcItem, uint16 src, Item* pDstItem, uint16 dst);

        void AddItemToBuyBackSlot(Item* pItem) { m_inventory.ToBuyback(pItem); }

        Item* GetItemFromBuyBackSlot(uint32 slot) { return m_inventory.InBuyback(slot); }

        void RemoveItemFromBuyBackSlot(uint32 slot, bool del) { m_inventory.ClearBuyback(slot, del); }

        uint32 GetMaxKeyringSize() const { return Inventory::MaxKeyring(); }

        void SendEquipError(InventoryResult msg, Item* pItem, Item* pItem2 = nullptr, uint32 itemid = 0) const;

        void SendBuyError(BuyResult msg, Creature* pCreature, uint32 item, uint32 param);

        void SendSellError(SellResult msg, Creature* pCreature, ObjectGuid itemGuid, uint32 param);
        void SendOpenContainer();

        bool IsTwoHandUsed() const
        {
            Item* mainItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            return mainItem && mainItem->GetProto()->InventoryType == INVTYPE_2HWEAPON;
        }

        void SendNewItem(Item* item, uint32 count, bool received, bool created, bool broadcast = false, bool showInChat = true);

        bool BuyItemFromVendor(ObjectGuid vendorGuid, uint32 item, uint8 count, uint8 bag, uint8 slot);

        float GetReputationPriceDiscount(Creature const* pCreature) const;

        Player* GetTrader() const { return m_trade ? m_trade->GetTrader() : nullptr; }

        TradeData* GetTradeData() const { return m_trade; }

        void OpenTradeWith(Player* other);

        void DropTrade();

        void TradeCancel(bool sendback);

        void ApplyEnchantment(Item* item, EnchantmentSlot slot, bool apply, bool apply_dur = true, bool ignore_condition = false);

        void ApplyEnchantment(Item* item, bool apply);

        void LoadCorpse();

        void LoadPet();

        void PrepareGossipMenu(Occupant* pSource, uint32 menuId = 0);

        void SendPreparedGossip(Occupant* pSource);
        void OnGossipSelect(Occupant* pSource, uint32 gossipListId);

        uint32 GetGossipTextId(uint32 menuId, Occupant* pSource);

        uint32 GetGossipTextId(Occupant* pSource);

        uint32 GetDefaultGossipMenuForSource(Occupant* pSource);

        uint32 GetQuestLevelForPlayer(Quest const* pQuest) const { return pQuest && (pQuest->GetQuestLevel() > 0) ? (uint32)pQuest->GetQuestLevel() : getLevel(); }

        void PrepareQuestMenu(ObjectGuid guid);

        void SendPreparedQuest(ObjectGuid guid);

        bool IsActiveQuest(uint32 quest_id) const;

        bool IsCurrentQuest(uint32 quest_id, uint8 completed_or_not = 0) const;

        Quest const* GetNextQuest(ObjectGuid guid, Quest const* pQuest);

        bool CanSeeStartQuest(Quest const* pQuest) const;

        bool CanTakeQuest(Quest const* pQuest, bool msg) const;

        bool CanAddQuest(Quest const* pQuest, bool msg) const;

        bool CanCompleteQuest(uint32 quest_id) const;

        bool CanCompleteRepeatableQuest(Quest const* pQuest) const;

        bool CanRewardQuest(Quest const* pQuest, bool msg) const;

        bool CanRewardQuest(Quest const* pQuest, uint32 reward, bool msg) const;

        Quest const* GetQuestTemplate(uint32 quest_id);
        void AddQuest(Quest const* pQuest, Object* questGiver);

        void CompleteQuest(uint32 quest_id, QuestStatus status = QUEST_STATUS_COMPLETE);

        void IncompleteQuest(uint32 quest_id);

        void RewardQuest(Quest const* pQuest, uint32 reward, Object* questGiver, bool announce = true);

        void FailQuest(uint32 quest_id);

        bool SatisfyQuestSkill(Quest const* qInfo, bool msg) const;

        bool SatisfyQuestLevel(Quest const* qInfo, bool msg) const;

        bool SatisfyQuestLog(bool msg) const;

        bool SatisfyQuestPreviousQuest(Quest const* qInfo, bool msg) const;

        bool SatisfyQuestClass(Quest const* qInfo, bool msg) const;

        bool SatisfyQuestRace(Quest const* qInfo, bool msg) const;

        bool SatisfyQuestReputation(Quest const* qInfo, bool msg) const;

        bool SatisfyQuestStatus(Quest const* qInfo, bool msg) const;

        bool SatisfyQuestTimed(Quest const* qInfo, bool msg) const;

        bool SatisfyQuestExclusiveGroup(Quest const* qInfo, bool msg) const;

        bool SatisfyQuestNextChain(Quest const* qInfo, bool msg) const;

        bool SatisfyQuestPrevChain(Quest const* qInfo, bool msg) const;
        bool CanGiveQuestSourceItemIfNeed(Quest const* pQuest, ItemPosCountVec* dest = nullptr) const;

        void GiveQuestSourceItemIfNeed(Quest const* pQuest);

        bool TakeQuestSourceItem(uint32 quest_id, bool msg);

        bool GetQuestRewardStatus(uint32 quest_id) const;

        QuestStatus GetQuestStatus(uint32 quest_id) const;

        void SetQuestStatus(uint32 quest_id, QuestStatus status);

        void SetQuestRewarded(uint32 quest_id, bool rewarded);

        uint32 GetQuestSlotQuestId(uint16 slot) const
        {
            return GetUInt32Value(quests::FieldOf(slot, QUEST_ID_OFFSET));
        }

        void SetQuestSlot(uint16 slot, uint32 quest_id, uint32 timer = 0)
        {
            SetUInt32Value(quests::FieldOf(slot, QUEST_ID_OFFSET), quest_id);
            SetUInt32Value(quests::FieldOf(slot, QUEST_COUNT_STATE_OFFSET), 0);
            SetUInt32Value(quests::FieldOf(slot, QUEST_TIME_OFFSET), timer);
        }

        void SetQuestSlotCounter(uint16 slot, uint8 counter, uint8 count)
        {
            uint16 const field = quests::FieldOf(slot, QUEST_COUNT_STATE_OFFSET);
            SetUInt32Value(field, quests::WithCounter(GetUInt32Value(field), counter, count));
        }

        uint8 GetQuestSlotCounter(uint16 slot, uint8 counter) const
        {
            return quests::CounterIn(GetUInt32Value(quests::FieldOf(slot, QUEST_COUNT_STATE_OFFSET)), counter);
        }

        void SetQuestSlotState(uint16 slot, uint8 state)
        {
            SetByteFlag(quests::FieldOf(slot, QUEST_COUNT_STATE_OFFSET), quests::STATE_BYTE, state);
        }
        void RemoveQuestSlotState(uint16 slot, uint8 state)
        {
            RemoveByteFlag(quests::FieldOf(slot, QUEST_COUNT_STATE_OFFSET), quests::STATE_BYTE, state);
        }
        void SetQuestSlotTimer(uint16 slot, uint32 timer)
        {
            SetUInt32Value(quests::FieldOf(slot, QUEST_TIME_OFFSET), timer);
        }

        void SwapQuestSlot(uint16 slot1, uint16 slot2)
        {
            for (uint16 word = 0; word < MAX_QUEST_OFFSET; ++word)
            {
                uint32 const first = GetUInt32Value(quests::FieldOf(slot1, word));
                uint32 const second = GetUInt32Value(quests::FieldOf(slot2, word));

                SetUInt32Value(quests::FieldOf(slot1, word), second);
                SetUInt32Value(quests::FieldOf(slot2, word), first);
            }
        }

        void UpdateForQuestObjects();

        bool CanShareQuest(uint32 quest_id) const;

        void SendQuestReward(Quest const* pQuest, uint32 XP);

        void SendQuestFailed(uint32 quest_id);
        void SendQuestFailedAtTaker(uint32 quest_id, uint32 reason = INVALIDREASON_DONT_HAVE_REQ);
        void SendQuestTimerFailed(uint32 quest_id);

        void SendCanTakeQuestResponse(uint32 msg) const;

        void SendQuestConfirmAccept(Quest const* pQuest, Player* pReceiver);
        void SendPushToPartyResponse(Player* pPlayer, uint8 msg);

        ObjectGuid GetDividerGuid() const { return m_journal.Divider(); }

        void SetDividerGuid(ObjectGuid guid) { m_journal.Divider(guid); }

        void ClearDividerGuid() { m_journal.NoDivider(); }

        uint32 GetInGameTime()
        {
            return m_ingametime;
        }

        void SetInGameTime(uint32 time) { m_ingametime = time; }

        void AddTimedQuest(uint32 quest_id) { m_journal.StartTiming(quest_id); }

        void RemoveTimedQuest(uint32 quest_id) { m_journal.StopTiming(quest_id); }

        bool LoadFromDB(ObjectGuid guid, SqlQueryHolder* holder);

        void GetZoneAndAreaAboardOrHere(uint32& zone, uint32& area) const;

        void GetWorldAnchor(uint32& mapId, float& x, float& y, float& z) const;

        Map* BoardingMap() const;

        TerrainInfo const* AnchorTerrain() const;

        void UpdateLiftMinions();

        void SaveToDB();

        void SaveInventoryAndGoldToDB();

        void SaveGoldToDB();

        void SendPetTameFailure(PetTameFailureReason reason);

        void SetBindPoint(ObjectGuid guid);

        void SendTalentWipeConfirm(ObjectGuid guid);
        void RewardRage(uint32 damage, bool attacker);
        void SendPetSkillWipeConfirm();

        void RegenerateAll();

        void Regenerate(Powers power);

        void RegenerateHealth();

        void setRegenTimer(uint32 time)
        {
            m_recovery.NextIn(time);
        }

        uint32 GetMoney() const
        {
            return GetUInt32Value(PLAYER_FIELD_COINAGE);
        }

        void ModifyMoney(int32 d);

        void SetMoney(uint32 value)
        {
            SetUInt32Value(PLAYER_FIELD_COINAGE, value);
            m_journal.MoneyNowIs(value);
        }

        QuestStatusMap& getQuestStatusMap() { return m_journal.All(); }

        QuestJournal& Journal() { return m_journal; }
        QuestJournal const& Journal() const { return m_journal; }

        ObjectGuid GetSelectionGuid() const { return m_curSelectionGuid; }

        void SetSelectionGuid(ObjectGuid guid) { m_curSelectionGuid = guid; SetTargetGuid(guid); }

        uint8 GetComboPoints() const { return m_comboPoints; }

        ObjectGuid GetComboTargetGuid() const { return m_comboTargetGuid; }

        void AddComboPoints(Unit* target, int8 count);

        void ClearComboPoints();
        void SetComboPoints();

        void SendMailResult(uint32 mailId, MailResponseType mailAction, MailResponseResult mailError, uint32 equipError = 0, uint32 item_guid = 0, uint32 item_count = 0);

        void PetSpellInitialize();

        void CharmSpellInitialize();

        void PossessSpellInitialize();

        void RemovePetActionBar() { m_petMgr.RemoveActionBar(); }

        bool HasSpell(uint32 spell) const override;

        bool HasActiveSpell(uint32 spell) const;

        TrainerSpellState GetTrainerSpellState(TrainerSpell const* trainer_spell, uint32 reqLevel) const;

        bool IsSpellFitByClassAndRace(uint32 spell_id, uint32* pReqlevel = nullptr) const;

        bool IsNeedCastPassiveLikeSpellAtLearn(SpellEntry const* spellInfo) const;

        bool IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const override;

        void KnockBackFrom(Unit* target, float horizontalSpeed, float verticalSpeed);

        void SendProficiency(ItemClass itemClass, uint32 itemSubclassMask);

        void SendInitialSpells();

        bool addSpell(uint32 spell_id, bool active, bool learning, bool dependent, bool disabled);

        void learnSpell(uint32 spell_id, bool dependent);
        void removeSpell(uint32 spell_id, bool disabled = false, bool learn_low_rank = true);
        void resetSpells();

        void learnDefaultSpells();

        void learnQuestRewardedSpells();

        void learnQuestRewardedSpells(Quest const* quest);

        void learnSpellHighRank(uint32 spellid);

        uint32 GetFreeTalentPoints() const
        {
            return GetUInt32Value(PLAYER_CHARACTER_POINTS1);
        }

        void SetFreeTalentPoints(uint32 points);

        void UpdateFreeTalentPoints(bool resetIfNeed = true);

        bool resetTalents(bool no_cost = false);

        uint32 resetTalentsCost() const;

        void InitTalentForLevel();

        void LearnTalent(uint32 talentId, uint32 talentRank);

        uint32 CalculateTalentsPoints() const;

        uint32 GetFreePrimaryProfessionPoints() const
        {
            return GetUInt32Value(PLAYER_CHARACTER_POINTS2);
        }

        void SetFreePrimaryProfessions(uint16 profs)
        {
            SetUInt32Value(PLAYER_CHARACTER_POINTS2, profs);
        }

        void InitPrimaryProfessions();

        PlayerSpellMap const& GetSpellMap() const
        {
            return m_spells;
        }

        PlayerSpellMap& GetSpellMap()
        {
            return m_spells;
        }

        SpellCooldowns const& GetSpellCooldownMap() const { return m_spellCooldownMgr.GetSpellCooldownMap(); }

        static uint32 const infinityCooldownDelay = MONTH;
        static uint32 const infinityCooldownDelayCheck = MONTH / 2;

        bool HasSpellCooldown(uint32 spell_id) const { return m_spellCooldownMgr.HasSpellCooldown(spell_id); }

        time_t GetSpellCooldownDelay(uint32 spell_id) const { return m_spellCooldownMgr.GetSpellCooldownDelay(spell_id); }

        void AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo, uint32 itemId, Spell* spell = nullptr, bool infinityCooldown = false) { m_spellCooldownMgr.AddSpellAndCategoryCooldowns(spellInfo, itemId, spell, infinityCooldown); }

        void AddSpellCooldown(uint32 spell_id, uint32 itemid, time_t end_time) { m_spellCooldownMgr.AddSpellCooldown(spell_id, itemid, end_time); }

        void SendCooldownEvent(SpellEntry const* spellInfo, uint32 itemId = 0, Spell* spell = nullptr) { m_spellCooldownMgr.SendCooldownEvent(spellInfo, itemId, spell); }

        void ProhibitSpellSchool(SpellSchoolMask idSchoolMask, uint32 unTimeMs) override;

        void RemoveSpellCooldown(uint32 spell_id, bool update = false) { m_spellCooldownMgr.RemoveSpellCooldown(spell_id, update); }

        void RemoveSpellCategoryCooldown(uint32 cat, bool update = false) { m_spellCooldownMgr.RemoveSpellCategoryCooldown(cat, update); }

        void SendClearCooldown(uint32 spell_id, Unit* target);

        GlobalCooldownMgr& GetGlobalCooldownMgr()
        {
            return m_GlobalCooldownMgr;
        }

        void RemoveAllSpellCooldown() { m_spellCooldownMgr.RemoveAllSpellCooldown(); }

        void _LoadSpellCooldowns(QueryResult* result) { m_spellCooldownMgr.LoadFromDB(result); }

        void _SaveSpellCooldowns() { m_spellCooldownMgr.SaveToDB(); }

        void setResurrectRequestData(ObjectGuid guid, uint32 mapId, float X, float Y, float Z, uint32 health, uint32 mana)
        {
            m_resurrect.from = guid;
            m_resurrect.at = Geometry::Placement::Somewhere(mapId, Geometry::Vector3(X, Y, Z));
            m_resurrect.health = health;
            m_resurrect.mana = mana;
        }

        void clearResurrectRequestData() { m_resurrect.Withdraw(); }

        bool isRessurectRequestedBy(ObjectGuid guid) const { return m_resurrect.StandsFrom(guid); }

        bool isRessurectRequested() const { return m_resurrect.Stands(); }

        void ResurectUsingRequestData();

        void RaiseOnOffer();

        uint32 getCinematic()
        {
            return m_cinematic;
        }

        void setCinematic(uint32 cine)
        {
            m_cinematic = cine;
        }

        static bool IsActionButtonDataValid(uint8 button, uint32 action, uint8 type, Player* player);

        ActionButton* addActionButton(uint8 button, uint32 action, uint8 type);

        void removeActionButton(uint8 button);

        void SendInitialActionButtons() const;

        PvPInfo pvpInfo;

        void UpdatePvP(bool state, bool ovrride = false);

        bool IsFFAPvP() const
        {
            return HasPlayerFlag(PLAYER_FLAGS_FFA_PVP);
        }

        void SetFFAPvP(bool state);

        void UpdateZone(uint32 newZone, uint32 newArea, bool sendInitialWorldStates = true);

        void UpdateArea(uint32 newArea);

        uint32 GetCachedZoneId() const
        {
            return m_zoneUpdateId;
        }

        void UpdateZoneDependentAuras();
        void UpdateAreaDependentAuras();

        void UpdatePvPFlag(time_t currTime);

        void UpdateContestedPvP(uint32 currTime);

        void SetContestedPvPTimer(uint32 newTime)
        {
            m_contestedPvPTimer = newTime;
        }

        void ResetContestedPvP()
        {
            clearUnitState(UNIT_STAT_ATTACK_PLAYER);
            RemovePlayerFlag(PLAYER_FLAGS_CONTESTED_PVP);
            m_contestedPvPTimer = 0;
        }

        bool IsGroupVisibleFor(Player* p) const;

        bool IsInSameGroupWith(Player const* p) const;

        bool IsInSameRaidWith(Player const* p) const
        {
            return p == this || (GetGroup() != nullptr && GetGroup() == p->GetGroup());
        }

        void UninviteFromGroup();
        static void RemoveFromGroup(Group* group, ObjectGuid guid, uint8 removeMethod = GROUP_LEAVE);
        void RemoveFromGroup()
        {
            RemoveFromGroup(GetGroup(), GetObjectGuid());
        }

        void SendUpdateToOutOfRangeGroupMembers();

        void SetInGuild(uint32 GuildId)
        {
            SetUInt32Value(PLAYER_GUILDID, GuildId);
        }

        void SetRank(uint32 rankId)
        {
            SetUInt32Value(PLAYER_GUILDRANK, rankId);
        }

        uint32 GetGuildId()
        {
            return GetUInt32Value(PLAYER_GUILDID);
        }

        uint32 GetRank()
        {
            return GetUInt32Value(PLAYER_GUILDRANK);
        }

        static void RemovePetitionsAndSigns(ObjectGuid guid);

        bool UpdateSkill(uint32 skill_id, uint32 step);

        bool UpdateSkillPro(uint16 SkillId, int32 Chance, uint32 step);

        bool UpdateCraftSkill(uint32 spellid);

        bool UpdateGatherSkill(uint32 SkillId, uint32 SkillValue, uint32 RedLevel, uint32 Multiplicator = 1);

        bool UpdateFishingSkill();

        uint32 GetBaseDefenseSkillValue() const
        {
            return GetPureSkillValue(SKILL_DEFENSE);
        }

        uint32 GetBaseWeaponSkillValue(WeaponAttackType attType) const;

        PlayerSheet& Sheet() override { return m_sheet; }
        PlayerSheet const& Sheet() const override { return m_sheet; }

        Pace& Pacing() override { return m_pace; }
        Pace const& Pacing() const override { return m_pace; }

        float GetMeleeCritFromAgility();

        float GetDodgeFromAgility();

        float GetSpellCritFromIntellect();

        float OCTRegenHPPerSpirit();

        float OCTRegenMPPerSpirit();

        ObjectGuid GetLootGuid() const
        {
            return m_lootGuid;
        }

        void SetLootGuid(ObjectGuid guid)
        {
            m_lootGuid = guid;
        }

        void RemovedInsignia(Player* looterPlr);

        WorldSession* GetSession() const
        {
            return m_session;
        }

        void SetSession(WorldSession* s)
        {
            m_session = s;
        }

        void BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const override;

        void DestroyForPlayer(Player* target) const override;

        void SendLogXPGain(uint32 GivenXP, Unit* victim, uint32 RestXP);

        void SendAttackSwingCantAttack();
        void SendAttackSwingCancelAttack();
        void SendAttackSwingDeadTarget();
        void SendAttackSwingNotStanding();
        void SendAttackSwingNotInRange();
        void SendAttackSwingBadFacingAttack();
        void SendAutoRepeatCancel();
        void SendExplorationExperience(uint32 Area, uint32 Experience);

        void SendResetInstanceSuccess(uint32 MapId);

        void SendResetInstanceFailed(uint32 reason, uint32 MapId);

        void SendResetFailedNotify(uint32 mapid);

        bool SetPosition(float x, float y, float z, float orientation, bool teleport = false);

        Corpse* GetCorpse() const;

        void SpawnCorpseBones();

        Corpse* CreateCorpse();

        void KillPlayer();

        uint32 GetResurrectionSpellId();

        void ResurrectPlayer(float restore_percent, bool applySickness = false);

        void BuildPlayerRepop();

        void RepopAtGraveyard();

        void DurabilityLossAll(double percent, bool inventory);

        void DurabilityLoss(Item* item, double percent);

        void DurabilityPointsLossAll(int32 points, bool inventory);

        void DurabilityPointsLoss(Item* item, int32 points);

        void DurabilityPointLossForEquipSlot(EquipmentSlots slot);
        uint32 DurabilityRepairAll(bool cost, float discountMod);
        uint32 DurabilityRepair(uint16 pos, bool cost, float discountMod);

        void StopMirrorTimers()
        {
            m_perils.Stop(FATIGUE_TIMER);
            m_perils.Stop(BREATH_TIMER);
            m_perils.Stop(FIRE_TIMER);
        }

        void SetLevitate(bool enable) override;

        void SetCanFly(bool enable) override;

        void SetFeatherFall(bool enable) override;

        void SetHover(bool enable) override;

        void SetRoot(bool enable) override;

        void SetWaterWalk(bool enable) override;

        void JoinedChannel(Channel* c);

        void LeftChannel(Channel* c);

        void CleanupChannels();

        void UpdateLocalChannels(uint32 newZone);

        void LeaveLFGChannel();

        void UpdateDefense();

        void UpdateWeaponSkill(WeaponAttackType attType);

        void UpdateCombatSkills(Unit* pVictim, WeaponAttackType attType, bool defence);

        void SetSkill(uint16 id, uint16 currVal, uint16 maxVal, uint16 step = 0);

        uint16 GetMaxSkillValue(uint32 skill) const;

        uint16 GetPureMaxSkillValue(uint32 skill) const;

        uint16 GetSkillValue(uint32 skill) const;

        uint16 GetBaseSkillValue(uint32 skill) const;

        uint16 GetPureSkillValue(uint32 skill) const;

        int16 GetSkillPermBonusValue(uint32 skill) const;

        int16 GetSkillTempBonusValue(uint32 skill) const;

        bool HasSkill(uint32 skill) const;

        void learnSkillRewardedSpells(uint32 id, uint32 value);

        Geometry::Placement& GetTeleportDest() { return m_teleport.To(); }

        uint32 GetTeleportOptions() const { return m_teleport.Options(); }

        bool IsBeingTeleported() const { return m_teleport.InFlight(); }

        bool IsBeingTeleportedNear() const { return m_teleport.InFlightNear(); }

        bool IsBeingTeleportedFar() const { return m_teleport.InFlightFar(); }

        void SetSemaphoreTeleportNear(bool semphsetting) { m_teleport.FlyingNear(semphsetting); }

        void SetSemaphoreTeleportFar(bool semphsetting) { m_teleport.FlyingFar(semphsetting); }

        void ProcessDelayedOperations();

        void CheckAreaExploreAndOutdoor();

        static Team TeamForRace(uint8 race);

        Team GetTeam() const { return m_team; }

        PvpTeamIndex GetTeamId() const { return m_team == ALLIANCE ? TEAM_INDEX_ALLIANCE : TEAM_INDEX_HORDE; }

        static uint32 getFactionForRace(uint8 race);

        void setFactionForRace(uint8 race);

        void InitDisplayIds();

        bool IsAtGroupRewardDistance(Occupant const* pRewardSource) const;

        void RewardSinglePlayerAtKill(Unit* pVictim);

        void RewardPlayerAndGroupAtEvent(uint32 creature_id, Occupant* pRewardSource);

        void RewardPlayerAndGroupAtCast(Occupant* pRewardSource, uint32 spellid = 0);

        bool isHonorOrXPTarget(Unit* pVictim) const;

        ReputationMgr& GetReputationMgr()
        {
            return m_reputationMgr;
        }

        ReputationMgr const& GetReputationMgr() const { return m_reputationMgr; }

        ReputationRank GetReputationRank(uint32 faction_id) const;

        void RewardReputation(Unit* pVictim, float rate);

        void RewardReputation(Quest const* pQuest);

        int32 CalculateReputationGain(ReputationSource source, int32 rep, int32 faction, uint32 creatureOrQuestLevel = 0, bool noAuraBonus = false);

        void UpdateSkillsForLevel();

        void UpdateSkillsToMaxSkillsForLevel();

        void ModifySkillBonus(uint32 skillid, int32 val, bool talent);

        bool AddHonorCP(float honor, uint8 type, uint32 victim, uint8 victimType)
        {
            return m_honor.Add(honor, type, victim, victimType);
        }
        void UpdateHonor() { m_honor.Reckon(); }
        void ResetHonor() { m_honor.Wipe(); }
        void ClearHonorInfo() { m_honor.Forget(); }
        bool RewardHonor(Unit* pVictim, uint32 groupsize);

        HonorLedger& Honors() { return m_honor; }
        HonorLedger const& Honors() const { return m_honor; }

        uint32 CalculateTotalKills(Unit* Victim, uint32 fromDate, uint32 toDate) const
        {
            return m_honor.KillsOf(Victim, fromDate, toDate);
        }

        HonorRankInfo GetHonorRankInfo() const { return m_honor.Rank(); }
        void SetHonorRankInfo(HonorRankInfo rank) { m_honor.Rank(rank); }

        void SetRankPoints(float rankPoints) { m_honor.Points(rankPoints); }
        float GetRankPoints(void) const { return m_honor.Points(); }

        HonorRankInfo GetHonorHighestRankInfo() const { return m_honor.HighestRank(); }
        void SetHonorHighestRankInfo(HonorRankInfo hr) { m_honor.HighestRank(hr); }

        float GetStoredHonor() const { return m_honor.Stored(); }
        void SetStoredHonor(float rating) { m_honor.Stored(rating); }

        uint32 GetHonorStoredKills(bool honorable) const { return m_honor.Kills(honorable); }
        void SetHonorStoredKills(uint32 kills, bool honorable) { m_honor.Kills(kills, honorable); }

        int32 GetHonorLastWeekStandingPos() const { return m_honor.LastWeekPlace(); }
        void SetHonorLastWeekStandingPos(int32 standingPos) { m_honor.LastWeekPlace(standingPos); }
        void SendPvPCredit(ObjectGuid guid, uint32 rank, uint32 points);

        uint32 GetDeathTimer() const { return m_deathTimer; }

        uint32 GetCorpseReclaimDelay(bool pvp) const;

        void UpdateCorpseReclaimDelay();

        void SendCorpseReclaimDelay(bool load = false);

        void InitStatBuffMods()
        {
            for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
            {
                SetFloatValue(PLAYER_FIELD_POSSTAT0 + i, 0);
            }
            for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
            {
                SetFloatValue(PLAYER_FIELD_NEGSTAT0 + i, 0);
            }
        }
        void ApplyStatBuffMod(Stats stat, float val, bool apply) { ApplyModSignedFloatValue((val > 0 ? PLAYER_FIELD_POSSTAT0 + stat : PLAYER_FIELD_NEGSTAT0 + stat), val, apply); }
        void ApplyStatPercentBuffMod(Stats stat, float val, bool apply)
        {
            ApplyPercentModFloatValue(PLAYER_FIELD_POSSTAT0 + stat, val, apply);
            ApplyPercentModFloatValue(PLAYER_FIELD_NEGSTAT0 + stat, val, apply);
        }
        float GetPosStat(Stats stat) const { return GetFloatValue(PLAYER_FIELD_POSSTAT0 + stat); }
        float GetNegStat(Stats stat) const { return GetFloatValue(PLAYER_FIELD_NEGSTAT0 + stat); }
        float GetResistanceBuffMods(SpellSchools school, bool positive) const { return GetFloatValue(positive ? PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE + school : PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE + school); }
        void SetResistanceBuffMods(SpellSchools school, bool positive, float val) { SetFloatValue(positive ? PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE + school : PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE + school, val); }
        void ApplyResistanceBuffModsMod(SpellSchools school, bool positive, float val, bool apply) { ApplyModSignedFloatValue(positive ? PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE + school : PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE + school, val, apply); }
        void ApplyResistanceBuffModsPercentMod(SpellSchools school, bool positive, float val, bool apply) { ApplyPercentModFloatValue(positive ? PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE + school : PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE + school, val, apply); }

        void SetRegularAttackTime();

        void SetBaseModValue(BaseModGroup modGroup, BaseModType modType, float value) { m_auraBaseMod[modGroup][modType] = value; }

        void HandleBaseModValue(BaseModGroup modGroup, BaseModType modType, float amount, bool apply);

        float GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const;

        float GetTotalBaseModValue(BaseModGroup modGroup) const;

        float GetTotalPercentageModValue(BaseModGroup modGroup) const { return m_auraBaseMod[modGroup][FLAT_MOD] + m_auraBaseMod[modGroup][PCT_MOD]; }

        void _ApplyAllStatBonuses();

        void _RemoveAllStatBonuses();

        void _ApplyWeaponDependentAuraMods(Item* item, WeaponAttackType attackType, bool apply);

        void _ApplyWeaponDependentAuraCritMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply);

        void _ApplyWeaponDependentAuraDamageMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply);

        void _ApplyItemMods(Item* item, uint8 slot, bool apply);

        void _RemoveAllItemMods();

        void _ApplyAllItemMods();

        void _ApplyItemBonuses(ItemPrototype const* proto, uint8 slot, bool apply);

        void _ApplyAmmoBonuses();
        void InitDataForForm(bool reapplyMods = false);

        void ApplyItemEquipSpell(Item* item, bool apply, bool form_change = false);

        void ApplyEquipSpell(SpellEntry const* spellInfo, Item* item, bool apply, bool form_change = false);

        void UpdateEquipSpellsAtFormChange();

        void CastItemCombatSpell(Unit* Target, WeaponAttackType attType);
        void CastItemUseSpell(Item* item, SpellCastTargets const& targets);

        void SendInitWorldStates(uint32 zone);
        void SendInitWorldStates(uint32 mapId, uint32 zone);
        void SendUpdateWorldState(uint32 Field, uint32 Value);

        void SendDirectMessage(WorldPacket* data) const;

        void SendAuraDurationsForTarget(Unit* target);

        PlayerMenu* PlayerTalkClass;

        std::vector<ItemSetEffect*> ItemSetEff;

        void SendLoot(ObjectGuid guid, LootType loot_type);

        void SendLootRelease(ObjectGuid guid);

        void SendNotifyLootItemRemoved(uint8 lootSlot);

        void SendNotifyLootMoneyRemoved();

        bool InArena() const;

        static uint32 GetMinLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId);

        static uint32 GetMaxLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId);

        BattleGroundBracketId GetBattleGroundBracketIdFromLevel(BattleGroundTypeId bgTypeId) const;

        bool GetBGAccessByLevel(BattleGroundTypeId bgTypeId) const;

        bool CanUseBattleGroundObject();

        bool isTotalImmune();

        bool CanUseCapturePoint();

        void UpdateSpeakTime();

        bool CanSpeak() const;

        float m_rageDecayRate;
        float m_rageDecayMultiplier;

        bool HasMovementFlag(MovementFlags f) const;
        void UpdateFallInformationIfNeed(MovementInfo const& minfo, uint16 opcode);

        void SetFallInformation(uint32 time, float z)
        {
            m_lastFallTime = time;
            m_lastFallZ = z;
        }

        void HandleFall(MovementInfo const& movementInfo);

        void BuildTeleportAckMsg(WorldPacket& data, float x, float y, float z, float ang) const;

        bool isMoving() const { return m_movementInfo.HasMovementFlag(movementFlagsMask); }

        bool isMovingOrTurning() const { return m_movementInfo.HasMovementFlag(movementOrTurningFlagsMask); }

        bool CanSwim() const override { return true; }
        bool CanFly() const override { return false; }
        bool IsFlying() const { return false; }
        bool IsFreeFlying() const { return false; }

        bool IsClientControl(Unit* target) const;
        void SetClientControl(Unit* target, uint8 allowMove);

        void SetMover(Unit* target) { m_mover = target ? target : this; }

        Unit* GetMover() const { return m_mover; }

        bool IsSelfMover() const { return m_mover == this; }

        ObjectGuid GetFarSightGuid() const { return GetGuidValue(PLAYER_FARSIGHT); }

        Transport* GetTransport() const { return m_transport; }

        void SetTransport(Transport* t) { m_transport = t; }

        float GetTransOffsetX() const { return m_movementInfo.GetTransportPos()->x; }

        float GetTransOffsetY() const { return m_movementInfo.GetTransportPos()->y; }

        float GetTransOffsetZ() const { return m_movementInfo.GetTransportPos()->z; }

        float GetTransOffsetO() const { return m_movementInfo.GetTransportPos()->o; }

        uint32 GetTransTime() const { return m_movementInfo.GetTransportTime(); }

        uint32 GetSaveTimer() const { return m_nextSave; }

        void SetSaveTimer(uint32 timer) { m_nextSave = timer; }

        Geometry::Placement m_recall;

        void SaveRecallPosition();

        void SetHomebindToLocation(Geometry::Placement const& loc, uint32 area_id);

        void RelocateToHomebind()
        {
            SetLocationMapId(m_hearth.MapId());
            Place().MoveTo(m_hearth.X(), m_hearth.Y(), m_hearth.Z());
        }

        bool TeleportToHomebind(uint32 options = 0)
        {
            return TeleportTo(m_hearth.MapId(), m_hearth.X(), m_hearth.Y(), m_hearth.Z(), Where().Facing(), options);
        }

        Hearth& Home() { return m_hearth; }
        Hearth const& Home() const { return m_hearth; }

        Rest& Resting() { return m_rest; }
        Rest const& Resting() const { return m_rest; }

        Mailbox& Post() { return m_post; }
        Mailbox const& Post() const { return m_post; }

        PlayedTime& Played() { return m_played; }
        PlayedTime const& Played() const { return m_played; }

        Weaponry& Arms() { return m_arms; }
        Weaponry const& Arms() const { return m_arms; }

        SpellModifiers& SpellMods() { return m_spellMods; }
        SpellModifiers const& SpellMods() const { return m_spellMods; }

        Duel& Duelling() { return m_duel; }
        Duel const& Duelling() const { return m_duel; }

        QueueSlots& Queues() { return m_queues; }
        QueueSlots const& Queues() const { return m_queues; }

        BattleGroundStay& Battle() { return m_battle; }
        BattleGroundStay const& Battle() const { return m_battle; }

        DungeonBinds& Binds() { return m_binds; }
        DungeonBinds const& Binds() const { return m_binds; }

        void ScheduleDelayedOperation(uint32 operation) { m_teleport.OnArrival(operation); }

        time_t LoginTime() const { return m_played.LoggedInAt(); }

        Object* GetObjectByTypeMask(ObjectGuid guid, TypeMask typemask);

        GuidSet m_clientGUIDs;

        GuidSet m_clientPlatforms;

        bool HaveAtClient(Occupant const* u)
        {
            return u == this ||
                   m_clientGUIDs.find(u->GetObjectGuid()) != m_clientGUIDs.end() ||
                   m_clientPlatforms.find(u->GetObjectGuid()) != m_clientPlatforms.end();
        }

        void Remember(Occupant* target);

        void ForgetAtClient(ObjectGuid guid)
        {
            m_clientGUIDs.erase(guid);
            m_clientPlatforms.erase(guid);
        }

        bool IsVisibleInGridForPlayer(Player* pl) const override;

        bool IsVisibleGloballyFor(Player* pl) const;

        void UpdateVisibilityOf(Occupant const* viewPoint, Occupant* target);
        void UpdateVisibilityOf(Occupant const* viewPoint, Occupant* target, UpdateData& data, std::set<Occupant*>& visibleNow);

        void HandleStealthedUnitsDetection();

        Camera& GetCamera()
        {
            return m_camera;
        }

        CinematicFlyover* GetCinematicFlyover() { return m_cinematicFlyover.get(); }

        void SetCinematicFlyover(std::unique_ptr<CinematicFlyover> flyover) { m_cinematicFlyover = std::move(flyover); }

        void ScheduleLoginEffect();
        void BeginLoginCinematicRoot();
        void ReleaseLoginCinematicRoot();

        uint8 m_forced_speed_changes[MAX_MOVE_TYPE];

        bool HasAtLoginFlag(AtLoginFlags f) const { return m_atLoginFlags & f; }

        void SetAtLoginFlag(AtLoginFlags f) { m_atLoginFlags |= f; }

        void RemoveAtLoginFlag(AtLoginFlags f, bool in_db_also = false);

        uint32 GetStableSlots() const { return m_petMgr.GetStableSlots(); }
        void SetStableSlots(uint32 slots) { m_petMgr.SetStableSlots(slots); }
        uint32 GetTemporaryUnsummonedPetNumber() const { return m_petMgr.GetTemporaryUnsummonedPetNumber(); }
        void SetTemporaryUnsummonedPetNumber(uint32 petnumber) { m_petMgr.SetTemporaryUnsummonedPetNumber(petnumber); }
        void UnsummonPetTemporaryIfAny() { m_petMgr.UnsummonTemporaryIfAny(); }
        void ResummonPetTemporaryUnSummonedIfAny() { m_petMgr.ResummonTemporaryUnsummonedIfAny(); }
        bool IsPetNeedBeTemporaryUnsummoned() const { return !IsInWorld() || !IsAlive() || IsMounted() ; }

        void SendCinematicStart(uint32 CinematicSequenceId);

        void UpdateHomebindTime(uint32 time);

        static void ConvertInstancesToGroup(Player* player, Group* group = nullptr, ObjectGuid player_guid = 0);

        AreaLockStatus GetAreaTriggerLockStatus(AreaTrigger const* at, uint32& miscRequirement);
        void SendTransferAbortedByLockStatus(MapEntry const* mapEntry, AreaTrigger const* at, AreaLockStatus lockStatus, uint32 miscRequirement = 0);

        Group* GetGroup()
        {
            return m_group.getTarget();
        }

        const Group* GetGroup() const { return (const Group*)m_group.getTarget(); }

        Invitations& Invites() { return m_invitations; }
        Invitations const& Invites() const { return m_invitations; }

        GroupReference& GetGroupRef()
        {
            return m_group;
        }

        void SetGroup(Group* group, int8 subgroup = -1);

        uint8 GetSubGroup() const { return m_group.getSubGroup(); }

        uint32 GetGroupUpdateFlag() const { return m_groupUpdateMask; }

        void SetGroupUpdateFlag(uint32 flag) { m_groupUpdateMask |= flag; }

        const uint64& GetAuraUpdateMask() const { return m_auraUpdateMask; }

        void SetAuraUpdateMask(uint8 slot) { m_auraUpdateMask |= (uint64(1) << slot); }

        Player* GetNextRandomRaidMember(float radius);

        PartyResult CanUninviteFromGroup() const;

        void SetBattleGroundRaid(Group* group, int8 subgroup = -1);

        void RemoveFromBattleGroundRaid();

        Group* GetOriginalGroup()
        {
            return m_originalGroup.getTarget();
        }

        GroupReference& GetOriginalGroupRef()
        {
            return m_originalGroup;
        }

        uint8 GetOriginalSubGroup() const { return m_originalGroup.getSubGroup(); }

        void SetOriginalGroup(Group* group, int8 subgroup = -1);

        GridReference<Player>& GetGridRef()
        {
            return m_gridRef;
        }

        MapReference& GetMapRef()
        {
            return m_mapRef;
        }

        bool IsTappedByMeOrMyGroup(Creature* creature);

        bool isAllowedToLoot(Creature* creature);

        bool canSeeSpellClickOn(Creature const* creature) const;

        void SaveMail();
    protected:

        uint32 m_contestedPvPTimer;

        QueueSlots m_queues;

        BattleGroundStay m_battle;

        DungeonBinds m_binds;

        PlayerSheet m_sheet;
        Pace m_pace;

        QuestJournal m_journal;

        uint32 m_ingametime;

        void _LoadActions(QueryResult* result);

        void _LoadAuras(QueryResult* result, uint32 timediff);

        void _LoadHonorCP(QueryResult* result) { m_honor.LoadFromDB(result); }
        void _LoadInventory(QueryResult* result, uint32 timediff);

        void _LoadItemLoot(QueryResult* result);

        void _LoadMails(QueryResult* result);

        void _LoadMailedItems(QueryResult* result);

        void _LoadQuestStatus(QueryResult* result);
        void _LoadGroup(QueryResult* result);

        void _LoadSkills(QueryResult* result);

        void _LoadSpells(QueryResult* result);

        bool _LoadHomeBind(QueryResult* result);
        void _LoadBGData(QueryResult* result);

        void _SaveActions();

        void _SaveAuras();

        void _SaveInventory();
        void _SaveHonorCP() { m_honor.SaveToDB(); }

        void _SaveQuestStatus();
        void _SaveSkills();

        void _SaveSpells();

        void _SaveBGData();

        void _SaveStats();

        HonorLedger m_honor;

        void outDebugStatsValues() const;

        ObjectGuid m_lootGuid = 0;

        Team m_team;
        uint32 m_nextSave;
        time_t m_speakTime;
        uint32 m_speakCount;

        uint32 m_atLoginFlags;

        Inventory m_inventory;

        uint32 m_ExtraFlags;
        ObjectGuid m_curSelectionGuid = 0;

        ObjectGuid m_comboTargetGuid = 0;
        int8 m_comboPoints;

        SkillStatusMap mSkillStatus;

        PlayerSpellMap m_spells;

        GlobalCooldownMgr m_GlobalCooldownMgr;

        float m_auraBaseMod[BASEMOD_END][MOD_END];
        ActionButtonList m_actionButtons;

        ResurrectOffer m_resurrect;

        WorldSession* m_session;

        typedef std::list<Channel*> JoinedChannelsList;
        JoinedChannelsList m_channels;

        uint32 m_cinematic;

        TradeData* m_trade;

        uint32 m_zoneUpdateId;
        uint32 m_zoneUpdateTimer;
        uint32 m_areaUpdateId;
        uint32 m_positionStatusUpdateTimer;

        uint32 m_deathTimer;
        time_t m_deathExpireTime;

        Transport* m_transport;

        uint32 m_resetTalentsCost;
        time_t m_resetTalentsTime;
        uint32 m_usedTalentCount;

        PlayerSocial* m_social;

        GroupReference m_group;

        Invitations m_invitations;
        GroupReference m_originalGroup;
        uint32 m_groupUpdateMask;
        uint64 m_auraUpdateMask;

        ObjectGuid m_miniPetGuid = 0;

        SummonOffer m_summon;

    private:
        uint32 m_created_date = 0;

        void UpdateKnownCurrencies(uint32 itemId, bool apply);

        void SetCanDelayTeleport(bool setting) { m_teleport.MayWait(setting); }

        bool IsHasDelayedTeleport() const { return m_teleport.Waits(IsAlive()); }

        bool SetDelayedTeleportFlagIfCan() { return m_teleport.WaitIfItMay(IsAlive()); }

        Unit* m_mover;

        Camera m_camera;

        std::unique_ptr<CinematicFlyover> m_cinematicFlyover;

        LoginCinematicRootOwnership m_loginCinematicRootOwnership;

        uint32 m_visibilityObserverSweepTimer;

        GridReference<Player> m_gridRef;

        MapReference m_mapRef;

        uint32 m_lastFallTime;
        float  m_lastFallZ;

        TeleportOrder m_teleport;

        Perils m_perils;

        Drink m_drink;

        Hearth m_hearth;

        Rest m_rest;

        Mailbox m_post;

        PlayedTime m_played;

        Weaponry m_arms;

        SpellModifiers m_spellMods;

        Duel m_duel;

        uint32 m_DetectInvTimer;

        ReputationMgr  m_reputationMgr;

        SpellCooldownMgr m_spellCooldownMgr;

        PetMgr m_petMgr;
};

void AddItemsSetItem(Player* player, Item* item);

void RemoveItemsSetItem(Player* player, ItemPrototype const* proto);

template <class T>
T SpellModifiers::Apply(uint32 spellId, SpellModOp op, T& base, Spell const* spell)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        return 0;
    }

    int32 byHundred = 0;
    int32 flat = 0;

    for (auto* mod : m_byNumber[op])
    {
        if (!Affects(spellInfo, mod, spell))
        {
            continue;
        }

        if (mod->type == SPELLMOD_FLAT)
        {
            flat += mod->value;
        }
        else if (mod->type == SPELLMOD_PCT)
        {

            if (base == T(0))
            {
                continue;
            }

            if (mod->op == SPELLMOD_CASTING_TIME && base >= T(10 * IN_MILLISECONDS) && mod->value <= -100)
            {
                continue;
            }

            byHundred += mod->value;
        }

        if (mod->charges > 0)
        {
            if (!spell)
            {
                spell = m_owner.FindCurrentSpellBySpellId(spellId);
            }

            if (!mod->lastAffected || mod->lastAffected != spell)
            {
                --mod->charges;

                if (mod->charges == 0)
                {
                    mod->charges = -1;
                    ++m_awaitingRemoval;
                }

                mod->lastAffected = spell;
            }
        }
    }

    float const difference = float(base) * float(byHundred) / 100.0f + float(flat);
    base = T(float(base) + difference);
    return T(difference);
}
