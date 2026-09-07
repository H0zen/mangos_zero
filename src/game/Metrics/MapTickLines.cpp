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

#include "Metrics/MapTickLines.h"

#include "Map.h"
#include "MapRoster.h"

#include <cstdio>

namespace
{
    /// A map is worth a line once its worst regular tick is past this. Below it there is
    /// nothing to say and plenty of maps to say it about.
    const uint32 INTERESTING_MS = 5;
}

std::string metrics::MapTickLines()
{
    std::string out;
    char buf[128];

    for (auto const& filed : sMapRoster.All())
    {
        Map const* map = filed.second;
        if (!map || map->Ticks().Samples() == 0)
        {
            continue;
        }

        const uint32 p99 = map->Ticks().Ms(0.99f);
        if (p99 < INTERESTING_MS && map->Ticks().Overruns() == 0)
        {
            continue;
        }

        uint32 worstMs = 0;
        const metrics::TickPhase worst = map->Ticks().Phases().Worst(worstMs);

        std::snprintf(buf, sizeof(buf),
                      "  map %u tick p50/p99/max %u/%u/%u ms over %u [%s %u ms] active %u",
                      map->GetId(), map->Ticks().Ms(0.5f), p99, map->Ticks().MsMax(),
                      map->Ticks().Overruns(), metrics::PhaseName(worst), worstMs,
                      uint32(map->ActiveObjectCount()));
        out += buf;
    }

    return out;
}
