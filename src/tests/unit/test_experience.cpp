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

// What a kill is worth.
//
// The numbers below are the client's own: a level 60 greys at 51, a level 10
// player greys at 4, and the reward for an even fight at level 10 is 95. They
// are checked here as arithmetic, with the server's rate handed in.

#include "doctest.h"

#include "Stats/Experience.h"

TEST_CASE("grey: nothing greys before level six")
{
    CHECK(xp::GreyLevel(1) == 0);
    CHECK(xp::GreyLevel(5) == 0);

    // The band opens at six: 6 - 5 - 0 leaves level one behind it.
    CHECK(xp::GreyLevel(6) == 1);
}

TEST_CASE("grey: the band widens with level")
{
    CHECK(xp::GreyLevel(10) == 4);     // 10 - 5 - 1
    CHECK(xp::GreyLevel(20) == 13);    // 20 - 5 - 2
    CHECK(xp::GreyLevel(39) == 31);    // 39 - 5 - 3
}

TEST_CASE("grey: sixty is named on its own")
{
    // The curve above 39 would give 47; the game greys a sixty at 51.
    CHECK(xp::GreyLevel(60) == 51);
    CHECK(xp::GreyLevel(40) == 31);    // 40 - 1 - 8
}

TEST_CASE("colour: the five bands, from red to grey")
{
    CHECK(xp::ColourOf(30, 35) == xp::Colour::Red);
    CHECK(xp::ColourOf(30, 33) == xp::Colour::Orange);
    CHECK(xp::ColourOf(30, 30) == xp::Colour::Yellow);
    CHECK(xp::ColourOf(30, 28) == xp::Colour::Yellow);
    CHECK(xp::ColourOf(30, 27) == xp::Colour::Green);
    CHECK(xp::ColourOf(30, xp::GreyLevel(30)) == xp::Colour::Grey);
}

TEST_CASE("base: an even fight, and the gap that stops counting")
{
    // (10 * 5 + 45) * (20 + 0) / 10 + 1) / 2
    CHECK(xp::BaseFromKill(10, 10) == 95);

    // The gap is capped at four levels: a victim ten levels above pays the same as one four
    // levels above.
    CHECK(xp::BaseFromKill(10, 14) == xp::BaseFromKill(10, 20));
    CHECK(xp::BaseFromKill(10, 14) > xp::BaseFromKill(10, 10));
}

TEST_CASE("base: below the grey line a kill is worth nothing")
{
    const uint32 grey = xp::GreyLevel(30);

    CHECK(xp::BaseFromKill(30, grey) == 0);
    CHECK(xp::BaseFromKill(30, grey - 1) == 0);
    CHECK(xp::BaseFromKill(30, grey + 1) > 0);
}

TEST_CASE("kill: an elite is worth twice, and the rate is applied last")
{
    xp::Quarry ordinary;
    ordinary.level = 30;

    xp::Quarry elite = ordinary;
    elite.elite = true;

    const uint32 plain = xp::FromKill(30, ordinary, 1.0f);

    CHECK(xp::FromKill(30, elite, 1.0f) == plain * 2);
    CHECK(xp::FromKill(30, ordinary, 2.0f) == plain * 2);
    CHECK(xp::FromKill(30, elite, 2.0f) == plain * 4);
}

TEST_CASE("kill: a thing worth nothing stays worth nothing at any rate")
{
    xp::Quarry totem;
    totem.level = 60;
    totem.elite = true;
    totem.worthNothing = true;

    CHECK(xp::FromKill(10, totem, 100.0f) == 0);
}

TEST_CASE("kill: a grey victim pays nothing even to an elite")
{
    xp::Quarry grey;
    grey.level = xp::GreyLevel(40);
    grey.elite = true;

    CHECK(xp::FromKill(40, grey, 5.0f) == 0);
}

TEST_CASE("group: the share rises with the party and levels off at five")
{
    CHECK(xp::GroupShare(1, false) == doctest::Approx(1.0f));
    CHECK(xp::GroupShare(2, false) == doctest::Approx(1.0f));
    CHECK(xp::GroupShare(3, false) == doctest::Approx(1.166f));
    CHECK(xp::GroupShare(4, false) == doctest::Approx(1.3f));
    CHECK(xp::GroupShare(5, false) == doctest::Approx(1.4f));
    CHECK(xp::GroupShare(40, false) == doctest::Approx(1.4f));

    // A raid shares evenly.
    CHECK(xp::GroupShare(5, true) == doctest::Approx(1.0f));
}
