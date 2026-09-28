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
#include <optional>
#include <string>
#include <vector>

namespace world::terrain
{
    struct WdtData
    {
        bool hasGlobalWmo = false;
        bool hasMainChunk = false;
        std::string globalWmoName;
        std::optional<Placement> globalWmoPlacement;
        uint32_t mphdFlags = 0;

        std::array<std::array<bool, 64>, 64> adtGrid{};

        bool HasAdt(int tx, int ty) const
        {
            if (tx < 0 || tx > 63 || ty < 0 || ty > 63)
            {
                return false;
            }
            return adtGrid[size_t(tx)][size_t(ty)];
        }

        bool HasAnyAdt() const
        {
            for (const auto& row : adtGrid)
            {
                for (bool v : row)
                {
                    if (v)
                    {
                        return true;
                    }
                }
            }
            return false;
        }
    };

    bool ParseWdt(const uint8_t* data, size_t size, WdtData& out);

    inline bool ParseWdt(const std::vector<uint8_t>& bytes, WdtData& out)
    {
        return ParseWdt(bytes.data(), bytes.size(), out);
    }
}
