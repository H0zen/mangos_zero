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

// How close you have to be to notice a trap that is hiding.
//
// The arithmetic is stealth's, applied to a thing that does not move: what you can
// see through decides whether you notice it at all, level decides how much sight
// you have left, and the radius it fires at is the floor -- a trap first seen from
// inside its own blast was never hidden, only invisible.

#include "doctest.h"

#include "TrapSight.h"
#include "Occupant.h"
#include "Unit.h"

namespace
{
    /// Someone who can see through the trap's invisibility, and nothing else.
    TrapWatcher Seer()
    {
        TrapWatcher watcher;
        watcher.invisibilityDetection = TRAP_SEEN_THROUGH;
        return watcher;
    }

    float const NARROW = 1.0f;                              // a trap that barely reaches
}

TEST_CASE("trap sight: without the detection nobody but a rogue finds it")
{
    TrapWatcher blind;
    blind.invisibilityDetection = TRAP_SEEN_THROUGH - 1;
    blind.isRogue = false;

    CHECK(TrapNoticedWithin(blind, NARROW) < 0.0f);

    // A rogue finds it, but only by walking up to it: the sight it would have had
    // is spent getting through the invisibility.
    blind.isRogue = true;
    CHECK(TrapNoticedWithin(blind, NARROW) == doctest::Approx(NARROW + INTERACTION_DISTANCE));
}

TEST_CASE("trap sight: seen through, it is noticed at its own range")
{
    CHECK(TrapNoticedWithin(Seer(), NARROW) == doctest::Approx(TRAP_SIGHT));
}

TEST_CASE("trap sight: an owner's level is sight taken off it")
{
    TrapWatcher watcher = Seer();
    watcher.hasOwner = true;
    watcher.ownerLevel = 60;

    // Five points a level, read back the way stealth writes it.
    CHECK(TrapNoticedWithin(watcher, NARROW) == doctest::Approx(TRAP_SIGHT - 3.0f));
}

TEST_CASE("trap sight: every level between them is a unit either way")
{
    TrapWatcher above = Seer();
    above.hasOwner = true;
    above.ownerLevel = 20;
    above.levelGap = 7;
    CHECK(TrapNoticedWithin(above, NARROW) == doctest::Approx(TRAP_SIGHT - 1.0f + 7.0f));

    TrapWatcher below = Seer();
    below.hasOwner = true;
    below.ownerLevel = 20;
    below.levelGap = -3;
    CHECK(TrapNoticedWithin(below, NARROW) == doctest::Approx(TRAP_SIGHT - 1.0f - 3.0f));

    // Far enough below and the arithmetic goes under the floor, which holds.
    below.levelGap = -7;
    CHECK(TrapNoticedWithin(below, NARROW) == doctest::Approx(NARROW + INTERACTION_DISTANCE));
}

TEST_CASE("trap sight: a trap nobody laid has no levels to compare")
{
    TrapWatcher wild = Seer();
    wild.hasOwner = false;
    wild.ownerLevel = 60;                                   // read by nothing while nobody owns it
    wild.levelGap = 40;

    CHECK(TrapNoticedWithin(wild, NARROW) == doctest::Approx(TRAP_SIGHT));
}

TEST_CASE("trap sight: five points of detection buy a unit, and paranoia sells one")
{
    TrapWatcher sharp = Seer();
    sharp.stealthDetect = 50;
    CHECK(TrapNoticedWithin(sharp, NARROW) == doctest::Approx(TRAP_SIGHT + 10.0f));

    TrapWatcher paranoid = Seer();
    paranoid.stealthDetect = -10;
    CHECK(TrapNoticedWithin(paranoid, NARROW) == doctest::Approx(TRAP_SIGHT - 2.0f));
}

TEST_CASE("trap sight: it is never noticed from inside its own blast")
{
    TrapWatcher watcher = Seer();
    watcher.hasOwner = true;
    watcher.ownerLevel = 60;

    // A wide trap: the arithmetic would put it well inside what it covers.
    CHECK(TrapNoticedWithin(watcher, 20.0f) == doctest::Approx(20.0f + INTERACTION_DISTANCE));

    // And the floor holds even when everything is against the watcher.
    watcher.levelGap = -60;
    watcher.stealthDetect = -500;
    CHECK(TrapNoticedWithin(watcher, 20.0f) == doctest::Approx(20.0f + INTERACTION_DISTANCE));
}

TEST_CASE("trap sight: and never from further than a player can pick anything out")
{
    TrapWatcher watcher = Seer();
    watcher.stealthDetect = 10000;

    CHECK(TrapNoticedWithin(watcher, NARROW) == doctest::Approx(MAX_PLAYER_STEALTH_DETECT_RANGE));

    // The ceiling wins over the floor: a trap wider than a player can see is read
    // at the ceiling, not at its radius.
    CHECK(TrapNoticedWithin(watcher, 200.0f) == doctest::Approx(MAX_PLAYER_STEALTH_DETECT_RANGE));
}
