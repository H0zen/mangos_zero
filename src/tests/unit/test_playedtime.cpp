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

// The two totals a character carries: everything, and everything at this level.

#include "doctest.h"

#include "PlayedTime.h"

namespace
{
    time_t const NOON = 1000000;
}

TEST_CASE("played: a fresh character has been played for no time at all")
{
    PlayedTime clocks;
    clocks.Fresh(NOON);

    CHECK(clocks.Total() == 0);
    CHECK(clocks.AtThisLevel() == 0);
    CHECK(clocks.Mark() == NOON);
}

TEST_CASE("played: coming into the world sets the mark and the hour he logged in")
{
    PlayedTime clocks;
    clocks.StartAt(NOON);

    CHECK(clocks.LoggedInAt() == NOON);
    CHECK(clocks.Mark() == NOON);
}

TEST_CASE("played: both totals carry the same seconds forward")
{
    PlayedTime clocks;
    clocks.StartAt(NOON);
    clocks.Advance(NOON + 60);

    CHECK(clocks.Total() == 60);
    CHECK(clocks.AtThisLevel() == 60);
    CHECK(clocks.Mark() == NOON + 60);
}

TEST_CASE("played: carrying forward twice does not count the same seconds twice")
{
    PlayedTime clocks;
    clocks.StartAt(NOON);
    clocks.Advance(NOON + 60);
    clocks.Advance(NOON + 60);
    clocks.Advance(NOON + 100);

    CHECK(clocks.Total() == 100);
}

TEST_CASE("played: the hour he logged in does not move with the mark")
{
    PlayedTime clocks;
    clocks.StartAt(NOON);
    clocks.Advance(NOON + 3600);

    CHECK(clocks.LoggedInAt() == NOON);
    CHECK(clocks.Mark() == NOON + 3600);
}

TEST_CASE("played: a clock that has gone backwards carries nothing")
{
    PlayedTime clocks;
    clocks.StartAt(NOON);
    clocks.Advance(NOON - 500);

    CHECK(clocks.Total() == 0);
    CHECK(clocks.Mark() == NOON);
    CHECK(clocks.Since(NOON - 500) == 0);
}

TEST_CASE("played: a new level starts the second total again and leaves the first")
{
    PlayedTime clocks;
    clocks.StartAt(NOON);
    clocks.Advance(NOON + 500);
    clocks.NewLevel();
    clocks.Advance(NOON + 800);

    CHECK(clocks.Total() == 800);
    CHECK(clocks.AtThisLevel() == 300);
}

TEST_CASE("played: what a row holds is read back unchanged")
{
    PlayedTime clocks;
    clocks.Total(123456);
    clocks.AtThisLevel(7890);

    CHECK(clocks.Total() == 123456);
    CHECK(clocks.AtThisLevel() == 7890);
}
