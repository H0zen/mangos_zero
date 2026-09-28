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

#ifndef MANGOS_H_NAV_DOORS
#define MANGOS_H_NAV_DOORS

#include "DetourNavMeshQuery.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace MMAP
{
    using BlockedPolys = std::unordered_map<dtPolyRef, int>;

    class DoorFilter : public dtQueryFilter
    {
        public:
            void SetBlocked(BlockedPolys const* blocked) { m_blocked = blocked; }

            bool passFilter(dtPolyRef ref, dtMeshTile const* tile, dtPoly const* poly) const override
            {
                return dtQueryFilter::passFilter(ref, tile, poly) && !(m_blocked && m_blocked->count(ref));
            }

        private:
            BlockedPolys const* m_blocked = nullptr;
    };

    class NavDoors
    {
        public:
            void Shut(uintptr_t door, dtNavMeshQuery const& query, float const* low, float const* high);
            void Open(uintptr_t door);

            BlockedPolys const& Blocked() const { return m_blocked; }

        private:
            std::unordered_map<uintptr_t, std::vector<dtPolyRef>> m_doors;
            BlockedPolys m_blocked;
    };
}

#endif
