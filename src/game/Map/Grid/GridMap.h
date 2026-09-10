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

#pragma once

#include <unordered_map>
#include <mutex>
#include <atomic>
#include "Platform/Define.h"
#include "Policies/Singleton.h"
#include "DBCStructure.h"
#include "GridDefines.h"
#include "Object.h"
#include "SharedDefines.h"
#include "terrain/FusedTerrain.hpp"

#include <bitset>
#include <list>
#include <optional>

class Creature;
class Unit;
class WorldPacket;
class InstanceData;
class Group;
class BattleGround;
class Map;

struct GridMapFileHeader
{
    uint32 mapMagic;
    uint32 versionMagic;
    uint32 buildMagic;
    uint32 areaMapOffset;
    uint32 areaMapSize;
    uint32 heightMapOffset;
    uint32 heightMapSize;
    uint32 liquidMapOffset;
    uint32 liquidMapSize;
    uint32 holesOffset;
    uint32 holesSize;
};

#define MAP_AREA_NO_AREA      0x0001

struct GridMapAreaHeader
{
    uint32 fourcc;
    uint16 flags;
    uint16 gridArea;
};

#define MAP_HEIGHT_NO_HEIGHT  0x0001
#define MAP_HEIGHT_AS_INT16   0x0002
#define MAP_HEIGHT_AS_INT8    0x0004

struct GridMapHeightHeader
{
    uint32 fourcc;
    uint32 flags;
    float gridHeight;
    float gridMaxHeight;
};

#define MAP_LIQUID_NO_TYPE    0x0001
#define MAP_LIQUID_NO_HEIGHT  0x0002

struct GridMapLiquidHeader
{
    uint32 fourcc;
    uint16 flags;
    uint16 liquidType;
    uint8 offsetX;
    uint8 offsetY;
    uint8 width;
    uint8 height;
    float liquidLevel;
};

enum GridMapLiquidStatus
{
    LIQUID_MAP_NO_WATER     = 0x00000000,
    LIQUID_MAP_ABOVE_WATER  = 0x00000001,
    LIQUID_MAP_WATER_WALK   = 0x00000002,
    LIQUID_MAP_IN_WATER     = 0x00000004,
    LIQUID_MAP_UNDER_WATER  = 0x00000008
};

#define MAP_LIQUID_TYPE_NO_WATER    0x00
#define MAP_LIQUID_TYPE_WATER       0x01
#define MAP_LIQUID_TYPE_OCEAN       0x02
#define MAP_LIQUID_TYPE_MAGMA       0x04
#define MAP_LIQUID_TYPE_SLIME       0x08

#define MAP_ALL_LIQUIDS   (MAP_LIQUID_TYPE_WATER | MAP_LIQUID_TYPE_OCEAN | MAP_LIQUID_TYPE_MAGMA | MAP_LIQUID_TYPE_SLIME)

#define MAP_LIQUID_TYPE_DARK_WATER  0x10
#define MAP_LIQUID_TYPE_WMO_WATER   0x20

struct GridMapLiquidData
{
    uint32 type_flags;
    uint32 entry;
    float level;
    float depth_level;
};

template<typename Countable>
class Referencable
{
    public:
        Referencable() { m_count = 0; }

        void AddRef() { ++m_count; }
        bool Release() { return (--m_count < 1); }
        bool IsReferenced() const { return (m_count > 0); }

    private:
        Referencable(const Referencable&);
        Referencable& operator=(const Referencable&);

        Countable m_count;
};

typedef std::atomic<long> AtomicLong;

#define MAX_HEIGHT            100000.0f

#define INVALID_HEIGHT       -100000.0f

#define FLOOR_SEARCH_UP            2.0f

#define FLOOR_BURIED_LIFT         50.0f

#define FLOOR_SEARCH_DOWN      10000.0f
#define DEFAULT_WATER_SEARCH      50.0f

class TerrainInfo : public Referencable<AtomicLong>
{
    public:
        explicit TerrainInfo(uint32 mapid);
        ~TerrainInfo();

        uint32 GetMapId() const { return m_mapId; }

        static bool ExistTile(uint32 mapid, int gx, int gy);

        world::terrain::Column ColumnAt(float x, float y, float zTop, float zBottom,
                                        const world::terrain::ILiveGeometry* live = nullptr,
                                        uint32 phasemask = 0) const;

        std::optional<float> StaticFloor(float x, float y, float z) const;

