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

#include "MapBroadcaster.h"
#include "Utilities/Errors.h"
#include <ctime>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <shared_mutex>
#include <list>
#include <utility>
#include "Platform/Define.h"
#include "DBCStructure.h"
#include "GridDefines.h"
#include "Script/ScriptSchedule.h"
#include "UpdateBacklog.h"
#include "Cell.h"
#include "Occupant.h"
#include "SharedDefines.h"
#include "GridMap.h"
#include "GameSystem/GridRefManager.h"
#include "MapRefManager.h"
#include "ScriptMgr.h"
#include "CreatureLinkingMgr.h"
#include "MapMailbox.h"
#include "Metrics/Distribution.h"
#include "Metrics/TickPhases.h"
#include "Metrics/TickRecord.h"
#include "DynamicCollision.h"

#include <bitset>
#include <optional>

struct CreatureInfo;
class Creature;
class TransportMap;
class InitialWorldUpdateBatch;
class InitialWorldEntryHook;
class Unit;
class WorldPacket;
class InstanceData;
class Group;
class MapPersistentState;
class WorldPersistentState;
class DungeonPersistentState;
class BattleGroundPersistentState;
struct ScriptInfo;
class BattleGround;
class GridMap;
class GameObjectModel;
class WeatherSystem;
class Transport;

namespace MaNGOS { struct ObjectUpdater; }

#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct InstanceTemplate
{
    uint32 map;

    uint32 parent;
    uint32 levelMin;
    uint32 levelMax;
    uint32 maxPlayers;
    uint32 reset_delay;
    int32 ghostEntranceMap;
    float ghostEntranceX;
    float ghostEntranceY;
    uint32 script_id;
};

#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

#define MIN_UNLOAD_DELAY      1

class Map : public GridRefManager<NGridType>, public MapBroadcaster
{
    friend class MapReference;
    friend class ObjectGridLoader;
    friend class ObjectWorldLoader;

    protected:
        Map(uint32 id, time_t, uint32 InstanceId);

    public:
        virtual ~Map();

        bool CanUnload(uint32 diff)
        {
            if (!m_unloadTimer)
            {
                return false;
            }
            if (m_unloadTimer <= diff)
            {
                return true;
            }
            m_unloadTimer -= diff;
            return false;
        }

        virtual bool Add(Player*, InitialWorldEntryHook* initialEntry = nullptr);
        virtual void Remove(Player*, bool);

        bool Rebind(Player* player, float x, float y, float z, float o);
        template<class T> void Add(T*);
        template<class T> void Remove(T*, bool);

        static void DeleteFromWorld(Player* player);

        virtual void Update(const uint32&);

        void PostPacket(WorldSession* session, ObjectGuid player,
                        std::unique_ptr<WorldPacket> packet)
        {
            m_mailbox.Post(session, player, std::move(packet));
        }

        metrics::TickRecord& Ticks() { return m_ticks; }
        metrics::TickRecord const& Ticks() const { return m_ticks; }

        size_t ActiveObjectCount() const { return m_activeNonPlayers.size(); }

        float GetVisibilityDistance() const { return m_VisibleDistance; }

        virtual void InitVisibilityDistance();

        float GetBroadcastRadius() const;

        void AddCinematicViewer(float radius);
        void RemoveCinematicViewer(float radius);

        void PlayerRelocation(Player*, float x, float y, float z, float angl);
        void CreatureRelocation(Creature* creature, float x, float y, float z, float orientation);

        template<class T, class CONTAINER> void Visit(const Cell& cell, TypeContainerVisitor<T, CONTAINER>& visitor);

        bool IsRemovalGrid(float x, float y) const
        {
            GridPair p = MaNGOS::ComputeGridPair(x, y);
            return (!getNGrid(p.x_coord, p.y_coord) || getNGrid(p.x_coord, p.y_coord)->GetGridState() == GRID_STATE_REMOVAL);
        }

        bool IsLoaded(float x, float y) const
        {
            GridPair p = MaNGOS::ComputeGridPair(x, y);
            return loaded(p);
        }

        bool IsGridLoaded(uint32 gridX, uint32 gridY) const { return loaded(GridPair(gridX, gridY)); }

