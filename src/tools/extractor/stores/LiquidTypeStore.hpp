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

#include "MpqDbcLoader.hpp"
#include "terrain/Terrain.hpp"

#include "Server/DBCfmt.h"

#include <cstdint>
#include <unordered_map>

namespace world
{
    enum class LiquidDbcType : uint32_t
    {
        Magma = 0,
        Slime = 2,
        Water = 3
    };

    constexpr uint32_t LIQUID_ROW_WATER = 1;
    constexpr uint32_t LIQUID_ROW_OCEAN = 2;
    constexpr uint32_t LIQUID_ROW_MAGMA = 3;
    constexpr uint32_t LIQUID_ROW_SLIME = 4;
    constexpr uint32_t LIQUID_ROW_NAXX_SLIME = 21;

    struct LiquidTypeInfo
    {
        uint32_t type = 0;
        uint32_t spellId = 0;
    };

    class LiquidTypeStore
    {
    public:
        bool LoadFromDbc(world::terrain::IMpqArchive& archive)
        {
            DBCFileLoader dbc;
            if (!LoadDbcFromMpq(archive, "DBFilesClient\\LiquidType.dbc", LiquidTypefmt, dbc))
            {
                return false;
            }
            if (dbc.GetCols() < COLUMN_COUNT)
            {
                return false;
            }

            m_entries.clear();
            for (uint32_t r = 0; r < dbc.GetNumRows(); ++r)
            {
                DBCFileLoader::Record rec = dbc.getRecord(r);
                LiquidTypeInfo info;
                info.type = rec.getUInt(COLUMN_TYPE);
                info.spellId = rec.getUInt(COLUMN_SPELL);
                m_entries[rec.getUInt(COLUMN_ID)] = info;
            }
            return true;
        }

        const LiquidTypeInfo* Find(uint32_t id) const
        {
            auto it = m_entries.find(id);
            return it != m_entries.end() ? &it->second : nullptr;
        }

        size_t Size() const { return m_entries.size(); }

    private:
        static constexpr uint32_t COLUMN_ID = 0;
        static constexpr uint32_t COLUMN_TYPE = 2;
        static constexpr uint32_t COLUMN_SPELL = 3;
        static constexpr uint32_t COLUMN_COUNT = 4;

        std::unordered_map<uint32_t, LiquidTypeInfo> m_entries;
    };

    inline world::terrain::LiquidKind ClassifyLiquid(uint32_t entry,
                                                     const LiquidTypeStore* store)
    {
        using world::terrain::LiquidKind;
        if (!entry)
        {
            return LiquidKind::None;
        }

        if (store)
        {
            if (const LiquidTypeInfo* info = store->Find(entry))
            {
                switch (static_cast<LiquidDbcType>(info->type))
                {
                    case LiquidDbcType::Magma: return LiquidKind::Magma;
                    case LiquidDbcType::Slime: return LiquidKind::Slime;
                    default:
                        return entry == LIQUID_ROW_OCEAN ? LiquidKind::Ocean
                                                         : LiquidKind::Water;
                }
            }
        }

        switch (entry)
        {
            case LIQUID_ROW_OCEAN: return LiquidKind::Ocean;
            case LIQUID_ROW_MAGMA: return LiquidKind::Magma;
            case LIQUID_ROW_SLIME:
            case LIQUID_ROW_NAXX_SLIME: return LiquidKind::Slime;
            default: return LiquidKind::Water;
        }
    }
}
