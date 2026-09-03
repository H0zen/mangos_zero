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

// The terrain query engine, driven by a synthetic tile source.
//
// FusedTerrain takes an ITileSource, and a source that is given wins outright --
// no file is consulted. So these run on hand-built tiles with no extracted client
// data anywhere, which is what lets them live in the tree and run on every box.
// What they pin is the ENGINE's contract: which surfaces reach the column, what a
// hole means, what an absent tile means, and that the live-geometry filter is
// carried through untouched.

#include "doctest.h"

#include "terrain/FusedTerrain.hpp"
#include "terrain/Terrain.hpp"

#include <memory>

using world::terrain::Column;
using world::terrain::FusedTerrain;
using world::terrain::GRID_PER_TILE;
using world::terrain::ILiveGeometry;
using world::terrain::ITileSource;
using world::terrain::LiquidKind;
using world::terrain::SurfaceKind;
using world::terrain::TerrainTile;
using world::terrain::TileIndex;
using world::terrain::V9_SIDE;

namespace
{
    /// World origin. Named because every case below queries it, and because the
    /// tile it falls in has to match what the source serves.
    const float ORIGIN_X = 0.f;
    const float ORIGIN_Y = 0.f;

    /// A flat tile at one height, with no statics and no liquid.
    std::shared_ptr<TerrainTile> FlatTile(int tx, int ty, float height)
    {
        auto tile = std::make_shared<TerrainTile>();
        tile->tx = tx;
        tile->ty = ty;
        tile->hasTerrain = true;
        tile->v9.assign(V9_SIDE * V9_SIDE, height);
        tile->v8.assign(GRID_PER_TILE * GRID_PER_TILE, height);
        return tile;
    }

    /// Serves exactly one tile, at the coordinates it was told, and nothing else.
    /// A source that answered everywhere would hide the "no tile here" contract.
    class OneTileSource : public ITileSource
    {
        public:
            OneTileSource(int tx, int ty, std::shared_ptr<TerrainTile> tile)
                : m_tx(tx), m_ty(ty), m_tile(std::move(tile)) {}

            std::shared_ptr<TerrainTile> Load(uint32_t, int tx, int ty) override
            {
                ++loads;
                return (tx == m_tx && ty == m_ty) ? m_tile : nullptr;
            }

            mutable int loads = 0;

        private:
            int m_tx, m_ty;
            std::shared_ptr<TerrainTile> m_tile;
    };

    /// A door or a lift: one solid surface the server poses at runtime.
    class OneSurfaceLiveGeometry : public ILiveGeometry
    {
        public:
            explicit OneSurfaceLiveGeometry(float z) : m_z(z) {}

            void AddSurfaces(float, float, float, float,
                             uint32_t filter, Column& out) const override
            {
                ++calls;
                seenFilter = filter;
                out.AddSolid(m_z, SurfaceKind::Live);
            }

            mutable int calls = 0;
            mutable uint32_t seenFilter = 0;

        private:
            float m_z;
    };

    int SolidCount(const Column& column)
    {
        int n = 0;
        for (const auto& surface : column.Surfaces())
        {
            if (surface.Solid())
            {
                ++n;
            }
        }
        return n;
    }
}

TEST_CASE("The origin falls in the tile the source is asked for")
{
    // Guards the arithmetic the rest of the file leans on: if this moved, every
    // case below would silently be querying an empty map and passing.
    CHECK(TileIndex(ORIGIN_X) == 32);
    CHECK(TileIndex(ORIGIN_Y) == 32);
}

TEST_CASE("A flat tile answers with its height, once")
{
    auto source = std::make_shared<OneTileSource>(32, 32, FlatTile(32, 32, 42.f));
    FusedTerrain terrain(0, source);

    const Column column = terrain.ColumnAt(ORIGIN_X, ORIGIN_Y, 1000.f, -1000.f);

    REQUIRE(SolidCount(column) == 1);
    REQUIRE(column.HighestSolid().has_value());
    CHECK(column.HighestSolid().value() == doctest::Approx(42.f));
    CHECK(column.Surfaces()[0].kind == SurfaceKind::Terrain);
}

