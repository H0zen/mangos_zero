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

/**
 * @file MoveMap.cpp
 * @brief Navigation mesh (MMAP) pathfinding system
 *
 * This file implements the MMAP (MoveMap) system which provides
 * pathfinding capabilities using Recast/Detour navigation meshes.
 *
 * Features:
 * - Navigation mesh loading and management per map tile
 * - Pathfinding query interface for units
 * - Configurable pathfinding per map/unit type
 * - Height and slope limit validation
 *
 * Pathfinding can be disabled globally via config, per map via
 * database, or per unit via creature flags.
 *
 * @see MMapManager for the singleton manager
 * @see MMapFactory for creation utilities
 * @see DetourNavMesh for underlying navigation mesh
 */

#include "Utilities/Errors.h"
#include <string>
#include <set>
#include <vector>
#include "Log.h"
#include "World.h"
#include "Creature.h"
#include "MoveMap.h"
#include "MoveMapSharedDefines.h"

namespace
{
    const int NAV_QUERY_NODES = 1024;

    uint64 MeshKey(uint32 mapId, NavAgent agent)
    {
        return (uint64(mapId) << 8) | uint64(agent);
    }

    std::string MMapFileName(uint32 mapId, NavAgent agent)
    {
        char leaf[96];
        snprintf(leaf, sizeof(leaf), "mmaps/%s/%04u.mmap", NAV_AGENTS[size_t(agent)].dir, mapId);
        return sWorld.GetDataPath() + leaf;
    }

    std::string MMapTileFileName(uint32 mapId, NavAgent agent, int32 x, int32 y)
    {
        char leaf[96];
        snprintf(leaf, sizeof(leaf), "mmaps/%s/%04u%02i%02i.mmtile", NAV_AGENTS[size_t(agent)].dir, mapId, x, y);
        return sWorld.GetDataPath() + leaf;
    }

    struct QueryDeleter
    {
        void operator()(dtNavMeshQuery* query) const { dtFreeNavMeshQuery(query); }
    };

    struct ThreadQuery
    {
        uint64 serial = 0;
        std::unique_ptr<dtNavMeshQuery, QueryDeleter> query;
    };

    dtNavMeshQuery const* QueryOnThisThread(uint64 meshKey, MMAP::MMapData const& data)
    {
        thread_local std::unordered_map<uint64, ThreadQuery> queries;
        ThreadQuery& mine = queries[meshKey];
        if (mine.serial == data.serial && mine.query)
        {
            return mine.query.get();
        }

        mine.serial = data.serial;
        mine.query.reset(dtAllocNavMeshQuery());
        if (!mine.query || dtStatusFailed(mine.query->init(data.navMesh, NAV_QUERY_NODES)))
        {
            sLog.outError("MMAP: no query for mesh %llu on this thread", (unsigned long long)meshKey);
            mine.query.reset();
        }
        return mine.query.get();
    }

    bool ReadTileData(FILE* file, MmapTileHeader const& header, std::vector<unsigned char>& out)
    {
        const long dataStart = ftell(file);
        fseek(file, 0, SEEK_END);
        const long dataEnd = ftell(file);
        fseek(file, dataStart, SEEK_SET);
        if (header.size == 0 || dataStart < 0 || dataEnd < dataStart ||
            uint64(header.size) > uint64(dataEnd - dataStart))
        {
            return false;
        }
        out.resize(header.size);
        return fread(out.data(), header.size, 1, file) == 1;
    }
}

namespace MMAP
{

    /**
     * @namespace MMAP
     * @brief MoveMap pathfinding namespace
     *
     * Contains all MMAP-related classes and functions for navigation
     * mesh pathfinding. Key components:
     *
     * - MMapFactory: Factory for creating/accessing MMapManager
     * - MMapManager: Singleton managing all navigation meshes
     * - MMapData: Per-map navigation mesh data container
     */

    /**
     * @var g_MMapManager
     * @brief Global singleton MMapManager instance
     */
    MMapManager* g_MMapManager = NULL;

