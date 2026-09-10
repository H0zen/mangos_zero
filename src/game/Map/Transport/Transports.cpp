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

#include "Platform/Define.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>

#include "Transports.h"
#include "VesselRoute.h"
#include "TransportMap.h"
#include "Map.h"
#include "Fleet.h"
#include "MapFoundry.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Path.h"
#include "GameTime.h"
#include "terrain/TileSerializer.hpp"
#include <list>

#include "DBCStores.h"
#include "ProgressBar.h"
#include "ScriptMgr.h"

Transport::Transport() : GameObject()
{
    m_updateFlag = (UPDATEFLAG_TRANSPORT | UPDATEFLAG_ALL | UPDATEFLAG_HAS_POSITION);
}

bool Transport::Create(uint32 guidlow, uint32 mapid, float x, float y, float z, float ang, uint8 animprogress)
{
    Place().MoveTo(x, y, z, ang);

    if (!IsPlaceable(*this))
    {
        sLog.outError("Transport (GUID: %u) not created. Suggested coordinates isn't valid (X: %f Y: %f)",
                      guidlow, x, y);
        return false;
    }

    Object::_Create(guidlow, 0, HIGHGUID_MO_TRANSPORT);

    GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(guidlow);

    if (!goinfo)
    {
        sLog.outErrorDb("Transport not created: entry in `gameobject_template` not found, guidlow: %u map: %u  (X: %f Y: %f Z: %f) ang: %f", guidlow, mapid, x, y, z, ang);
        return false;
    }

    SetGOInfo(goinfo);

    SetObjectScale(goinfo->size);

    SetGoType(GAMEOBJECT_TYPE_MO_TRANSPORT);
    SetUInt32Value(GAMEOBJECT_FACTION, goinfo->faction);

    SetAllGoFlags((GO_FLAG_TRANSPORT | GO_FLAG_NODESPAWN));

    SetUInt32Value(GAMEOBJECT_LEVEL, m_period);

    SetEntry(goinfo->id);
    SetUInt32Value(GAMEOBJECT_DISPLAYID, goinfo->displayId);
    SetGoState(GO_STATE_READY);
    SetGoArtKit(0);
    SetGoAnimProgress(animprogress);
    SetName(goinfo->name);

    if (const uint32 mapId = VesselMapIdOf(goinfo->id))
    {
        m_map = sMapFoundry.OpenDeck(mapId, *this);

        if (m_map)
        {
            m_map->Commission();
        }
    }
    else
    {
        sLog.outErrorDb("Transport %u (%s, display %u) has no map of its own.",
                        goinfo->id, goinfo->name, goinfo->displayId);
    }

    return true;
}

void Transport::PinRouteGrids()
{

    uint32 pinned = 0;
    for (VesselLeg const& leg : m_route.Legs())
    {
        Map* sailed = sMapFoundry.OpenWorld(leg.mapId);
        if (!sailed)
        {
            continue;
        }

        for (Geometry::Vector3 const& node : leg.nodes)
        {
            sailed->ForceLoadGrid(node.x, node.y);
            ++pinned;
        }
    }

    DETAIL_LOG("Transport %u '%s': %u route node(s) pinned.", GetEntry(), GetName(), pinned);
}

Transport* Transport::GetTransport(Map const* map, ObjectGuid guid)
{
    if (!map || !guid)
    {
        return nullptr;
    }

    return sFleet.OnMapByGuid(map->GetId(), guid);
}

namespace
{

    std::unordered_map<uint32, uint32> s_vesselMapByEntry;
    std::unordered_set<uint32> s_vesselMapIds;
}

