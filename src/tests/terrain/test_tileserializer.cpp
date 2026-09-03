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

// Writing a tile and reading it back.
//
// The round trip has to be exact, and the grids have to come back as VIEWS into
// the mapping rather than copies -- that is the whole point, and it is the one
// property a plain value comparison would not notice going away.

#include "doctest.h"

#include "terrain/Terrain.hpp"
#include "terrain/CollisionModel.hpp"
#include "terrain/TileSerializer.hpp"

#include <cstdio>
#include <string>
#include <vector>

using world::terrain::CHUNKS;
using world::terrain::GRID_PER_TILE;
using world::terrain::LiquidKind;
using world::terrain::ReadTile;
using world::terrain::TerrainTile;
using world::terrain::V9_SIDE;
using world::terrain::WriteTile;

namespace
{
    /// A path that is removed when the case ends, pass or fail.
    class ScratchPath
    {
        public:
            ScratchPath() : m_path(std::tmpnam(nullptr)) {}
            ~ScratchPath() { std::remove(m_path.c_str()); }

            const std::string& Path() const { return m_path; }

        private:
            std::string m_path;
    };

    /// A tile with recognisable values in every grid, so a misaligned or
    /// mis-ordered section shows up as wrong data rather than as a crash.
    TerrainTile BuildTile()
    {
        TerrainTile tile;
        tile.tx = 32;
        tile.ty = 33;
        tile.hasTerrain = true;
        tile.isGlobalWmo = false;

        std::vector<float> v9(size_t(V9_SIDE) * V9_SIDE);
        for (size_t i = 0; i < v9.size(); ++i)
        {
            v9[i] = float(i) * 0.5f;
        }
        tile.v9.Adopt(std::move(v9));

        std::vector<float> v8(size_t(GRID_PER_TILE) * GRID_PER_TILE);
        for (size_t i = 0; i < v8.size(); ++i)
        {
            v8[i] = 1000.f - float(i);
        }
        tile.v8.Adopt(std::move(v8));

        for (size_t i = 0; i < tile.holes.size(); ++i)
        {
            tile.holes[i] = uint16_t(i * 7);
            tile.areaIds[i] = uint16_t(1500 + i);
        }

        tile.hasLiquid = true;

        std::vector<float> heights(size_t(V9_SIDE) * V9_SIDE, 12.5f);
        heights[0] = 3.25f;
        tile.liquidHeight.Adopt(std::move(heights));

        const size_t cells = size_t(GRID_PER_TILE) * GRID_PER_TILE;
        tile.liquidShow.Adopt(std::vector<uint8_t>(cells, 1));
        tile.liquidKind.Adopt(std::vector<uint8_t>(cells, uint8_t(LiquidKind::Ocean)));
        tile.liquidEntry.Adopt(std::vector<uint16_t>(cells, 3));
        tile.liquidDeep.Adopt(std::vector<uint8_t>(cells, 1));

        return tile;
    }
}

TEST_CASE("A tile survives the round trip unchanged")
{
    ScratchPath scratch;
    const TerrainTile written = BuildTile();

    REQUIRE(WriteTile(written, scratch.Path()));

    auto read = ReadTile(scratch.Path());
    REQUIRE(static_cast<bool>(read));

    CHECK(read->tx == written.tx);
    CHECK(read->ty == written.ty);
    CHECK(read->hasTerrain == written.hasTerrain);
    CHECK(read->isGlobalWmo == written.isGlobalWmo);
    CHECK(read->hasLiquid == written.hasLiquid);

    REQUIRE(read->v9.size() == written.v9.size());
    REQUIRE(read->v8.size() == written.v8.size());
    for (size_t i = 0; i < read->v9.size(); ++i)
    {
        CHECK(read->v9[i] == doctest::Approx(written.v9[i]));
    }
    for (size_t i = 0; i < read->v8.size(); ++i)
    {
        CHECK(read->v8[i] == doctest::Approx(written.v8[i]));
    }

    CHECK(read->holes == written.holes);
    CHECK(read->areaIds == written.areaIds);

    REQUIRE(read->liquidHeight.size() == written.liquidHeight.size());
    CHECK(read->liquidHeight[0] == doctest::Approx(3.25f));
    CHECK(read->liquidHeight[1] == doctest::Approx(12.5f));

    REQUIRE(read->liquidEntry.size() == written.liquidEntry.size());
    CHECK(read->liquidEntry[0] == 3);
    CHECK(read->liquidKind[0] == uint8_t(LiquidKind::Ocean));
    CHECK(read->liquidDeep[0] == 1);
    CHECK(read->liquidShow[0] == 1);
}

