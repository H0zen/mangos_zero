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
#include "Log.h"
#include "World.h"
#include "Creature.h"
#include "MoveMap.h"
#include "MoveMapSharedDefines.h"

namespace
{
    const int NAV_QUERY_NODES = 1024;

    /// Named from the expansion, never from the format string: a deck map's id is seven
    /// digits and the old sizing silently cut the extension off.
    std::string MMapFileName(uint32 mapId)
    {
        char leaf[64];
        snprintf(leaf, sizeof(leaf), "mmaps/%04u.mmap", mapId);
        return sWorld.GetDataPath() + leaf;
    }

    std::string MMapTileFileName(uint32 mapId, int32 x, int32 y)
    {
        char leaf[64];
        snprintf(leaf, sizeof(leaf), "mmaps/%04u%02i%02i.mmtile", mapId, x, y);
        return sWorld.GetDataPath() + leaf;
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

    // ######################## MMapManager ########################
    MMapManager::~MMapManager()
    {
        for (MMapDataSet::iterator i = loadedMMaps.begin(); i != loadedMMaps.end(); ++i)
        {
            delete i->second;
        }

        // by now we should not have maps loaded
        // if we had, tiles in MMapData->mmapLoadedTiles, their actual data is lost!
    }

    MMapData* MMapManager::findMapData(uint32 mapId)
    {
        std::lock_guard<std::mutex> guard(mapsLock);
        MMapDataSet::iterator itr = loadedMMaps.find(mapId);
        return itr != loadedMMaps.end() ? itr->second : NULL;
    }

    MMapData* MMapManager::loadMapData(uint32 mapId)
    {
        std::lock_guard<std::mutex> guard(mapsLock);

        MMapDataSet::iterator itr = loadedMMaps.find(mapId);
        if (itr != loadedMMaps.end())
        {
            return itr->second;
        }

        const std::string fileName = MMapFileName(mapId);

        FILE* file = fopen(fileName.c_str(), "rb");
        if (!file)
        {
            if (MMapFactory::IsPathfindingEnabled(mapId))
            {
                sLog.outError("MMAP:loadMapData: Error: Could not open mmap file '%s'", fileName.c_str());
            }
            return NULL;
        }

        dtNavMeshParams params;
        size_t file_read = fread(&params, sizeof(dtNavMeshParams), 1, file);
        fclose(file);
        if (file_read != 1)
        {
            sLog.outError("MMAP:loadMapData: Failed to load mmap %04u from file %s", mapId, fileName.c_str());
            return NULL;
        }

        dtNavMesh* mesh = dtAllocNavMesh();
        MANGOS_ASSERT(mesh);
        dtStatus dtResult = mesh->init(&params);
        if (dtStatusFailed(dtResult))
        {
            dtFreeNavMesh(mesh);
            sLog.outError("MMAP:loadMapData: Failed to initialize dtNavMesh for mmap %04u from file %s", mapId, fileName.c_str());
            return NULL;
        }

        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:loadMapData: Loaded %04u.mmap", mapId);

        MMapData* mmap_data = new MMapData(mesh);
        loadedMMaps.insert(std::pair<uint32, MMapData*>(mapId, mmap_data));
        return mmap_data;
    }

    uint32 MMapManager::packTileID(int32 x, int32 y)
    {
        return uint32(x << 16 | y);
    }

    uint32 MMapManager::getLoadedMapsCount()
    {
        std::lock_guard<std::mutex> guard(mapsLock);
        return uint32(loadedMMaps.size());
    }

    bool MMapManager::loadMap(uint32 mapId, int32 x, int32 y)
    {
        MMapData* mmap = loadMapData(mapId);
        if (!mmap)
        {
            return false;
        }

        const uint32 packedGridPos = packTileID(x, y);
        {
            std::shared_lock<std::shared_mutex> guard(mmap->meshLock);
            if (mmap->mmapLoadedTiles.find(packedGridPos) != mmap->mmapLoadedTiles.end())
            {
                sLog.outError("MMAP:loadMap: Asked to load already loaded navmesh tile. %04u%02i%02i.mmtile", mapId, x, y);
                return false;
            }
        }

        const int32 filenameTileX = y;
        const int32 filenameTileY = x;
        const std::string fileName = MMapTileFileName(mapId, filenameTileX, filenameTileY);

        FILE* file = fopen(fileName.c_str(), "rb");
        if (!file)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "ERROR: MMAP:loadMap: Could not open mmtile file '%s'", fileName.c_str());
            return false;
        }

