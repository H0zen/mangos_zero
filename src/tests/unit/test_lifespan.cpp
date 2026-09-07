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

// The clock a conjured thing keeps.
//
// Two cases decide whether this type can be trusted with an area effect. A clock
// that was never granted anything must never say it is out -- an effect with no
// duration is one that lasts until its caster ends it, and a clock that reported
// otherwise would delete it on its first tick. And a tick longer than what is
// left must land on nought rather than wrapping, because the remainder is
// unsigned and a wrapped clock is an effect that outlives the world.

#include "doctest.h"

#include "Lifespan.h"

TEST_CASE("A clock nobody set never runs out")
{
    Lifespan life;

    CHECK_FALSE(life.Bounded());
    CHECK(life.Left() == 0);
    CHECK_FALSE(life.Spent());

    CHECK_FALSE(life.Spend(1000));
    CHECK_FALSE(life.Spend(0xFFFFFFFF));
    CHECK_FALSE(life.Spent());
}

TEST_CASE("Spending takes the time off, and the last spend says so")
{
    Lifespan life;
    life.Grant(1000);

    CHECK(life.Bounded());
    CHECK(life.Granted() == 1000);

    CHECK_FALSE(life.Spend(400));
    CHECK(life.Left() == 600);

    CHECK_FALSE(life.Spend(599));
    CHECK(life.Left() == 1);

    CHECK(life.Spend(1));
    CHECK(life.Left() == 0);
    CHECK(life.Spent());
}

TEST_CASE("A tick longer than the clock lands on nought")
{
    Lifespan life;
    life.Grant(50);

    CHECK(life.Spend(5000));
    CHECK(life.Left() == 0);

    // And it keeps saying so, so a caller that reads late still learns it is over.
    CHECK(life.Spend(1));
    CHECK(life.Left() == 0);
}

TEST_CASE("Shortening brings the end closer; pushing out stops at the full length")
{
    Lifespan life;
    life.Grant(1000);
    life.Spend(500);

    life.Shorten(200);
    CHECK(life.Left() == 300);

    life.Shorten(-100);
    CHECK(life.Left() == 400);

    // Nothing outlives the term it was granted.
    life.Shorten(-5000);
    CHECK(life.Left() == 1000);

    // Taking off more than is left ends it rather than wrapping.
    life.Shorten(4000);
    CHECK(life.Left() == 0);
    CHECK(life.Spent());
}

TEST_CASE("Shortening an unset clock changes nothing")
{
    Lifespan life;

    life.Shorten(-5000);
    CHECK(life.Left() == 0);
    CHECK_FALSE(life.Bounded());
    CHECK_FALSE(life.Spent());
}

TEST_CASE("Restoring puts back the full length")
{
    Lifespan life;
    life.Grant(800);
    life.Spend(800);
    REQUIRE(life.Spent());

    life.Restore();
    CHECK(life.Left() == 800);
    CHECK_FALSE(life.Spent());
}

TEST_CASE("Clearing leaves no clock at all")
{
    Lifespan life;
    life.Grant(800);
    life.Clear();

    CHECK_FALSE(life.Bounded());
    CHECK_FALSE(life.Spend(10000));
}

TEST_CASE("A granted nought is no clock")
{
    Lifespan life;
    life.Grant(0);

    CHECK_FALSE(life.Bounded());
    CHECK_FALSE(life.Spend(1));
}
