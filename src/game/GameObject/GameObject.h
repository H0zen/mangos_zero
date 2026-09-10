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
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include <ctime>
#include <string>
#include <vector>
#include "SharedDefines.h"
#include "Occupant.h"
#include "CapturePoint.h"
#include "SpawnClock.h"
#include "LiftPath.h"
#include "UserTally.h"
#include "Chest.h"
#include "TrapSight.h"
#include "Behaviour.h"
#include "LootClaim.h"
#include "LootMgr.h"
#include "Utilities/EventProcessor.h"
#include <memory>

#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

class Map;
class GameObjectAI;

struct GameObjectInfo
{
    uint32  id;
    uint32  type;
    uint32  displayId;
    char*   name;
    uint32  faction;
    uint32  flags;
    float   size;
    union
    {

        struct
        {
            uint32 startOpen;
            uint32 lockId;
            uint32 autoCloseTime;
            uint32 noDamageImmune;
            uint32 openTextID;
            uint32 closeTextID;
        } door;

        struct
        {
            uint32 startOpen;
            uint32 lockId;
            uint32 autoCloseTime;
            uint32 linkedTrapId;
            uint32 noDamageImmune;
            uint32 large;
            uint32 openTextID;
            uint32 closeTextID;
            uint32 losOK;
        } button;

        struct
        {
            uint32 lockId;
            uint32 questList;
            uint32 pageMaterial;
            uint32 gossipID;
            uint32 customAnim;
            uint32 noDamageImmune;
            uint32 openTextID;
            uint32 losOK;
            uint32 allowMounted;
            uint32 large;
        } questgiver;

        struct
        {
            uint32 lockId;
            uint32 lootId;
            uint32 chestRestockTime;
            uint32 consumable;
            uint32 minSuccessOpens;
            uint32 maxSuccessOpens;
            uint32 eventId;
            uint32 linkedTrapId;
            uint32 questId;
            uint32 level;
            uint32 losOK;
            uint32 leaveLoot;
            uint32 notInCombat;
            uint32 logLoot;
            uint32 openTextID;
            uint32 groupLootRules;
        } chest;

        struct
        {
            uint32 floatingTooltip;
            uint32 highlight;
            uint32 serverOnly;
            uint32 large;
            uint32 floatOnWater;
            uint32 questID;
        } _generic;

        struct
        {
            uint32 lockId;
            uint32 level;
            uint32 radius;
            uint32 spellId;
            uint32 charges;
            uint32 cooldown;
            uint32 autoCloseTime;
            uint32 startDelay;
            uint32 serverOnly;
            uint32 stealthed;
            uint32 large;
            uint32 stealthAffected;
            uint32 openTextID;
            uint32 closeTextID;
        } trap;

        struct
        {
            uint32 slots;
            uint32 height;
            uint32 onlyCreatorUse;
        } chair;

        struct
        {
            uint32 focusId;
            uint32 dist;
            uint32 linkedTrapId;
            uint32 serverOnly;
            uint32 questID;
            uint32 large;
        } spellFocus;

        struct
        {
            uint32 pageID;
            uint32 language;
            uint32 pageMaterial;
            uint32 allowMounted;
        } text;

        struct
        {
            uint32 lockId;
            uint32 questId;
            uint32 eventId;
            uint32 autoCloseTime;
            uint32 customAnim;
            uint32 consumable;
            uint32 cooldown;
            uint32 pageId;
            uint32 language;
            uint32 pageMaterial;
            uint32 spellId;
            uint32 noDamageImmune;
            uint32 linkedTrapId;
            uint32 large;
            uint32 openTextID;
            uint32 closeTextID;
            uint32 losOK;
            uint32 allowMounted;
            uint32 floatingTooltip;
            uint32 gossipID;
        } goober;

        struct
        {
            uint32 pause;
            uint32 startOpen;
            uint32 autoCloseTime;
        } transport;

        struct
        {
            uint32 lockId;
            uint32 radius;
            uint32 damageMin;
            uint32 damageMax;
            uint32 damageSchool;
            uint32 autoCloseTime;
            uint32 openTextID;
            uint32 closeTextID;
        } areadamage;

        struct
        {
            uint32 lockId;
            uint32 cinematicId;
            uint32 eventID;
            uint32 openTextID;
        } camera;