        MmapTileHeader fileHeader;
        if (fread(&fileHeader, sizeof(MmapTileHeader), 1, file) != 1 ||
            fileHeader.mmapMagic != MMAP_MAGIC)
        {
            sLog.outError("MMAP:loadMap: Bad header in mmap %04u%02i%02i.mmtile",
                          mapId, filenameTileX, filenameTileY);
            fclose(file);
            return false;
        }

        if (fileHeader.mmapVersion != MMAP_VERSION)
        {
            sLog.outError("MMAP:loadMap: %04u%02i%02i.mmtile was built "
                          "with generator v%i, expected v%i",
                          mapId, filenameTileX, filenameTileY,
                          fileHeader.mmapVersion, MMAP_VERSION);
            fclose(file);
            return false;
        }

        const long dataStart = ftell(file);
        fseek(file, 0, SEEK_END);
        const long dataEnd = ftell(file);
        fseek(file, dataStart, SEEK_SET);
        if (fileHeader.size == 0 || dataStart < 0 || dataEnd < dataStart ||
            uint64(fileHeader.size) > uint64(dataEnd - dataStart))
        {
            sLog.outError("MMAP:loadMap: Bad data size in mmap %04u%02i%02i.mmtile",
                          mapId, filenameTileX, filenameTileY);
            fclose(file);
            return false;
        }

        unsigned char* data = (unsigned char*)dtAlloc(fileHeader.size, DT_ALLOC_PERM);
        MANGOS_ASSERT(data);

        const size_t result = fread(data, fileHeader.size, 1, file);
        fclose(file);
        if (result != 1)
        {
            sLog.outError("MMAP:loadMap: Bad header or data in mmap %04u%02i%02i.mmtile",
                          mapId, filenameTileX, filenameTileY);
            dtFree(data);
            return false;
        }

        dtMeshHeader* header = (dtMeshHeader*)data;
        dtTileRef tileRef = 0;

        std::unique_lock<std::shared_mutex> guard(mmap->meshLock);
        dtStatus dtResult = mmap->navMesh->addTile(data, fileHeader.size, DT_TILE_FREE_DATA, 0, &tileRef);
        if (dtStatusFailed(dtResult))
        {
            sLog.outError("MMAP:loadMap: Could not load %04u%02i%02i.mmtile into navmesh",
                          mapId, filenameTileX, filenameTileY);
            dtFree(data);
            return false;
        }

