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
#include <sstream>
#include <string>
#include <map>
#include <set>
#include "MapPersistentStateMgr.h"
#include "SQLStorages.h"
#include "Player.h"
#include "Log.h"
#include "CellImpl.h"
#include "Map.h"
#include "MapRoster.h"
#include "Timer.h"
#include "GridNotifiersImpl.h"
#include "ObjectMgr.h"
#include "GameEventMgr.h"
#include "World.h"
#include "Group.h"
#include "InstanceData.h"
#include "ProgressBar.h"
#include <vector>
#include <cstdarg>

static uint32 resetEventTypeDelay[MAX_RESET_EVENT_TYPE] = { 0,
        3600, 900, 300, 60,
        60, 30, 10, 5 };

MapPersistentState::MapPersistentState(uint32 MapId, uint32 InstanceId)
    : m_instanceid(InstanceId), m_mapid(MapId),
    m_usedByMap(nullptr)
{
}

MapPersistentState::~MapPersistentState()
{
}

MapEntry const* MapPersistentState::GetMapEntry() const
{
    return sMapStore.LookupEntry(m_mapid);
}

bool MapPersistentState::UnloadIfEmpty()
{
    if (CanBeUnload())
    {
        sMapPersistentStateMgr.RemovePersistentState(GetMapId(), GetInstanceId());
        return false;
    }
    else
    {
        return true;
    }
}

void MapPersistentState::SaveCreatureRespawnTime(uint32 loguid, time_t t)
{
    SetCreatureRespawnTime(loguid, t);

    if (GetMapEntry()->IsBattleGround())
    {
        return;
    }

    CharacterDatabase.BeginTransaction();

    static SqlStatementID delSpawnTime ;
    static SqlStatementID insSpawnTime ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delSpawnTime, "DELETE FROM `creature_respawn` WHERE `guid` = ? AND `instance` = ?");
    stmt.PExecute(loguid, m_instanceid);

    if (t > sWorld.GetGameTime())
    {
        stmt = CharacterDatabase.CreateStatement(insSpawnTime, "INSERT INTO `creature_respawn` VALUES ( ?, ?, ? )");
        stmt.PExecute(loguid, uint64(t), m_instanceid);
    }

    CharacterDatabase.CommitTransaction();
}

void MapPersistentState::SaveGORespawnTime(uint32 loguid, time_t t)
{
    SetGORespawnTime(loguid, t);

    if (GetMapEntry()->IsBattleGround())
    {
        return;
    }

    CharacterDatabase.BeginTransaction();

    static SqlStatementID delSpawnTime ;
    static SqlStatementID insSpawnTime ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delSpawnTime, "DELETE FROM `gameobject_respawn` WHERE `guid` = ? AND `instance` = ?");
    stmt.PExecute(loguid, m_instanceid);

    if (t > sWorld.GetGameTime())
    {
        stmt = CharacterDatabase.CreateStatement(insSpawnTime, "INSERT INTO `gameobject_respawn` VALUES ( ?, ?, ? )");
        stmt.PExecute(loguid, uint64(t), m_instanceid);
    }

    CharacterDatabase.CommitTransaction();
}

void MapPersistentState::SetCreatureRespawnTime(uint32 loguid, time_t t)
{
    if (t > sWorld.GetGameTime())
    {
        m_creatureRespawnTimes[loguid] = t;
    }
    else
    {
        m_creatureRespawnTimes.erase(loguid);
        UnloadIfEmpty();
    }
}

void MapPersistentState::SetGORespawnTime(uint32 loguid, time_t t)
{
    if (t > sWorld.GetGameTime())
    {
        m_goRespawnTimes[loguid] = t;
    }
    else
    {
        m_goRespawnTimes.erase(loguid);
        UnloadIfEmpty();
    }
}

void MapPersistentState::ClearRespawnTimes()
{
    m_goRespawnTimes.clear();
    m_creatureRespawnTimes.clear();

    UnloadIfEmpty();
}

void MapPersistentState::AddCreatureToGrid(uint32 guid, CreatureData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    m_gridObjectGuids[cell_id].creatures.insert(guid);
}

void MapPersistentState::RemoveCreatureFromGrid(uint32 guid, CreatureData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    m_gridObjectGuids[cell_id].creatures.erase(guid);
}

void MapPersistentState::AddGameobjectToGrid(uint32 guid, GameObjectData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    m_gridObjectGuids[cell_id].gameobjects.insert(guid);
}

