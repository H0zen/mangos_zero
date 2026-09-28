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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace world::terrain { class TerrainTile; }

namespace world::nav
{
    struct NavConfig
    {
        float cellSize = 0.266666f;
        float maxWalkableAngle = 60.0f;
        int walkableHeight = 6;
        int walkableClimb = 4;
        int walkableRadius = 2;
        int subTileSize = 80;
        int threads = 0;
        std::string offMeshFile;
    };

    void SubTileSpan(float lo, float hi, float origin, float width, float pad, int side,
                     int& first, int& last);

    struct CellRect
    {
        int ixFirst = 0;
        int ixLast = 0;
        int iyFirst = 0;
        int iyLast = 0;
    };

    bool NeighbourCellRect(int deltaGx, int deltaGy, CellRect& out);

    class NavMeshBuilder
    {
    public:
        NavMeshBuilder(std::string tileDir, std::string outDir, NavConfig cfg = {});

        using ProgressFn = void (*)(void* context, uint32_t mapId, const char* mapName,
                                    size_t done, size_t total);
        void SetProgress(ProgressFn fn, void* context);

        using MapDoneFn = void (*)(void* context, uint32_t mapId, const char* mapName,
                                   int written, size_t total);
        void SetMapDone(MapDoneFn fn);

        int BakeAll(long mapFilter = -1);

    private:
        int BakeMap(uint32_t mapId, const std::string& mapName,
                    const std::vector<std::pair<int, int>>& grids,
                    std::shared_ptr<const world::terrain::TerrainTile> globalWmo = nullptr);

        std::string m_tileDir;
        std::string m_outDir;
        NavConfig m_cfg;
        ProgressFn m_progress = nullptr;
        void* m_progressContext = nullptr;
        MapDoneFn m_mapDone = nullptr;
    };
}
