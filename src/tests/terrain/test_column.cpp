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

// Multi-surface selection: the point where a naive server puts creatures in the
// floor of a building and lets them fall through bridges.
//
// A height at (x, y) with no reference height is meaningless, because there are
// several: the ADT under the bridge, the bridge deck, the storey above it, the
// water beside it. Column gathers them all and each caller selects. These tests
// state what each selection means, and they need no terrain data at all --
// Column is a value type, so the scenarios are written out directly.

#include "doctest.h"

#include "terrain/Column.hpp"

using world::terrain::Column;
using world::terrain::LiquidInfo;
using world::terrain::LiquidKind;
using world::terrain::SurfaceKind;

TEST_CASE("A bridge and the ground beneath it are two different answers")
{
    // The canonical failure: report the ground under a bridge to someone walking
    // across it, and he drops through.
    Column column;
    column.AddSolid(10.f, SurfaceKind::Terrain);  // the valley floor
    column.AddSolid(30.f, SurfaceKind::Static);   // the bridge deck

    SUBCASE("standing on the deck selects the deck")
    {
        REQUIRE(column.Floor(31.f).has_value());
        CHECK(column.Floor(31.f).value() == doctest::Approx(30.f));
    }

    SUBCASE("walking underneath selects the ground")
    {
        REQUIRE(column.Floor(11.f).has_value());
        CHECK(column.Floor(11.f).value() == doctest::Approx(10.f));
    }

    SUBCASE("the highest surface anywhere is the wrong answer for the man below")
    {
        // HighestSolid() exists, and is exactly what must NOT be used as "the
        // ground": from under the bridge it hands back the deck.
        REQUIRE(column.HighestSolid().has_value());
        CHECK(column.HighestSolid().value() == doctest::Approx(30.f));
        CHECK(column.HighestSolid().value() != column.Floor(11.f).value());
    }
}

TEST_CASE("A two-storey building answers per storey, with a ceiling above")
{
    Column column;
    column.AddSolid(0.f,  SurfaceKind::Static);   // ground floor
    column.AddSolid(20.f, SurfaceKind::Static);   // first floor
    column.AddSolid(40.f, SurfaceKind::Static);   // roof

    SUBCASE("on the upper storey the floor is the upper storey")
    {
        REQUIRE(column.Floor(21.f).has_value());
        CHECK(column.Floor(21.f).value() == doctest::Approx(20.f));
    }

    SUBCASE("downstairs the floor is the ground floor")
    {
        REQUIRE(column.Floor(1.f).has_value());
        CHECK(column.Floor(1.f).value() == doctest::Approx(0.f));
    }

    SUBCASE("the ceiling is the next surface up, not the roof")
    {
        // Standing downstairs, what is over your head is the first floor at 20 --
        // not the roof at 40.
        REQUIRE(column.LowestSolidAbove(1.f).has_value());
        CHECK(column.LowestSolidAbove(1.f).value() == doctest::Approx(20.f));
    }

    SUBCASE("above the roof there is no ceiling")
    {
        CHECK_FALSE(column.LowestSolidAbove(41.f).has_value());
    }
}

TEST_CASE("A cave under a hillside is reachable from inside")
{
    // The cave mouth: terrain skin high above, cave floor below it.
    Column column;
    column.AddSolid(5.f,  SurfaceKind::Static);   // cave floor
    column.AddSolid(60.f, SurfaceKind::Terrain);  // hillside overhead

    REQUIRE(column.Floor(6.f).has_value());
    CHECK(column.Floor(6.f).value() == doctest::Approx(5.f));

    // And the hill is genuinely overhead, not underfoot.
    REQUIRE(column.LowestSolidAbove(6.f).has_value());
    CHECK(column.LowestSolidAbove(6.f).value() == doctest::Approx(60.f));
}

TEST_CASE("Liquid is gathered but never selected as a solid floor")
{
    // At the water's edge the lake surface sits above the lake bed. A selection
    // for "what do I stand on" must not answer with the water.
    Column column;
    column.AddSolid(5.f, SurfaceKind::Terrain);   // lake bed

    LiquidInfo water;
    water.level = 8.f;
    water.kind = LiquidKind::Water;
    water.entry = 2;
    water.deep = false;
    column.AddLiquid(water);

    SUBCASE("the floor is the bed, not the surface")
    {
        REQUIRE(column.Floor(6.f).has_value());
        CHECK(column.Floor(6.f).value() == doctest::Approx(5.f));

        REQUIRE(column.HighestSolidAtOrBelow(100.f).has_value());
        CHECK(column.HighestSolidAtOrBelow(100.f).value() == doctest::Approx(5.f));
    }

    SUBCASE("and it is not a ceiling either")
    {
        // Swimming above the bed, nothing solid is overhead.
        CHECK_FALSE(column.LowestSolidAbove(6.f).has_value());
    }

    SUBCASE("the liquid is still there when asked for as liquid")
    {
        auto liquid = column.HighestLiquid();
        REQUIRE(liquid.has_value());
        CHECK(liquid->z == doctest::Approx(8.f));
        CHECK(liquid->kind == SurfaceKind::Liquid);
        CHECK_FALSE(liquid->Solid());

        const LiquidInfo info = liquid->AsLiquid();
        CHECK(info.level == doctest::Approx(8.f));
        CHECK(info.kind == LiquidKind::Water);
        CHECK(info.entry == 2);
        CHECK_FALSE(info.deep);
    }
}