void MapPersistentState::RemoveGameobjectFromGrid(uint32 guid, GameObjectData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    m_gridObjectGuids[cell_id].gameobjects.erase(guid);
}

void MapPersistentState::InitPools()
{

    if (!GetSpawnedPoolData().IsInitialized())
    {
        GetSpawnedPoolData().SetInitialized();
        sPoolMgr.Initialize(this);
        sGameEventMgr.Initialize(this);
    }
}

SpawnedPoolData WorldPersistentState::m_sharedSpawnedPoolData;

bool WorldPersistentState::CanBeUnload() const
{

    return  false;
}

DungeonPersistentState::DungeonPersistentState(uint32 MapId, uint32 InstanceId, time_t resetTime, bool canReset)
    : MapPersistentState(MapId, InstanceId), m_resetTime(resetTime), m_canReset(canReset)
{
}

DungeonPersistentState::~DungeonPersistentState()
{
    DEBUG_LOG("Unloading DungeonPersistantState of map %u instance %u", GetMapId(), GetInstanceId());
    UnbindThisState();
}

void DungeonPersistentState::UnbindThisState()
{
    while (!m_playerList.empty())
    {
        Player* player = *(m_playerList.begin());
        player->Binds().Release(GetMapId(), true);
    }
    while (!m_groupList.empty())
    {
        Group* group = *(m_groupList.begin());
        group->Binds().Release(GetMapId(), true);
    }
}

bool DungeonPersistentState::CanBeUnload() const
{

    return MapPersistentState::CanBeUnload() && !HasBounds() && !HasRespawnTimes();
}

void DungeonPersistentState::SaveToDB()
{

    std::string data;

    if (Map* map = GetMap())
    {
        InstanceData* iData = map->GetInstanceData();
        if (iData && iData->Save())
        {
            data = iData->Save();
            CharacterDatabase.escape_string(data);
        }
    }

    CharacterDatabase.PExecute("INSERT INTO `instance` VALUES ('%u', '%u', '" UI64FMTD "', '%s')", GetInstanceId(), GetMapId(), (uint64)GetResetTimeForDB(), data.c_str());
}

void DungeonPersistentState::DeleteRespawnTimes()
{
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `creature_respawn` WHERE `instance` = '%u'", GetInstanceId());
    CharacterDatabase.PExecute("DELETE FROM `gameobject_respawn` WHERE `instance` = '%u'", GetInstanceId());
    CharacterDatabase.CommitTransaction();

    ClearRespawnTimes();
}

void DungeonPersistentState::DeleteFromDB()
{
    MapPersistentStateManager::DeleteInstanceFromDB(GetInstanceId());
}

InstanceTemplate const* DungeonPersistentState::GetTemplate() const
{
    return ObjectMgr::GetInstanceTemplate(GetMapId());
}

time_t DungeonPersistentState::GetResetTimeForDB() const
{

    const MapEntry* entry = sMapStore.LookupEntry(GetMapId());
    if (!entry || entry->InstanceType == MAP_RAID)
    {
        return 0;
    }
    else
    {
        return GetResetTime();
    }
}

bool BattleGroundPersistentState::CanBeUnload() const
{

    return MapPersistentState::CanBeUnload();
}

uint32 DungeonResetScheduler::GetMaxResetTimeFor(InstanceTemplate const* temp)
{
    if (!temp)
    {
        return 0;
    }

    return temp->reset_delay * DAY;
}

time_t DungeonResetScheduler::CalculateNextResetTime(InstanceTemplate const* temp, time_t prevResetTime)
{
    uint32 diff = sWorld.getConfig(CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR) * HOUR;
    uint32 period = GetMaxResetTimeFor(temp);
    return ((prevResetTime + MINUTE) / DAY * DAY) + period + diff;
}

