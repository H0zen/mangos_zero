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

#include "Fleet.h"

#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "Log.h"
#include "Map.h"
#include "MapFoundry.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "TransportMap.h"
#include "Transports.h"
#include "VesselRoute.h"

#include <set>
#include <string>

Fleet::~Fleet()
{

    Scuttle();
}

void Fleet::MintDeckMaps()
{
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `name` FROM `transports`");

    if (!result)
    {
        sLog.outString(">> No vessel maps to mint. DB table `transports` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    uint32 minted = 0;

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        const uint32 entry = fields[0].GetUInt32();
        const std::string name = fields[1].GetCppString();

        GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(entry);

        if (!goinfo || goinfo->type != GAMEOBJECT_TYPE_MO_TRANSPORT)
        {
            continue;
        }

        Transport::RegisterVesselMap(entry, name.c_str());
        ++minted;
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Minted %u vessel deck map(s)", minted);
}

void Fleet::Launch()
{
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `name`, `period` FROM `transports`");

    uint32 count = 0;
    uint32 mapped = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u transports", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Transport* t = new Transport;

        Field* fields = result->Fetch();

        const uint32 entry = fields[0].GetUInt32();
        const std::string name = fields[1].GetCppString();
        const uint32 storedPeriod = fields[2].GetUInt32();

        const GameObjectInfo* goinfo = ObjectMgr::GetGameObjectInfo(entry);

        if (!goinfo)
        {
            sLog.outErrorDb("Transport ID:%u, Name: %s, will not be loaded, gameobject_template missing", entry, name.c_str());
            delete t;
            continue;
        }

        if (goinfo->type != GAMEOBJECT_TYPE_MO_TRANSPORT)
        {
            sLog.outErrorDb("Transport ID:%u, Name: %s, will not be loaded, gameobject_template type wrong", entry, name.c_str());
            delete t;
            continue;
        }

        float const speed = goinfo->moTransport.moveSpeed ? float(goinfo->moTransport.moveSpeed) : 30.0f;
        float const accel = goinfo->moTransport.accelRate ? float(goinfo->moTransport.accelRate) : 1.0f;
        VesselRoute const route = VesselRoute::Along(goinfo->moTransport.taxiPathId, speed, accel);

        t->m_route = route;
        t->m_period = route.Period() ? route.Period() : storedPeriod;

        DETAIL_LOG("Transport %u (%s): lap %u ms over %u legs, %u ms of it waiting; `transports`.`period` says %u",
                   entry, name.c_str(), route.Period(), uint32(route.Legs().size()), route.Waiting(), storedPeriod);

        std::set<uint32> mapsUsed = route.Maps();

        VesselPose const start = route.PoseAt(0);
        if (!start.known)
        {
            sLog.outErrorDb("Transport (path id %u) path size = 0. Transport ignored, check DBC files or transport GO data0 field.", goinfo->moTransport.taxiPathId);
            delete t;
            continue;
        }

        float const x = start.at.x, y = start.at.y, z = start.at.z, o = 1.0f;
        uint32 const mapid = start.mapId;

        const MapEntry* pMapInfo = sMapStore.LookupEntry(mapid);
        if (!pMapInfo || pMapInfo->Instanceable())
        {
            delete t;
            continue;
        }

        Transport::RegisterVesselMap(entry, name.c_str());

        if (!t->Create(entry, mapid, x, y, z, o, GO_ANIMPROGRESS_DEFAULT))
        {
            delete t;
            continue;
        }

        m_vessels.insert(t);

        for (const uint32 called : mapsUsed)
        {
            m_byMap[called].insert(t);
        }

        t->SetMap(sMapFoundry.OpenWorld(mapid));

        t->AddToWorld();
        t->SetActiveObjectState(true);
        t->GetMap()->AddToActive(t);

        t->PinRouteGrids();

        if (t->AsMap())
        {
            ++mapped;
            DETAIL_LOG("Transport %u '%s' is map %u", entry, name.c_str(), t->VesselMapId());
        }

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %u transports, %u with a map of their own", count, mapped);

    result = WorldDatabase.Query("SELECT `gameobject`.`guid`,`gameobject`.`id`,`transports`.`name` FROM `gameobject`,`transports` WHERE `gameobject`.`id` = `transports`.`entry`");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            const uint32 guid  = fields[0].GetUInt32();
            const uint32 entry = fields[1].GetUInt32();
            const std::string name = fields[2].GetCppString();
            sLog.outErrorDb("Transport %u '%s' have record (GUID: %u) in `gameobject`. Transports DON'T must have any records in `gameobject` or its behavior will be unpredictable/bugged.", entry, name.c_str(), guid);
        }
        while (result->NextRow());

        delete result;
    }
}

void Fleet::Scuttle()
{

    for (Transport* vessel : m_vessels)
    {
        vessel->WithdrawFromWorld();
    }

    for (Transport* vessel : m_vessels)
    {
        delete vessel;
    }

    m_vessels.clear();
    m_byMap.clear();
}

void Fleet::SettleCrossings()
{
    for (Transport* vessel : m_vessels)
    {
        if (vessel->IsCrossing())
        {
            vessel->CompleteCrossing();
        }
    }
}

Fleet::Vessels const& Fleet::On(uint32 mapId) const
{
    static const Vessels none;

    auto const called = m_byMap.find(mapId);

    return called == m_byMap.end() ? none : called->second;
}

Transport* Fleet::OnMapByGuid(uint32 mapId, ObjectGuid guid) const
{
    for (Transport* vessel : On(mapId))
    {
        if (vessel->GetObjectGuid() == guid)
        {
            return vessel;
        }
    }

    return nullptr;
}

Transport* Fleet::ByGuid(ObjectGuid guid) const
{
    for (Transport* vessel : m_vessels)
    {
        if (vessel->GetObjectGuid() == guid)
        {
            return vessel;
        }
    }

    return nullptr;
}

Transport* Fleet::ByLowGuid(uint32 lowGuid) const
{
    for (Transport* vessel : m_vessels)
    {
        if (vessel->GetGUIDLow() == lowGuid)
        {
            return vessel;
        }
    }

    return nullptr;
}