        struct
        {
            uint32 taxiPathId;
            uint32 moveSpeed;
            uint32 accelRate;
            uint32 startEventID;
            uint32 stopEventID;
            uint32 transportPhysics;
            uint32 mapID;
        } moTransport;

        struct
        {
            uint32 _data0;
            uint32 lootId;
        } fishnode;

        struct
        {
            uint32 reqParticipants;
            uint32 spellId;
            uint32 animSpell;
            uint32 ritualPersistent;
            uint32 casterTargetSpell;
            uint32 casterTargetSpellTargets;
            uint32 castersGrouped;
            uint32 ritualNoTargetCheck;
        } summoningRitual;

        struct
        {
            uint32 actionHouseID;
        } auctionhouse;

        struct
        {
            uint32 creatureID;
            uint32 charges;
        } guardpost;

        struct
        {
            uint32 spellId;
            uint32 charges;
            uint32 partyOnly;
        } spellcaster;

        struct
        {
            uint32 minLevel;
            uint32 maxLevel;
            uint32 areaID;
        } meetingstone;

        struct
        {
            uint32 lockId;
            uint32 pickupSpell;
            uint32 radius;
            uint32 returnAura;
            uint32 returnSpell;
            uint32 noDamageImmune;
            uint32 openTextID;
            uint32 losOK;
        } flagstand;

        struct
        {
            uint32 radius;
            uint32 lootId;
            uint32 minSuccessOpens;
            uint32 maxSuccessOpens;
            uint32 lockId;
        } fishinghole;

        struct
        {
            uint32 lockId;
            uint32 eventID;
            uint32 pickupSpell;
            uint32 noDamageImmune;
            uint32 openTextID;
        } flagdrop;

        struct
        {
            uint32 gameType;
        } miniGame;

        struct
        {
            uint32 radius;
            uint32 spell;
            uint32 worldState1;
            uint32 worldState2;
            uint32 winEventID1;
            uint32 winEventID2;
            uint32 contestedEventID1;
            uint32 contestedEventID2;
            uint32 progressEventID1;
            uint32 progressEventID2;
            uint32 neutralEventID1;
            uint32 neutralEventID2;
            uint32 neutralPercent;
            uint32 worldState3;
            uint32 minSuperiority;
            uint32 maxSuperiority;
            uint32 minTime;
            uint32 maxTime;
            uint32 large;
            uint32 highlight;
        } capturePoint;

        struct
        {
            uint32 startOpen;
            uint32 radius;
            uint32 auraID1;
            uint32 conditionID1;
            uint32 auraID2;
            uint32 conditionID2;
            uint32 serverOnly;
        } auraGenerator;

        struct
        {
            uint32 data[24];
        } raw;
    };

    uint32 MinMoneyLoot;
    uint32 MaxMoneyLoot;