        bool GetUnloadLock(const GridPair& p) const { return getNGrid(p.x_coord, p.y_coord)->getUnloadLock(); }
        void SetUnloadLock(const GridPair& p, bool on) { getNGrid(p.x_coord, p.y_coord)->setUnloadExplicitLock(on); }
        void ForceLoadGrid(float x, float y);
        bool UnloadGrid(const uint32& x, const uint32& y, bool pForce);
        virtual void UnloadAll(bool pForce);

        void ResetGridExpiry(NGridType& grid, float factor = 1) const
        {
            grid.ResetTimeTracker((time_t)((float)i_gridExpiry * factor));
        }

        time_t GetGridExpiry(void) const { return i_gridExpiry; }
        uint32 GetId(void) const { return i_id; }

        virtual void RemoveAllObjectsInRemoveList();

        bool CreatureRespawnRelocation(Creature* c);

        bool CheckGridIntegrity(Creature* c, bool moved) const;

        uint32 GetInstanceId() const { return i_InstanceId; }
        virtual bool CanEnter(Player* player);
        const char* GetMapName() const;

        bool Instanceable() const { return i_mapEntry && i_mapEntry->Instanceable(); }
        bool IsDungeon() const { return i_mapEntry && i_mapEntry->IsDungeon(); }
        bool IsRaid() const { return i_mapEntry && i_mapEntry->IsRaid(); }
        bool IsBattleGround() const { return i_mapEntry && i_mapEntry->IsBattleGround(); }
        bool IsContinent() const { return i_mapEntry && i_mapEntry->IsContinent(); }

        virtual TransportMap* AsTransport() { return nullptr; }
        virtual TransportMap const* AsTransport() const { return nullptr; }

        MapPersistentState* GetPersistentState() const { return m_persistentState; }

        void AddObjectToRemoveList(Occupant* obj);

        void UpdateObjectVisibility(Occupant* obj, Cell cell, CellPair cellpair);

        void resetMarkedCells()
        {
            marked_cells.reset();
        }

        bool isCellMarked(uint32 pCellId) { return marked_cells.test(pCellId); }
        void markCell(uint32 pCellId) { marked_cells.set(pCellId); }

        bool HavePlayers() const { return !m_mapRefManager.isEmpty(); }
        uint32 GetPlayersCountExceptGMs() const;
        bool ActiveObjectsNearGrid(uint32 x, uint32 y) const;

        typedef MapRefManager PlayerList;
        PlayerList const& GetPlayers() const { return m_mapRefManager; }

        ScriptSchedule& Scripts() { return m_scripts; }

        void AddToActive(Occupant* obj);

        void RemoveFromActive(Occupant* obj);

        Player* GetPlayer(ObjectGuid guid);
        Creature* GetCreature(ObjectGuid guid);
        Pet* GetPet(ObjectGuid guid);
        Creature* GetAnyTypeCreature(ObjectGuid guid);
        GameObject* GetGameObject(ObjectGuid guid);
        DynamicObject* GetDynamicObject(ObjectGuid guid);
        Corpse* GetCorpse(ObjectGuid guid);
        Unit* GetUnit(ObjectGuid guid);
        Occupant* GetOccupant(ObjectGuid guid);

        using MapStoredObjectTypesContainer = TypeUnorderedMapContainer<ObjectGuid, TypeList<Creature, Pet, GameObject, DynamicObject>> ;
        MapStoredObjectTypesContainer& GetObjectsStore()
        {
            return m_objectsStore;
        }

        UpdateBacklog& Backlog() { return m_backlog; }

        uint32 GenerateLocalLowGuid(HighGuid guidhigh);

        const TerrainInfo* GetTerrain() const { return m_TerrainData; }

        std::vector<Player*> const& ExternalObservers() const { return m_externalObservers; }
        void SetExternalObservers(std::vector<Player*>&& observers)
        {
            m_externalObservers = std::move(observers);
        }

        void CreateInstanceData(bool load);
        InstanceData* GetInstanceData() const { return i_data; }
        virtual uint32 GetScriptId() const { return sScriptMgr.GetBoundScriptId(SCRIPTED_MAP, GetId()); }

        static const uint32 PHASE_ANY = 1;

        world::terrain::Column ColumnAt(float x, float y, float zTop, float zBottom) const;

