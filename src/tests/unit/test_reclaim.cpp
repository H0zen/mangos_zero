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

// How long a ghost waits before it may take its body back.
//
// Thirty seconds, then a minute, then two: the ladder of a player who keeps
// dying. Every rung is forgotten five minutes after it was climbed.

#include "doctest.h"

#include "Reclaim.h"

TEST_CASE("ladder: which deaths climb it is the server's choice")
{
    reclaim::Climbs neither;
    CHECK_FALSE(reclaim::Climbing(true, neither));
    CHECK_FALSE(reclaim::Climbing(false, neither));

    reclaim::Climbs pvpOnly;
    pvpOnly.onPvP = true;
    CHECK(reclaim::Climbing(true, pvpOnly));
    CHECK_FALSE(reclaim::Climbing(false, pvpOnly));

    reclaim::Climbs both;
    both.onPvP = true;
    both.onPvE = true;
    CHECK(reclaim::Climbing(true, both));
    CHECK(reclaim::Climbing(false, both));
}

TEST_CASE("rung: a ghost with nothing remembered stands on the first")
{
    const time_t now = 1000000;

    CHECK(reclaim::Rung(now, 0) == 0);
    CHECK(reclaim::Rung(now, now) == 0);
    CHECK(reclaim::Rung(now, now - 1) == 0);
}

TEST_CASE("rung: one for every five minutes still remembered")
{
    const time_t now = 1000000;
    const uint32 step = reclaim::FORGETS_AFTER;

    CHECK(reclaim::Rung(now, now + step - 1) == 0);
    CHECK(reclaim::Rung(now, now + step) == 1);
    CHECK(reclaim::Rung(now, now + 2 * step) == 2);
}

TEST_CASE("rung: the ladder has three rungs and no more")
{
    const time_t now = 1000000;
    const uint32 step = reclaim::FORGETS_AFTER;

    CHECK(reclaim::Rung(now, now + 3 * step) == 2);
    CHECK(reclaim::Rung(now, now + 100 * step) == 2);
}

TEST_CASE("wait: thirty seconds, a minute, two minutes")
{
    CHECK(reclaim::Wait(0) == 30);
    CHECK(reclaim::Wait(1) == 60);
    CHECK(reclaim::Wait(2) == 120);

    // Asking beyond the top rung answers with the top rung.
    CHECK(reclaim::Wait(99) == 120);
}

TEST_CASE("climbing: a first death is remembered for five minutes")
{
    const time_t now = 1000000;

    CHECK(reclaim::Climbed(now, 0) == now + reclaim::FORGETS_AFTER);
    CHECK(reclaim::Climbed(now, now) == now + reclaim::FORGETS_AFTER);
}

TEST_CASE("climbing: dying again while it is remembered goes a rung up")
{
    const time_t now = 1000000;
    const uint32 step = reclaim::FORGETS_AFTER;

    // Standing on the first rung, a second death sets the second.
    CHECK(reclaim::Climbed(now, now + step) == now + 3 * step);

    // And the ladder stops at three rungs however often he dies.
    CHECK(reclaim::Climbed(now, now + 3 * step) == now + 3 * step);
    CHECK(reclaim::Climbed(now, now + 10 * step) == now + 3 * step);
}

TEST_CASE("the ladder as a player would feel it")
{
    const time_t now = 1000000;
    time_t remembered = 0;

    // First death: no wait beyond the first rung.
    CHECK(reclaim::Wait(reclaim::Rung(now, remembered)) == 30);
    remembered = reclaim::Climbed(now, remembered);

    // Second, straight away: a minute.
    CHECK(reclaim::Wait(reclaim::Rung(now, remembered)) == 60);
    remembered = reclaim::Climbed(now, remembered);

    // Third: two minutes, and it stays there.
    CHECK(reclaim::Wait(reclaim::Rung(now, remembered)) == 120);
    remembered = reclaim::Climbed(now, remembered);
    CHECK(reclaim::Wait(reclaim::Rung(now, remembered)) == 120);

    // A quarter of an hour later the whole ladder has been forgotten.
    const time_t later = now + 15 * MINUTE + 1;
    CHECK(reclaim::Wait(reclaim::Rung(later, remembered)) == 30);
}
