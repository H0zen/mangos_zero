/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the 1.12.x client.
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

    MMapManager* g_MMapManager = nullptr;

    std::set<uint32>* g_mmapDisabledIds = nullptr;

    MMapManager* MMapFactory::createOrGetMMapManager()
    {
        if (g_MMapManager == nullptr)
        {
            g_MMapManager = new MMapManager();
        }

        return g_MMapManager;
    }

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
            idstr = strtok(nullptr, ",");
        }

        delete[] mapList;
    }

    bool MMapFactory::IsPathfindingEnabled(uint32 mapId, const Unit* unit = nullptr)
    {
        if (!sWorld.getConfig(CONFIG_BOOL_MMAP_ENABLED))
        {
            return false;
        }

        if (unit)
        {

            if (IsPlayer(unit))
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

            if (IsCreature(unit) && ((Creature*)unit)->IsPet() && unit->GetOwner() &&IsPlayer(unit->GetOwner()))
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

        g_mmapDisabledIds = nullptr;
        g_MMapManager = nullptr;
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

    MMapManager::~MMapManager()
    {
        for (MMapDataSet::iterator i = loadedMMaps.begin(); i != loadedMMaps.end(); ++i)
        {
            delete i->second;
        }

    }

    bool MMapManager::loadMapData(uint32 mapId)
    {

        if (loadedMMaps.find(mapId) != loadedMMaps.end())
        {
            return true;
        }

        const std::string fileName = MMapFileName(mapId);

        FILE* file = fopen(fileName.c_str(), "rb");
        if (!file)
        {
            if (MMapFactory::IsPathfindingEnabled(mapId))
            {
                sLog.outError("MMAP:loadMapData: Error: Could not open mmap file '%s'", fileName.c_str());
            }
            return false;
        }

        dtNavMeshParams params;
        size_t file_read = fread(&params, sizeof(dtNavMeshParams), 1, file);
        if (file_read <= 0)
        {
            sLog.outError("MMAP:loadMapData: Failed to load mmap %04u from file %s", mapId, fileName.c_str());
            fclose(file);
            return false;
        }
        fclose(file);

        dtNavMesh* mesh = dtAllocNavMesh();
        MANGOS_ASSERT(mesh);
        dtStatus dtResult = mesh->init(&params);
        if (dtStatusFailed(dtResult))
        {
            dtFreeNavMesh(mesh);
            sLog.outError("MMAP:loadMapData: Failed to initialize dtNavMesh for mmap %04u from file %s", mapId, fileName.c_str());
            return false;
        }

        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:loadMapData: Loaded %04u.mmap", mapId);

        MMapData* mmap_data = new MMapData(mesh);
        mmap_data->mmapLoadedTiles.clear();

        loadedMMaps.insert(std::pair<uint32, MMapData*>(mapId, mmap_data));
        return true;
    }

    uint32 MMapManager::packTileID(int32 x, int32 y)
    {
        return uint32(x << 16 | y);
    }

    bool MMapManager::loadMap(uint32 mapId, int32 x, int32 y)
    {

        if (!loadMapData(mapId))
        {
            return false;
        }

        MMapData* mmap = loadedMMaps[mapId];
        MANGOS_ASSERT(mmap->navMesh);

        uint32 packedGridPos = packTileID(x, y);
        if (mmap->mmapLoadedTiles.find(packedGridPos) != mmap->mmapLoadedTiles.end())
        {
            sLog.outError("MMAP:loadMap: Asked to load already loaded navmesh tile. %04u%02i%02i.mmtile", mapId, x, y);
            return false;
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
        size_t file_read = fread(&fileHeader, sizeof(MmapTileHeader), 1, file);

        if (file_read <= 0)
        {
            sLog.outError("MMAP:loadMap: Could not load mmap "
                          "%04u%02i%02i.mmtile",
                          mapId, filenameTileX, filenameTileY);
            fclose(file);
            return false;
        }

        if (fileHeader.mmapMagic != MMAP_MAGIC)
        {
            sLog.outError("MMAP:loadMap: Bad header in mmap "
                          "%04u%02i%02i.mmtile",
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

        unsigned char* data = (unsigned char*)dtAlloc(fileHeader.size, DT_ALLOC_PERM);
        MANGOS_ASSERT(data);

        size_t result = fread(data, fileHeader.size, 1, file);
        if (!result)
        {
            sLog.outError("MMAP:loadMap: Bad header or data in mmap "
                          "%04u%02i%02i.mmtile",
                          mapId, filenameTileX, filenameTileY);
            fclose(file);
            return false;
        }

        fclose(file);

        dtMeshHeader* header = (dtMeshHeader*)data;
        dtTileRef tileRef = 0;

        dtStatus dtResult = mmap->navMesh->addTile(data, fileHeader.size, DT_TILE_FREE_DATA, 0, &tileRef);
        if (dtStatusFailed(dtResult))
        {
            sLog.outError("MMAP:loadMap: Could not load "
                          "%04u%02i%02i.mmtile into navmesh",
                          mapId, filenameTileX, filenameTileY);
            dtFree(data);
            return false;
        }

        mmap->mmapLoadedTiles.insert(std::pair<uint32, dtTileRef>(packedGridPos, tileRef));
        ++loadedTiles;
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING,
                         "MMAP:loadMap: Loaded mmtile "
                         "%04u[%02i,%02i] into %04u[%02i,%02i]",
                         mapId, filenameTileX, filenameTileY, mapId,
                         header->x, header->y);
        return true;
    }

    bool MMapManager::unloadMap(uint32 mapId, int32 x, int32 y)
    {

        if (loadedMMaps.find(mapId) == loadedMMaps.end())
        {

            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Asked to unload not loaded navmesh map. %04u%02i%02i.mmtile", mapId, x, y);
            return false;
        }

        MMapData* mmap = loadedMMaps[mapId];

        uint32 packedGridPos = packTileID(x, y);
        if (mmap->mmapLoadedTiles.find(packedGridPos) == mmap->mmapLoadedTiles.end())
        {

            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Asked to unload not loaded navmesh tile. %04u%02i%02i.mmtile", mapId, x, y);
            return false;
        }

        dtTileRef tileRef = mmap->mmapLoadedTiles[packedGridPos];

        dtStatus dtResult = mmap->navMesh->removeTile(tileRef, nullptr, nullptr);
        if (dtStatusFailed(dtResult))
        {

            sLog.outError("MMAP:unloadMap: Could not unload %04u%02i%02i.mmtile from navmesh", mapId, x, y);
            MANGOS_ASSERT(false);
        }
        else
        {
            mmap->mmapLoadedTiles.erase(packedGridPos);
            --loadedTiles;
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Unloaded mmtile %04u[%02i,%02i] from %04u", mapId, x, y, mapId);
            return true;
        }

        return false;
    }

    bool MMapManager::unloadMap(uint32 mapId)
    {
        if (loadedMMaps.find(mapId) == loadedMMaps.end())
        {

            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Asked to unload not loaded navmesh map %04u", mapId);
            return false;
        }

        MMapData* mmap = loadedMMaps[mapId];
        for (MMapTileSet::iterator i = mmap->mmapLoadedTiles.begin(); i != mmap->mmapLoadedTiles.end(); ++i)
        {
            uint32 x = (i->first >> 16);
            uint32 y = (i->first & 0x0000FFFF);
            dtStatus dtResult = mmap->navMesh->removeTile(i->second, nullptr, nullptr);
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

        delete mmap;
        loadedMMaps.erase(mapId);
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMap: Unloaded %04u.mmap", mapId);

        return true;
    }

    bool MMapManager::unloadMapInstance(uint32 mapId, uint32 instanceId)
    {

        if (loadedMMaps.find(mapId) == loadedMMaps.end())
        {

            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMapInstance: Asked to unload not loaded navmesh map %04u", mapId);
            return false;
        }

        MMapData* mmap = loadedMMaps[mapId];
        if (mmap->navMeshQueries.find(instanceId) == mmap->navMeshQueries.end())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMapInstance: Asked to unload not loaded dtNavMeshQuery mapId %04u instanceId %u", mapId, instanceId);
            return false;
        }

        dtNavMeshQuery* query = mmap->navMeshQueries[instanceId];

        dtFreeNavMeshQuery(query);
        mmap->navMeshQueries.erase(instanceId);
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:unloadMapInstance: Unloaded mapId %04u instanceId %u", mapId, instanceId);

        return true;
    }

    dtNavMesh const* MMapManager::GetNavMesh(uint32 mapId)
    {
        if (loadedMMaps.find(mapId) == loadedMMaps.end())
        {
            return nullptr;
        }

        return loadedMMaps[mapId]->navMesh;
    }

    dtNavMeshQuery const* MMapManager::GetNavMeshQuery(uint32 mapId, uint32 instanceId)
    {
        if (loadedMMaps.find(mapId) == loadedMMaps.end())
        {
            return nullptr;
        }

        MMapData* mmap = loadedMMaps[mapId];
        if (mmap->navMeshQueries.find(instanceId) == mmap->navMeshQueries.end())
        {

            dtNavMeshQuery* query = dtAllocNavMeshQuery();
            MANGOS_ASSERT(query);
            dtStatus dtResult = query->init(mmap->navMesh, 1024);
            if (dtStatusFailed(dtResult))
            {
                dtFreeNavMeshQuery(query);
                sLog.outError("MMAP:GetNavMeshQuery: Failed to initialize dtNavMeshQuery for mapId %04u instanceId %u", mapId, instanceId);
                return nullptr;
            }

            DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MMAP:GetNavMeshQuery: created dtNavMeshQuery for mapId %04u instanceId %u", mapId, instanceId);
            mmap->navMeshQueries.insert(std::pair<uint32, dtNavMeshQuery*>(instanceId, query));
        }

        return mmap->navMeshQueries[instanceId];
    }
}
