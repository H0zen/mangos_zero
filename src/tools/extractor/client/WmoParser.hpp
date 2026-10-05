/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
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

#pragma once

#include "terrain/Geometry.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace world::terrain
{
    struct WmoLiquid
    {
        uint32_t tilesX = 0;
        uint32_t tilesY = 0;
        Vec3 corner{};
        std::vector<float> heights;
        std::vector<uint8_t> flags;
        uint16_t entry = 0;
    };

    struct WmoGroupData
    {
        uint32_t mogpFlags = 0;
        uint32_t groupWmoId = 0;
        std::vector<Vec3> verts;
        std::vector<std::array<uint16_t, 3>> tris;
        bool hasLiquid = false;
        WmoLiquid liquid;
    };

    struct WmoDoodad
    {
        std::string name;
        Vec3 pos{};
        float quat[4] = {0.f, 0.f, 0.f, 1.f};
        float scale = 1.f;
    };

    struct WmoDoodadSet
    {
        uint32_t start = 0;
        uint32_t count = 0;
    };

    struct WmoRootData
    {
        uint32_t nGroups = 0;
        uint32_t wmoId = 0;
        uint32_t flags = 0;
        std::vector<WmoDoodadSet> sets;
        std::vector<WmoDoodad> doodads;
    };

    bool ParseWmoRoot(const uint8_t* data, size_t size, WmoRootData& out);

    bool ParseWmoGroup(const uint8_t* data, size_t size, uint32_t rootFlags,
                       WmoGroupData& out);

    std::string WmoGroupPath(const std::string& root, uint32_t index);

    inline bool ParseWmoRoot(const std::vector<uint8_t>& b, WmoRootData& out)
    {
        return ParseWmoRoot(b.data(), b.size(), out);
    }

    inline bool ParseWmoGroup(const std::vector<uint8_t>& b, uint32_t rootFlags,
                              WmoGroupData& out)
    {
        return ParseWmoGroup(b.data(), b.size(), rootFlags, out);
    }
}
