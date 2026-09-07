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
    // instance id and phaseMask isn't set to values different from std.

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

    // Forced, not taken from `gameobject_template`: a vessel is a transport and never
    // despawns, whatever the row happens to say.
    SetAllGoFlags((GO_FLAG_TRANSPORT | GO_FLAG_NODESPAWN));

    // THE ROUTE'S PERIOD. The lap is set from the route before this runs, and the client
    // works the same number out for itself from the same DBC rows.
    SetUInt32Value(GAMEOBJECT_LEVEL, m_period);

    SetEntry(goinfo->id);
    SetUInt32Value(GAMEOBJECT_DISPLAYID, goinfo->displayId);
    SetGoState(GO_STATE_READY);
    SetGoArtKit(0);
    SetGoAnimProgress(animprogress);
    SetName(goinfo->name);

    // THE VESSEL IS A MAP. Blizzard gave her a Map.dbc row and no terrain for it; the baker
    // fills that in from the hull's own model, so from here she answers height, collision and
    // routing through the ordinary engines. Model space is that map's space, which is why
    // nothing in the chain applies a transform.
    if (const uint32 mapId = VesselMapIdOf(goinfo->id))
    {
        m_map = sMapFoundry.OpenDeck(mapId, *this);

        // A map that could not be commissioned is kept all the same: it is still the relay,
        // and it is what refuses to take anyone aboard.
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
    // THE ROUTE'S GRIDS, LOADED AT START-UP AND NEVER LET GO. The vessel is an active object
    // in them, so what the relay finds ashore is whatever those cells hold -- and a cell that
    // had expired holds nothing, silently, and only once she was already sailing past it.
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
    /// Resolved once per vessel entry and then authoritative: the store it is derived from
    /// is MUTATED below (minted rows are injected into it), so re-deriving would see a
    /// different world each time.
    std::unordered_map<uint32, uint32> s_vesselMapByEntry;
    std::unordered_set<uint32> s_vesselMapIds;
}

void Transport::RegisterVesselMap(uint32 goEntry, char const* vesselName)
{
    if (s_vesselMapByEntry.find(goEntry) != s_vesselMapByEntry.end())
    {
        return;
    }

    // EVERY VESSEL MAP HERE IS MINTED. Blizzard keys a hull's map by its directory string,
    // "Transport<entry>" -- and 1.12's Map.dbc format string skips that field, so the
    // server never loads it and there is nothing to match against. The hull is in the
    // client all the same, so it gets a map of its own: an id minted here and a Map.dbc
    // entry injected to carry it. Nothing on the wire ever carries this number.
    //
    // The baker must agree, which is what tools/vessels.txt is for -- every line in it is
    // a minted id for the same reason.
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
    // DERIVED, NEVER STORED. Being aboard is not a fact anyone records: it is what having
    // this map means. A creature summoned at sea, a crew member read from `creature`, a
    // player who walked up the gangplank -- all the same answer, from the same question,
    // with nothing to keep in step.
    //
    // A lift passenger and a vehicle rider are NOT here: their map is the world's, and the
    // seat transform is the vehicle system's business, not this one's.
    Map* map = obj.GetMap();
    TransportMap* hull = map ? map->AsTransport() : nullptr;
    return hull ? hull->Vessel() : nullptr;
}

