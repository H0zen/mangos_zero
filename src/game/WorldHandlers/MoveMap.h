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

#ifndef MANGOS_H_MOVE_MAP
#define MANGOS_H_MOVE_MAP

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include "../../dep/recastnavigation/Detour/Include/DetourAlloc.h"
#include "../../dep/recastnavigation/Detour/Include/DetourNavMesh.h"
#include "../../dep/recastnavigation/Detour/Include/DetourNavMeshQuery.h"
#include "MoveMapSharedDefines.h"

#include "Platform/Define.h"

class Unit;

inline void* dtCustomAlloc(size_t size, dtAllocHint /*hint*/)
{
    return (void*)new unsigned char[size];
}

inline void dtCustomFree(void* ptr)
{
    delete[](unsigned char*)ptr;
}

namespace MMAP
{
    typedef std::unordered_map<uint32, dtTileRef> MMapTileSet;

    struct MMapData
    {
        MMapData(dtNavMesh* mesh, uint64 serial) : navMesh(mesh), serial(serial) {}
        ~MMapData() { dtFreeNavMesh(navMesh); }

        MMapData(MMapData const&) = delete;
        MMapData& operator=(MMapData const&) = delete;

        dtNavMesh* const navMesh;
        uint64 const serial;
        mutable std::shared_mutex tilesLock;
        MMapTileSet mmapLoadedTiles;
    };

    typedef std::unordered_map<uint64, std::shared_ptr<MMapData>> MMapDataSet;

    class NavMeshLease
    {
        public:
            NavMeshLease() = default;
            NavMeshLease(std::shared_ptr<MMapData> data, dtNavMeshQuery const* query)
                : m_data(std::move(data)), m_lock(m_data->tilesLock), m_query(query) {}

            explicit operator bool() const { return m_data && m_query; }
            dtNavMesh const* Mesh() const { return m_data ? m_data->navMesh : NULL; }
            dtNavMeshQuery const* Query() const { return m_query; }

        private:
            std::shared_ptr<MMapData> m_data;
            std::shared_lock<std::shared_mutex> m_lock;
            dtNavMeshQuery const* m_query = NULL;
    };

    class MMapManager
    {
        public:
            bool loadMap(uint32 mapId, int32 x, int32 y);
            bool unloadMap(uint32 mapId, int32 x, int32 y);
            bool unloadMap(uint32 mapId);

            NavMeshLease Lease(uint32 mapId, NavAgent agent);

            uint32 getLoadedTilesCount() const { return loadedTiles; }
            uint32 getLoadedMapsCount() const;
        private:
            std::shared_ptr<MMapData> Find(uint32 mapId, NavAgent agent) const;
            std::shared_ptr<MMapData> loadMapData(uint32 mapId, NavAgent agent);
            bool loadTile(uint32 mapId, NavAgent agent, int32 x, int32 y);
            bool unloadTile(uint32 mapId, NavAgent agent, int32 x, int32 y);
            uint32 packTileID(int32 x, int32 y);

            mutable std::shared_mutex mapsLock;
            MMapDataSet loadedMMaps;
            std::atomic<uint32> loadedTiles{0};
            std::atomic<uint64> nextSerial{1};
    };

    class MMapFactory
    {
        public:
            static MMapManager* createOrGetMMapManager();
            static void clear();
            static void preventPathfindingOnMaps(const char* ignoreMapIds);
            static bool IsPathfindingEnabled(uint32 mapId, const Unit* unit);
            static bool IsPathfindingForceEnabled(const Unit* unit);
            static bool IsPathfindingForceDisabled(const Unit* unit);
    };
}

#endif  // _MOVE_MAP_H
