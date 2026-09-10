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
#include "Utilities/PackedValues.h"
#include <ctime>
#include <set>
#include <mutex>
#include <vector>
#include "Platform/Define.h"
#include "Policies/Singleton.h"
#include <list>
#include <map>
#include "Database/DatabaseEnv.h"
#include "DBCEnums.h"
#include "DBCStores.h"
#include "ObjectGuid.h"
#include "PoolManager.h"

struct InstanceTemplate;
struct MapEntry;
struct GameObjectData;
struct CreatureData;

class Player;
class Group;
class Map;

typedef std::set<uint32> CellGuidSet;

struct MapCellObjectGuids
{
    CellGuidSet creatures;
    CellGuidSet gameobjects;
};

typedef std::unordered_map < uint32, MapCellObjectGuids > MapCellObjectGuidsMap;

class MapPersistentStateManager;

class MapPersistentState
{
    friend class MapPersistentStateManager;

    protected:

        MapPersistentState(uint32 MapId, uint32 InstanceId);

    public:

        virtual ~MapPersistentState();

        uint32 GetInstanceId() const { return m_instanceid; }
        uint32 GetMapId() const { return m_mapid; }

        MapEntry const* GetMapEntry() const;

        bool IsUsedByMap() const { return m_usedByMap; }
        Map* GetMap() const { return m_usedByMap; }
        void SetUsedByMapState(Map* map)
        {
            m_usedByMap = map;
            if (!map)
            {
                UnloadIfEmpty();
            }
        }

        time_t GetCreatureRespawnTime(uint32 loguid) const
        {
            RespawnTimes::const_iterator itr = m_creatureRespawnTimes.find(loguid);
            return itr != m_creatureRespawnTimes.end() ? itr->second : 0;
        }
        void SaveCreatureRespawnTime(uint32 loguid, time_t t);
        time_t GetGORespawnTime(uint32 loguid) const
        {
            RespawnTimes::const_iterator itr = m_goRespawnTimes.find(loguid);
            return itr != m_goRespawnTimes.end() ? itr->second : 0;
        }
        void SaveGORespawnTime(uint32 loguid, time_t t);

        void InitPools();
        virtual SpawnedPoolData& GetSpawnedPoolData() = 0;

        template<typename T>
            bool IsSpawnedPoolObject(uint32 db_guid_or_pool_id) { return GetSpawnedPoolData().IsSpawnedObject<T>(db_guid_or_pool_id); }

        MapCellObjectGuids const& GetCellObjectGuids(uint32 cell_id) { return m_gridObjectGuids[cell_id]; }
        void AddCreatureToGrid(uint32 guid, CreatureData const* data);
        void RemoveCreatureFromGrid(uint32 guid, CreatureData const* data);
        void AddGameobjectToGrid(uint32 guid, GameObjectData const* data);
        void RemoveGameobjectFromGrid(uint32 guid, GameObjectData const* data);
    protected:
        virtual bool CanBeUnload() const = 0;

        bool UnloadIfEmpty();
        void ClearRespawnTimes();
        bool HasRespawnTimes() const { return !m_creatureRespawnTimes.empty() || !m_goRespawnTimes.empty(); }

    private:
        void SetCreatureRespawnTime(uint32 loguid, time_t t);
        void SetGORespawnTime(uint32 loguid, time_t t);

    private:
        typedef std::unordered_map<uint32, time_t> RespawnTimes;

        uint32 m_instanceid;
        uint32 m_mapid;
        Map* m_usedByMap;

        RespawnTimes m_creatureRespawnTimes;
        RespawnTimes m_goRespawnTimes;
        MapCellObjectGuidsMap m_gridObjectGuids;
};

inline bool MapPersistentState::CanBeUnload() const
{

    return !m_usedByMap;
}

class WorldPersistentState : public MapPersistentState
{
    public:

        explicit WorldPersistentState(uint32 MapId) : MapPersistentState(MapId, 0) {}

