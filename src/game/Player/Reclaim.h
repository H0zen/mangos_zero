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

/**
 * How long a ghost waits before it may take its body back.
 *
 * Thirty seconds the first time, a minute the second, two minutes the third and after: dying
 * again while the last death is still counted moves you a rung up a ladder of three. The
 * ladder forgets a rung every five minutes, so a careful hour resets it entirely.
 *
 * Whether dying to a player and dying to the world climb the same ladder is a server's
 * choice, and it is handed in.
 */
namespace reclaim
{
    /// The rungs, in seconds.
    constexpr uint32 LADDER[] = { 30, 60, 120 };
    constexpr uint32 RUNGS = 3;

    /// How long a rung is remembered.
    constexpr uint32 FORGETS_AFTER = 5 * MINUTE;

    /// Which deaths climb the ladder at all.
    struct Climbs
    {
        bool onPvP = false;
        bool onPvE = false;
    };

    inline bool Climbing(bool pvp, Climbs const& which)
    {
        return pvp ? which.onPvP : which.onPvE;
    }

    /// The rung a ghost is on, given when its ladder runs out. Nought once it has.
    inline uint32 Rung(time_t now, time_t forgetsAt)
    {
        if (now >= forgetsAt)
        {
            return 0;
        }

        const uint32 rung = uint32((forgetsAt - now) / FORGETS_AFTER);

        return rung >= RUNGS ? RUNGS - 1 : rung;
    }

    /// The wait itself.
    inline uint32 Wait(uint32 rung)
    {
        return LADDER[rung >= RUNGS ? RUNGS - 1 : rung];
    }

    /// When the ladder will be forgotten after one more death now.
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
