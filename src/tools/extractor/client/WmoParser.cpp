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

#include <string>
#include "WmoParser.hpp"
#include "ChunkReaders.hpp"

#include <cstdio>
#include <cstring>

namespace world::terrain
{
    namespace
    {
        using namespace world::terrain::internal;

        constexpr uint8_t MATERIAL_DETAIL = 0x04;
        constexpr uint8_t MATERIAL_COLLISION = 0x08;
        constexpr uint8_t MATERIAL_RENDER = 0x20;

        constexpr uint32_t MOHD_USE_LIQUID_DBC_ID = 0x4;
        constexpr uint32_t MOGP_LIQUID_IS_OCEAN = 0x80000;

        constexpr uint32_t LIQUID_NONE = 15;
        constexpr uint32_t LIQUID_FIRST_DBC_ID = 21;

        constexpr size_t MOGP_HEADER = 68;
        constexpr size_t MOGP_FLAGS = 0x08;
        constexpr size_t MOGP_GROUP_LIQUID = 0x34;
        constexpr size_t MOGP_UNIQUE_ID = 0x38;

        constexpr size_t MOHD_N_GROUPS = 0x04;
        constexpr size_t MOHD_WMO_ID = 0x20;
        constexpr size_t MOHD_FLAGS = 0x3C;

        constexpr uint32_t ROW_WATER = 1;
        constexpr uint32_t ROW_OCEAN = 2;
        constexpr uint32_t ROW_MAGMA = 3;
        constexpr uint32_t ROW_SLIME = 4;

        uint32_t CanonicalLiquidEntry(uint32_t entry, uint32_t mogpFlags)
        {
            if (!entry || entry >= LIQUID_FIRST_DBC_ID)
            {
                return entry;
            }

            switch ((entry - 1u) & 3u)
            {
                case 0:
                    return ((mogpFlags & MOGP_LIQUID_IS_OCEAN) != 0) ? ROW_OCEAN : ROW_WATER;
                case 1:
                    return ROW_OCEAN;
                case 2:
                    return ROW_MAGMA;
                case 3:
                    return ROW_SLIME;
                default:
                    return entry;
            }
        }

        void ReadDoodads(const uint8_t* d, size_t n, WmoRootData& out)
        {
            const uint8_t* mods = nullptr;
            uint32_t modsSize = 0;
            const uint8_t* modn = nullptr;
            uint32_t modnSize = 0;
            const uint8_t* modd = nullptr;
            uint32_t moddSize = 0;

            size_t pos = 0;
            while (pos + 8 <= n)
            {
                const uint32_t sz = RdU32(d + pos + 4);
                if (pos + 8 + sz > n)
                {
                    break;
                }
                if (TagIs(d + pos, "MODS"))
                {
                    mods = d + pos + 8;
                    modsSize = sz;
                }
                else if (TagIs(d + pos, "MODN"))
                {
                    modn = d + pos + 8;
                    modnSize = sz;
                }
                else if (TagIs(d + pos, "MODD"))
                {
                    modd = d + pos + 8;
                    moddSize = sz;
                }
                pos += 8 + sz;
            }

            if (!modd || !modn || moddSize < 40)
            {
                return;
            }

            if (mods)
            {
                const uint32_t nSets = modsSize / 32;
                out.sets.reserve(nSets);
                for (uint32_t i = 0; i < nSets; ++i)
                {
                    const uint8_t* p = mods + i * 32;
                    out.sets.push_back({RdU32(p + 20), RdU32(p + 24)});
                }
            }
            if (out.sets.empty())
            {
                out.sets.push_back({0, moddSize / 40});
            }

            const uint32_t nDefs = moddSize / 40;
            out.doodads.reserve(nDefs);
            for (uint32_t i = 0; i < nDefs; ++i)
            {
                const uint8_t* p = modd + i * 40;
                const uint32_t nameOfs = RdU32(p + 0) & 0x00FFFFFFu;

                WmoDoodad dd;
                if (nameOfs < modnSize)
                {
                    const char* s = reinterpret_cast<const char*>(modn + nameOfs);
                    dd.name.assign(s, ::strnlen(s, modnSize - nameOfs));
                }
                dd.pos = {RdF32(p + 4), RdF32(p + 8), RdF32(p + 12)};
                dd.quat[0] = RdF32(p + 16);
                dd.quat[1] = RdF32(p + 20);
                dd.quat[2] = RdF32(p + 24);
                dd.quat[3] = RdF32(p + 28);
                dd.scale = RdF32(p + 32);
                if (!(dd.scale > 0.f))
                {
                    dd.scale = 1.f;
                }
                out.doodads.push_back(std::move(dd));
            }
        }
    }

    std::string WmoGroupPath(const std::string& root, uint32_t index)
    {
        std::string base = root;
        const size_t dot = base.find_last_of('.');
        if (dot != std::string::npos)
        {
            base.erase(dot);
        }
        char suffix[16];
        std::snprintf(suffix, sizeof(suffix), "_%03u.wmo", index);
        return base + suffix;
    }

    bool ParseWmoRoot(const uint8_t* d, size_t n, WmoRootData& out)
    {
        out = WmoRootData{};
        if (!d || n < 8)
        {
            return false;
        }

        bool sawHeader = false;
        size_t pos = 0;
        while (pos + 8 <= n)
        {
            const uint32_t sz = RdU32(d + pos + 4);
            if (pos + 8 + sz > n)
            {
                break;
            }
            if (TagIs(d + pos, "MOHD") && sz >= 8)
            {
                const uint8_t* h = d + pos + 8;
                out.nGroups = RdU32(h + MOHD_N_GROUPS);
                if (sz >= MOHD_WMO_ID + 4)
                {
                    out.wmoId = RdU32(h + MOHD_WMO_ID);
                }
                if (sz >= MOHD_FLAGS + 4)
                {
                    out.flags = RdU32(h + MOHD_FLAGS);
                }
                sawHeader = true;
            }
            pos += 8 + sz;
        }

        ReadDoodads(d, n, out);
        return sawHeader;
    }