        ~WorldPersistentState() {}

        SpawnedPoolData& GetSpawnedPoolData() override { return m_sharedSpawnedPoolData; }
    protected:
        bool CanBeUnload() const override;

    private:
        static SpawnedPoolData m_sharedSpawnedPoolData;
};

class DungeonPersistentState : public MapPersistentState
{
    public:

        DungeonPersistentState(uint32 MapId, uint32 InstanceId, time_t resetTime, bool canReset);

        ~DungeonPersistentState();

        SpawnedPoolData& GetSpawnedPoolData() override { return m_spawnedPoolData; }

        InstanceTemplate const* GetTemplate() const;

        uint8 GetPlayerCount() const { return (uint8)m_playerList.size(); }
        uint8 GetGroupCount() const { return (uint8)m_groupList.size(); }

        void AddPlayer(Player* player) { m_playerList.push_back(player); }
        bool RemovePlayer(Player* player) { m_playerList.remove(player); return UnloadIfEmpty(); }

        void AddGroup(Group* group) { m_groupList.push_back(group); }
        bool RemoveGroup(Group* group) { m_groupList.remove(group); return UnloadIfEmpty(); }

        time_t GetResetTime() const { return m_resetTime; }
        void SetResetTime(time_t resetTime) { m_resetTime = resetTime; }
        time_t GetResetTimeForDB() const;

        bool CanReset() const { return m_canReset; }
        void SetCanReset(bool canReset) { m_canReset = canReset; }

        void SaveToDB();

        void DeleteFromDB();

        void DeleteRespawnTimes();

        void UnbindThisState();

    protected:
        bool CanBeUnload() const override;
        bool HasBounds() const { return !m_playerList.empty() || !m_groupList.empty(); }

    private:
        typedef std::list<Player*> PlayerListType;
        typedef std::list<Group*> GroupListType;

        time_t m_resetTime;
        bool m_canReset;

        PlayerListType m_playerList;
        GroupListType m_groupList;

        SpawnedPoolData m_spawnedPoolData;
};

class BattleGroundPersistentState : public MapPersistentState
{
    public:

        BattleGroundPersistentState(uint32 MapId, uint32 InstanceId)
            : MapPersistentState(MapId, InstanceId) {}

        ~BattleGroundPersistentState() {}

        SpawnedPoolData& GetSpawnedPoolData() override { return m_spawnedPoolData; }
    protected:
        bool CanBeUnload() const override;

    private:
        SpawnedPoolData m_spawnedPoolData;
};

enum ResetEventType
{
    RESET_EVENT_NORMAL_DUNGEON      = 0,
    RESET_EVENT_INFORM_1            = 1,
    RESET_EVENT_INFORM_2            = 2,
    RESET_EVENT_INFORM_3            = 3,
    RESET_EVENT_INFORM_LAST         = 4,
    RESET_EVENT_FORCED_INFORM_1     = 5,
    RESET_EVENT_FORCED_INFORM_2     = 6,
    RESET_EVENT_FORCED_INFORM_3     = 7,
    RESET_EVENT_FORCED_INFORM_LAST  = 8,
};

enum InstanceResetFailReason
{
    INSTANCERESET_FAIL_GENERAL  = 0,
    INSTANCERESET_FAIL_OFFLINE  = 1,
    INSTANCERESET_FAIL_ZONING   = 2,
    INSTANCERESET_FAIL_SILENTLY = 3
};

#define MAX_RESET_EVENT_TYPE   9

struct DungeonResetEvent
{
    ResetEventType type   : 8;
    uint16 mapid;
    uint32 instanceId;

    DungeonResetEvent() : type(RESET_EVENT_NORMAL_DUNGEON), mapid(0), instanceId(0) {}
    DungeonResetEvent(ResetEventType t, uint32 _mapid, uint32 _instanceid)
        : type(t), mapid(_mapid), instanceId(_instanceid) {}

