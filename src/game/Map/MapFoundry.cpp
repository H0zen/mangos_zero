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

#include "MapFoundry.h"

#include "BattleGround/BattleGround.h"
#include "DBCStores.h"
#include "GridDefines.h"
#include "InstanceLedger.h"
#include "LivingWorld.h"
#include "Log.h"
#include "Map.h"
#include "MapPersistentStateMgr.h"
#include "MapRoster.h"
#include "ObjectMgr.h"
#include "TransportMap.h"
#include "Transports.h"
#include "World.h"

MapFoundry::MapFoundry()
    : m_gridCleanUpDelay(sWorld.getConfig(CONFIG_UINT32_INTERVAL_GRIDCLEAN))
{
}

void MapFoundry::SetGridCleanUpDelay(uint32 ms)
{
    m_gridCleanUpDelay = ms < MIN_GRID_DELAY ? MIN_GRID_DELAY : ms;
}

Map* MapFoundry::OpenWorld(uint32 mapId)
{
    // A deck belongs to its vessel and is cast with her in hand. Asked for by id alone, the
    // most that can be given is the one already open.
    if (Transport::IsVesselMapId(mapId))
    {
        return sMapRoster.Find(mapId);
    }

    std::lock_guard<std::mutex> bench(m_bench);

    return Shared(mapId, nullptr);
}

TransportMap* MapFoundry::OpenDeck(uint32 deckMapId, Transport& vessel)
{
    std::lock_guard<std::mutex> bench(m_bench);

    Map* deck = Shared(deckMapId, &vessel);

    return deck ? deck->AsTransport() : nullptr;
}

Map* MapFoundry::OpenFor(Player& player, uint32 mapId)
{
    const MapEntry* entry = sMapStore.LookupEntry(mapId);
    if (!entry)
    {
        return nullptr;
    }

    if (entry->Instanceable())
    {
        return sInstanceLedger.OpenFor(player, mapId);
    }

    return OpenWorld(mapId);
}

Map* MapFoundry::Shared(uint32 mapId, Transport* vessel)
{
    const MapEntry* entry = sMapStore.LookupEntry(mapId);
    if (!entry || entry->Instanceable())
    {
        return nullptr;
    }

    if (Map* open = sMapRoster.Find(mapId))
    {
        return open;
    }

    Map* cast = vessel ? static_cast<Map*>(new TransportMap(mapId, m_gridCleanUpDelay, vessel))
                       : static_cast<Map*>(new WorldMap(mapId, m_gridCleanUpDelay));

    sMapRoster.Enrol(MapKey(mapId), cast);

    sLivingWorld.PinActiveGrids(*cast);

    // A map everybody shares always has its saved state waiting for it.
    cast->CreateInstanceData(true);

    return cast;
}

DungeonMap* MapFoundry::CastDungeon(uint32 mapId, uint32 instanceId, DungeonPersistentState* save)
{
    const MapEntry* entry = sMapStore.LookupEntry(mapId);
    if (!entry)
    {
        sLog.outError("MapFoundry::CastDungeon: no Map.dbc row for map %u", mapId);
        MANGOS_ASSERT(false);
    }

    if (!ObjectMgr::GetInstanceTemplate(mapId))
    {
        sLog.outError("MapFoundry::CastDungeon: no instance template for map %u", mapId);
        MANGOS_ASSERT(false);
    }

    DEBUG_LOG("MapFoundry::CastDungeon: %scopy %u of map %u", save ? "" : "new ", instanceId, mapId);

    DungeonMap* cast = new DungeonMap(mapId, m_gridCleanUpDelay, instanceId);

    cast->CreateInstanceData(save != nullptr);

    return cast;
}

BattleGroundMap* MapFoundry::CastBattleGround(uint32 mapId, uint32 instanceId, BattleGround* bg)
{
    DEBUG_LOG("MapFoundry::CastBattleGround: copy %u of map %u for battleground type %u",
              instanceId, mapId, bg->GetTypeID());

    BattleGroundMap* cast = new BattleGroundMap(mapId, m_gridCleanUpDelay, instanceId);

    MANGOS_ASSERT(cast->IsBattleGround());

    cast->SetBG(bg);
    bg->SetBgMap(cast);

    // A battleground never outlives the match, so there is nothing saved to load.
    cast->CreateInstanceData(false);

    return cast;
}