void DungeonResetScheduler::LoadResetTimes()
{
    time_t now = time(nullptr);
    time_t today = (now / DAY) * DAY;
    time_t nextWeek = today + (7 * DAY);

    typedef std::map<uint32, std::pair<uint32, time_t> > ResetTimeMapType;
    ResetTimeMapType InstResetTime;

    QueryResult* result = CharacterDatabase.Query("SELECT `id`, `map`, `resettime` FROM `instance` WHERE `resettime` > 0");
    if (result)
    {
        do
        {
            if (time_t resettime = time_t((*result)[2].GetUInt64()))
            {
                uint32 id = (*result)[0].GetUInt32();
                uint32 mapid = (*result)[1].GetUInt32();

                MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);

                if (!mapEntry || !mapEntry->IsDungeon())
                {
                    sMapPersistentStateMgr.DeleteInstanceFromDB(id);
                    continue;
                }

                InstResetTime[id] = std::pair<uint32, uint64>(mapid, resettime);
            }
        }
        while (result->NextRow());
        delete result;

        result = CharacterDatabase.Query("SELECT MAX(`respawntime`), `instance` FROM `creature_respawn` WHERE `instance` > 0 GROUP BY `instance`");
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();

                time_t resettime    = time_t(fields[0].GetUInt64() + 2 * HOUR);
                uint32 instance     = fields[1].GetUInt32();

                ResetTimeMapType::iterator itr = InstResetTime.find(instance);
                if (itr != InstResetTime.end() && itr->second.second != resettime)
                {
                    CharacterDatabase.DirectPExecute("UPDATE `instance` SET `resettime` = '" UI64FMTD "' WHERE `id` = '%u'", uint64(resettime), instance);
                    itr->second.second = resettime;
                }
            }
            while (result->NextRow());
            delete result;
        }

        for (ResetTimeMapType::iterator itr = InstResetTime.begin(); itr != InstResetTime.end(); ++itr)
        {
            if (itr->second.second > now)
            {
                ScheduleReset(true, itr->second.second, DungeonResetEvent(RESET_EVENT_NORMAL_DUNGEON, itr->second.first, itr->first));
            }
        }
    }

    uint32 diff = sWorld.getConfig(CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR) * HOUR;
    m_resetTimeByMapId.resize(sMapStore.GetNumRows() + 1);
    result = CharacterDatabase.Query("SELECT `mapid`, `resettime` FROM `instance_reset`");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 mapid            = fields[0].GetUInt32();

            MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);

            if (!mapEntry || !mapEntry->IsDungeon() || !ObjectMgr::GetInstanceTemplate(mapid))
            {
                sLog.outError("MapPersistentStateManager::LoadResetTimes: invalid mapid %u in instance_reset!", mapid);
                CharacterDatabase.DirectPExecute("DELETE FROM `instance_reset` WHERE `mapid` = '%u'", mapid);
                continue;
            }

            uint64 oldresettime = fields[1].GetUInt64();
            uint64 newresettime = (oldresettime / DAY) * DAY + diff;
            if (oldresettime != newresettime)
            {
                CharacterDatabase.DirectPExecute("UPDATE `instance_reset` SET `resettime` = '" UI64FMTD "' WHERE `mapid` = '%u'", newresettime, mapid);
            }

            SetResetTimeFor(mapid, newresettime);
        }
        while (result->NextRow());
        delete result;
    }

    m_InstanceSaves._CleanupExpiredInstancesAtTime(now);

    for (uint32 i = 0; i < sInstanceTemplate.GetMaxEntry(); i++)
    {

        InstanceTemplate const* temp = ObjectMgr::GetInstanceTemplate(i);
        if (!temp || !temp->reset_delay)
        {
            continue;
        }

        MapEntry const* mapEntry = sMapStore.LookupEntry(temp->map);
        if (!mapEntry || !mapEntry->IsDungeon())
        {
            continue;
        }

        uint32 period = GetMaxResetTimeFor(temp);
        time_t t = GetResetTimeFor(temp->map);
        if (!t)
        {

            t = today + period + diff;
            CharacterDatabase.DirectPExecute("INSERT INTO `instance_reset` VALUES ('%u','" UI64FMTD "')", temp->map, (uint64)t);
        }

        if (t < now || t > nextWeek)
        {

            t = (t / DAY) * DAY;
            t += ((today - t) / period + 1) * period + diff;
            CharacterDatabase.DirectPExecute("UPDATE `instance_reset` SET `resettime` = '" UI64FMTD "' WHERE `mapid` = '%u'", (uint64)t, temp->map);
        }

        SetResetTimeFor(temp->map, t);

        ResetEventType type = RESET_EVENT_INFORM_1;
        for (; type < RESET_EVENT_INFORM_LAST; type = ResetEventType(type + 1))
        {
            if (t > time_t(now + resetEventTypeDelay[type]))
            {
                break;
            }
        }
        ScheduleReset(true, t - resetEventTypeDelay[type], DungeonResetEvent(type, temp->map, 0));
    }
}

