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

#include "LivingWorld.h"

#include "GridDefines.h"
#include "Log.h"
#include "Map.h"
#include "MapFoundry.h"
#include "ObjectMgr.h"
#include "World.h"

#include <set>
#include <utility>

namespace
{
    /// The three maps that are open before anyone logs in: the two continents and the tram.
    const uint32 s_continents[] = { 0, 1, 369 };
}

LivingWorld::Awakening LivingWorld::Awaken()
{
    m_tally = Awakening();
    m_awakening = true;

    for (const uint32 continent : s_continents)
    {
        if (!sMapFoundry.OpenWorld(continent))
        {
            sLog.outError("LivingWorld::Awaken() - unable to open map %u", continent);
        }
    }

    m_awakening = false;

    return m_tally;
}

void LivingWorld::PinActiveGrids(Map& map)
{
    const bool forceLoad = sWorld.isForceLoadMap(map.GetId());

    uint32 requests = 0;
    uint32 newlyLoaded = 0;
    uint32 alreadyLoaded = 0;
    std::set<std::pair<uint32, uint32>> grids;

    // Counted on its own: on a force-loaded map these are pinned anyway as part of every
    // spawn, and the number is still what says how much of the map has to stay awake.
    uint32 activeCreatures = 0;
    auto const active = sObjectMgr.GetActiveCreatureGuids()->equal_range(map.GetId());
    for (auto itr = active.first; itr != active.second; ++itr)
    {
        ++activeCreatures;
    }

    auto const pin = [&](float x, float y)
    {
        ++requests;

        const GridPair grid = MaNGOS::ComputeGridPair(x, y);
        grids.insert(std::make_pair(grid.x_coord, grid.y_coord));

        if (map.IsLoaded(x, y))
        {
            ++alreadyLoaded;
        }
        else
        {
            ++newlyLoaded;
        }

        map.ForceLoadGrid(x, y);
    };

    if (forceLoad)
    {
        for (auto const& spawn : *sObjectMgr.GetCreatureDataMap())
        {
            if (spawn.second.mapid == map.GetId())
            {
                pin(spawn.second.posX, spawn.second.posY);
            }
        }
    }
    else
    {
        for (auto itr = active.first; itr != active.second; ++itr)
        {
            if (CreatureData const* data = sObjectMgr.GetCreatureData(itr->second))
            {
                pin(data->posX, data->posY);
            }
        }
    }

    if (!m_awakening)
    {
        return;
    }

    m_tally.grids += uint32(grids.size());
    m_tally.newlyLoaded += newlyLoaded;
    if (forceLoad)
    {
        ++m_tally.forcedMaps;
    }

    if (forceLoad)
    {
        sLog.outString("[LivingWorld] map %u: force-load=ON, creature-rows=%u, ForceLoadGrid-requests=%u, unique-grids=%u, newly-loaded=%u (explicit-locks-set), already-loaded=%u, extra-active-creatures=%u",
                       map.GetId(), requests, requests, uint32(grids.size()), newlyLoaded, alreadyLoaded, activeCreatures);
    }
    else
    {
        sLog.outString("[LivingWorld] map %u: force-load=OFF, extra-active-creatures=%u, ForceLoadGrid-requests=%u, unique-grids=%u, newly-loaded=%u (explicit-locks-set), already-loaded=%u",
                       map.GetId(), activeCreatures, requests, uint32(grids.size()), newlyLoaded, alreadyLoaded);
    }
}
