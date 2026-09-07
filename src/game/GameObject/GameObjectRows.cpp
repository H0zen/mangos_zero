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
 * @file GameObjectRows.cpp
 * @brief The rows a world object is spawned from and written back to.
 *
 * A chest, a door and a herb are rows in `gameobject` that a map reads when it opens the
 * grid they stand in, and that a builder writes back with `.gobject add`. What SQL that
 * takes is nothing the object itself has to know.
 */

#include "GameObject.h"

#include "Database/DatabaseEnv.h"
#include "GameEventMgr.h"
#include "Log.h"
#include "Map.h"
#include "MapPersistentStateMgr.h"
#include "ObjectMgr.h"
#include "PoolManager.h"
#include "SQLStorages.h"
#include "World.h"

#include <sstream>

/**
 * @brief Saves the loaded game object back to the database.
 */
void GameObject::SaveToDB()
{
    // this should only be used when the gameobject has already been loaded
    // preferably after adding to map, because mapid may not be valid otherwise
    GameObjectData const* data = sObjectMgr.GetGOData(GetGUIDLow());
    if (!data)
    {
        sLog.outError("GameObject::SaveToDB failed, can not get gameobject data!");
        return;
    }

    SaveToDB(GetMapId());
}

/**
 * @brief Saves the game object spawn data to the database for a map.
 *
 * @param mapid The map id to persist.
 */
void GameObject::SaveToDB(uint32 mapid)
{
    const GameObjectInfo* goI = GetGOInfo();

    if (!goI)
    {
        return;
    }

    // update in loaded data (changing data only in this place)
    GameObjectData& data = sObjectMgr.NewGOData(GetGUIDLow());

    // data->guid = guid don't must be update at save
    data.id = GetEntry();
    data.mapid = mapid;
    data.posX = GetGoPositionX();
    data.posY = GetGoPositionY();
    data.posZ = GetGoPositionZ();
    data.orientation = GetFloatValue(GAMEOBJECT_FACING);
    data.rotation0 = GetFloatValue(GAMEOBJECT_ROTATION + 0);
    data.rotation1 = GetFloatValue(GAMEOBJECT_ROTATION + 1);
    data.rotation2 = GetFloatValue(GAMEOBJECT_ROTATION + 2);
    data.rotation3 = GetFloatValue(GAMEOBJECT_ROTATION + 3);
    data.spawntimesecs = m_spawn.AsSpawnTimeSecs();
    data.animprogress = GetGoAnimProgress();
    data.go_state = GetGoState();

    // updated in DB
    std::ostringstream ss;
    ss << "INSERT INTO `gameobject` VALUES ( "
       << GetGUIDLow() << ", "
       << GetEntry() << ", "
       << mapid << ", "
       << GetGoPositionX() << ", "
       << GetGoPositionY() << ", "
       << GetGoPositionZ() << ", "
       << GetFloatValue(GAMEOBJECT_FACING) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 1) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 2) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 3) << ", "
       << m_spawn.AsSpawnTimeSecs() << ", "
       << uint32(GetGoAnimProgress()) << ", "
       << uint32(GetGoState()) << ")";

    WorldDatabase.BeginTransaction();
    WorldDatabase.PExecuteLog("DELETE FROM `gameobject` WHERE `guid` = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("%s", ss.str().c_str());
    WorldDatabase.CommitTransaction();
}

namespace
{
    /// Clears the respawn time this object is remembered by, in every copy of its map.
    struct GameObjectRespawnDeleteWorker
    {
        explicit GameObjectRespawnDeleteWorker(uint32 guid) : i_guid(guid) {}

        void operator()(MapPersistentState* state)
        {
            state->SaveGORespawnTime(i_guid, 0);
        }

        uint32 i_guid;
    };
}

/**
 * @brief Deletes the static database spawn record for this game object.
 */
void GameObject::DeleteFromDB()
{
    if (!HasStaticDBSpawnData())
    {
        DEBUG_LOG("Trying to delete not saved gameobject!");
        return;
    }

    GameObjectRespawnDeleteWorker worker(GetGUIDLow());
    sMapPersistentStateMgr.DoForAllStatesWithMapId(GetMapId(), worker);

    sObjectMgr.DeleteGOData(GetGUIDLow());
    WorldDatabase.PExecuteLog("DELETE FROM `gameobject` WHERE `guid` = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("DELETE FROM `game_event_gameobject` WHERE `guid` = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("DELETE FROM `gameobject_battleground` WHERE `guid` = '%u'", GetGUIDLow());
}