void DungeonResetScheduler::ScheduleReset(bool add, time_t time, DungeonResetEvent event)
{
    if (add)
    {
        m_resetTimeQueue.insert(std::pair<time_t, DungeonResetEvent>(time, event));
    }
    else
    {

        ResetTimeQueue::iterator itr;
        std::pair<ResetTimeQueue::iterator, ResetTimeQueue::iterator> range;
        range = m_resetTimeQueue.equal_range(time);
        for (itr = range.first; itr != range.second; ++itr)
        {
            if (itr->second == event)
            {
                m_resetTimeQueue.erase(itr);
                return;
            }
        }

        if (itr == range.second)
        {
            for (itr = m_resetTimeQueue.begin(); itr != m_resetTimeQueue.end(); ++itr)
            {
                if (itr->second == event)
                {
                    m_resetTimeQueue.erase(itr);
                    return;
                }
            }

            if (itr == m_resetTimeQueue.end())
            {
                sLog.outError("DungeonResetScheduler::ScheduleReset: can not cancel the reset, the event(%d,%d,%d) was not found!", event.type, event.mapid, event.instanceId);
            }
        }
    }
}

void DungeonResetScheduler::Update()
{
    time_t now = time(nullptr), t;
    while (!m_resetTimeQueue.empty() && (t = m_resetTimeQueue.begin()->first) < now)
    {
        DungeonResetEvent& event = m_resetTimeQueue.begin()->second;
        if (event.type == RESET_EVENT_NORMAL_DUNGEON)
        {

            m_InstanceSaves._ResetInstance(event.mapid, event.instanceId);
        }
        else
        {

            time_t resetTime = GetResetTimeFor(event.mapid);
            uint32 timeLeft = uint32(std::max(int32(resetTime - now), 0));
            bool warn = event.type != RESET_EVENT_INFORM_LAST && event.type != RESET_EVENT_FORCED_INFORM_LAST;
            m_InstanceSaves._ResetOrWarnAll(event.mapid, warn, timeLeft);
            if (event.type != RESET_EVENT_INFORM_LAST && event.type != RESET_EVENT_FORCED_INFORM_LAST)
            {

                event.type = ResetEventType(event.type + 1);
                ScheduleReset(true, resetTime - resetEventTypeDelay[event.type], event);
            }
            else
            {

                InstanceTemplate const* instanceTemplate = ObjectMgr::GetInstanceTemplate(event.mapid);
                MANGOS_ASSERT(instanceTemplate);

                time_t next_reset = DungeonResetScheduler::CalculateNextResetTime(instanceTemplate, resetTime);

                CharacterDatabase.DirectPExecute("UPDATE `instance_reset` SET `resettime` = '" UI64FMTD "' WHERE `mapid` = '%u'", uint64(next_reset), uint32(event.mapid));

                SetResetTimeFor(event.mapid, next_reset);

                ResetEventType type = RESET_EVENT_INFORM_1;
                for (; type < RESET_EVENT_INFORM_LAST; type = ResetEventType(type + 1))
                {
                    if (next_reset > time_t(now + resetEventTypeDelay[type]))
                    {
                        break;
                    }
                }

                event.type = type;
                ScheduleReset(true, next_reset - resetEventTypeDelay[event.type], event);
            }
        }
        m_resetTimeQueue.erase(m_resetTimeQueue.begin());
    }
}

void DungeonResetScheduler::ResetAllRaid()
{
    time_t now = time(nullptr);
    ResetTimeQueue rTQ;
    rTQ.clear();

    time_t timeleft = resetEventTypeDelay[RESET_EVENT_FORCED_INFORM_1];

    for (ResetTimeQueue::iterator itr = m_resetTimeQueue.begin(); itr != m_resetTimeQueue.end(); ++itr)
    {
        DungeonResetEvent& event = itr->second;

        if (event.type == RESET_EVENT_NORMAL_DUNGEON)
        {
            rTQ.insert(std::pair<time_t, DungeonResetEvent>(itr->first, event));
            continue;
        }
        event.type = RESET_EVENT_FORCED_INFORM_1;
        time_t next_reset = now + timeleft;
        SetResetTimeFor(event.mapid, next_reset);
        rTQ.insert(std::pair<time_t, DungeonResetEvent>(now, event));
    }
    m_resetTimeQueue = rTQ;
}