TEST_CASE("A map with no tile there returns an empty column, not a height")
{
    // The refusal has to survive to the caller. A sentinel height here is what
    // puts spawns at Z = 0 in the middle of the ocean.
    auto source = std::make_shared<OneTileSource>(32, 32, FlatTile(32, 32, 42.f));
    FusedTerrain terrain(0, source);

    // Far enough away to land in a different tile.
    const float farX = 5000.f;
    REQUIRE(TileIndex(farX) != 32);

    const Column column = terrain.ColumnAt(farX, farX, 1000.f, -1000.f);

    CHECK(column.Empty());
    CHECK_FALSE(column.Floor(0.f).has_value());
}

TEST_CASE("A surface above the window's top is left out")
{
    // Probing a window that ends below the ground must not report the ground:
    // that is how a caller asks "is there anything under this bridge deck".
    auto source = std::make_shared<OneTileSource>(32, 32, FlatTile(32, 32, 42.f));
    FusedTerrain terrain(0, source);

    const Column below = terrain.ColumnAt(ORIGIN_X, ORIGIN_Y, 10.f, -1000.f);
    CHECK(below.Empty());

    const Column above = terrain.ColumnAt(ORIGIN_X, ORIGIN_Y, 50.f, -1000.f);
    REQUIRE(above.HighestSolid().has_value());
    CHECK(above.HighestSolid().value() == doctest::Approx(42.f));
}

TEST_CASE("A heightmap sample is not clipped at the window's bottom")
{
    // Deliberate asymmetry, and worth pinning: a caller probing down from far
    // above would otherwise get an empty column on a map whose only surface is
    // terrain. The tile holds one value; there is nothing to gain by hiding it.
    auto source = std::make_shared<OneTileSource>(32, 32, FlatTile(32, 32, 42.f));
    FusedTerrain terrain(0, source);

    const Column column = terrain.ColumnAt(ORIGIN_X, ORIGIN_Y, 1000.f, 500.f);

    REQUIRE(column.HighestSolid().has_value());
    CHECK(column.HighestSolid().value() == doctest::Approx(42.f));
}

TEST_CASE("A hole in the heightmap yields no terrain surface")
{
    // Holes are how a cave mouth or a ramp into a building is cut out of the ADT.
    // Reporting the skin across one puts a floor where the client renders a gap.
    auto tile = FlatTile(32, 32, 42.f);
    tile->holes.fill(0xFFFF);   // every 2x2 block of every chunk punched out

    auto source = std::make_shared<OneTileSource>(32, 32, tile);
    FusedTerrain terrain(0, source);

    const Column column = terrain.ColumnAt(ORIGIN_X, ORIGIN_Y, 1000.f, -1000.f);

    CHECK(column.Empty());
}

TEST_CASE("ADT liquid reaches the column as liquid, beside the ground")
{
    auto tile = FlatTile(32, 32, 5.f);
    tile->hasLiquid = true;
    tile->liquidHeight.assign(V9_SIDE * V9_SIDE, 8.f);
    tile->liquidShow.assign(GRID_PER_TILE * GRID_PER_TILE, 1);
    tile->liquidKind.assign(GRID_PER_TILE * GRID_PER_TILE,
                            static_cast<uint8_t>(LiquidKind::Ocean));
    tile->liquidEntry.assign(GRID_PER_TILE * GRID_PER_TILE, uint16_t(3));
    tile->liquidDeep.assign(GRID_PER_TILE * GRID_PER_TILE, uint8_t(1));

    auto source = std::make_shared<OneTileSource>(32, 32, tile);
    FusedTerrain terrain(0, source);

    const Column column = terrain.ColumnAt(ORIGIN_X, ORIGIN_Y, 1000.f, -1000.f);

    // One solid (the bed) and one liquid (the surface) -- never merged.
    CHECK(SolidCount(column) == 1);
    REQUIRE(column.HighestSolidAtOrBelow(1000.f).has_value());
    CHECK(column.HighestSolidAtOrBelow(1000.f).value() == doctest::Approx(5.f));

    auto liquid = column.HighestLiquid();
    REQUIRE(liquid.has_value());
    CHECK(liquid->AsLiquid().level == doctest::Approx(8.f));
    CHECK(liquid->AsLiquid().kind == LiquidKind::Ocean);
    CHECK(liquid->AsLiquid().entry == 3);
    CHECK(liquid->AsLiquid().deep);
}

