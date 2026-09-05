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

#include "TrapSight.h"

#include "Occupant.h"
#include "Unit.h"

float TrapNoticedWithin(TrapWatcher const& watcher, float trapRadius)
{
    float noticed = TRAP_SIGHT;

    if (watcher.invisibilityDetection < TRAP_SEEN_THROUGH)
    {
        // Nobody else finds it at all. A rogue does, but only by walking into it.
        if (!watcher.isRogue)
        {
            return -1.0f;
        }

        noticed = 0.0f;
    }

    if (watcher.hasOwner)
    {
        // Rank 4 stealth is five points a level, and this reads as its counterpart.
        noticed -= watcher.ownerLevel / 20.0f;

        // Every level between them is a unit of sight either way.
        noticed += watcher.levelGap;
    }

    // Five points of detection buy a unit; paranoia spends them the other way.
    noticed += watcher.stealthDetect / 5.0f;

    if (noticed > MAX_PLAYER_STEALTH_DETECT_RANGE)
    {
        return MAX_PLAYER_STEALTH_DETECT_RANGE;
    }

    // NEVER INSIDE ITS OWN RADIUS. A trap first seen from where it has already gone
    // off is a trap that was never hidden, only invisible.
    float const floor = trapRadius + INTERACTION_DISTANCE;

    return noticed < floor ? floor : noticed;
}