void Transport::RegisterVesselMap(uint32 goEntry, char const* vesselName)
{
    if (s_vesselMapByEntry.find(goEntry) != s_vesselMapByEntry.end())
    {
        return;
    }

    const uint32 minted = world::terrain::MintedVesselMapId(goEntry);

    MapEntry* row = new MapEntry();
    row->MapID = minted;
    row->InstanceType = MAP_COMMON;
    row->AreaTableID = 0;
    row->LoadingScreenID = 0;

    static std::list<std::string> s_names;
    s_names.push_back(vesselName ? vesselName : "Vessel");
    for (uint32 i = 0; i < 8; ++i)
    {
        row->MapName_lang[i] = const_cast<char*>(s_names.back().c_str());
    }

    sMapStore.SetEntry(minted, row);

    s_vesselMapByEntry[goEntry] = minted;
    s_vesselMapIds.insert(minted);

    DETAIL_LOG("Transport %u '%s' has no Map.dbc row; map %u minted.", goEntry,
               vesselName ? vesselName : "", minted);
}

uint32 Transport::VesselMapIdOf(uint32 goEntry)
{
    const auto found = s_vesselMapByEntry.find(goEntry);
    return found != s_vesselMapByEntry.end() ? found->second : 0;
}

bool Transport::IsVesselMapId(uint32 mapId)
{
    return s_vesselMapIds.find(mapId) != s_vesselMapIds.end();
}

Transport* Transport::VesselOf(Occupant const& obj)
{

    Map* map = obj.GetMap();
    TransportMap* hull = map ? map->AsTransport() : nullptr;
    return hull ? hull->Vessel() : nullptr;
}

void Transport::WithdrawFromWorld()
{

    if (m_withdrawn)
    {
        return;
    }
    m_withdrawn = true;
    m_crossing = false;

    if (m_map)
    {
        m_map->ReleaseCrew();
    }

    if (Map* sailed = GetMap())
    {
        sailed->RemoveFromActive(this);
    }

    if (IsInWorld())
    {
        RemoveFromWorld();
    }

    m_map = nullptr;
}

void Transport::TeleportTransport(uint32 newMapid, float x, float y, float z)
{
    Map* oldMap = GetMap();

    Place().MoveTo(x, y, z);

    if (!oldMap || oldMap->GetId() == newMapid)
    {
        return;
    }

    m_crossingTo = newMapid;
    m_crossingX = x;
    m_crossingY = y;
    m_crossingZ = z;
    m_crossing = true;
}

void Transport::CompleteCrossing()
{
    if (!m_crossing)
    {
        return;
    }

    m_crossing = false;

    const uint32 newMapid = m_crossingTo;
    m_crossingTo = 0;

    Map* oldMap = GetMap();
    Map* newMap = sMapFoundry.OpenWorld(newMapid);
    if (!oldMap || !newMap || oldMap == newMap)
    {
        return;
    }

    if (m_map)
    {

        m_map->VesselLeavingWorld(oldMap, newMapid, m_crossingX, m_crossingY, m_crossingZ,
                                  Where().Facing());
    }

    oldMap->RemoveFromActive(this);
    RemoveFromWorld();

    SetMap(newMap);

    AddToWorld();
    newMap->AddToActive(this);

    if (m_map)
    {
        m_map->VesselEnteredWorld(newMap);
    }
}

void Transport::Update(uint32 update_diff, uint32 )
{

    if (m_crossing)
    {
        return;
    }

    if (m_period != 0)
    {

        const uint32 mapBefore = GetMapId();

        m_timer = uint32(GameTime::GetAbsoluteTimeMS() % m_period);

        if (VesselLeg const* leg = m_route.LegAt(m_timer))
        {
            if (leg->mapId != GetMapId())
            {
                Geometry::Vector3 const berth = leg->From();
                TeleportTransport(leg->mapId, berth.x, berth.y, berth.z);
                return;
            }
        }

        VesselPose const pose = m_route.PoseAt(m_timer);
        if (pose.known && pose.mapId == GetMapId())
        {
            Place().MoveTo(pose.at.x, pose.at.y, pose.at.z);
        }

        DETAIL_FILTER_LOG(LOG_FILTER_TRANSPORT_MOVES, "%s at %f %f %f on map %u",
                          GetName(), pose.at.x, pose.at.y, pose.at.z, pose.mapId);

        if (GetMapId() != mapBefore)
        {
            return;
        }
    }

    if (m_map)
    {
        m_map->Update(update_diff);
    }
}
