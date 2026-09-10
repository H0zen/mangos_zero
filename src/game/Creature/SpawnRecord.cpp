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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "SpawnRecord.h"

#include <sstream>

#include "Creature.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Map.h"
#include "MapRoster.h"
#include "MapPersistentStateMgr.h"
#include "ObjectMgr.h"

namespace
{

    struct ForgetRespawnTime
    {
        explicit ForgetRespawnTime(uint32 guid) : i_guid(guid) {}

        void operator()(MapPersistentState* state)
        {
            state->SaveCreatureRespawnTime(i_guid, 0);
        }

        uint32 i_guid;
    };

    struct TakeOutOfMap
    {
        explicit TakeOutOfMap(ObjectGuid guid) : i_guid(guid) {}

        void operator()(Map* map)
        {
            if (Creature* standing = map->GetCreature(i_guid))
            {
                standing->AddObjectToRemoveList();
            }
        }

        ObjectGuid i_guid = 0;
    };

    struct PutIntoMap
    {
        PutIntoMap(uint32 guid, CreatureData const* data) : i_guid(guid), i_data(data) {}

        void operator()(Map* map)
        {

            if (map->IsCellLoaded(i_data->posX, i_data->posY))
            {
                Creature* standing = new Creature;
                if (!standing->LoadFromDB(i_guid, map))
                {
                    delete standing;
                }
            }
        }

        uint32 i_guid;
        CreatureData const* i_data;
    };
}

bool npcs::Listed(Creature const& who)
{
    return sObjectMgr.GetCreatureData(who.GetGUIDLow()) != nullptr;
}

void npcs::Save(Creature& who)
{

    if (who.IsPet() || who.IsTemporarySummon())
    {
        return;
    }

    if (!Listed(who))
    {
        sLog.outError("npcs::Save failed, can not get creature data!");
        return;
    }

    SaveOn(who, who.GetMapId());
}

void npcs::SaveOn(Creature& who, uint32 mapId)
{
    if (who.IsPet() || who.IsTemporarySummon())
    {
        return;
    }

    CreatureData& data = sObjectMgr.NewOrExistCreatureData(who.GetGUIDLow());

    uint32 displayId = who.GetNativeDisplayId();

    CreatureInfo const* cinfo = who.GetCreatureInfo();
    if (cinfo)
    {

        if (displayId != cinfo->ModelId[0] && displayId != cinfo->ModelId[1] &&
            displayId != cinfo->ModelId[2] && displayId != cinfo->ModelId[3])
        {
            for (int i = 0; i < MAX_CREATURE_MODEL && displayId; ++i)
            {
                if (cinfo->ModelId[i])
                {
                    if (CreatureModelInfo const* minfo = sObjectMgr.GetCreatureModelInfo(cinfo->ModelId[i]))
                    {
                        if (displayId == minfo->modelid_other_gender)
                        {
                            displayId = 0;
                        }
                    }
                }
            }
        }
        else
        {
            displayId = 0;
        }
    }

    data.id = who.GetEntry();
    data.mapid = mapId;
    data.modelid_override = displayId;
    data.equipmentId = who.GetEquipmentId();
    data.posX = who.Where().X();
    data.posY = who.Where().Y();
    data.posZ = who.Where().Z();
    data.orientation = who.Where().Facing();
    data.spawntimesecs = who.Watch().RespawnDelay();

    data.spawndist = who.GetDefaultMovementType() == IDLE_MOTION_TYPE ? 0 : who.Stationed().Radius();
    data.currentwaypoint = 0;
    data.curhealth = who.GetHealth();
    data.curmana = who.GetPower(POWER_MANA);
    data.is_dead = who.Watch().DeadByDefault();

    data.movementType = !who.Stationed().Radius() && who.GetDefaultMovementType() == RANDOM_MOTION_TYPE
        ? IDLE_MOTION_TYPE : who.GetDefaultMovementType();

    WorldDatabase.BeginTransaction();

    WorldDatabase.PExecuteLog("DELETE FROM `creature` WHERE `guid`=%u", who.GetGUIDLow());

    std::ostringstream ss;
    ss << "INSERT INTO `creature` VALUES ("
       << who.GetGUIDLow() << ","
       << data.id << ","
       << data.mapid << ","
       << data.modelid_override << ","
       << data.equipmentId << ","
       << data.posX << ","
       << data.posY << ","
       << data.posZ << ","
       << data.orientation << ","
       << data.spawntimesecs << ","
       << (float) data.spawndist << ","
       << data.currentwaypoint << ","
       << data.curhealth << ","
       << data.curmana << ","
       << (data.is_dead  ? 1 : 0) << ","
       << uint32(data.movementType) << ")";

    WorldDatabase.PExecuteLog("%s", ss.str().c_str());

    WorldDatabase.CommitTransaction();
}

void npcs::Forget(Creature& who)
{

    if (who.IsPet() || who.IsTemporarySummon())
    {
        return;
    }

    CreatureData const* data = sObjectMgr.GetCreatureData(who.GetGUIDLow());
    if (!data)
    {
        DEBUG_LOG("Trying to delete not saved creature!");
        return;
    }

    Forget(who.GetGUIDLow(), data);
}

void npcs::Forget(uint32 lowGuid, CreatureData const* data)
{
    ForgetRespawnTime worker(lowGuid);
    sMapPersistentStateMgr.DoForAllStatesWithMapId(data->mapid, worker);

    sObjectMgr.DeleteCreatureData(lowGuid);

    WorldDatabase.BeginTransaction();
    WorldDatabase.PExecuteLog("DELETE FROM `creature` WHERE `guid`=%u", lowGuid);
    WorldDatabase.PExecuteLog("DELETE FROM `creature_addon` WHERE `guid`=%u", lowGuid);
    WorldDatabase.PExecuteLog("DELETE FROM `creature_movement` WHERE `id`=%u", lowGuid);
    WorldDatabase.PExecuteLog("DELETE FROM `game_event_creature` WHERE `guid`=%u", lowGuid);
    WorldDatabase.PExecuteLog("DELETE FROM `game_event_creature_data` WHERE `guid`=%u", lowGuid);
    WorldDatabase.PExecuteLog("DELETE FROM `creature_battleground` WHERE `guid`=%u", lowGuid);
    WorldDatabase.PExecuteLog("DELETE FROM `creature_linking` WHERE `guid`=%u OR `master_guid`=%u", lowGuid, lowGuid);
    WorldDatabase.CommitTransaction();
}

void npcs::SaveRespawnTime(Creature& who)
{
    if (who.IsPet() || !Listed(who))
    {
        return;
    }

    if (who.Watch().RespawnsAt() > time(nullptr))
    {
        who.GetMap()->GetPersistentState()->SaveCreatureRespawnTime(who.GetGUIDLow(), who.Watch().RespawnsAt());
    }
    else if (who.Watch().CorpseGoesAt() > time(nullptr))
    {
        who.GetMap()->GetPersistentState()->SaveCreatureRespawnTime(who.GetGUIDLow(), who.Watch().CorpseGoesAt() + who.Watch().RespawnDelay());
    }
}

void npcs::SpawnInMaps(uint32 lowGuid, CreatureData const* data)
{
    PutIntoMap worker(lowGuid, data);
    sMapRoster.EachOnMap(data->mapid, worker);
}

void npcs::RemoveFromMaps(uint32 lowGuid, CreatureData const* data)
{
    TakeOutOfMap worker(data->GetObjectGuid(lowGuid));
    sMapRoster.EachOnMap(data->mapid, worker);
}