        std::optional<float> Floor(float x, float y, float z) const;
        std::optional<float> FloorNear(float x, float y, float z, float maxSearchDist = 4.0f) const;
        float GetHeight(float x, float y, float z) const;
        bool GetHeightInRange(float x, float y, float& z, float maxSearchDist = 4.0f) const;
        bool IsInLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2) const;
        bool GetHitPosition(float srcX, float srcY, float srcZ, float& destX, float& destY, float& destZ, float modifyDist) const;

        void InsertGameObjectModel(const GameObjectModel& mdl);
        void RemoveGameObjectModel(const GameObjectModel& mdl);
        bool ContainsGameObjectModel(const GameObjectModel& mdl) const;
        void RefreshGameObjectModel(GameObjectModel& mdl);

        CreatureLinkingHolder* GetCreatureLinkingHolder()
        {
            return &m_creatureLinkingHolder;
        }

        void TeleportAllPlayersTo(TeleportLocation loc);

        WeatherSystem* GetWeatherSystem() const { return m_weatherSystem; }

        void SetWeather(uint32 zoneId, WeatherType type, float grade, bool permanently);

        bool GetReachableRandomPosition(Unit* unit, float& x, float& y, float& z, float radius);
        bool GetReachableRandomPointOnGround(float& x, float& y, float& z, float radius);
        bool GetRandomPointInTheAir(float& x, float& y, float& z, float radius);
        bool GetRandomPointUnderWater(float& x, float& y, float& z, float radius, GridMapLiquidData& liquid_status);

        bool HasPlayerInOrAroundGrid(uint32 gridX, uint32 gridY) const;
        bool IsCellLoaded(float x, float y) const;

    private:
        void LoadMapAndVMap(int gx, int gy);

        void SetTimer(uint32 t) { i_gridExpiry = t < MIN_GRID_DELAY ? MIN_GRID_DELAY : t; }

        void SendInitSelf(Player* player, InitialWorldUpdateBatch* batch);

        void SendInitTransports(Player* player, InitialWorldUpdateBatch* batch);
        void SendRemoveTransports(Player* player);

        bool CreatureCellRelocation(Creature* creature, const Cell &new_cell);

        bool loaded(const GridPair&) const;
        void EnsureGridCreated(const GridPair&);
        bool EnsureGridLoaded(Cell const&);

        void buildNGridLinkage(NGridType* pNGridType) { pNGridType->link(this); }

        void VisitNearbyCellsOf(Occupant* obj,
            TypeContainerVisitor<MaNGOS::ObjectUpdater, GridTypeMapContainer> &gridVisitor,
            TypeContainerVisitor<MaNGOS::ObjectUpdater, WorldTypeMapContainer> &worldVisitor);

        bool isGridObjectDataLoaded(uint32 x, uint32 y) const { return getNGrid(x, y)->isGridObjectDataLoaded(); }
        void setGridObjectDataLoaded(bool pLoaded, uint32 x, uint32 y) { getNGrid(x, y)->setGridObjectDataLoaded(pLoaded); }

        void setNGrid(NGridType* grid, uint32 x, uint32 y);

    protected:

        uint32 Hearers(Audience const& who, Listener const& tell) override;

        uint32 Across(Audience const& who, Listener const& tell) override;

        void EnsureGridLoadedAtEnter(Cell const&, Player* player = nullptr);

        NGridType* getNGrid(uint32 x, uint32 y) const
        {
            MANGOS_ASSERT(x < MAX_NUMBER_OF_GRIDS);
            MANGOS_ASSERT(y < MAX_NUMBER_OF_GRIDS);
            return i_grids[x][y];
        }

        MapEntry const* i_mapEntry;

        std::vector<Player*> m_externalObservers;
        uint32 i_id;
        uint32 i_InstanceId;
        uint32 m_unloadTimer;
        float m_VisibleDistance;
        std::multiset<float> m_cinematicViewerRadii;
        float m_cinematicViewerRadius;
        MapPersistentState* m_persistentState;

        MapRefManager m_mapRefManager;
        MapRefManager::iterator m_mapRefIter;

        typedef std::set<Occupant*> ActiveNonPlayers;
        ActiveNonPlayers m_activeNonPlayers;
        ActiveNonPlayers::iterator m_activeNonPlayersIter;
        MapStoredObjectTypesContainer m_objectsStore;

    private:
        time_t i_gridExpiry;

        MapMailbox m_mailbox;
        UpdateBacklog m_backlog;

        metrics::TickRecord m_ticks;

        NGridType* i_grids[MAX_NUMBER_OF_GRIDS][MAX_NUMBER_OF_GRIDS];

        TerrainInfo* const m_TerrainData;
        bool m_bLoadedGrids[MAX_NUMBER_OF_GRIDS][MAX_NUMBER_OF_GRIDS];

        std::bitset<TOTAL_NUMBER_OF_CELLS_PER_MAP* TOTAL_NUMBER_OF_CELLS_PER_MAP> marked_cells;

        std::set<Occupant*> i_objectsToRemove;

        ScriptSchedule m_scripts;

        InstanceData* i_data;

        ObjectGuidGenerator<HIGHGUID_UNIT> m_CreatureGuids;
        ObjectGuidGenerator<HIGHGUID_GAMEOBJECT> m_GameObjectGuids;
        ObjectGuidGenerator<HIGHGUID_DYNAMICOBJECT> m_DynObjectGuids;
        ObjectGuidGenerator<HIGHGUID_PET> m_PetGuids;

        template<class T>
            void AddToGrid(T*, NGridType*, Cell const&);

        template<class T>
            void RemoveFromGrid(T*, NGridType*, Cell const&);

        CreatureLinkingHolder m_creatureLinkingHolder;

        DynamicCollision m_dyn_tree;

        WeatherSystem* m_weatherSystem;

};

