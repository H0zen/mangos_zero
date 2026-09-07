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

// The chance that doing something teaches you a little more of it.
//
// Chances are in tenths of a percent throughout, so a server that pays 25 for an
// orange recipe is asking for 250 here.

#include "doctest.h"

#include "SkillGain.h"

namespace
{
    skill::Chances Paid()
    {
        skill::Chances paid;
        paid.orange = 100;
        paid.yellow = 75;
        paid.green = 25;
        paid.grey = 0;

        return paid;
    }
}

TEST_CASE("chance: the colour is the whole of the answer")
{
    const skill::Chances paid = Paid();

    // Bands at 200 grey, 150 green, 125 yellow -- a node whose red level is 100.
    CHECK(skill::ChanceAt(210, 200, 150, 125, paid) == 0);      // grey
    CHECK(skill::ChanceAt(200, 200, 150, 125, paid) == 0);      // the grey line itself
    CHECK(skill::ChanceAt(160, 200, 150, 125, paid) == 250);    // green
    CHECK(skill::ChanceAt(130, 200, 150, 125, paid) == 750);    // yellow
    CHECK(skill::ChanceAt(110, 200, 150, 125, paid) == 1000);   // orange
}

TEST_CASE("chance: each band opens at its own level, not above it")
{
    const skill::Chances paid = Paid();

    CHECK(skill::ChanceAt(150, 200, 150, 125, paid) == 250);    // green opens at 150
    CHECK(skill::ChanceAt(149, 200, 150, 125, paid) == 750);    // one under is still yellow
    CHECK(skill::ChanceAt(125, 200, 150, 125, paid) == 750);    // yellow opens at 125
    CHECK(skill::ChanceAt(124, 200, 150, 125, paid) == 1000);   // one under is orange
}

TEST_CASE("chance: a server that pays nothing teaches nothing")
{
    skill::Chances none;

    CHECK(skill::ChanceAt(1, 200, 150, 125, none) == 0);
    CHECK(skill::ChanceAt(300, 200, 150, 125, none) == 0);
}

TEST_CASE("thinning: skinning and mining halve every step")
{
    CHECK(skill::Thinned(1000, 0, 75) == 1000);      // under the first step
    CHECK(skill::Thinned(1000, 74, 75) == 1000);
    CHECK(skill::Thinned(1000, 75, 75) == 500);      // once over it
    CHECK(skill::Thinned(1000, 150, 75) == 250);     // twice
    CHECK(skill::Thinned(1000, 225, 75) == 125);     // three times
}

TEST_CASE("thinning: a step of nothing turns the falling off off")
{
    CHECK(skill::Thinned(1000, 300, 0) == 1000);
}

TEST_CASE("fishing: the chance rises to seventy-five and falls away after it")
{
    CHECK(skill::FishingChance(1) == 1000);
    CHECK(skill::FishingChance(74) == 1000);

    // 2500 / (75 - 50) = 100, in tenths of a percent.
    CHECK(skill::FishingChance(75) == 1000);
    CHECK(skill::FishingChance(100) == 500);
    CHECK(skill::FishingChance(300) == 100);
}
