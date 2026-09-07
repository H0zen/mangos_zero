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

// What a drop is rolled at, and what coin a body carries.

#include "doctest.h"

#include "LootMgr.h"

namespace
{
    loot::DropRates Doubled()
    {
        loot::DropRates rates;
        for (uint32 quality = 0; quality < MAX_ITEM_QUALITY; ++quality)
        {
            rates.byQuality[quality] = 2.0f;
        }
        rates.referenced = 3.0f;

        return rates;
    }
}

TEST_CASE("drop: a certainty stays a certainty whatever the rates say")
{
    // This is what makes a quest item a quest item.
    CHECK(loot::ChanceOf(100.0f, true, false, ITEM_QUALITY_EPIC, Doubled()) == doctest::Approx(100.0f));
    CHECK(loot::ChanceOf(150.0f, true, false, ITEM_QUALITY_POOR, Doubled()) == doctest::Approx(150.0f));
}

TEST_CASE("drop: an unrated roll is the stated chance")
{
    CHECK(loot::ChanceOf(10.0f, false, false, ITEM_QUALITY_EPIC, Doubled()) == doctest::Approx(10.0f));
    CHECK(loot::ChanceOf(10.0f, false, true, 0, Doubled()) == doctest::Approx(10.0f));
}

TEST_CASE("drop: quality decides which rate applies")
{
    loot::DropRates rates;
    rates.byQuality[ITEM_QUALITY_POOR] = 1.0f;
    rates.byQuality[ITEM_QUALITY_EPIC] = 5.0f;

    CHECK(loot::ChanceOf(10.0f, true, false, ITEM_QUALITY_POOR, rates) == doctest::Approx(10.0f));
    CHECK(loot::ChanceOf(10.0f, true, false, ITEM_QUALITY_EPIC, rates) == doctest::Approx(50.0f));
}

TEST_CASE("drop: a reference is rated on its own")
{
    CHECK(loot::ChanceOf(10.0f, true, true, 0, Doubled()) == doctest::Approx(30.0f));
}

TEST_CASE("drop: a quality this build does not know is left alone")
{
    CHECK(loot::ChanceOf(10.0f, true, false, MAX_ITEM_QUALITY + 5, Doubled()) == doctest::Approx(10.0f));
}

TEST_CASE("coin: nothing at all when the row names no maximum")
{
    CHECK(loot::Coin(0, 0, 0, 5.0f) == 0);
    CHECK(loot::Coin(100, 0, 100, 5.0f) == 0);
}

TEST_CASE("coin: a range of one pays its own amount")
{
    CHECK(loot::Coin(50, 50, 50, 1.0f) == 50);
    CHECK(loot::Coin(80, 50, 50, 1.0f) == 50);      // a most under the least is still the most
}

TEST_CASE("coin: a range pays what was drawn, at the server's rate")
{
    CHECK(loot::Coin(10, 100, 40, 1.0f) == 40);
    CHECK(loot::Coin(10, 100, 40, 2.5f) == 100);
    CHECK(loot::Coin(10, 100, 40, 0.0f) == 0);
}
