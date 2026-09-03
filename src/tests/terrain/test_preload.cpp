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

// Mapping the whole map at startup instead of on first touch.
//
// What has to hold afterwards is that the runtime asks the source for nothing:
// every cell, present or missing, has already been settled.

#include "doctest.h"

#include "terrain/FusedTerrain.hpp"
#include "terrain/Terrain.hpp"

#include <cstdint>
#include <memory>

using world::terrain::FusedTerrain;
using world::terrain::GRID_PER_TILE;
using world::terrain::ITileSource;
using world::terrain::TerrainTile;
using world::terrain::V9_SIDE;

namespace
{
    std::shared_ptr<TerrainTile> FlatTile(int tx, int ty, float height)
    {
        auto tile = std::make_shared<TerrainTile>();
        tile->tx = tx;
        tile->ty = ty;
        tile->hasTerrain = true;
        tile->v9.Adopt(std::vector<float>(static_cast<size_t>(V9_SIDE) * V9_SIDE, height));
        tile->v8.Adopt(std::vector<float>(static_cast<size_t>(GRID_PER_TILE) * GRID_PER_TILE, height));
        return tile;
    }

    /// Serves a small square of cells and counts every question asked of it.
    class CountingSource : public ITileSource
    {
        public:
            CountingSource(int lo, int hi) : m_lo(lo), m_hi(hi) {}

            std::shared_ptr<TerrainTile> Load(uint32_t, int tx, int ty) override
            {
                ++loads;
                const bool inside = tx >= m_lo && tx <= m_hi && ty >= m_lo && ty <= m_hi;
                return inside ? FlatTile(tx, ty, 42.f) : nullptr;
            }

            int loads = 0;

        private:
            int m_lo, m_hi;
    };
}

TEST_CASE("Preloading maps every cell the map has, and counts the empty ones")
{
    auto source = std::make_shared<CountingSource>(30, 33);   // a 4x4 block
    FusedTerrain terrain(0, source);

    REQUIRE(terrain.ResidentTiles() == 0);

    const auto stats = terrain.PreloadAll();

    CHECK(stats.mapped == 16);
    CHECK(stats.absent == uint32_t(FusedTerrain::GRID_COUNT * FusedTerrain::GRID_COUNT - 16));
    CHECK(terrain.ResidentTiles() == 16);
}

TEST_CASE("After preloading, the runtime asks the source nothing more")
{
    // The point of paying at startup: no map thread opens a file mid-tick, and
    // a query over an empty cell does not reprobe a file that is not there.
    auto source = std::make_shared<CountingSource>(32, 32);
    FusedTerrain terrain(0, source);

    terrain.PreloadAll();
    const int afterPreload = source->loads;
    REQUIRE(afterPreload > 0);

    for (int i = 0; i < 50; ++i)
    {
        terrain.ColumnAt(0.f, 0.f, 1000.f, -1000.f);        // a mapped cell
        terrain.ColumnAt(5000.f, 5000.f, 1000.f, -1000.f);  // an empty one
    }

    CHECK(source->loads == afterPreload);
}

TEST_CASE("Preloading twice does no work the second time")
{
    auto source = std::make_shared<CountingSource>(32, 32);
    FusedTerrain terrain(0, source);

    terrain.PreloadAll();
    const int afterFirst = source->loads;

    const auto second = terrain.PreloadAll();

    CHECK(source->loads == afterFirst);
    CHECK(second.mapped == 0);
    CHECK(second.absent == 0);
}

TEST_CASE("A preloaded cell answers with its data")
{
    auto source = std::make_shared<CountingSource>(32, 32);
    FusedTerrain terrain(0, source);

    terrain.PreloadAll();

    const auto column = terrain.ColumnAt(0.f, 0.f, 1000.f, -1000.f);
    REQUIRE(column.HighestSolid().has_value());
    CHECK(column.HighestSolid().value() == doctest::Approx(42.f));
}