TEST_CASE("The grids come back as views into the mapping, not copies")
{
    // The point of the exercise. If these ever start owning again, a resident
    // tile is back on the heap and the map has to sweep tiles out to bound it.
    ScratchPath scratch;
    REQUIRE(WriteTile(BuildTile(), scratch.Path()));

    auto read = ReadTile(scratch.Path());
    REQUIRE(static_cast<bool>(read));

    REQUIRE(static_cast<bool>(read->mapping));

    CHECK_FALSE(read->v9.Owns());
    CHECK_FALSE(read->v8.Owns());
    CHECK_FALSE(read->liquidHeight.Owns());
    CHECK_FALSE(read->liquidShow.Owns());
    CHECK_FALSE(read->liquidKind.Owns());
    CHECK_FALSE(read->liquidEntry.Owns());
    CHECK_FALSE(read->liquidDeep.Owns());

    // And they really do point inside the mapped bytes.
    const uint8_t* base = read->mapping->Data();
    const uint8_t* end = base + read->mapping->Size();
    const uint8_t* v9 = reinterpret_cast<const uint8_t*>(read->v9.data());
    CHECK(v9 >= base);
    CHECK(v9 < end);
}

TEST_CASE("Every mapped grid is aligned for its element type")
{
    // A view is a reinterpret_cast onto mapped bytes, so a section that landed
    // on an odd offset would be undefined to read on a strict platform and slow
    // on a forgiving one. The writer pads for exactly this.
    ScratchPath scratch;
    REQUIRE(WriteTile(BuildTile(), scratch.Path()));

    auto read = ReadTile(scratch.Path());
    REQUIRE(static_cast<bool>(read));

    auto aligned = [](const void* p, size_t a)
    {
        return reinterpret_cast<uintptr_t>(p) % a == 0;
    };

    CHECK(aligned(read->v9.data(), alignof(float)));
    CHECK(aligned(read->v8.data(), alignof(float)));
    CHECK(aligned(read->liquidHeight.data(), alignof(float)));
    CHECK(aligned(read->liquidEntry.data(), alignof(uint16_t)));
}

TEST_CASE("A tile with no liquid and no terrain still round-trips")
{
    ScratchPath scratch;

    TerrainTile empty;
    empty.tx = 1;
    empty.ty = 2;
    empty.hasTerrain = false;
    empty.hasLiquid = false;

    REQUIRE(WriteTile(empty, scratch.Path()));

    auto read = ReadTile(scratch.Path());
    REQUIRE(static_cast<bool>(read));

    CHECK(read->tx == 1);
    CHECK(read->ty == 2);
    CHECK_FALSE(read->hasTerrain);
    CHECK_FALSE(read->hasLiquid);
    CHECK(read->v9.empty());
    CHECK(read->instances.empty());
}

