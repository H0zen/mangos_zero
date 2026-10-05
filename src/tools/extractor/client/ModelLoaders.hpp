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

#include "IMpqArchive.hpp"
#include "WmoParser.hpp"
#include "stores/LiquidTypeStore.hpp"
#include "terrain/ICollisionModel.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace world::terrain
{
    class WmoLoader
    {
    public:
        WmoLoader(IMpqArchive& archive, const world::LiquidTypeStore* liquidTypes)
            : m_archive(archive), m_liquidTypes(liquidTypes) {}

        std::shared_ptr<const ICollisionModel> Load(const std::string& rootPath);

        const WmoRootData* Root(const std::string& rootPath);

    private:
        IMpqArchive& m_archive;
        const world::LiquidTypeStore* m_liquidTypes;
        std::unordered_map<std::string, std::shared_ptr<const ICollisionModel>> m_cache;
        std::unordered_map<std::string, WmoRootData> m_roots;
    };

    class M2Loader
    {
    public:
        explicit M2Loader(IMpqArchive& archive) : m_archive(archive) {}

        std::shared_ptr<const ICollisionModel> Load(const std::string& path);

    private:
        IMpqArchive& m_archive;
        std::unordered_map<std::string, std::shared_ptr<const ICollisionModel>> m_cache;
    };
}