void Transport::WithdrawFromWorld()
{
    // Guarded because both teardown paths call it, and the second runs after the maps have
    // been deleted -- GetMap() would then point at freed memory.
    if (m_withdrawn)
    {
        return;
    }
    m_withdrawn = true;
    m_crossing = false;

    // Nothing else does this. A vessel is in no cell, so no grid unload reaches it, and the
    // map that owns it is deleted before the vessel is -- ~Object then asserts on an object
    // still flagged in-world, against a map that no longer exists.
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

/**
 * @brief Teleports the transport and its player passengers to another map position.
 *
 * @param newMapid The destination map id.
 * @param x The destination X coordinate.
 * @param y The destination Y coordinate.
 * @param z The destination Z coordinate.
 */
void Transport::TeleportTransport(uint32 newMapid, float x, float y, float z)
{
    Map* oldMap = GetMap();

    // The route decided WHEN; what crossing means for anyone standing on her is not this
    // object's business and never was. Her own map is told and owns every consequence.
    Place().MoveTo(x, y, z);

    // A node flagged for teleport that does not leave this map: nothing changes for anyone.
    // Her passengers' coordinates are her own map's and do not move, and the client draws
    // the jump itself out of the path progress.
    if (!oldMap || oldMap->GetId() == newMapid)
    {
        return;
    }

    // AND NOT ONE STEP FURTHER ON THIS THREAD. Crossing writes into another map's active
    // list, object store and player list, and that map may be running right now on another
    // core. Worse, half a crossing is a vessel that reports a map she is no longer on: the
    // passengers arriving from her own map were handed the OTHER continent's ships, and their
    // clients then tried to sail them along paths that do not exist there.
    //
    // So the route only says GO. All of it happens at once, at the barrier.
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

    // THIS SIDE FIRST, while she is still on it: the shore loses her, and her passengers are
    // started on their way. The transfer packet they get names the map they are leaving,
    // which one line later would already be the map they are going to.
    if (m_map)
    {
        // The node's own coordinates, not the pose. Same number the client's path is built
        // from, so nobody is put down anywhere the ship is not.
        m_map->VesselLeavingWorld(oldMap, newMapid, m_crossingX, m_crossingY, m_crossingZ,
                                  Where().Facing());
    }

    // Off the old grid properly: a game object left in a cell of a map it is no longer on is
    // a dangling entry the next visit of that cell walks straight into.
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

/**
 * @brief Updates global transport position along its generated path.
 *
 * @param update_diff The elapsed update time.
 * @param p_time The current path time parameter.
 */
void Transport::Update(uint32 update_diff, uint32 /*p_time*/)
{
    // Between two world maps: she belongs to neither until the barrier hands her over, and
    // her own map does not tick without her.
    if (m_crossing)
    {
        return;
    }

    // The route clock, and nothing else. This vessel is never redrawn, never repositioned
    // and never composed with anything: her pose is advanced only so that the cell sweep in
    // her own map's tick knows WHICH GRID of the world to look in for the objects ashore.
    // What the client is sent is the path progress below and her entry, and it draws her
    // itself, from an animation the server does not have.
    if (m_period != 0)
    {
        // Absolute wall-clock, NOT milliseconds since this process started. The phase of a
        // route has to survive a restart: keyed off uptime, every vessel on the server sails
        // from the beginning of its path each time we come up, while the client -- which
        // interpolates the hull from the value we hand it -- draws her somewhere else
        // entirely, and the two then argue about a ship neither has.
        const uint32 mapBefore = GetMapId();

        m_timer = uint32(GameTime::GetAbsoluteTimeMS() % m_period);

        // THE TIME OF THE TRANSFER, and the one thing the route has to decide. The leg she
        // is on at this moment names the map she sails; when that changes she moves, and
        // everyone aboard follows. It is read off the same lap the client works out for
        // itself, so the hull the player sees arrives when we say it does.
        if (VesselLeg const* leg = m_route.LegAt(m_timer))
        {
            if (leg->mapId != GetMapId())
            {
                Geometry::Vector3 const berth = leg->From();
                TeleportTransport(leg->mapId, berth.x, berth.y, berth.z);
                return;
            }
        }

        // The pose, off the same route and the same clock: where she is between two nodes,
        // not the last one she went past. It is still only a grid hint -- nothing is drawn
        // from it and nothing is composed with it -- but it is now a hint that is right.
        VesselPose const pose = m_route.PoseAt(m_timer);
        if (pose.known && pose.mapId == GetMapId())
        {
            Place().MoveTo(pose.at.x, pose.at.y, pose.at.z);
        }

        DETAIL_FILTER_LOG(LOG_FILTER_TRANSPORT_MOVES, "%s at %f %f %f on map %u",
                          GetName(), pose.at.x, pose.at.y, pose.at.z, pose.mapId);

        // A seam moved us, and everything below belongs to the new map's tick.
        if (GetMapId() != mapBefore)
        {
            return;
        }
    }

    // LAST, AND IT MUST STAY LAST. The ship's own map runs nested inside this tick, on the
    // thread of the world map she sails and after that map has finished walking its own
    // containers -- which is what lets everything it sends go straight out, with no queue
    // and no tick of latency.
    if (m_map)
    {
        m_map->Update(update_diff);
    }
}
