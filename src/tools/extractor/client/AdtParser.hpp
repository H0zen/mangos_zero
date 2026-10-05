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

#include "ChunkReaders.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace world::terrain
{
    constexpr int ADT_CHUNKS = 16;
    constexpr int ADT_CELLS_PER_CHUNK = 8;
    constexpr int ADT_GRID = ADT_CHUNKS * ADT_CELLS_PER_CHUNK;
    constexpr int ADT_V9 = ADT_GRID + 1;

    struct AdtData
    {
        bool hasTerrain = false;
        std::vector<float> v9;
        std::vector<float> v8;
        std::array<uint16_t, ADT_CHUNKS * ADT_CHUNKS> holes{};
        std::array<uint16_t, ADT_CHUNKS * ADT_CHUNKS> areaIds{};

        bool hasLiquid = false;
        bool hasMh2o = false;
        std::vector<float> liquidHeight;
        std::vector<uint8_t> liquidShow;
        std::vector<uint16_t> liquidEntry;
        std::vector<uint8_t> liquidDark;
        std::vector<uint8_t> liquidNoLight;

        std::vector<std::string> wmoNames;
        std::vector<std::string> m2Names;
        std::vector<Placement> wmoPlacements;
        std::vector<Placement> m2Placements;
    };

    bool ParseAdt(const uint8_t* data, size_t size, AdtData& out);

    inline bool ParseAdt(const std::vector<uint8_t>& bytes, AdtData& out)
    {
        return ParseAdt(bytes.data(), bytes.size(), out);
    }
}