TEST_CASE("Deep water is carried through as its own flag")
{
    // Swim fatigue keys off this, so it must survive the round trip rather than
    // being inferred from depth.
    Column column;

    LiquidInfo dark;
    dark.level = 12.f;
    dark.kind = LiquidKind::Ocean;
    dark.deep = true;
    column.AddLiquid(dark);

    auto liquid = column.HighestLiquid();
    REQUIRE(liquid.has_value());
    CHECK(liquid->AsLiquid().deep);
    CHECK(liquid->AsLiquid().kind == LiquidKind::Ocean);
}

TEST_CASE("Only the highest liquid is reported when several stack")
{
    Column column;

    LiquidInfo low;
    low.level = 4.f;
    low.kind = LiquidKind::Water;
    column.AddLiquid(low);

    LiquidInfo high;
    high.level = 9.f;
    high.kind = LiquidKind::Magma;
    column.AddLiquid(high);

    auto liquid = column.HighestLiquid();
    REQUIRE(liquid.has_value());
    CHECK(liquid->z == doctest::Approx(9.f));
    CHECK(liquid->AsLiquid().kind == LiquidKind::Magma);
}

TEST_CASE("A point sunk into the ground still stands on it")
{
    // A spawn placed a little under the hillside it belongs to. Floor() looks a
    // short way up before giving in, so the answer is the surface it would rest
    // on once freed -- not nothing, and not whatever is far overhead.
    Column column;
    column.AddSolid(10.f, SurfaceKind::Terrain);

    SUBCASE("within the tolerance, the surface above counts as below")
    {
        REQUIRE(column.Floor(9.f).has_value());
        CHECK(column.Floor(9.f).value() == doctest::Approx(10.f));
    }

    SUBCASE("past the tolerance it falls back to the nearest surface above")
    {
        // Nothing at or under the reference, so the only floor it could ever
        // stand on is the one overhead.
        REQUIRE(column.Floor(0.f).has_value());
        CHECK(column.Floor(0.f).value() == doctest::Approx(10.f));
    }

    SUBCASE("a caller may ask with no tolerance at all")
    {
        CHECK_FALSE(column.HighestSolidAtOrBelow(9.f).has_value());
    }
}

TEST_CASE("An empty column refuses every question")
{
    // A refusal, never a sentinel height: the caller must decide what to do with
    // "there is nothing here", and cannot be handed a plausible-looking number.
    Column column;

    CHECK(column.Empty());
    CHECK_FALSE(column.Floor(0.f).has_value());
    CHECK_FALSE(column.HighestSolid().has_value());
    CHECK_FALSE(column.HighestSolidAtOrBelow(0.f).has_value());
    CHECK_FALSE(column.LowestSolidAbove(0.f).has_value());
    CHECK_FALSE(column.HighestLiquid().has_value());
}

TEST_CASE("Clear returns a column to empty")
{
    Column column;
    column.AddSolid(10.f, SurfaceKind::Terrain);
    REQUIRE_FALSE(column.Empty());

    column.Clear();

    CHECK(column.Empty());
    CHECK(column.Surfaces().empty());
    CHECK_FALSE(column.Floor(10.f).has_value());
}

TEST_CASE("The gather keeps every surface, so a caller can select its own")
{
    // The whole point of one gather and many selections: nothing is dropped on
    // the way in, so a question nobody anticipated can still be answered.
    Column column;
    column.AddSolid(10.f, SurfaceKind::Terrain);
    column.AddSolid(30.f, SurfaceKind::Static);

    LiquidInfo water;
    water.level = 12.f;
    water.kind = LiquidKind::Water;
    column.AddLiquid(water);

    REQUIRE(column.Surfaces().size() == 3);

    int solids = 0, liquids = 0;
    for (const auto& surface : column.Surfaces())
    {
        surface.Solid() ? ++solids : ++liquids;
    }
    CHECK(solids == 2);
    CHECK(liquids == 1);
}
