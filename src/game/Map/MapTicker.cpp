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

#include "MapTicker.h"

#include "GridDefines.h"
#include "Log.h"
#include "Map.h"
#include "MapRoster.h"
#include "Fleet.h"
#include "World.h"

#include <vector>

MapTicker::MapTicker()
{
    m_timer.SetInterval(sWorld.getConfig(CONFIG_UINT32_INTERVAL_MAPUPDATE));
}

void MapTicker::Start(uint32 threads)
{
    if (threads > 0 && m_pool.activate(threads) == -1)
    {
        abort();
    }
}

void MapTicker::Halt()
{
    if (m_pool.activated())
    {
        sLog.outString("[shutdown] MapTicker::Halt: retiring the map workers...");
        m_pool.deactivate();
        sLog.outString("[shutdown] MapTicker::Halt: map workers retired");
    }
}

void MapTicker::SetInterval(uint32 ms)
{

    if (ms > MIN_MAP_UPDATE_DELAY)
    {
        ms = MIN_MAP_UPDATE_DELAY;
    }

    m_timer.SetInterval(ms);
    m_timer.Reset();
}

void MapTicker::Run(uint32 diff)
{
    m_timer.Update(diff);
    if (!m_timer.Passed())
    {
        return;
    }

    const uint32 round = uint32(m_timer.GetCurrent());

    for (auto const& filed : sMapRoster.All())
    {
        Map* map = filed.second;

        if (map->AsTransport())
        {
            continue;
        }

        if (m_pool.activated())
        {
            m_pool.schedule_update(*map, round);
        }
        else
        {
            map->Update(round);
        }
    }

    if (m_pool.activated())
    {
        m_pool.wait();
    }

    sFleet.SettleCrossings();

    std::vector<MapKey> retiring;

    for (auto const& filed : sMapRoster.All())
    {
        Map* map = filed.second;

        if (map->AsTransport())
        {
            continue;
        }

        if (map->CanUnload(round))
        {
            retiring.push_back(filed.first);
        }
    }

    for (MapKey const& key : retiring)
    {
        sMapRoster.Retire(key);
    }

    m_timer.SetCurrent(0);
}
