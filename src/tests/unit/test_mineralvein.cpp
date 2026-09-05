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

// What a mineral vein comes up as.
//
// The ores here are the classic ones, so the cases read as they do in the
// ground: a vein placed as iron can come up as tin, and whichever of the two it
// is then holds its own rare ore -- tin silver, iron gold.

#include "doctest.h"

#include "MineralVein.h"

namespace
{
    uint32 const TIN = 1732;
    uint32 const SILVER = 1733;
    uint32 const GOLD = 1734;
    uint32 const IRON = 1735;
    uint32 const MITHRIL = 2040;
    uint32 const TRUESILVER = 2047;

    uint32 const DARK_IRON = 165658;
    uint32 const SEARING_GORGE = 46;

    uint32 const NOWHERE = 0;

    MineralVeins Ground()
    {
        MineralVeins veins;
        veins.Holds(IRON, TIN, GOLD);
        veins.Holds(MITHRIL, IRON, TRUESILVER);
        veins.Holds(TIN, 0, SILVER);                        // nothing poorer than tin
        veins.GroundHolds(SEARING_GORGE, DARK_IRON);
        return veins;
    }
}

TEST_CASE("mineral vein: with no table at all a vein is what it was placed as")
{
    MineralVeins const bare;

    CHECK(bare.SpawnedAs(IRON, NOWHERE, true, true, true) == IRON);
    CHECK(bare.PoorerThan(IRON) == 0);
    CHECK(bare.RicherThan(IRON) == 0);
    CHECK(bare.InZone(SEARING_GORGE) == 0);
}

TEST_CASE("mineral vein: the ladders are read off the table")
{
    MineralVeins const veins = Ground();

    CHECK(veins.PoorerThan(IRON) == TIN);
    CHECK(veins.RicherThan(IRON) == GOLD);
    CHECK(veins.PoorerThan(TIN) == 0);
    CHECK(veins.RicherThan(TIN) == SILVER);
    CHECK(veins.InZone(SEARING_GORGE) == DARK_IRON);

    CHECK(veins.Ladders() == 3);
    CHECK(veins.Grounds() == 1);
}

TEST_CASE("mineral vein: a vein that rolls nothing is what it was placed as")
{
    MineralVeins const veins = Ground();

    CHECK(veins.SpawnedAs(IRON, NOWHERE, true, false, false) == IRON);
    CHECK(veins.SpawnedAs(TIN, NOWHERE, true, true, false) == TIN);   // nothing poorer
}

TEST_CASE("mineral vein: coming up poorer sticks even when the rare roll fails")
{
    MineralVeins const veins = Ground();

    // A vein placed as iron that comes up tin stays tin. This is the whole point of
    // the poorer roll, and it is a separate roll from the rare one.
    CHECK(veins.SpawnedAs(IRON, NOWHERE, false, true, false) == TIN);
    CHECK(veins.SpawnedAs(MITHRIL, NOWHERE, false, true, false) == IRON);
}

TEST_CASE("mineral vein: the rare ore is the rare ore of what it came up as")
{
    MineralVeins const veins = Ground();

    // Iron that stayed iron holds gold; iron that came up tin holds silver.
    CHECK(veins.SpawnedAs(IRON, NOWHERE, false, false, true) == GOLD);
    CHECK(veins.SpawnedAs(IRON, NOWHERE, false, true, true) == SILVER);

    // And mithril that came up iron holds gold, not truesilver.
    CHECK(veins.SpawnedAs(MITHRIL, NOWHERE, false, false, true) == TRUESILVER);
    CHECK(veins.SpawnedAs(MITHRIL, NOWHERE, false, true, true) == GOLD);
}

TEST_CASE("mineral vein: where the ground holds an ore, it settles the vein alone")
{
    MineralVeins const veins = Ground();

    CHECK(veins.SpawnedAs(IRON, SEARING_GORGE, true, false, false) == DARK_IRON);

    // Even a vein that would have come up rich comes up dark iron instead.
    CHECK(veins.SpawnedAs(MITHRIL, SEARING_GORGE, true, true, true) == DARK_IRON);

    // And when that roll fails, no other is made: it is what it was placed as.
    CHECK(veins.SpawnedAs(MITHRIL, SEARING_GORGE, false, true, true) == MITHRIL);
}

TEST_CASE("mineral vein: a later row replaces an earlier one for the same ore")
{
    MineralVeins veins = Ground();
    veins.Holds(IRON, 0, TRUESILVER);

    CHECK(veins.PoorerThan(IRON) == 0);
    CHECK(veins.RicherThan(IRON) == TRUESILVER);
    CHECK(veins.Ladders() == 3);
}
