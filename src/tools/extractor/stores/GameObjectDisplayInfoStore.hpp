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

#include "Server/DBCfmt.h"

#include <cctype>
#include <cstdint>
#include <map>
#include <string>

namespace world
{
    class GameObjectDisplayInfoStore
    {
    public:
        bool LoadFromDbc(world::terrain::IMpqArchive& archive)
        {
            DBCFileLoader dbc;
            if (!LoadDbcFromMpq(archive, "DBFilesClient\\GameObjectDisplayInfo.dbc",
                                GameObjectDisplayInfofmt, dbc))
            {
                return false;
            }

            m_models.clear();
            for (uint32_t r = 0; r < dbc.GetNumRows(); ++r)
            {
                DBCFileLoader::Record rec = dbc.getRecord(r);
                const char* name = rec.getString(1);
                if (name && *name)
                {
                    m_models[rec.getUInt(0)] = name;
                }
            }
            return true;
        }

        const std::map<uint32_t, std::string>& All() const { return m_models; }

        static bool IsWmo(const std::string& path)
        {
            return path.size() >= 4 &&
                   std::tolower(static_cast<unsigned char>(path[path.size() - 3])) == 'w' &&
                   std::tolower(static_cast<unsigned char>(path[path.size() - 2])) == 'm' &&
                   std::tolower(static_cast<unsigned char>(path[path.size() - 1])) == 'o';
        }

    private:
        std::map<uint32_t, std::string> m_models;
    };
}
