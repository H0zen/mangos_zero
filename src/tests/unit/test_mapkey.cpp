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

// How live maps are filed, and the range that finds every copy of one.
//
// MapRoster::EachOnMap walks [MapKey(id, 0), MapKey(id + 1, 0)) and nothing else.
// That is only every copy of the map if the ordering puts the map id first and
// leaves no copy outside the range -- so a respawn broadcast, a game event and an
// instance reset all reach exactly the copies they are meant to reach, and never a
// neighbouring map's.

#include "doctest.h"

#include "MapKey.h"

#include <map>
#include <vector>

namespace
{
    /// The copies EachOnMap would visit, in the order it would visit them.
    std::vector<uint32> CopiesOn(std::map<MapKey, uint32> const& sheet, uint32 mapId)
    {
        std::vector<uint32> seen;

        auto const first = sheet.lower_bound(MapKey(mapId, 0));
        auto const last = sheet.lower_bound(MapKey(mapId + 1, 0));

        for (auto itr = first; itr != last; ++itr)
        {
            seen.push_back(itr->first.instance);
        }

        return seen;
    }
}

TEST_CASE("A map id orders before its copy number")
{
    CHECK(MapKey(0, 9) < MapKey(1, 0));
    CHECK(MapKey(1, 0) < MapKey(1, 1));
    CHECK_FALSE(MapKey(1, 1) < MapKey(1, 1));
    CHECK(MapKey(1, 0) == MapKey(1, 0));
    CHECK_FALSE(MapKey(1, 0) == MapKey(1, 1));
}

TEST_CASE("A continent is filed under copy zero")
{
    CHECK(MapKey(0).instance == 0);
    CHECK(MapKey(0) == MapKey(0, 0));
}

TEST_CASE("The per-map range finds every copy and only that map's")
{
    std::map<MapKey, uint32> sheet;

    sheet[MapKey(0)] = 1;                   // Eastern Kingdoms
    sheet[MapKey(1)] = 2;                   // Kalimdor
    sheet[MapKey(33, 4)] = 3;               // Shadowfang Keep, three copies open
    sheet[MapKey(33, 7)] = 4;
    sheet[MapKey(33, 12)] = 5;
    sheet[MapKey(34, 1)] = 6;               // the map next door

    CHECK(CopiesOn(sheet, 33) == std::vector<uint32>({4, 7, 12}));
    CHECK(CopiesOn(sheet, 0) == std::vector<uint32>({0}));
    CHECK(CopiesOn(sheet, 34) == std::vector<uint32>({1}));
    CHECK(CopiesOn(sheet, 35).empty());
}

TEST_CASE("Map zero is a map, not an absence")
{
    std::map<MapKey, uint32> sheet;

    sheet[MapKey(0)] = 1;

    // Eastern Kingdoms is id 0, so anything that treats the id as a truth value drops
    // half the world out of the sheet.
    CHECK(sheet.find(MapKey(0)) != sheet.end());
    CHECK(CopiesOn(sheet, 0).size() == 1);
}