    bool IsDespawnAtAction() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_CHEST:  return chest.consumable;
            case GAMEOBJECT_TYPE_GOOBER: return goober.consumable;
            default: return false;
        }
    }

    uint32 GetLockId() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_DOOR:       return door.lockId;
            case GAMEOBJECT_TYPE_BUTTON:     return button.lockId;
            case GAMEOBJECT_TYPE_QUESTGIVER: return questgiver.lockId;
            case GAMEOBJECT_TYPE_CHEST:      return chest.lockId;
            case GAMEOBJECT_TYPE_TRAP:       return trap.lockId;
            case GAMEOBJECT_TYPE_GOOBER:     return goober.lockId;
            case GAMEOBJECT_TYPE_AREADAMAGE: return areadamage.lockId;
            case GAMEOBJECT_TYPE_CAMERA:     return camera.lockId;
            case GAMEOBJECT_TYPE_FLAGSTAND:  return flagstand.lockId;
            case GAMEOBJECT_TYPE_FISHINGHOLE: return fishinghole.lockId;
            case GAMEOBJECT_TYPE_FLAGDROP:   return flagdrop.lockId;
            default: return 0;
        }
    }

    bool GetDespawnPossibility() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_DOOR:       return door.noDamageImmune;
            case GAMEOBJECT_TYPE_BUTTON:     return button.noDamageImmune;
            case GAMEOBJECT_TYPE_QUESTGIVER: return questgiver.noDamageImmune;
            case GAMEOBJECT_TYPE_GOOBER:     return goober.noDamageImmune;
            case GAMEOBJECT_TYPE_FLAGSTAND:  return flagstand.noDamageImmune;
            case GAMEOBJECT_TYPE_FLAGDROP:   return flagdrop.noDamageImmune;
            default: return true;
        }
    }

    uint32 GetCharges() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_TRAP:        return trap.charges;
            case GAMEOBJECT_TYPE_GUARDPOST:   return guardpost.charges;
            case GAMEOBJECT_TYPE_SPELLCASTER: return spellcaster.charges;
            default: return 0;
        }
    }

    uint32 GetCooldown() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_TRAP:        return trap.cooldown;
            case GAMEOBJECT_TYPE_GOOBER:      return goober.cooldown;
            default: return 0;
        }
    }

    uint32 GetLinkedGameObjectEntry() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_BUTTON:      return button.linkedTrapId;
            case GAMEOBJECT_TYPE_CHEST:       return chest.linkedTrapId;
            case GAMEOBJECT_TYPE_SPELL_FOCUS: return spellFocus.linkedTrapId;
            case GAMEOBJECT_TYPE_GOOBER:      return goober.linkedTrapId;
            default: return 0;
        }
    }

    uint32 GetAutoCloseTime() const
    {
        uint32 autoCloseTime = 0;
        switch (type)
        {
            case GAMEOBJECT_TYPE_DOOR:          autoCloseTime = door.autoCloseTime; break;
            case GAMEOBJECT_TYPE_BUTTON:        autoCloseTime = button.autoCloseTime; break;
            case GAMEOBJECT_TYPE_TRAP:          autoCloseTime = trap.autoCloseTime; break;
            case GAMEOBJECT_TYPE_GOOBER:        autoCloseTime = goober.autoCloseTime; break;
            case GAMEOBJECT_TYPE_TRANSPORT:     autoCloseTime = transport.autoCloseTime; break;
            case GAMEOBJECT_TYPE_AREADAMAGE:    autoCloseTime = areadamage.autoCloseTime; break;
            default: break;
        }
        return autoCloseTime / 0x10000;
    }

    uint32 GetLootId() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_CHEST:       return chest.lootId;
            case GAMEOBJECT_TYPE_FISHINGHOLE: return fishinghole.lootId;
            default: return 0;
        }
    }

    bool IsServerOnly() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_GENERIC:     return _generic.serverOnly != 0;
            case GAMEOBJECT_TYPE_TRAP:        return trap.serverOnly != 0;
            case GAMEOBJECT_TYPE_SPELL_FOCUS: return spellFocus.serverOnly != 0;
            default: return false;
        }
    }

    uint32 GetQuestId() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_CHEST:       return chest.questId;
            case GAMEOBJECT_TYPE_GENERIC:     return _generic.questID;
            case GAMEOBJECT_TYPE_SPELL_FOCUS: return spellFocus.questID;
            case GAMEOBJECT_TYPE_GOOBER:      return goober.questId;
            default: return 0;
        }
    }

    uint32 GetGossipMenuId() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_QUESTGIVER:    return questgiver.gossipID;
            case GAMEOBJECT_TYPE_GOOBER:        return goober.gossipID;
            default: return 0;
        }
    }
};

#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

struct GameObjectLocale
{
    std::vector<std::string> Name;
};

enum GOState
{
    GO_STATE_ACTIVE             = 0x00,
    GO_STATE_READY              = 0x01,
    GO_STATE_ACTIVE_ALTERNATIVE = 0x02,
};

#define MAX_GO_STATE              3

struct GameObjectData
{
    uint32 id;
    uint32 mapid;
    float posX;
    float posY;
    float posZ;
    float orientation;
    float rotation0;
    float rotation1;
    float rotation2;
    float rotation3;
    int32  spawntimesecs;
    uint32 animprogress;
    GOState go_state;
};

enum LootState
{
    GO_NOT_READY = 0,
    GO_READY,
    GO_ACTIVATED,
    GO_JUST_DEACTIVATED
};

class Unit;
class GameObjectModel;

namespace Geometry
{
    class Quat;
}

struct GameObjectDisplayInfoEntry;

#define FISHING_BOBBER_READY_TIME 5

#define GO_ANIMPROGRESS_DEFAULT 100