TEST_CASE("A tile's collision geometry round-trips, and comes back mapped")
{
    // The models are the other half of a tile's bulk. If these went back to
    // being copied, a resident tile would still put its triangles on the heap.
    ScratchPath scratch;

    world::terrain::TriSoup soup;
    soup.verts.Adopt(std::vector<world::terrain::Vec3>{
        {0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {0.f, 10.f, 0.f}, {10.f, 10.f, 5.f}});
    soup.tris.Adopt(std::vector<std::array<uint32_t, 3>>{{0, 1, 2}, {1, 3, 2}});

    auto model = std::make_shared<world::terrain::CollisionModel>(std::move(soup));
    REQUIRE(model->TriangleCount() == 2);
    REQUIRE_FALSE(model->GetBvh().Empty());

    world::terrain::TerrainTile tile = BuildTile();
    world::terrain::StaticInstance inst;
    inst.xf = world::terrain::Transform(world::terrain::Vec3{1.f, 2.f, 3.f},
                                        world::terrain::Mat3{}, 2.0f);
    inst.model = model;
    inst.worldBounds.expand(world::terrain::Vec3{0.f, 0.f, 0.f});
    inst.worldBounds.expand(world::terrain::Vec3{20.f, 20.f, 10.f});
    inst.adtId = 42;
    tile.instances.push_back(inst);

    REQUIRE(WriteTile(tile, scratch.Path()));

    auto read = ReadTile(scratch.Path());
    REQUIRE(static_cast<bool>(read));
    REQUIRE(read->instances.size() == 1);

    const world::terrain::StaticInstance& back = read->instances[0];
    CHECK(back.adtId == 42);
    CHECK(back.xf.scale == doctest::Approx(2.0f));
    CHECK(back.xf.pos.x == doctest::Approx(1.f));
    CHECK(back.xf.pos.z == doctest::Approx(3.f));
    REQUIRE(static_cast<bool>(back.model));

    const auto* mesh =
        static_cast<const world::terrain::CollisionModel*>(back.model.get());

    REQUIRE(mesh->Soup().verts.size() == 4);
    REQUIRE(mesh->Soup().tris.size() == 2);
    CHECK(mesh->Soup().verts[1].x == doctest::Approx(10.f));
    CHECK(mesh->Soup().verts[3].z == doctest::Approx(5.f));
    CHECK_FALSE(mesh->GetBvh().Empty());

    // Mapped, not copied -- for the geometry and for the tree over it.
    CHECK_FALSE(mesh->Soup().verts.Owns());
    CHECK_FALSE(mesh->Soup().tris.Owns());
    CHECK_FALSE(mesh->GetBvh().Nodes().Owns());

    const uint8_t* base = read->mapping->Data();
    const uint8_t* end = base + read->mapping->Size();
    const uint8_t* verts = reinterpret_cast<const uint8_t*>(mesh->Soup().verts.data());
    CHECK(verts >= base);
    CHECK(verts < end);
}

TEST_CASE("A mapped model still answers a raycast")
{
    // Reading geometry in place has to give the same answers as reading a copy;
    // a ray through the middle of a known quad is the cheapest proof of that.
    ScratchPath scratch;

    world::terrain::TriSoup soup;
    soup.verts.Adopt(std::vector<world::terrain::Vec3>{
        {0.f, 0.f, 0.f}, {10.f, 0.f, 0.f}, {0.f, 10.f, 0.f}, {10.f, 10.f, 0.f}});
    soup.tris.Adopt(std::vector<std::array<uint32_t, 3>>{{0, 1, 2}, {1, 3, 2}});

    auto model = std::make_shared<world::terrain::CollisionModel>(std::move(soup));

    world::terrain::TerrainTile tile;
    tile.tx = 32;
    tile.ty = 32;
    world::terrain::StaticInstance inst;
    inst.model = model;
    inst.worldBounds.expand(world::terrain::Vec3{0.f, 0.f, -1.f});
    inst.worldBounds.expand(world::terrain::Vec3{10.f, 10.f, 1.f});
    tile.instances.push_back(inst);

    REQUIRE(WriteTile(tile, scratch.Path()));

    auto read = ReadTile(scratch.Path());
    REQUIRE(static_cast<bool>(read));
    REQUIRE(read->instances.size() == 1);

    const auto& mapped = *read->instances[0].model;
    CHECK_FALSE(mapped.Empty());

    // Straight down through the middle of the quad, from five above it.
    auto hit = mapped.RaycastNearest(world::terrain::Vec3{5.f, 5.f, 5.f},
                                     world::terrain::Vec3{0.f, 0.f, -1.f}, 100.f);
    REQUIRE(hit.has_value());
    CHECK(*hit == doctest::Approx(5.f));

    // And a ray that misses the quad entirely finds nothing.
    auto miss = mapped.RaycastNearest(world::terrain::Vec3{50.f, 50.f, 5.f},
                                      world::terrain::Vec3{0.f, 0.f, -1.f}, 100.f);
    CHECK_FALSE(miss.has_value());
}

TEST_CASE("A missing tile reads as nothing")
{
    auto read = ReadTile("no-such-file.tile");
    CHECK_FALSE(static_cast<bool>(read));
}

TEST_CASE("A truncated tile is refused rather than read past the end")
{
    ScratchPath scratch;
    REQUIRE(WriteTile(BuildTile(), scratch.Path()));

    // Keep only the first part of the file.
    std::vector<uint8_t> bytes;
    {
        std::FILE* f = std::fopen(scratch.Path().c_str(), "rb");
        REQUIRE(f != nullptr);
        std::fseek(f, 0, SEEK_END);
        const long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        bytes.resize(size_t(size));
        REQUIRE(std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size());
        std::fclose(f);
    }
    REQUIRE(bytes.size() > 64);

    {
        std::FILE* f = std::fopen(scratch.Path().c_str(), "wb");
        REQUIRE(f != nullptr);
        std::fwrite(bytes.data(), 1, 64, f);
        std::fclose(f);
    }

    auto read = ReadTile(scratch.Path());
    CHECK_FALSE(static_cast<bool>(read));
}

TEST_CASE("A tile whose magic or version is wrong is refused")
{
    ScratchPath scratch;
    REQUIRE(WriteTile(BuildTile(), scratch.Path()));

    // Corrupt the first byte of the magic.
    {
        std::FILE* f = std::fopen(scratch.Path().c_str(), "r+b");
        REQUIRE(f != nullptr);
        const uint8_t wrong = 0x00;
        std::fwrite(&wrong, 1, 1, f);
        std::fclose(f);
    }

    auto read = ReadTile(scratch.Path());
    CHECK_FALSE(static_cast<bool>(read));
}
