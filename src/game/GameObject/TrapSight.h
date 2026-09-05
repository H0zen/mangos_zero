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

/**
 * How close somebody has to be to notice a trap that is trying not to be noticed.
 *
 * A hidden trap is not read at the map's ordinary sight range. It has a range of
 * its own, worked out per watcher from the same pieces stealth on a unit is: what
 * the watcher can see through, how the two of them compare in level, and what the
 * trap covers -- because a trap nobody can see until they are inside its radius
 * is a trap that always fires, which is not what hiding it was for.
 *
 * This is the arithmetic alone. Whether the trap is hiding from this particular
 * watcher at all -- whether it is stealthed, and whose side its owner is on -- is
 * decided before asking.
 */

/// What one watcher brings to the question.
struct TrapWatcher
{
    /// Only a rogue can find a trap they cannot see through.
    bool isRogue = false;

    /// SPELL_AURA_MOD_INVISIBILITY_DETECTION against invisibility type 8.
    int32 invisibilityDetection = 0;

    /// SPELL_AURA_MOD_STEALTH_DETECT, which paranoia makes negative.
    int32 stealthDetect = 0;

    /// A trap laid by nobody hides on its own terms, with no levels to compare.
    bool hasOwner = false;

    /// The level the owner really is.
    uint32 ownerLevel = 0;

    /// The watcher's level as the owner reads it, less the owner's as the watcher reads it.
    int32 levelGap = 0;
};

/// Sight of a hidden trap at rank 4 stealth, before anything is taken off it.
float const TRAP_SIGHT = 10.5f;

/// Invisibility detection below this leaves the trap unseen by anyone but a rogue.
int32 const TRAP_SEEN_THROUGH = 200;

/**
 * @brief How close the watcher must come before the trap is noticed.
 *
 * @param watcher What the watcher brings.
 * @param trapRadius How far from the trap it fires.
 * @return The distance, never less than the radius it fires at plus arm's length,
 *         and never more than a player can pick anything out at. A negative answer
 *         is a trap this watcher never notices, however close they come.
 */
float TrapNoticedWithin(TrapWatcher const& watcher, float trapRadius);