        mmap->mmapLoadedTiles.insert(std::pair<uint32, dtTileRef>(packedGridPos, tileRef));
        ++loadedTiles;
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING,
                         "MMAP:loadMap: Loaded mmtile %04u[%02i,%02i] into %04u[%02i,%02i]",
                         mapId, filenameTileX, filenameTileY, mapId, header->x, header->y);
        return true;
    }

    bool MMapManager::unloadMap(uint32 mapId, int32 x, int32 y)
    {
        MMapData* mmap = findMapData(mapId);
        if (!mmap)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Asked to unload not loaded navmesh map. %04u%02i%02i.mmtile", mapId, x, y);
            return false;
        }

        std::unique_lock<std::shared_mutex> guard(mmap->meshLock);
        const uint32 packedGridPos = packTileID(x, y);
        MMapTileSet::iterator tile = mmap->mmapLoadedTiles.find(packedGridPos);
        if (tile == mmap->mmapLoadedTiles.end())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Asked to unload not loaded navmesh tile. %04u%02i%02i.mmtile", mapId, x, y);
            return false;
        }

        dtStatus dtResult = mmap->navMesh->removeTile(tile->second, NULL, NULL);
        if (dtStatusFailed(dtResult))
        {
            sLog.outError("MMAP:unloadMap: Could not unload %04u%02i%02i.mmtile from navmesh", mapId, x, y);
            MANGOS_ASSERT(false);
            return false;
        }

        mmap->mmapLoadedTiles.erase(tile);
        --loadedTiles;
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Unloaded mmtile %04u[%02i,%02i] from %04u", mapId, x, y, mapId);
        return true;
    }

    bool MMapManager::unloadMap(uint32 mapId)
    {
        std::lock_guard<std::mutex> mapsGuard(mapsLock);
        MMapDataSet::iterator itr = loadedMMaps.find(mapId);
        if (itr == loadedMMaps.end())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Asked to unload not loaded navmesh map %04u", mapId);
            return false;
        }

        MMapData* mmap = itr->second;
        {
            std::unique_lock<std::shared_mutex> guard(mmap->meshLock);
            for (MMapTileSet::iterator i = mmap->mmapLoadedTiles.begin(); i != mmap->mmapLoadedTiles.end(); ++i)
            {
                uint32 x = (i->first >> 16);
                uint32 y = (i->first & 0x0000FFFF);
                dtStatus dtResult = mmap->navMesh->removeTile(i->second, NULL, NULL);
                if (dtStatusFailed(dtResult))
                {
                    sLog.outError("MMAP:unloadMap: Could not unload %04u%02u%02u.mmtile from navmesh", mapId, x, y);
                }
                else
                {
                    --loadedTiles;
                    DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Unloaded mmtile %04u[%02u,%02u] from %04u", mapId, x, y, mapId);
                }
            }
        }

        delete mmap;
        loadedMMaps.erase(itr);
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Unloaded %04u.mmap", mapId);
        return true;
    }

    bool MMapManager::unloadMapInstance(uint32 mapId, uint32 instanceId)
    {
        MMapData* mmap = findMapData(mapId);
        if (!mmap)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMapInstance: Asked to unload not loaded navmesh map %04u", mapId);
            return false;
        }

        std::lock_guard<std::mutex> guard(mmap->queryLock);
        NavMeshQuerySet::iterator query = mmap->navMeshQueries.find(instanceId);
        if (query == mmap->navMeshQueries.end())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMapInstance: Asked to unload not loaded dtNavMeshQuery mapId %04u instanceId %u", mapId, instanceId);
            return false;
        }

        dtFreeNavMeshQuery(query->second);
        mmap->navMeshQueries.erase(query);
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMapInstance: Unloaded mapId %04u instanceId %u", mapId, instanceId);
        return true;
    }

    dtNavMesh const* MMapManager::GetNavMesh(uint32 mapId)
    {
        MMapData* mmap = findMapData(mapId);
        return mmap ? mmap->navMesh : NULL;
    }

    std::shared_mutex* MMapManager::GetMeshLock(uint32 mapId)
    {
        MMapData* mmap = findMapData(mapId);
        return mmap ? &mmap->meshLock : NULL;
    }

    dtNavMeshQuery const* MMapManager::GetNavMeshQuery(uint32 mapId, uint32 instanceId)
    {
        MMapData* mmap = findMapData(mapId);
        if (!mmap)
        {
            return NULL;
        }

        std::lock_guard<std::mutex> guard(mmap->queryLock);
        NavMeshQuerySet::iterator existing = mmap->navMeshQueries.find(instanceId);
        if (existing != mmap->navMeshQueries.end())
        {
            return existing->second;
        }

        dtNavMeshQuery* query = dtAllocNavMeshQuery();
        MANGOS_ASSERT(query);
        dtStatus dtResult = query->init(mmap->navMesh, NAV_QUERY_NODES);
        if (dtStatusFailed(dtResult))
        {
            dtFreeNavMeshQuery(query);
            sLog.outError("MMAP:GetNavMeshQuery: Failed to initialize dtNavMeshQuery for mapId %04u instanceId %u", mapId, instanceId);
            return NULL;
        }

        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:GetNavMeshQuery: created dtNavMeshQuery for mapId %04u instanceId %u", mapId, instanceId);
        mmap->navMeshQueries.insert(std::pair<uint32, dtNavMeshQuery*>(instanceId, query));
        return query;
    }
}
