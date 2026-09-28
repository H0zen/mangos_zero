/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
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

#include "NavDoors.h"

namespace MMAP
{
    namespace
    {
        constexpr int MAX_DOOR_POLYS = 64;
    }

    void NavDoors::Shut(uintptr_t door, dtNavMeshQuery const& query, float const* low, float const* high)
    {
        Open(door);

        float centre[3];
        float half[3];
        for (int axis = 0; axis < 3; ++axis)
        {
            centre[axis] = (low[axis] + high[axis]) * 0.5f;
            half[axis] = (high[axis] - low[axis]) * 0.5f;
        }

        dtQueryFilter const everything;
        dtPolyRef found[MAX_DOOR_POLYS];
        int count = 0;
        if (dtStatusFailed(query.queryPolygons(centre, half, &everything, found, &count, MAX_DOOR_POLYS)) || count <= 0)
        {
            return;
        }

        std::vector<dtPolyRef>& polys = m_doors[door];
        polys.assign(found, found + count);
        for (dtPolyRef ref : polys)
        {
            ++m_blocked[ref];
        }
    }

    void NavDoors::Open(uintptr_t door)
    {
        auto const shut = m_doors.find(door);
        if (shut == m_doors.end())
        {
            return;
        }

        for (dtPolyRef ref : shut->second)
        {
            auto const blocked = m_blocked.find(ref);
            if (blocked != m_blocked.end() && --blocked->second <= 0)
            {
                m_blocked.erase(blocked);
            }
        }
        m_doors.erase(shut);
    }
}