    bool operator == (const DungeonResetEvent& e) { return e.mapid == mapid && e.instanceId == instanceId; }
};

class DungeonResetScheduler
{
    public:
        explicit DungeonResetScheduler(MapPersistentStateManager& mgr) : m_InstanceSaves(mgr) {}
        void LoadResetTimes();

    public:
        time_t GetResetTimeFor(uint32 mapid) { return m_resetTimeByMapId[mapid]; }

        static uint32 GetMaxResetTimeFor(InstanceTemplate const* temp);
        static time_t CalculateNextResetTime(InstanceTemplate const* temp, time_t prevResetTime);
    public:
        void SetResetTimeFor(uint32 mapid, time_t t)
        {
            m_resetTimeByMapId[mapid] = t;
        }

        void ScheduleReset(bool add, time_t time, DungeonResetEvent event);

        void Update();

        void ResetAllRaid();
    private:
        MapPersistentStateManager& m_InstanceSaves;

        typedef std::vector < time_t  > ResetTimeVector;
        ResetTimeVector m_resetTimeByMapId;

        typedef std::multimap < time_t , DungeonResetEvent > ResetTimeQueue;
        ResetTimeQueue m_resetTimeQueue;
};

class MapPersistentStateManager : public MaNGOS::Singleton<MapPersistentStateManager>
{
    friend class DungeonResetScheduler;

    public:
        MapPersistentStateManager();
        ~MapPersistentStateManager();

    public:

        void InitWorldMaps();
        void LoadCreatureRespawnTimes();
        void LoadGameobjectRespawnTimes();

        MapPersistentState* AddPersistentState(MapEntry const* mapEntry, uint32 instanceId, time_t resetTime, bool canReset, bool load = false, bool initPools = true);

        MapPersistentState* GetPersistentState(uint32 mapId, uint32 InstanceId);

        void RemovePersistentState(uint32 mapId, uint32 instanceId);

        template<typename Do>
            void DoForAllStatesWithMapId(uint32 mapId, Do& _do);

    public:
        void CleanupInstances();
        void PackInstances();

        DungeonResetScheduler& GetScheduler()
        {
            return m_Scheduler;
        }

        static void DeleteInstanceFromDB(uint32 instanceid);

        void GetStatistics(uint32& numStates, uint32& numBoundPlayers, uint32& numBoundGroups);

        void Update()
        {
            m_Scheduler.Update();
        }

    private:
        typedef std::unordered_map < uint32 , MapPersistentState* > PersistentStateMap;

        void _ResetOrWarnAll(uint32 mapid, bool warn, uint32 timeleft);
        void _ResetInstance(uint32 mapid, uint32 instanceId);
        void _CleanupExpiredInstancesAtTime(time_t t);

        void _ResetSave(PersistentStateMap& holder, PersistentStateMap::iterator& itr);
        void _DelHelper(DatabaseType& db, const char* fields, const char* table, const char* queryTail, ...);

        bool lock_instLists;

        PersistentStateMap m_instanceSaveByInstanceId;

        PersistentStateMap m_instanceSaveByMapId;

        DungeonResetScheduler m_Scheduler;
};

template<typename Do>
    inline void MapPersistentStateManager::DoForAllStatesWithMapId(uint32 mapId, Do& _do)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
    if (!mapEntry)
    {
        return;
    }

    if (mapEntry->Instanceable())
    {
        for (PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.begin(); itr != m_instanceSaveByInstanceId.end();)
        {
            if (itr->second->GetMapId() == mapId)
            {
                _do((itr++)->second);
            }
            else
            {
                ++itr;
            }
        }
    }
    else
    {
        if (MapPersistentState* state = GetPersistentState(mapId, 0))
        {
            _do(state);
        }
    }
}

#define sMapPersistentStateMgr MaNGOS::Singleton<MapPersistentStateManager>::Instance()