MapPersistentStateManager::MapPersistentStateManager() : lock_instLists(false), m_Scheduler(*this)
{
}

MapPersistentStateManager::~MapPersistentStateManager()
{

    lock_instLists = true;
    for (PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.begin(); itr != m_instanceSaveByInstanceId.end(); ++itr)
    {
        delete  itr->second;
    }
    for (PersistentStateMap::iterator itr = m_instanceSaveByMapId.begin(); itr != m_instanceSaveByMapId.end(); ++itr)
    {
        delete  itr->second;
    }
}

MapPersistentState* MapPersistentStateManager::AddPersistentState(MapEntry const* mapEntry, uint32 instanceId, time_t resetTime, bool canReset, bool load , bool initPools )
{
    if (MapPersistentState* old_save = GetPersistentState(mapEntry->MapID, instanceId))
    {
        return old_save;
    }

    if (mapEntry->IsDungeon())
    {
        if (!resetTime)
        {

            if (mapEntry->InstanceType == MAP_RAID)
            {
                resetTime = m_Scheduler.GetResetTimeFor(mapEntry->MapID);
            }
            else
            {
                resetTime = time(nullptr) + 2 * HOUR;

                m_Scheduler.ScheduleReset(true, resetTime, DungeonResetEvent(RESET_EVENT_NORMAL_DUNGEON, mapEntry->MapID, instanceId));
            }
        }
    }

    DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MapPersistentStateManager::AddPersistentState: mapid = %d, instanceid = %d, reset time = '" UI64FMTD "', canRset = %u", mapEntry->MapID, instanceId, uint64(resetTime), canReset ? 1 : 0);

    MapPersistentState* state;
    if (mapEntry->IsDungeon())
    {
        DungeonPersistentState* dungeonState = new DungeonPersistentState(mapEntry->MapID, instanceId, resetTime, canReset);
        if (!load)
        {
            dungeonState->SaveToDB();
        }
        state = dungeonState;
    }
    else if (mapEntry->IsBattleGround())
    {
        state = new BattleGroundPersistentState(mapEntry->MapID, instanceId);
    }
    else
    {
        state = new WorldPersistentState(mapEntry->MapID);
    }

    if (instanceId)
    {
        m_instanceSaveByInstanceId[instanceId] = state;
    }
    else
    {
        m_instanceSaveByMapId[mapEntry->MapID] = state;
    }

    if (initPools)
    {
        state->InitPools();
    }

    return state;
}

MapPersistentState* MapPersistentStateManager::GetPersistentState(uint32 mapId, uint32 instanceId)
{
    if (instanceId)
    {
        PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.find(instanceId);
        return itr != m_instanceSaveByInstanceId.end() ? itr->second : nullptr;
    }
    else
    {
        PersistentStateMap::iterator itr = m_instanceSaveByMapId.find(mapId);
        return itr != m_instanceSaveByMapId.end() ? itr->second : nullptr;
    }
}

void MapPersistentStateManager::DeleteInstanceFromDB(uint32 instanceid)
{
    if (instanceid)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `instance` WHERE `id` = '%u'", instanceid);
        CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `instance` = '%u'", instanceid);
        CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `instance` = '%u'", instanceid);
        CharacterDatabase.PExecute("DELETE FROM `creature_respawn` WHERE `instance` = '%u'", instanceid);
        CharacterDatabase.PExecute("DELETE FROM `gameobject_respawn` WHERE `instance` = '%u'", instanceid);
        CharacterDatabase.CommitTransaction();
    }
}

void MapPersistentStateManager::RemovePersistentState(uint32 mapId, uint32 instanceId)
{
    if (lock_instLists)
    {
        return;
    }

    if (instanceId)
    {
        PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.find(instanceId);
        if (itr != m_instanceSaveByInstanceId.end())
        {

            if (itr->second->GetMapEntry()->IsDungeon())
            {
                if (time_t resettime = ((DungeonPersistentState*)itr->second)->GetResetTimeForDB())
                {
                    CharacterDatabase.PExecute("UPDATE `instance` SET `resettime` = '" UI64FMTD "' WHERE `id` = '%u'", (uint64)resettime, instanceId);
                }
            }

            _ResetSave(m_instanceSaveByInstanceId, itr);
        }
    }
    else
    {
        PersistentStateMap::iterator itr = m_instanceSaveByMapId.find(mapId);
        if (itr != m_instanceSaveByMapId.end())
        {
            _ResetSave(m_instanceSaveByMapId, itr);
        }
    }
}