TEST_CASE("A cell the liquid mask does not show carries no liquid")
{
    auto tile = FlatTile(32, 32, 5.f);
    tile->hasLiquid = true;
    tile->liquidHeight.assign(V9_SIDE * V9_SIDE, 8.f);
    tile->liquidShow.assign(GRID_PER_TILE * GRID_PER_TILE, 0);   // shown nowhere

    auto source = std::make_shared<OneTileSource>(32, 32, tile);
    FusedTerrain terrain(0, source);

    const Column column = terrain.ColumnAt(ORIGIN_X, ORIGIN_Y, 1000.f, -1000.f);

    CHECK_FALSE(column.HighestLiquid().has_value());
    CHECK(SolidCount(column) == 1);
}

TEST_CASE("Live geometry is composed in, and its filter is passed through untouched")
{
    // The engine carries `filter` as an opaque token and never interprets it --
    // that is what lets the game put phase, door state or anything else behind it
    // without the terrain library learning a game type.
    auto source = std::make_shared<OneTileSource>(32, 32, FlatTile(32, 32, 5.f));
    FusedTerrain terrain(0, source);

    OneSurfaceLiveGeometry door(20.f);
    const uint32_t token = 0xDEADBEEF;

    const Column column = terrain.ColumnAt(ORIGIN_X, ORIGIN_Y, 1000.f, -1000.f,
                                           &door, token);

    CHECK(door.calls == 1);
    CHECK(door.seenFilter == token);

    // The ground and the door both, and the door is the floor for someone on it.
    CHECK(SolidCount(column) == 2);
    REQUIRE(column.Floor(21.f).has_value());
    CHECK(column.Floor(21.f).value() == doctest::Approx(20.f));
    REQUIRE(column.Floor(6.f).has_value());
    CHECK(column.Floor(6.f).value() == doctest::Approx(5.f));
}

TEST_CASE("Live geometry is not consulted when none is given")
{
    auto source = std::make_shared<OneTileSource>(32, 32, FlatTile(32, 32, 5.f));
    FusedTerrain terrain(0, source);

    const Column column = terrain.ColumnAt(ORIGIN_X, ORIGIN_Y, 1000.f, -1000.f);

    CHECK(SolidCount(column) == 1);
}

TEST_CASE("A tile is loaded once and then served from the cache")
{
    auto source = std::make_shared<OneTileSource>(32, 32, FlatTile(32, 32, 42.f));
    FusedTerrain terrain(0, source);

    for (int i = 0; i < 5; ++i)
    {
        terrain.ColumnAt(ORIGIN_X, ORIGIN_Y, 1000.f, -1000.f);
    }

    CHECK(source->loads == 1);
    CHECK(terrain.ResidentTiles() == 1);
}

TEST_CASE("A tile that is not there is remembered as not there")
{
    // The negative is memoised too: without it every query over open ocean pays
    // a failed load, forever.
    auto source = std::make_shared<OneTileSource>(32, 32, FlatTile(32, 32, 42.f));
    FusedTerrain terrain(0, source);

    const float farX = 5000.f;
    for (int i = 0; i < 5; ++i)
    {
        terrain.ColumnAt(farX, farX, 1000.f, -1000.f);
    }

    CHECK(source->loads == 1);
}

TEST_CASE("The area id comes from the tile, and is zero where there is none")
{
    auto tile = FlatTile(32, 32, 42.f);
    tile->areaIds.fill(1519);   // Stormwind, as a recognisable non-zero

    auto source = std::make_shared<OneTileSource>(32, 32, tile);
    FusedTerrain terrain(0, source);

    CHECK(terrain.GetAreaId(ORIGIN_X, ORIGIN_Y) == 1519);
    CHECK(terrain.GetAreaId(5000.f, 5000.f) == 0);
}

TEST_CASE("With nothing in the way, line of sight is clear")
{
    // No statics anywhere, so nothing can block. The interesting case -- a wall --
    // needs a collision model and belongs with the model tests; this one pins that
    // an empty world does not accidentally report itself as opaque.
    auto source = std::make_shared<OneTileSource>(32, 32, FlatTile(32, 32, 42.f));
    FusedTerrain terrain(0, source);

    CHECK(terrain.IsInLineOfSight(ORIGIN_X, ORIGIN_Y, 50.f,
                                  ORIGIN_X + 30.f, ORIGIN_Y, 50.f));
    CHECK(terrain.NearestHitFraction(ORIGIN_X, ORIGIN_Y, 50.f,
                                     ORIGIN_X + 30.f, ORIGIN_Y, 50.f) > 1.0f);
}
