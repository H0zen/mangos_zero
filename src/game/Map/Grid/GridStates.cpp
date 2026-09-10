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

#include "GridStates.h"
#include "ObjectGridLoader.h"
#include "Log.h"
#include "World.h"

GridState::~GridState()
{
    DEBUG_LOG("GridState destroyed");
}

GridState const& GridStateFor(grid_state_t state)
{
    static const InvalidState invalid;
    static const ActiveState active;
    static const IdleState idle;
    static const RemovalState removal;

    switch (state)
    {
        case GRID_STATE_ACTIVE:
            return active;
        case GRID_STATE_IDLE:
            return idle;
        case GRID_STATE_REMOVAL:
            return removal;
        default:
            return invalid;
    }
}

void
InvalidState::Update(Map&, NGridType&, GridInfo&, const uint32& , const uint32& , const uint32&) const
{
}

void
ActiveState::Update(Map& m, NGridType& grid, GridInfo& info, const uint32& x, const uint32& y, const uint32& t_diff) const
{

    info.UpdateTimeTracker(t_diff);
    if (info.getTimeTracker().Passed())
    {
        if (grid.ActiveObjectsInGrid() == 0 && !m.ActiveObjectsNearGrid(x, y))
        {
            ObjectGridStoper stoper(grid);
            stoper.StopN();
            grid.SetGridState(GRID_STATE_IDLE);
        }
        else
        {
            m.ResetGridExpiry(grid, 0.1f);
        }
    }
}

void
IdleState::Update(Map& m, NGridType& grid, GridInfo&, const uint32& x, const uint32& y, const uint32&) const
{
    m.ResetGridExpiry(grid);
    grid.SetGridState(GRID_STATE_REMOVAL);
    DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "Grid[%u,%u] on map %u moved to REMOVAL state", x, y, m.GetId());
}

void
RemovalState::Update(Map& m, NGridType& grid, GridInfo& info, const uint32& x, const uint32& y, const uint32& t_diff) const
{
    if (!info.getUnloadLock())
    {
        info.UpdateTimeTracker(t_diff);
        if (info.getTimeTracker().Passed())
        {
            if (!m.UnloadGrid(x, y, false))
            {
                DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "Grid[%u,%u] for map %u differed unloading due to players or active objects nearby", x, y, m.GetId());
                m.ResetGridExpiry(grid);
            }
        }
    }
}
