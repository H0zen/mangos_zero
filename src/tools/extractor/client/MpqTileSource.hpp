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
#include "ModelLoaders.hpp"
#include "WdtParser.hpp"
#include "stores/MapDbcStore.hpp"
#include "terrain/Terrain.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace world::terrain
{
    class MpqTileSource : public ITileSource
    {
    public:
        MpqTileSource(IMpqArchive& archive, const world::MapDbcStore* maps,
                      const world::LiquidTypeStore* liquidTypes)
            : m_archive(archive), m_maps(maps), m_liquidTypes(liquidTypes),
              m_wmo(archive, liquidTypes), m_m2(archive) {}

        std::shared_ptr<TerrainTile> Load(uint32_t mapId, int tx, int ty) override;

        void SetLoadStatics(bool on) { m_loadStatics = on; }

        std::string MapDirectory(uint32_t mapId) const;
        std::string AdtPath(uint32_t mapId, int tx, int ty) const;
        std::string WdtPath(uint32_t mapId) const;

        const WdtData* Wdt(uint32_t mapId);

    private:
        std::shared_ptr<TerrainTile> LoadAdt(uint32_t mapId, int tx, int ty);
        std::shared_ptr<TerrainTile> LoadGlobalWmo(uint32_t mapId);
        void AttachWmoDoodads(const Placement& p, const std::string& wmoPath,
                              const Transform& wmoXf, TerrainTile& tile);

        IMpqArchive& m_archive;
        const world::MapDbcStore* m_maps;
        const world::LiquidTypeStore* m_liquidTypes;
        WmoLoader m_wmo;
        M2Loader m_m2;
        bool m_loadStatics = true;

        std::unordered_map<uint32_t, WdtData> m_wdtCache;
        std::unordered_map<uint32_t, std::shared_ptr<TerrainTile>> m_globalWmoCache;
    };
}