class GameObject : public Occupant, public Spoilable
{

    public:
        explicit GameObject();
        ~GameObject();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        void CleanupsBeforeDelete() override;

        bool Create(uint32 guidlow, uint32 name_id, Map* map, float x, float y, float z, float ang,
            float rotation0 = 0.0f, float rotation1 = 0.0f, float rotation2 = 0.0f, float rotation3 = 0.0f, uint32 animprogress = GO_ANIMPROGRESS_DEFAULT, GOState go_state = GO_STATE_READY);
        void Update(uint32 update_diff, uint32 p_time) override;

        GameObjectInfo const* GetGOInfo() const { return m_goInfo; }
        void SetGOInfo(GameObjectInfo const* pg);

        GameObjectBehaviour& Behaves() const { return *m_behaviour; }

        template <typename Kind>
        Kind* Behaves() const { return dynamic_cast<Kind*>(m_behaviour.get()); }

        bool IsLift() const { return GetGoType() == GAMEOBJECT_TYPE_TRANSPORT; }

        uint32 LiftPhase() const;

        bool IsMovingPlatform() const;

        bool HasStaticDBSpawnData() const;

        void GetQuaternion(Geometry::Quat& q) const;
        void SetQuaternion(Geometry::Quat const& q);

        void SetDisplayId(uint32 model_id);

        const char* GetNameForLocaleIdx(int32 locale_idx) const override;

        void SaveToDB();
        void SaveToDB(uint32 mapid);
        bool LoadFromDB(uint32 guid, Map* map);
        virtual void DeleteFromDB();

        void SetOwnerGuid(ObjectGuid ownerGuid)
        {
            m_spawn.Permanent(false);
            SetGuidValue(OBJECT_FIELD_CREATED_BY, ownerGuid);
        }
        ObjectGuid GetOwnerGuid() const { return GetGuidValue(OBJECT_FIELD_CREATED_BY); }
        Unit* GetOwner() const;

        bool IsControlledByPlayer() const override
        {
            return (GetOwnerGuid() != 0 && GuidHigh(GetOwnerGuid()) == HIGHGUID_PLAYER);
        }

        void SetSpellId(uint32 id)
        {
            m_spawn.Permanent(false);
            m_spellId = id;
        }
        uint32 GetSpellId() const { return m_spellId;}

        SpawnClock& Clock() { return m_spawn; }
        SpawnClock const& Clock() const { return m_spawn; }

        time_t GetRespawnTime() const { return m_spawn.Moment(); }
        time_t GetRespawnTimeEx() const { return m_spawn.NextUp(time(nullptr)); }

        void SetRespawnTime(time_t respawn) { m_spawn.In(respawn > 0 ? uint32(respawn) : 0); }
        void Respawn();
        bool isSpawned() const { return m_spawn.IsUp(); }
        bool isSpawnedByDefault() const { return m_spawn.IsPermanent(); }
        void SetSpawnedByDefault(bool b) { m_spawn.Permanent(b); }
        uint32 GetRespawnDelay() const { return m_spawn.Delay(); }
        void Refresh();
        void Delete();

        static void AddToRemoveListInMaps(uint32 db_guid, GameObjectData const* data);
        static void SpawnInMaps(uint32 db_guid, GameObjectData const* data);

        GameobjectTypes GetGoType() const { return GameobjectTypes(GetUInt32Value(GAMEOBJECT_TYPE_ID)); }
        void SetGoType(GameobjectTypes type) { SetUInt32Value(GAMEOBJECT_TYPE_ID, type); }
        GOState GetGoState() const { return GOState(GetUInt32Value(GAMEOBJECT_STATE)); }

        float GetGoPositionX() const { return GetFloatValue(GAMEOBJECT_POS_X); }
        float GetGoPositionY() const { return GetFloatValue(GAMEOBJECT_POS_Y); }
        float GetGoPositionZ() const { return GetFloatValue(GAMEOBJECT_POS_Z); }
        float GetGoFacing() const { return GetFloatValue(GAMEOBJECT_FACING); }
        void SetGoPosition(float x, float y, float z)
        {
            SetFloatValue(GAMEOBJECT_POS_X, x);
            SetFloatValue(GAMEOBJECT_POS_Y, y);
            SetFloatValue(GAMEOBJECT_POS_Z, z);
        }