void MapPersistentStateManager::_DelHelper(DatabaseType& db, const char* fields, const char* table, const char* queryTail, ...)
{
    Tokens fieldTokens = StrSplit(fields, ", ");
    MANGOS_ASSERT(fieldTokens.size() != 0);

    va_list ap;
    char szQueryTail [MAX_QUERY_LEN];
    va_start(ap, queryTail);
    vsnprintf(szQueryTail, MAX_QUERY_LEN, queryTail, ap);
    va_end(ap);

    QueryResult* result = db.PQuery("SELECT %s FROM %s %s", fields, table, szQueryTail);
    if (result)
    {
        do
        {
            Field* resultFields = result->Fetch();
            std::ostringstream ss;
            for (size_t i = 0; i < fieldTokens.size(); ++i)
            {
                std::string fieldValue = resultFields[i].GetCppString();
                db.escape_string(fieldValue);
                ss << (i != 0 ? " AND " : "") << fieldTokens[i] << " = '" << fieldValue << "'";
            }
            db.PExecute("DELETE FROM %s WHERE %s", table, ss.str().c_str());
        }
        while (result->NextRow());
        delete result;
    }
}

void MapPersistentStateManager::CleanupInstances()
{
    BarGoLink bar(2);
    bar.step();

    m_Scheduler.LoadResetTimes();

    CharacterDatabase.BeginTransaction();
    sLog.outString("|>  Clean character/group - instance binds with invalid group/characters...");

    _DelHelper(CharacterDatabase, "`character_instance`.`guid`, `instance`", "`character_instance`", "LEFT JOIN `characters` ON `character_instance`.`guid` = `characters`.`guid` WHERE `characters`.`guid` IS NULL");
    _DelHelper(CharacterDatabase, "`group_instance`.`leaderGuid`, `instance`", "`group_instance`", "LEFT JOIN `characters` ON `group_instance`.`leaderGuid` = `characters`.`guid` LEFT JOIN `groups` ON `group_instance`.`leaderGuid` = `groups`.`leaderGuid` WHERE `characters`.`guid` IS NULL OR `groups`.`leaderGuid` IS NULL");

    sLog.outString("|>  Clean instances that do not have any players or groups bound to them...");

    _DelHelper(CharacterDatabase, "`id`, `map`", "`instance`", "LEFT JOIN `character_instance` ON `character_instance`.`instance` = `id` LEFT JOIN `group_instance` ON `group_instance`.`instance` = `id` WHERE `character_instance`.`instance` IS NULL AND `group_instance`.`instance` IS NULL");

    sLog.outString("|>  Clean invalid instance references in other tables...");

    _DelHelper(CharacterDatabase, "`character_instance`.`guid`, `instance`", "`character_instance`", "LEFT JOIN `instance` ON `character_instance`.`instance` = `instance`.`id` WHERE `instance`.`id` IS NULL");
    _DelHelper(CharacterDatabase, "`group_instance`.`leaderGuid`, `instance`", "`group_instance`", "LEFT JOIN `instance` ON `group_instance`.`instance` = `instance`.`id` WHERE `instance`.`id` IS NULL");

    sLog.outString("|>  Clean unused respawn data...");

    CharacterDatabase.Execute("DELETE FROM `creature_respawn` WHERE `instance` <> 0 AND `instance` NOT IN (SELECT `id` FROM `instance`)");
    CharacterDatabase.Execute("DELETE FROM `gameobject_respawn` WHERE `instance` <> 0 AND `instance` NOT IN (SELECT `id` FROM `instance`)");

    CharacterDatabase.CommitTransaction();

    bar.step();

    sLog.outString(">> Instances cleaned up");
    sLog.outString();
}

