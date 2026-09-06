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

// The arithmetic of rest: what an hour in an inn is worth, and how much may be held.

#include "doctest.h"

#include "Rest.h"

namespace
{
    uint32 const A_LEVEL = 40000;           // a plausible next-level cost
    time_t const EIGHT_HOURS = 8 * 60 * 60;
    time_t const A_HUNDRED_AND_SIXTY_HOURS = 20 * EIGHT_HOURS;

    /// What the client draws is twice what is kept.
    float Drawn(float kept) { return kept * 2.0f; }
}

TEST_CASE("rest: eight hours in an inn is worth one bubble")
{
    float const kept = rest::Gained(A_LEVEL, EIGHT_HOURS, 1.0f);

    CHECK(Drawn(kept) == doctest::Approx(A_LEVEL / 20.0f));
}

TEST_CASE("rest: twenty bubbles is a whole level and takes a hundred and sixty hours")
{
    float const kept = rest::Gained(A_LEVEL, A_HUNDRED_AND_SIXTY_HOURS, 1.0f);

    CHECK(Drawn(kept) == doctest::Approx(float(A_LEVEL)));
}

TEST_CASE("rest: no time in an inn is worth nothing")
{
    CHECK(rest::Gained(A_LEVEL, 0, 1.0f) == doctest::Approx(0.0f));
}

TEST_CASE("rest: what accrues is proportional to the time and to the rate")
{
    float const one = rest::Gained(A_LEVEL, EIGHT_HOURS, 1.0f);

    CHECK(rest::Gained(A_LEVEL, 2 * EIGHT_HOURS, 1.0f) == doctest::Approx(2 * one));
    CHECK(rest::Gained(A_LEVEL, EIGHT_HOURS, 3.0f) == doctest::Approx(3 * one));
    CHECK(rest::Gained(2 * A_LEVEL, EIGHT_HOURS, 1.0f) == doctest::Approx(2 * one));
}

TEST_CASE("rest: the ceiling is a level and a half of what the client draws")
{
    CHECK(Drawn(rest::Ceiling(A_LEVEL)) == doctest::Approx(1.5f * A_LEVEL));
}

TEST_CASE("rest: the ceiling is thirty bubbles")
{
    float const bubble = rest::Gained(A_LEVEL, EIGHT_HOURS, 1.0f);

    CHECK(rest::Ceiling(A_LEVEL) / bubble == doctest::Approx(30.0f));
}

TEST_CASE("rest: filling from empty to the ceiling takes two hundred and forty hours")
{
    float const perSecond = rest::Gained(A_LEVEL, 1, 1.0f);
    float const seconds = rest::Ceiling(A_LEVEL) / perSecond;

    CHECK(seconds / 3600.0f == doctest::Approx(240.0f));
}
