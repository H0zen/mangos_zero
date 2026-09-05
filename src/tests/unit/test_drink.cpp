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

// The four names cut out of the range a character can drink into.

#include "doctest.h"

#include "Drink.h"

TEST_CASE("drink: nothing in him is sober")
{
    CHECK(Drink::NameOf(0) == DRUNKEN_SOBER);
}

TEST_CASE("drink: the bottom bit does not count, so one is still sober")
{
    CHECK(Drink::NameOf(1) == DRUNKEN_SOBER);
    CHECK(Drink::NameOf(2) == DRUNKEN_TIPSY);
    CHECK(Drink::NameOf(3) == DRUNKEN_TIPSY);
}

TEST_CASE("drink: the three thresholds are where the names change")
{
    CHECK(Drink::NameOf(12799) == DRUNKEN_TIPSY);
    CHECK(Drink::NameOf(12800) == DRUNKEN_DRUNK);

    CHECK(Drink::NameOf(22999) == DRUNKEN_DRUNK);
    CHECK(Drink::NameOf(23000) == DRUNKEN_SMASHED);
}

TEST_CASE("drink: the fullest a sixteen-bit amount goes is still smashed")
{
    CHECK(Drink::NameOf(65535) == DRUNKEN_SMASHED);
}

TEST_CASE("drink: the four names are in the order the client draws them")
{
    CHECK(DRUNKEN_SOBER < DRUNKEN_TIPSY);
    CHECK(DRUNKEN_TIPSY < DRUNKEN_DRUNK);
    CHECK(DRUNKEN_DRUNK < DRUNKEN_SMASHED);
    CHECK(MAX_DRUNKEN == DRUNKEN_SMASHED + 1);
}
