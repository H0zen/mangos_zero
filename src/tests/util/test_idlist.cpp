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

// Parsing an id list out of a config value.
//
// The case that matters is zero. Eastern Kingdoms is map 0, so a parser that
// treats a zero result as "this did not parse" drops half the world from any
// list that names it -- silently, because a config value that half-works looks
// exactly like one that works.

#include "doctest.h"

#include "Utilities/IdList.h"

#include <vector>

using MaNGOS::ParseIdList;

TEST_CASE("Zero is a map id, not a parse failure")
{
    const std::vector<uint32> ids = ParseIdList("0,1");

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 0);
    CHECK(ids[1] == 1);
}

TEST_CASE("A lone zero survives")
{
    const std::vector<uint32> ids = ParseIdList("0");

    REQUIRE(ids.size() == 1);
    CHECK(ids[0] == 0);
}

TEST_CASE("An ordinary list parses in order")
{
    const std::vector<uint32> ids = ParseIdList("1,530,571");

    REQUIRE(ids.size() == 3);
    CHECK(ids[0] == 1);
    CHECK(ids[1] == 530);
    CHECK(ids[2] == 571);
}

TEST_CASE("Whitespace and quotes around a value are ignored")
{
    const std::vector<uint32> ids = ParseIdList("\" 0 , 1 \"");

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 0);
    CHECK(ids[1] == 1);
}

TEST_CASE("An empty value yields nothing")
{
    CHECK(ParseIdList("").empty());
    CHECK(ParseIdList("   ").empty());
    CHECK(ParseIdList(",,,").empty());
}

TEST_CASE("Text that is not a number is refused")
{
    // The reason the old guard existed. It still has to hold, now that a zero
    // result is no longer what marks a failure.
    CHECK(ParseIdList("abc").empty());
    CHECK(ParseIdList("map0").empty());
}

TEST_CASE("A number with a tail is refused rather than truncated")
{
    // "12abc" reading as 12 would accept a typo as a valid id.
    CHECK(ParseIdList("12abc").empty());
}

TEST_CASE("A bad entry does not take the good ones with it")
{
    const std::vector<uint32> ids = ParseIdList("0,rubbish,1");

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 0);
    CHECK(ids[1] == 1);
}

TEST_CASE("A negative id is refused")
{
    CHECK(ParseIdList("-1").empty());
}