    /**
     * @var g_mmapDisabledIds
     * @brief Set of map IDs with disabled pathfinding
     *
     * Maps in this set will not load navigation meshes and all
     * pathfinding requests will fall back to legacy methods.
     */
    std::set<uint32>* g_mmapDisabledIds = NULL;

    /**
     * @brief Create or retrieve the MMapManager singleton
     * @return Pointer to the MMapManager instance
     *
     * Creates the manager on first call. All subsequent calls return
     * the existing instance.
     */
    MMapManager* MMapFactory::createOrGetMMapManager()
    {
        if (g_MMapManager == NULL)
        {
            g_MMapManager = new MMapManager();
        }

        return g_MMapManager;
    }

    /**
     * @brief Disable pathfinding on specified maps
     * @param ignoreMapIds Comma-separated list of map IDs
     *
     * Parses the configuration string and adds each map ID to the
     * disabled set. Maps in this set will not use pathfinding.
     *
     * Example: "0,1,489" disables pathfinding for Eastern Kingdoms,
     * Kalimdor, and Warsong Gulch.
     */
    void MMapFactory::preventPathfindingOnMaps(const char* ignoreMapIds)
    {
        if (!g_mmapDisabledIds)
        {
            g_mmapDisabledIds = new std::set<uint32>();
        }

        uint32 strLenght = strlen(ignoreMapIds) + 1;
        char* mapList = new char[strLenght];
        memcpy(mapList, ignoreMapIds, sizeof(char)*strLenght);

        char* idstr = strtok(mapList, ",");
        while (idstr)
        {
            g_mmapDisabledIds->insert(uint32(atoi(idstr)));
            idstr = strtok(NULL, ",");
        }

        delete[] mapList;
    }

    /**
     * @brief Check if pathfinding is enabled for a map and unit
     * @param mapId Map ID to check
     * @param unit Unit requesting path (optional, affects per-unit checks)
     * @return true if pathfinding should be used
     *
     * Pathfinding is enabled if:
     * - Global MMAP config is enabled
     * - Map is not in disabled list
     * - Unit-specific checks pass (players always enabled, pets inherit,
     *   creatures check flags)
     *
     * @note Players always have pathfinding enabled if global config is on
     */
    bool MMapFactory::IsPathfindingEnabled(uint32 mapId, const Unit* unit = NULL)
    {
        if (!sWorld.getConfig(CONFIG_BOOL_MMAP_ENABLED))
        {
            return false;
        }

        if (unit)
        {
            // Always use mmaps for players
            if (unit->GetTypeId() == TYPEID_PLAYER)
            {
                return true;
            }

            if (IsPathfindingForceDisabled(unit))
            {
                return false;
            }

            if (IsPathfindingForceEnabled(unit))
            {
                return true;
            }

            // Always use mmaps for pets of players
            if (unit->GetTypeId() == TYPEID_UNIT && ((Creature*)unit)->IsPet() && unit->GetOwner() &&
                unit->GetOwner()->GetTypeId() == TYPEID_PLAYER)
            {
                return true;
            }
        }

        return g_mmapDisabledIds->find(mapId) == g_mmapDisabledIds->end();
    }

    void MMapFactory::clear()
    {
        delete g_mmapDisabledIds;
        delete g_MMapManager;

        g_mmapDisabledIds = NULL;
        g_MMapManager = NULL;
    }

