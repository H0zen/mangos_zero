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

// When a thing placed in the world next comes or goes.
//
// The same moment on the clock means opposite things to the two kinds of spawn,
// and the database keeps both in one signed column. These pin down which way
// each of them reads, and that the column round-trips.

#include "doctest.h"

#include "SpawnClock.h"

TEST_CASE("spawn clock: a thing on no clock is simply there")
{
    SpawnClock clock;
    clock.Never();

    CHECK(clock.IsUp());
    CHECK(clock.Delay() == 0);
    CHECK(clock.Moment() == 0);

    // Nothing pending can take it away, because nothing is timing it.
    clock.ChangesAt(time(nullptr) + 500);
    CHECK(clock.IsUp());
}

TEST_CASE("spawn clock: a permanent spawn is there while nothing is pending")
{
    SpawnClock clock;
    clock.ComesBackAfter(300);

    CHECK(clock.IsPermanent());
    CHECK(clock.Delay() == 300);
    CHECK(clock.IsUp());                                    // standing there now

    // It has been taken, and is due back at that moment.
    clock.ChangesAt(time(nullptr) + 300);
    CHECK_FALSE(clock.IsUp());

    // And when the moment is cleared it is back.
    clock.ChangesAt(0);
    CHECK(clock.IsUp());
}

TEST_CASE("spawn clock: a fleeting spawn is there until its moment is cleared")
{
    SpawnClock clock;
    clock.GoesAwayAfter(300);

    CHECK_FALSE(clock.IsPermanent());
    CHECK(clock.Delay() == 300);

    // It reads the other way round: with nothing pending it is already gone.
    CHECK_FALSE(clock.IsUp());

    clock.ChangesAt(time(nullptr) + 300);
    CHECK(clock.IsUp());                                    // here until then
}

TEST_CASE("spawn clock: setting a wait sets the moment with it")
{
    SpawnClock clock;
    clock.ComesBackAfter(0);

    time_t const before = time(nullptr);
    clock.In(120);

    CHECK(clock.Delay() == 120);
    CHECK(clock.Moment() >= before + 120);
    CHECK(clock.Moment() <= time(nullptr) + 120);

    // Nothing at all takes it back off the clock.
    clock.In(0);
    CHECK(clock.Delay() == 0);
    CHECK(clock.Moment() == 0);
    CHECK(clock.IsUp());
}

TEST_CASE("spawn clock: the next moment it is up is now, when it already is")
{
    SpawnClock clock;
    clock.ComesBackAfter(300);

    time_t const now = time(nullptr);
    CHECK(clock.NextUp(now) == now);

    clock.ChangesAt(now + 300);
    CHECK(clock.NextUp(now) == now + 300);

    // A moment already past is no moment to wait for.
    clock.ChangesAt(now - 10);
    CHECK(clock.NextUp(now) == now);
}

TEST_CASE("spawn clock: the sign of the column is which kind it is")
{
    SpawnClock permanent;
    permanent.ComesBackAfter(300);
    CHECK(permanent.AsSpawnTimeSecs() == 300);

    SpawnClock fleeting;
    fleeting.GoesAwayAfter(300);
    CHECK(fleeting.AsSpawnTimeSecs() == -300);

    // And it comes back out of the column as what it went in as.
    SpawnClock read;

    read.FromSpawnTimeSecs(300);
    CHECK(read.IsPermanent());
    CHECK(read.Delay() == 300);
    CHECK(read.AsSpawnTimeSecs() == 300);

    read.FromSpawnTimeSecs(-300);
    CHECK_FALSE(read.IsPermanent());
    CHECK(read.Delay() == 300);
    CHECK(read.AsSpawnTimeSecs() == -300);

    // Zero is a permanent spawn on no clock, which is what the world's own are.
    read.FromSpawnTimeSecs(0);
    CHECK(read.IsPermanent());
    CHECK(read.Delay() == 0);
    CHECK(read.IsUp());
}