        void SetGoFacing(float facing) { SetFloatValue(GAMEOBJECT_FACING, facing); }

        bool HasGoFlag(uint32 flag) const { return HasFlag(GAMEOBJECT_FLAGS, flag); }
        void SetGoFlag(uint32 flag) { SetFlag(GAMEOBJECT_FLAGS, flag); }
        void RemoveGoFlag(uint32 flag) { RemoveFlag(GAMEOBJECT_FLAGS, flag); }
        void ApplyGoFlag(uint32 flag, bool apply) { ApplyModFlag(GAMEOBJECT_FLAGS, flag, apply); }
        uint32 GetGoFlags() const { return GetUInt32Value(GAMEOBJECT_FLAGS); }
        void SetAllGoFlags(uint32 flags) { SetUInt32Value(GAMEOBJECT_FLAGS, flags); }
        void SetGoState(GOState state);
        uint32 GetGoArtKit() const { return GetUInt32Value(GAMEOBJECT_ARTKIT); }
        void SetGoArtKit(uint32 artkit) { SetUInt32Value(GAMEOBJECT_ARTKIT, artkit); }
        uint32 GetGoAnimProgress() const { return GetUInt32Value(GAMEOBJECT_ANIMPROGRESS); }
        void SetGoAnimProgress(uint32 animprogress) { SetUInt32Value(GAMEOBJECT_ANIMPROGRESS, animprogress); }
        uint32 GetDisplayId() const { return GetUInt32Value(GAMEOBJECT_DISPLAYID); }
        void SetDisplayIdx(uint32 modelId);

        void SendGameObjectCustomAnim(uint32 animId = 0);
        void SendGameObjectReset();

        float ComputeBoundingRadius() const override;

        void Use(Unit* user);

    private:

        bool HasQuestBusinessWith(Player* seeker) const;
        bool HoldsQuestLootFor(Player* seeker) const;

        bool IsTrapHidingFrom(Player const* watcher) const;
        TrapWatcher WatchedBy(Player const* watcher) const;

    public:

        void RollIfMineralVein();

        LootState getLootState() const { return m_lootState; }

        time_t UsableAt() const { return m_usableAt; }
        void UsableAt(time_t when) { m_usableAt = when; }

        time_t ClosesAt() const { return m_closesAt; }
        void ClosesAt(time_t when) { m_closesAt = when; }
        void SetLootState(LootState s);

        void ClearAllUsesData();

        void SaveRespawnTime();

        Loot loot;

        Loot* Spoils() override { return &loot; }

        bool OpenableBy(Player const& who) const override;
        bool FillSpoilsFor(Player& who, LootType& how, PermissionTypes& permission) override;

        LootClaim& Claim() { return m_claim; }
        LootClaim const& Claim() const { return m_claim; }

        bool OffersQuest(uint32 quest_id) const;
        bool TakesQuest(uint32 quest_id) const;

        bool ActivateToQuest(Player* seeker) const;
        void UseDoorOrButton(uint32 time_to_restore = 0, bool alternative = false);

        void ResetDoorOrButton();

        void SummonLinkedTrapIfAny();
        void TriggerLinkedGameObject(Unit* target);

        bool IsVisibleForInState(Player const* u, Occupant const* viewPoint, bool inVisibleList) const override;

        bool IsCollisionEnabled() const;

        GameObject* LookupFishingHoleAround(float range);

        float GetInteractionDistance() const;

        uint32 GetScriptId();

        bool AIM_Initialize();

        GameObjectAI* AI() const { return m_AI.get(); }

        GridReference<GameObject>& GetGridRef()
        {
            return m_gridRef;
        }

        GameObjectModel* m_model;

    protected:
        uint32      m_spellId;
        SpawnClock  m_spawn;
        LootState   m_lootState;
        time_t      m_usableAt;
        time_t      m_closesAt;

        GameObjectInfo const* m_goInfo;

        LootClaim m_claim;

        std::unique_ptr<GameObjectBehaviour> m_behaviour;

        bool m_AI_locked;

        std::unique_ptr<GameObjectAI> m_AI;

    private:
        void SwitchDoorOrButton(bool activate, bool alternative = false);
        void UpdateModel();
        void UpdateCollisionState() const;

        GridReference<GameObject> m_gridRef;
};