void MapPersistentStateManager::PackInstances()
{

    std::set<uint32> InstanceSet;

    QueryResult* result = CharacterDatabase.Query("SELECT `id` FROM `instance`");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            InstanceSet.insert(fields[0].GetUInt32());
        }
        while (result->NextRow());
        delete result;
    }

    BarGoLink bar(InstanceSet.size() + 1);
    bar.step();

    uint32 InstanceNumber = 1;

    for (std::set<uint32>::iterator i = InstanceSet.begin(); i != InstanceSet.end(); ++i)
    {
        if (*i != InstanceNumber)
        {
            CharacterDatabase.BeginTransaction();

            CharacterDatabase.PExecute("UPDATE `creature_respawn` SET `instance` = '%u' WHERE `instance` = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE `gameobject_respawn` SET `instance` = '%u' WHERE `instance` = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE `corpse` SET `instance` = '%u' WHERE `instance` = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE `character_instance` SET `instance` = '%u' WHERE `instance` = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE `instance` SET `id` = '%u' WHERE `id` = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE `group_instance` SET `instance` = '%u' WHERE `instance` = '%u'", InstanceNumber, *i);

            CharacterDatabase.CommitTransaction();
        }

        ++InstanceNumber;
        bar.step();
    }

    sLog.outString(">> Instance numbers remapped, next instance id is %u", InstanceNumber);
    sLog.outString();
}

void MapPersistentStateManager::_ResetSave(PersistentStateMap& holder, PersistentStateMap::iterator& itr)
{

    lock_instLists = true;
    delete itr->second;
    holder.erase(itr++);
    lock_instLists = false;
}

void MapPersistentStateManager::_ResetInstance(uint32 mapid, uint32 instanceId)
{
    DEBUG_LOG("MapPersistentStateManager::_ResetInstance %u, %u", mapid, instanceId);

    PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.find(instanceId);
    if (itr != m_instanceSaveByInstanceId.end())
    {

        if (Map* iMap = itr->second->GetMap())
        {
            MANGOS_ASSERT(iMap->IsDungeon());

            ((DungeonMap*)iMap)->Reset(INSTANCE_RESET_RESPAWN_DELAY);
            return;
        }

        _ResetSave(m_instanceSaveByInstanceId, itr);
    }

    DeleteInstanceFromDB(instanceId);
}

struct MapPersistantStateResetWorker
{
    MapPersistantStateResetWorker() {};
    void operator()(Map* map)
    {
        ((DungeonMap*)map)->TeleportAllPlayersTo(TELEPORT_LOCATION_HOMEBIND);
        ((DungeonMap*)map)->Reset(INSTANCE_RESET_GLOBAL);
    }
};

struct MapPersistantStateWarnWorker
{
    MapPersistantStateWarnWorker(time_t _timeLeft) : timeLeft(_timeLeft)
    {};

    void operator()(Map* map)
    {
        ((DungeonMap*)map)->SendResetWarnings(timeLeft);
    }

    time_t timeLeft;
};

void MapPersistentStateManager::_ResetOrWarnAll(uint32 mapid, bool warn, uint32 timeLeft)
{

    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    if (!mapEntry->IsDungeon())
    {
        return;
    }

    time_t now = time(nullptr);

    if (!warn)
    {

        InstanceTemplate const* temp = ObjectMgr::GetInstanceTemplate(mapid);
        if (!temp || !temp->reset_delay)
        {
            sLog.outError("MapPersistentStateManager::ResetOrWarnAll: no instance template or reset delay for map %d", mapid);
            return;
        }

        std::vector<DungeonPersistentState*> unbindList;

        for (PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.begin(); itr != m_instanceSaveByInstanceId.end(); ++itr)
        {
            if (itr->second->GetMapId() == mapid)
            {
                unbindList.push_back((DungeonPersistentState*)itr->second);
            }
        }

        for (std::vector<DungeonPersistentState*>::const_iterator it = unbindList.begin(); it != unbindList.end(); ++it)
        {
            (*it)->UnbindThisState();
        }

        MapPersistantStateResetWorker worker;
        sMapRoster.EachOnMap(mapid, worker);

        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `character_instance` USING `character_instance` LEFT JOIN `instance` ON `character_instance`.`instance` = `id` WHERE `map` = '%u'", mapid);
        CharacterDatabase.PExecute("DELETE FROM `group_instance` USING `group_instance` LEFT JOIN `instance` ON `group_instance`.`instance` = `id` WHERE `map` = '%u'", mapid);
        CharacterDatabase.PExecute("DELETE FROM `instance` WHERE `map` = '%u'", mapid);
        CharacterDatabase.CommitTransaction();

        time_t next_reset = DungeonResetScheduler::CalculateNextResetTime(temp, now + timeLeft);

        CharacterDatabase.PExecute("UPDATE `instance_reset` SET `resettime` = '" UI64FMTD "' WHERE `mapid` = '%u'", (uint64)next_reset, mapid);
        return;
    }

    MapPersistantStateWarnWorker worker(timeLeft);
    sMapRoster.EachOnMap(mapid, worker);
}