    bool MMapFactory::IsPathfindingForceEnabled(const Unit* unit)
    {
        if (const Creature* pCreature = dynamic_cast<const Creature*>(unit))
        {
            if (const CreatureInfo* pInfo = pCreature->GetCreatureInfo())
            {
                if (pInfo->ExtraFlags & CREATURE_FLAG_EXTRA_MMAP_FORCE_ENABLE)
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool MMapFactory::IsPathfindingForceDisabled(const Unit* unit)
    {
        if (const Creature* pCreature = dynamic_cast<const Creature*>(unit))
        {
            if (const CreatureInfo* pInfo = pCreature->GetCreatureInfo())
            {
                if (pInfo->ExtraFlags & CREATURE_FLAG_EXTRA_MMAP_FORCE_DISABLE)
                {
                    return true;
                }
            }
        }

        return false;
    }

    std::shared_ptr<MMapData> MMapManager::Find(uint32 mapId, NavAgent agent) const
    {
        std::shared_lock<std::shared_mutex> guard(mapsLock);
        MMapDataSet::const_iterator found = loadedMMaps.find(MeshKey(mapId, agent));
        return found == loadedMMaps.end() ? nullptr : found->second;
    }

    NavMeshLease MMapManager::Lease(uint32 mapId, NavAgent agent)
    {
        std::shared_ptr<MMapData> data = Find(mapId, agent);
        if (!data)
        {
            return NavMeshLease();
        }
        dtNavMeshQuery const* query = QueryOnThisThread(MeshKey(mapId, agent), *data);
        return query ? NavMeshLease(std::move(data), query) : NavMeshLease();
    }

    uint32 MMapManager::getLoadedMapsCount() const
    {
        std::shared_lock<std::shared_mutex> guard(mapsLock);
        return uint32(loadedMMaps.size());
    }

    uint32 MMapManager::packTileID(int32 x, int32 y)
    {
        return uint32(x << 16 | y);
    }

    std::shared_ptr<MMapData> MMapManager::loadMapData(uint32 mapId, NavAgent agent)
    {
        if (std::shared_ptr<MMapData> loaded = Find(mapId, agent))
        {
            return loaded;
        }

        std::unique_lock<std::shared_mutex> guard(mapsLock);
        MMapDataSet::const_iterator raced = loadedMMaps.find(MeshKey(mapId, agent));
        if (raced != loadedMMaps.end())
        {
            return raced->second;
        }

        const std::string fileName = MMapFileName(mapId, agent);
        FILE* file = fopen(fileName.c_str(), "rb");
        if (!file)
        {
            if (MMapFactory::IsPathfindingEnabled(mapId, NULL))
            {
                sLog.outError("MMAP:loadMapData: Could not open mmap file '%s'", fileName.c_str());
            }
            return nullptr;
        }

        dtNavMeshParams params;
        const size_t fileRead = fread(&params, sizeof(dtNavMeshParams), 1, file);
        fclose(file);
        if (fileRead != 1)
        {
            sLog.outError("MMAP:loadMapData: Failed to read %s", fileName.c_str());
            return nullptr;
        }

        dtNavMesh* mesh = dtAllocNavMesh();
        MANGOS_ASSERT(mesh);
        if (dtStatusFailed(mesh->init(&params)))
        {
            dtFreeNavMesh(mesh);
            sLog.outError("MMAP:loadMapData: Failed to initialize dtNavMesh from %s", fileName.c_str());
            return nullptr;
        }

        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:loadMapData: Loaded %s", fileName.c_str());
        std::shared_ptr<MMapData> data = std::make_shared<MMapData>(mesh, nextSerial++);
        loadedMMaps.emplace(MeshKey(mapId, agent), data);
        return data;
    }

    bool MMapManager::loadMap(uint32 mapId, int32 x, int32 y)
    {
        bool loaded = false;
        for (size_t agent = 0; agent < size_t(NavAgent::Count); ++agent)
        {
            loaded |= loadTile(mapId, NavAgent(agent), x, y);
        }
        return loaded;
    }

    bool MMapManager::loadTile(uint32 mapId, NavAgent agent, int32 x, int32 y)
    {
        std::shared_ptr<MMapData> const mmap = loadMapData(mapId, agent);
        if (!mmap)
        {
            return false;
        }

        const int32 fileX = y;
        const int32 fileY = x;
        const std::string fileName = MMapTileFileName(mapId, agent, fileX, fileY);
        FILE* file = fopen(fileName.c_str(), "rb");
        if (!file)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:loadMap: no tile %s", fileName.c_str());
            return false;
        }

        MmapTileHeader fileHeader;
        std::vector<unsigned char> bytes;
        const bool headerOk = fread(&fileHeader, sizeof(MmapTileHeader), 1, file) == 1 &&
                              fileHeader.mmapMagic == MMAP_MAGIC;
        const bool versionOk = headerOk && fileHeader.mmapVersion == MMAP_VERSION;
        const bool dataOk = versionOk && ReadTileData(file, fileHeader, bytes);
        fclose(file);
        if (!dataOk)
        {
            sLog.outError("MMAP:loadMap: %s: %s", fileName.c_str(),
                          !headerOk ? "bad header" : !versionOk ? "wrong generator version" : "bad data");
            return false;
        }

        unsigned char* data = (unsigned char*)dtAlloc(fileHeader.size, DT_ALLOC_PERM);
        MANGOS_ASSERT(data);
        memcpy(data, bytes.data(), fileHeader.size);

        const uint32 packedGridPos = packTileID(x, y);
        std::unique_lock<std::shared_mutex> guard(mmap->tilesLock);
        if (mmap->mmapLoadedTiles.count(packedGridPos))
        {
            sLog.outError("MMAP:loadMap: %s is already loaded", fileName.c_str());
            dtFree(data);
            return false;
        }

        dtTileRef tileRef = 0;
        if (dtStatusFailed(mmap->navMesh->addTile(data, fileHeader.size, DT_TILE_FREE_DATA, 0, &tileRef)))
        {
            sLog.outError("MMAP:loadMap: Could not add %s to the navmesh", fileName.c_str());
            dtFree(data);
            return false;
        }

        mmap->mmapLoadedTiles.emplace(packedGridPos, tileRef);
        ++loadedTiles;
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:loadMap: Loaded %s", fileName.c_str());
        return true;
    }

    bool MMapManager::unloadMap(uint32 mapId, int32 x, int32 y)
    {
        bool unloaded = false;
        for (size_t agent = 0; agent < size_t(NavAgent::Count); ++agent)
        {
            unloaded |= unloadTile(mapId, NavAgent(agent), x, y);
        }
        return unloaded;
    }

    bool MMapManager::unloadTile(uint32 mapId, NavAgent agent, int32 x, int32 y)
    {
        std::shared_ptr<MMapData> const mmap = Find(mapId, agent);
        if (!mmap)
        {
            return false;
        }

        std::unique_lock<std::shared_mutex> guard(mmap->tilesLock);
        MMapTileSet::iterator tile = mmap->mmapLoadedTiles.find(packTileID(x, y));
        if (tile == mmap->mmapLoadedTiles.end())
        {
            return false;
        }

        if (dtStatusFailed(mmap->navMesh->removeTile(tile->second, NULL, NULL)))
        {
            sLog.outError("MMAP:unloadMap: Could not unload %s/%04u%02i%02i.mmtile",
                          NAV_AGENTS[size_t(agent)].dir, mapId, y, x);
            MANGOS_ASSERT(false);
            return false;
        }

        mmap->mmapLoadedTiles.erase(tile);
        --loadedTiles;
        return true;
    }

    bool MMapManager::unloadMap(uint32 mapId)
    {
        std::vector<std::shared_ptr<MMapData>> gone;
        {
            std::unique_lock<std::shared_mutex> guard(mapsLock);
            for (size_t agent = 0; agent < size_t(NavAgent::Count); ++agent)
            {
                MMapDataSet::iterator const found = loadedMMaps.find(MeshKey(mapId, NavAgent(agent)));
                if (found != loadedMMaps.end())
                {
                    gone.push_back(std::move(found->second));
                    loadedMMaps.erase(found);
                }
            }
        }

        for (std::shared_ptr<MMapData> const& mmap : gone)
        {
            std::unique_lock<std::shared_mutex> guard(mmap->tilesLock);
            loadedTiles -= uint32(mmap->mmapLoadedTiles.size());
        }
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Unloaded the meshes of %04u", mapId);
        return !gone.empty();
    }
}