        std::optional<float> GetWaterLevel(float x, float y, float z, float* pGround = nullptr) const;
        float GetWaterOrGroundLevel(float x, float y, float z, float* pGround = nullptr, bool swim = false) const;
        bool IsInWater(float x, float y, float z, GridMapLiquidData* data = 0) const;
        bool IsUnderWater(float x, float y, float z) const;

        bool IsSwimmable(float x, float y, float z, float radius = 1.5f,
                         GridMapLiquidData* data = nullptr) const;
        bool IsAboveWater(float x, float y, float z, float* pWaterZ = nullptr) const;

        GridMapLiquidStatus getLiquidStatus(float x, float y, float z, uint8 ReqLiquidType, GridMapLiquidData* data = 0) const;

        uint16 GetAreaFlag(float x, float y, float z, bool* isOutdoors = 0) const;
        uint8 GetTerrainType(float x, float y) const;

        uint32 GetAreaId(float x, float y, float z) const;
        uint32 GetZoneId(float x, float y, float z) const;
        void GetZoneAndAreaId(uint32& zoneid, uint32& areaid, float x, float y, float z) const;

        bool GetAreaInfo(float x, float y, float z, uint32& mogpflags, int32& adtId, int32& rootId, int32& groupId) const;
        bool IsOutdoors(float x, float y, float z) const;

        bool IsInLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2) const;
        float NearestHitFraction(float x1, float y1, float z1, float x2, float y2, float z2) const;

        void CleanUpGrids(const uint32 diff);

    protected:
        friend class Map;
        bool Load(const uint32 x, const uint32 y);
        void Unload(const uint32 x, const uint32 y);

    private:
        TerrainInfo(const TerrainInfo&);
        TerrainInfo& operator=(const TerrainInfo&);

        const uint32 m_mapId;

        mutable world::terrain::FusedTerrain m_terrain;

        int16 m_GridRef[MAX_NUMBER_OF_GRIDS][MAX_NUMBER_OF_GRIDS];

        bool m_navLoaded[MAX_NUMBER_OF_GRIDS][MAX_NUMBER_OF_GRIDS];

        IntervalTimer i_timer;

        typedef std::mutex LOCK_TYPE;
        LOCK_TYPE m_refMutex;
};

class TerrainManager : public MaNGOS::Singleton<TerrainManager>
{
        typedef std::unordered_map<uint32,  TerrainInfo*> TerrainDataMap;
        friend class MaNGOS::Singleton<TerrainManager>;

    public:
        TerrainInfo* LoadTerrain(const uint32 mapId);
        void UnloadTerrain(const uint32 mapId);

        void Update(const uint32 diff);
        void UnloadAll();

        uint16 GetAreaFlag(uint32 mapid, float x, float y, float z) const
        {
            TerrainInfo* pData = const_cast<TerrainManager*>(this)->LoadTerrain(mapid);
            return pData->GetAreaFlag(x, y, z);
        }
        uint32 GetAreaId(uint32 mapid, float x, float y, float z) const
        {
            return TerrainManager::GetAreaIdByAreaFlag(GetAreaFlag(mapid, x, y, z), mapid);
        }
        uint32 GetZoneId(uint32 mapid, float x, float y, float z) const
        {
            return TerrainManager::GetZoneIdByAreaFlag(GetAreaFlag(mapid, x, y, z), mapid);
        }
        void GetZoneAndAreaId(uint32& zoneid, uint32& areaid, uint32 mapid, float x, float y, float z)
        {
            TerrainManager::GetZoneAndAreaIdByAreaFlag(zoneid, areaid, GetAreaFlag(mapid, x, y, z), mapid);
        }

        static uint32 GetAreaIdByAreaFlag(uint16 areaflag, uint32 map_id);
        static uint32 GetZoneIdByAreaFlag(uint16 areaflag, uint32 map_id);
        static void GetZoneAndAreaIdByAreaFlag(uint32& zoneid, uint32& areaid, uint16 areaflag, uint32 map_id);

    private:
        TerrainManager();
        ~TerrainManager();

        TerrainManager(const TerrainManager&);
        TerrainManager& operator=(const TerrainManager&);

        typedef std::mutex LOCK_TYPE;
        LOCK_TYPE m_mutex;
        TerrainDataMap i_TerrainMap;
};

#define sTerrainMgr TerrainManager::Instance()
