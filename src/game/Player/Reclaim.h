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

#pragma once

#include "Platform/Define.h"
#include "Common/TimeConstants.h"

#include <ctime>

namespace reclaim
{

    constexpr uint32 LADDER[] = { 30, 60, 120 };
    constexpr uint32 RUNGS = 3;

    constexpr uint32 FORGETS_AFTER = 5 * MINUTE;

    struct Climbs
    {
        bool onPvP = false;
        bool onPvE = false;
    };

    inline bool Climbing(bool pvp, Climbs const& which)
    {
        return pvp ? which.onPvP : which.onPvE;
    }

    inline uint32 Rung(time_t now, time_t forgetsAt)
    {
        if (now >= forgetsAt)
        {
            return 0;
        }

        const uint32 rung = uint32((forgetsAt - now) / FORGETS_AFTER);

        return rung >= RUNGS ? RUNGS - 1 : rung;
    }

    inline uint32 Wait(uint32 rung)
    {
        return LADDER[rung >= RUNGS ? RUNGS - 1 : rung];
    }

    inline time_t Climbed(time_t now, time_t forgetsAt)
    {
        if (now >= forgetsAt)
        {
            return now + FORGETS_AFTER;
        }

        const uint32 rung = uint32((forgetsAt - now) / FORGETS_AFTER + 1);

        return now + (rung < RUNGS ? rung + 1 : RUNGS) * FORGETS_AFTER;
    }
}