void MapPersistentStateManager::GetStatistics(uint32& numStates, uint32& numBoundPlayers, uint32& numBoundGroups)
{
    numStates = 0;
    numBoundPlayers = 0;
    numBoundGroups = 0;

    for (PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.begin(); itr != m_instanceSaveByInstanceId.end(); ++itr)
    {
        if (!itr->second->GetMapEntry()->IsDungeon())
        {
            continue;
        }

        ++numStates;
        numBoundPlayers += ((DungeonPersistentState*)itr->second)->GetPlayerCount();
        numBoundGroups += ((DungeonPersistentState*)itr->second)->GetGroupCount();
    }
}

void MapPersistentStateManager::_CleanupExpiredInstancesAtTime(time_t t)
{
    _DelHelper(CharacterDatabase, "id, map", "instance", "LEFT JOIN instance_reset ON mapid = map WHERE (instance.resettime < '" UI64FMTD "' AND instance.resettime > '0') OR (NOT instance_reset.resettime IS NULL AND instance_reset.resettime < '" UI64FMTD "')", (uint64)t, (uint64)t);
}

void MapPersistentStateManager::InitWorldMaps()
{
    MapPersistentState* state = nullptr;
    for (uint32 mapid = 0; mapid < sMapStore.GetNumRows(); ++mapid)
    {
        if (MapEntry const* entry = sMapStore.LookupEntry(mapid))
        {
            if (!entry->Instanceable())
            {
                state = AddPersistentState(entry, 0, 0, false, true, false);
            }
        }
    }

    if (state)
    {
        state->InitPools();
    }
}

void MapPersistentStateManager::LoadCreatureRespawnTimes()
{

    CharacterDatabase.DirectExecute("DELETE FROM `creature_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())");

    uint32 count = 0;

    QueryResult* result = CharacterDatabase.Query("SELECT `guid`, `respawntime`, `map`, `instance`, `resettime` FROM `creature_respawn` LEFT JOIN `instance` ON `instance` = `id`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 creature respawn time.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 loguid               = fields[0].GetUInt32();
        uint64 respawn_time         = fields[1].GetUInt64();
        uint32 mapId                = fields[2].GetUInt32();
        uint32 instanceId           = fields[3].GetUInt32();
        time_t resetTime            = (time_t)fields[4].GetUInt64();

        CreatureData const* data = sObjectMgr.GetCreatureData(loguid);
        if (!data)
        {
            continue;
        }

        MapEntry const* mapEntry = sMapStore.LookupEntry(data->mapid);
        if (!mapEntry || (instanceId && (mapId != data->mapid || !mapEntry->Instanceable())))
        {
            continue;
        }

        MapPersistentState* state = AddPersistentState(mapEntry, instanceId, resetTime, mapEntry->IsDungeon(), true);
        if (!state)
        {
            continue;
        }

        state->SetCreatureRespawnTime(loguid, time_t(respawn_time));

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u creature respawn times", count);
    sLog.outString();
}

void MapPersistentStateManager::LoadGameobjectRespawnTimes()
{

    CharacterDatabase.DirectExecute("DELETE FROM `gameobject_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())");

    uint32 count = 0;

    QueryResult* result = CharacterDatabase.Query("SELECT `guid`, `respawntime`, `map`, `instance`, `resettime` FROM `gameobject_respawn` LEFT JOIN `instance` ON `instance` = `id`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 gameobject respawn time.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 loguid               = fields[0].GetUInt32();
        uint64 respawn_time         = fields[1].GetUInt64();
        uint32 mapId                = fields[2].GetUInt32();
        uint32 instanceId           = fields[3].GetUInt32();
        time_t resetTime            = (time_t)fields[4].GetUInt64();

        GameObjectData const* data = sObjectMgr.GetGOData(loguid);
        if (!data)
        {
            continue;
        }

        MapEntry const* mapEntry = sMapStore.LookupEntry(data->mapid);
        if (!mapEntry || (instanceId && (mapId != data->mapid || mapEntry->Instanceable())))
        {
            continue;
        }

        MapPersistentState* state = AddPersistentState(mapEntry, instanceId, resetTime, mapEntry->IsDungeon(), true);
        if (!state)
        {
            continue;
        }

        state->SetGORespawnTime(loguid, time_t(respawn_time));

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u gameobject respawn times", count);
    sLog.outString();
}