    bool ParseWmoGroup(const uint8_t* d, size_t n, uint32_t rootFlags, WmoGroupData& out)
    {
        out = WmoGroupData{};
        if (!d || n < 8)
        {
            return false;
        }

        const uint8_t* mopy = nullptr;
        uint32_t mopySize = 0;
        const uint8_t* movi = nullptr;
        uint32_t moviSize = 0;
        const uint8_t* movt = nullptr;
        uint32_t movtSize = 0;
        const uint8_t* mliq = nullptr;
        uint32_t mliqSize = 0;
        const uint8_t* mogp = nullptr;

        size_t pos = 0;
        while (pos + 8 <= n)
        {
            const uint8_t* tag = d + pos;
            const uint32_t sz = RdU32(d + pos + 4);
            uint32_t advance = sz;
            if (TagIs(tag, "MOGP"))
            {
                advance = MOGP_HEADER;
                mogp = d + pos + 8;
            }
            else if (TagIs(tag, "MOPY"))
            {
                mopy = d + pos + 8;
                mopySize = sz;
            }
            else if (TagIs(tag, "MOVI"))
            {
                movi = d + pos + 8;
                moviSize = sz;
            }
            else if (TagIs(tag, "MOVT"))
            {
                movt = d + pos + 8;
                movtSize = sz;
            }
            else if (TagIs(tag, "MLIQ"))
            {
                mliq = d + pos + 8;
                mliqSize = sz;
            }

            if (advance != MOGP_HEADER && pos + 8 + sz > n)
            {
                break;
            }
            pos += 8 + advance;
        }

        uint32_t groupLiquid = 0;
        if (mogp && mogp + MOGP_HEADER <= d + n)
        {
            out.mogpFlags = RdU32(mogp + MOGP_FLAGS);
            groupLiquid = RdU32(mogp + MOGP_GROUP_LIQUID);
            out.groupWmoId = RdU32(mogp + MOGP_UNIQUE_ID);
        }

        if (mliq && mliqSize >= 30)
        {
            const uint32_t xverts = RdU32(mliq + 0), yverts = RdU32(mliq + 4);
            const uint32_t xtiles = RdU32(mliq + 8), ytiles = RdU32(mliq + 12);
            const float px = RdF32(mliq + 16), py = RdF32(mliq + 20), pz = RdF32(mliq + 24);
            const uint64_t vbytes = uint64_t(xverts) * yverts * 8;
            const uint64_t fbytes = uint64_t(xtiles) * ytiles;

            if (xverts && yverts && xtiles && ytiles && 30 + vbytes + fbytes <= mliqSize)
            {
                const uint8_t* verts = mliq + 30;
                const uint8_t* tileFlags = verts + vbytes;

                out.hasLiquid = true;
                out.liquid.tilesX = xtiles;
                out.liquid.tilesY = ytiles;
                out.liquid.corner = {px, py, pz};
                out.liquid.heights.resize(size_t(xverts) * yverts);
                for (size_t i = 0; i < out.liquid.heights.size(); ++i)
                {
                    out.liquid.heights[i] = RdF32(verts + i * 8 + 4);
                }
                out.liquid.flags.assign(tileFlags, tileFlags + fbytes);

                uint32_t entry;
                if (rootFlags & MOHD_USE_LIQUID_DBC_ID)
                {
                    entry = groupLiquid;
                }
                else if (groupLiquid == LIQUID_NONE)
                {
                    entry = 0;
                }
                else
                {
                    entry = groupLiquid + 1;
                }

                if (!entry)
                {
                    for (uint8_t tf : out.liquid.flags)
                    {
                        if ((tf & 0x0F) != LIQUID_NONE)
                        {
                            entry = uint32_t(tf & 0x0F) + 1u;
                            break;
                        }
                    }
                }

                out.liquid.entry =
                    static_cast<uint16_t>(CanonicalLiquidEntry(entry, out.mogpFlags));
            }
        }

        if (!movi || !movt)
        {
            return out.hasLiquid;
        }

        const uint32_t nVert = movtSize / 12;
        out.verts.reserve(nVert);
        for (uint32_t v = 0; v < nVert; ++v)
        {
            out.verts.push_back({RdF32(movt + v * 12 + 0), RdF32(movt + v * 12 + 4),
                                 RdF32(movt + v * 12 + 8)});
        }

        const uint32_t nTri = moviSize / 6;
        out.tris.reserve(nTri);
        for (uint32_t t = 0; t < nTri; ++t)
        {
            const uint8_t flags =
                (mopy && 2 * t < mopySize) ? static_cast<uint8_t>(mopy[2 * t]) : 0;

            const bool isRenderFace = (flags & MATERIAL_RENDER) && !(flags & MATERIAL_DETAIL);
            const bool collides = (flags & MATERIAL_COLLISION) || isRenderFace;
            if (mopy && mopySize != 0 && !collides)
            {
                continue;
            }

            const uint16_t a = RdU16(movi + (3 * t + 0) * 2);
            const uint16_t b = RdU16(movi + (3 * t + 1) * 2);
            const uint16_t c = RdU16(movi + (3 * t + 2) * 2);
            if (a >= nVert || b >= nVert || c >= nVert)
            {
                continue;
            }
            out.tris.push_back({a, b, c});
        }

        return !out.tris.empty() || out.hasLiquid;
    }
}
