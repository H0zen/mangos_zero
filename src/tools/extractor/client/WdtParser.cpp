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

#include "WdtParser.hpp"

#include <algorithm>

namespace world::terrain
{
    using namespace world::terrain::internal;

    bool ParseWdt(const uint8_t* data, size_t size, WdtData& out)
    {
        if (!data || size < 8)
        {
            return false;
        }

        out = WdtData{};

        const uint8_t* mwmo = nullptr;
        uint32_t mwmoSize = 0;
        const uint8_t* modf = nullptr;
        uint32_t modfSize = 0;

        size_t pos = 0;
        while (pos + 8 <= size)
        {
            const uint8_t* tag = data + pos;
            const uint32_t csize = RdU32(data + pos + 4);
            const uint8_t* body = data + pos + 8;
            if (pos + 8 + csize > size)
            {
                break;
            }

            if (TagIs(tag, "MPHD"))
            {
                if (csize >= 4)
                {
                    out.mphdFlags = RdU32(body);
                    out.hasGlobalWmo = (out.mphdFlags & 0x0001) != 0;
                }
            }
            else if (TagIs(tag, "MAIN"))
            {
                constexpr size_t ENTRY = 8;
                const size_t entries = std::min<size_t>(csize / ENTRY, 64ULL * 64);
                for (size_t i = 0; i < entries; ++i)
                {
                    out.adtGrid[i / 64][i % 64] = (RdU32(body + i * ENTRY) & 0x1) != 0;
                }
                out.hasMainChunk = true;
            }
            else if (TagIs(tag, "MWMO"))
            {
                mwmo = body;
                mwmoSize = csize;
            }
            else if (TagIs(tag, "MODF"))
            {
                modf = body;
                modfSize = csize;
            }

            pos += 8 + csize;
        }

        if (out.hasGlobalWmo && mwmo && mwmoSize > 0)
        {
            const char* s = reinterpret_cast<const char*>(mwmo);
            uint32_t len = 0;
            while (len < mwmoSize && s[len] != '\0')
            {
                ++len;
            }
            out.globalWmoName.assign(s, len);
        }

        if (out.hasGlobalWmo && modf && modfSize >= 64)
        {
            out.globalWmoPlacement = ReadModf(modf);
        }

        return true;
    }
}