class WorldMap : public Map
{
    private:
        using Map::GetPersistentState;
    public:
        WorldMap(uint32 id, time_t expiry) : Map(id, expiry, 0) {}
        ~WorldMap() {}

        WorldPersistentState* GetPersistanceState() const;
};

class DungeonMap : public Map
{
    private:
        using Map::GetPersistentState;
    public:
        DungeonMap(uint32 id, time_t, uint32 InstanceId);
        ~DungeonMap();
        bool Add(Player*, InitialWorldEntryHook* initialEntry = nullptr) override;
        void Remove(Player*, bool) override;
        void Update(const uint32&) override;
        bool Reset(InstanceResetMethod method);
        void PermBindAllPlayers(Player* player);
        void UnloadAll(bool pForce) override;
        void SendResetWarnings(uint32 timeLeft) const;
        void SetResetSchedule(bool on);
        uint32 GetMaxPlayers() const;

        uint32 GetScriptId() const override { return sScriptMgr.GetBoundScriptId(SCRIPTED_INSTANCE, GetId()); }

        DungeonPersistentState* GetPersistanceState() const;

        void InitVisibilityDistance() override;
    private:
        bool m_resetAfterUnload;
        bool m_unloadWhenEmpty;
};

class BattleGroundMap : public Map
{
    private:
        using Map::GetPersistentState;
    public:
        BattleGroundMap(uint32 id, time_t, uint32 InstanceId);
        ~BattleGroundMap();

        void Update(const uint32&) override;
        bool Add(Player*, InitialWorldEntryHook* initialEntry = nullptr) override;
        void Remove(Player*, bool) override;
        bool CanEnter(Player* player) override;
        void SetUnload();
        void UnloadAll(bool pForce) override;

        void InitVisibilityDistance() override;
        BattleGround* GetBG()
        {
            return m_bg;
        }
        void SetBG(BattleGround* bg) { m_bg = bg; }

        uint32 GetScriptId() const override { return sScriptMgr.GetBoundScriptId(SCRIPTED_BATTLEGROUND, GetId()); }

        BattleGroundPersistentState* GetPersistanceState() const;

    private:
        BattleGround* m_bg;
};

template<class T, class CONTAINER>
    inline void Map::Visit(const Cell& cell, TypeContainerVisitor<T, CONTAINER>& visitor)
{
    const uint32 x = cell.GridX();
    const uint32 y = cell.GridY();
    const uint32 cell_x = cell.CellX();
    const uint32 cell_y = cell.CellY();

    if (!cell.NoCreate() || loaded(GridPair(x, y)))
    {
        EnsureGridLoaded(cell);
        getNGrid(x, y)->Visit(cell_x, cell_y, visitor);
    }
    else if (NGridType* ng = getNGrid(x, y))
    {

        if (ng->isCellObjectDataLoaded(cell_x, cell_y))
        {
            ng->Visit(cell_x, cell_y, visitor);
        }
    }
}
