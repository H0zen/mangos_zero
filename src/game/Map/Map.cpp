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

#include <cmath>
#include "PerKind.h"
#include "SpawnRecord.h"
#include "Utilities/Errors.h"
#include <algorithm>
#include <vector>
#include <set>
#include "Utilities/MathDefines.h"
#include "Map.h"
#include "InitialWorldEntry.h"
#include "GameObjectModel.h"
#include "GridStates.h"
#include "Fleet.h"
#include "Player.h"
#include "GridNotifiers.h"
#include "Log.h"
#include "OpcodeTable.h"
#include "Threading/WorkSentry.h"
#include "CellImpl.h"
#include "InstanceData.h"
#include "GridNotifiersImpl.h"
#include "Transports.h"
#include "TransportMap.h"
#include "PlayerRegistry.h"
#include "CorpseManager.h"
#include "ObjectMgr.h"
#include "Mint.h"
#include "World.h"
#include "Group.h"
#include "MapRefManager.h"
#include "DBCEnums.h"
#include "MapPersistentStateMgr.h"
#include "MoveMap.h"
#include "Chat.h"
#include "Weather.h"
#include "Transports.h"
#include "ObjectGridLoader.h"
#include "Corpse.h"

Map::~Map()
{

    UnloadAll(true);

    if (m_persistentState)
    {
        m_persistentState->SetUsedByMapState(nullptr);
    }

    delete i_data;
    i_data = nullptr;

    MMAP::MMapFactory::createOrGetMMapManager()->unloadMapInstance(m_TerrainData->GetMapId(), GetInstanceId());

    if (m_TerrainData->Release())
    {
        sTerrainMgr.UnloadTerrain(m_TerrainData->GetMapId());
    }

    delete m_weatherSystem;
    m_weatherSystem = nullptr;
}

void Map::LoadMapAndVMap(int gx, int gy)
{
    if (m_bLoadedGrids[gx][gy])
    {
        return;
    }

    if (m_TerrainData->Load(gx, gy))
    {
        m_bLoadedGrids[gx][gy] = true;
    }
}

Map::Map(uint32 id, time_t expiry, uint32 InstanceId)
    : i_mapEntry(sMapStore.LookupEntry(id)),
    i_id(id), i_InstanceId(InstanceId), m_unloadTimer(0),
    m_VisibleDistance(DEFAULT_VISIBILITY_DISTANCE),
    m_cinematicViewerRadius(0.0f), m_persistentState(nullptr),
    m_activeNonPlayersIter(m_activeNonPlayers.end()),
    i_gridExpiry(expiry), m_TerrainData(sTerrainMgr.LoadTerrain(id)),
    m_scripts(*this),
    i_data(nullptr)
{

    m_CreatureGuids.Set(sMint.FirstTemporaryCreature());
    m_GameObjectGuids.Set(sMint.FirstTemporaryGameObject());

    for (unsigned int j = 0; j < MAX_NUMBER_OF_GRIDS; ++j)
    {
        for (unsigned int idx = 0; idx < MAX_NUMBER_OF_GRIDS; ++idx)
        {

            m_bLoadedGrids[idx][j] = false;
            setNGrid(nullptr, idx, j);
        }
    }

    Map::InitVisibilityDistance();

    m_TerrainData->AddRef();

    m_persistentState = sMapPersistentStateMgr.AddPersistentState(i_mapEntry, GetInstanceId(), 0, IsDungeon());
    m_persistentState->SetUsedByMapState(this);

    m_weatherSystem = new WeatherSystem(this);
}

void Map::InitVisibilityDistance()
{

    m_VisibleDistance = World::GetMaxVisibleDistanceOnContinents();
}

float Map::GetBroadcastRadius() const
{
    return m_cinematicViewerRadius > m_VisibleDistance ? m_cinematicViewerRadius : m_VisibleDistance;
}

void Map::AddCinematicViewer(float radius)
{
    m_cinematicViewerRadii.insert(radius);
    m_cinematicViewerRadius = *m_cinematicViewerRadii.rbegin();
}

void Map::RemoveCinematicViewer(float radius)
{
    std::multiset<float>::iterator itr = m_cinematicViewerRadii.find(radius);
    if (itr == m_cinematicViewerRadii.end())
    {
        sLog.outError("Map::RemoveCinematicViewer: no viewer registered with radius %.1f on map %u", radius, GetId());
        return;
    }

    m_cinematicViewerRadii.erase(itr);
    m_cinematicViewerRadius = m_cinematicViewerRadii.empty() ? 0.0f : *m_cinematicViewerRadii.rbegin();
}

template<class T>

void Map::AddToGrid(T* obj, NGridType* grid, Cell const& cell)
{
    (*grid)(cell.CellX(), cell.CellY()).template AddGridObject<T>(obj);
}

template<>

void Map::AddToGrid(Player* obj, NGridType* grid, Cell const& cell)
{
    (*grid)(cell.CellX(), cell.CellY()).AddWorldObject(obj);
    grid->incPlayerCount();
}

template<>

void Map::AddToGrid(Corpse* obj, NGridType* grid, Cell const& cell)
{
    if (obj->OutlivesItsGrid())
    {
        (*grid)(cell.CellX(), cell.CellY()).AddWorldObject(obj);
    }
    else
    {
        (*grid)(cell.CellX(), cell.CellY()).AddGridObject(obj);
    }
}

template<>

void Map::AddToGrid(Creature* obj, NGridType* grid, Cell const& cell)
{
    if (obj->OutlivesItsGrid())
    {
        (*grid)(cell.CellX(), cell.CellY()).AddWorldObject<Creature>(obj);
    }
    else
    {
        (*grid)(cell.CellX(), cell.CellY()).AddGridObject<Creature>(obj);
    }

    obj->SetCurrentCell(cell);
}

template<class T>

void Map::RemoveFromGrid(T* obj, NGridType* grid, Cell const& cell)
{
    (*grid)(cell.CellX(), cell.CellY()).template RemoveGridObject<T>(obj);
}

template<>

void Map::RemoveFromGrid(Player* obj, NGridType* grid, Cell const& cell)
{
    (*grid)(cell.CellX(), cell.CellY()).RemoveWorldObject(obj);
    grid->decPlayerCount();
}

template<>

void Map::RemoveFromGrid(Corpse* obj, NGridType* grid, Cell const& cell)
{
    if (obj->OutlivesItsGrid())
    {
        (*grid)(cell.CellX(), cell.CellY()).RemoveWorldObject(obj);
    }
    else
    {
        (*grid)(cell.CellX(), cell.CellY()).RemoveGridObject(obj);
    }
}

template<>

void Map::RemoveFromGrid(Creature* obj, NGridType* grid, Cell const& cell)
{
    if (obj->OutlivesItsGrid())
    {
        (*grid)(cell.CellX(), cell.CellY()).RemoveWorldObject<Creature>(obj);
    }
    else
    {
        (*grid)(cell.CellX(), cell.CellY()).RemoveGridObject<Creature>(obj);
    }
}

void Map::DeleteFromWorld(Player* pl)
{
    sPlayerRegistry.Remove(pl);
    delete pl;
}

void
Map::EnsureGridCreated(const GridPair& p)
{
    if (!getNGrid(p.x_coord, p.y_coord))
    {
        setNGrid(new NGridType(p.x_coord * MAX_NUMBER_OF_GRIDS + p.y_coord, p.x_coord, p.y_coord, i_gridExpiry, sWorld.getConfig(CONFIG_BOOL_GRID_UNLOAD)),
            p.x_coord, p.y_coord);

        buildNGridLinkage(getNGrid(p.x_coord, p.y_coord));

        getNGrid(p.x_coord, p.y_coord)->SetGridState(GRID_STATE_IDLE);

        int gx = (MAX_NUMBER_OF_GRIDS - 1) - p.x_coord;
        int gy = (MAX_NUMBER_OF_GRIDS - 1) - p.y_coord;

        if (!m_bLoadedGrids[gx][gy])
        {
            LoadMapAndVMap(gx, gy);
        }
    }
}

void
Map::EnsureGridLoadedAtEnter(const Cell& cell, Player* player)
{
    NGridType* grid;

    bool loadedNow = EnsureGridLoaded(cell);

    if (loadedNow)
    {
        grid = getNGrid(cell.GridX(), cell.GridY());

        if (player)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_MOVES, "Player %s enter cell[%u,%u] triggers of loading grid[%u,%u] on map %u", player->GetName(), cell.CellX(), cell.CellY(), cell.GridX(), cell.GridY(), i_id);
        }
        else
        {
            DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_MOVES, "Active object nearby triggers of loading grid [%u,%u] on map %u", cell.GridX(), cell.GridY(), i_id);
        }

        ResetGridExpiry(*getNGrid(cell.GridX(), cell.GridY()), 0.1f);
        grid->SetGridState(GRID_STATE_ACTIVE);
    }
    else
    {
        grid = getNGrid(cell.GridX(), cell.GridY());
    }

    if (player)
    {
        AddToGrid(player, grid, cell);
    }
}

bool Map::EnsureGridLoaded(const Cell& cell)
{
    EnsureGridCreated(GridPair(cell.GridX(), cell.GridY()));
    NGridType* grid = getNGrid(cell.GridX(), cell.GridY());

    MANGOS_ASSERT(grid != nullptr);
    if (!isGridObjectDataLoaded(cell.GridX(), cell.GridY()))
    {
        grid->markGridObjectDataLoading();
        ObjectGridLoader loader(*grid, this, cell);
        loader.LoadN();

        setGridObjectDataLoaded(true, cell.GridX(), cell.GridY());

        sCorpseManager.AddCorpsesToGrid(GridPair(cell.GridX(), cell.GridY()), (*grid)(cell.CellX(), cell.CellY()), this);
        return true;
    }

    return false;
}

void Map::ForceLoadGrid(float x, float y)
{
    if (!IsLoaded(x, y))
    {
        Cell cell(MaNGOS::ComputeCellPair(x, y));
        EnsureGridCreated(GridPair(cell.GridX(), cell.GridY()));
        if (EnsureGridLoaded(cell))
        {
            NGridType* grid = getNGrid(cell.GridX(), cell.GridY());
            ResetGridExpiry(*grid, 0.1f);
            grid->SetGridState(GRID_STATE_ACTIVE);
        }
        getNGrid(cell.GridX(), cell.GridY())->setUnloadExplicitLock(true);
    }
}

bool Map::Add(Player* player, InitialWorldEntryHook* initialEntry)
{
    player->GetMapRef().link(this, player);
    player->SetMap(this);

    CellPair p = MaNGOS::ComputeCellPair(player->Where().X(), player->Where().Y());
    Cell cell(p);
    EnsureGridLoadedAtEnter(cell, player);
    player->AddToWorld();

    if (initialEntry)
    {
        initialEntry->AfterAddToWorld(*player);
    }

    std::optional<InitialWorldUpdateBatch> initialUpdates;
    if (player->GetSession()->PlayerLoading() && player->GetCamera().GetBody() == player)
    {
        initialUpdates.emplace();
    }
    auto* batch = initialUpdates ? &*initialUpdates : nullptr;

    SendInitSelf(player, batch);
    SendInitTransports(player, batch);

    NGridType* grid = getNGrid(cell.GridX(), cell.GridY());
    player->GetViewPoint().Event_AddedToWorld(
        &(*grid)(cell.CellX(), cell.CellY()), player, batch);

    if (batch && !batch->WasSent())
    {
        if (!batch->FlushAttempted())
        {
            sLog.outError("Initial object update batch for player %u was not flushed by the owner camera", player->GetGUIDLow());
        }
        player->GetSession()->KickPlayer();
    }
    else
    {
        UpdateObjectVisibility(player, cell, p);
    }

    if (i_data)
    {
        i_data->OnPlayerEnter(player);
    }

    return true;
}

template<class T>
    void Map::Add(T* obj)
{
    MANGOS_ASSERT(obj);

    CellPair p = MaNGOS::ComputeCellPair(obj->Where().X(), obj->Where().Y());
    if (p.x_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP || p.y_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP)
    {
        sLog.outError("Map::Add: Object (GUID: %u TypeId: %u) have invalid coordinates X:%f Y:%f grid cell [%u:%u]", obj->GetGUIDLow(), obj->GetTypeId(), obj->Where().X(), obj->Where().Y(), p.x_coord, p.y_coord);
        return;
    }

    obj->SetMap(this);

    Cell cell(p);
    if (obj->IsActiveObject())
    {
        EnsureGridLoadedAtEnter(cell);
    }
    else
    {
        EnsureGridCreated(GridPair(cell.GridX(), cell.GridY()));
    }

    NGridType* grid = getNGrid(cell.GridX(), cell.GridY());
    MANGOS_ASSERT(grid != nullptr);

    AddToGrid(obj, grid, cell);
    obj->AddToWorld();

    if (obj->IsActiveObject())
    {
        AddToActive(obj);
    }

    DEBUG_FILTER_LOG(LOG_FILTER_GRID_ADD, "%s enters grid[%u,%u]", obj->GetGuidStr().c_str(), cell.GridX(), cell.GridY());

    obj->GetViewPoint().Event_AddedToWorld(&(*grid)(cell.CellX(), cell.CellY()));
    obj->SetAsNewObject(true);
    UpdateObjectVisibility(obj, cell, p);
    obj->SetAsNewObject(false);
}

namespace
{

    struct CameraSweep
    {
        Audience const& who;
        MapBroadcaster::Listener const& tell;
        float range;
        uint32 told = 0;

        void Visit(CameraMapType& m)
        {
            for (CameraMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            {
                Camera* camera = itr->getSource();
                Player* owner = camera->GetOwner();

                if (!who.Admits(owner))
                {
                    continue;
                }

                if (who.HasRange()
                    && !camera->GetBody()->Where().WithinDist(who.Subject()->Where(), range))
                {
                    continue;
                }

                tell(owner);
                ++told;
            }
        }

        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };
}

uint32 Map::Hearers(Audience const& who, MapBroadcaster::Listener const& tell)
{
    uint32 told = 0;

    switch (who.How())
    {
        case Audience::Gathering::Near:
        case Audience::Gathering::Ranged:
        {
            Occupant const* subject = who.Subject();
            if (!subject || !subject->IsInWorld())
            {
                return 0;
            }

            CellPair p = MaNGOS::ComputeCellPair(subject->Where().X(), subject->Where().Y());
            if (p.x_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP || p.y_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP)
            {
                sLog.outError("Map::Hearers: %s has invalid coordinates X:%f Y:%f grid cell [%u:%u]",
                              subject->GetGuidStr().c_str(), subject->Where().X(), subject->Where().Y(),
                              p.x_coord, p.y_coord);
                return 0;
            }

            Cell cell(p);
            cell.SetNoCreate();

            if (!loaded(GridPair(cell.data.Part.grid_x, cell.data.Part.grid_y)))
            {
                return 0;
            }

            const float range = who.HasRange() ? who.Range() : GetBroadcastRadius();

            CameraSweep sweep{ who, tell, range };
            TypeContainerVisitor<CameraSweep, WorldTypeMapContainer> visit(sweep);
            cell.Visit(p, visit, *this, *subject, range);

            told = sweep.told;
            break;
        }

        case Audience::Gathering::Roll:
        case Audience::Gathering::Zone:
        {
            const bool byZone = who.How() == Audience::Gathering::Zone;

            for (auto itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
            {
                Player* listener = itr->getSource();
                if (!who.Admits(listener))
                {
                    continue;
                }

                if (byZone && listener->GetTerrain()->GetZoneId(listener->Where().X(), listener->Where().Y(),
                                                                listener->Where().Z()) != who.Zone())
                {
                    continue;
                }

                tell(listener);
                ++told;
            }
            break;
        }
    }

    return told;
}

uint32 Map::Across(Audience const& who, MapBroadcaster::Listener const& tell)
{

    if (who.HasRange())
    {
        return 0;
    }

    uint32 told = 0;

    for (Transport* vessel : sFleet.On(GetId()))
    {
        TransportMap* hull = vessel->AsMap();
        if (!hull || vessel->GetMap() != this)
        {
            continue;
        }

        PlayerList const& aboard = hull->GetPlayers();
        for (PlayerList::const_iterator itr = aboard.begin(); itr != aboard.end(); ++itr)
        {
            Player* passenger = itr->getSource();
            if (!who.Admits(passenger) || !passenger->GetSession())
            {
                continue;
            }

            tell(passenger);
            ++told;
        }
    }

    return told;
}

bool Map::loaded(const GridPair& p) const
{
    return (getNGrid(p.x_coord, p.y_coord) && isGridObjectDataLoaded(p.x_coord, p.y_coord));
}

void Map::VisitNearbyCellsOf(Occupant* obj,
    TypeContainerVisitor<MaNGOS::ObjectUpdater, GridTypeMapContainer> &gridVisitor,
    TypeContainerVisitor<MaNGOS::ObjectUpdater, WorldTypeMapContainer> &worldVisitor)
{
    if (!IsPlaceable(*obj))
    {
        return;
    }

    CellArea area = Cell::CalculateCellArea(obj->Where().X(), obj->Where().Y(), GetVisibilityDistance());

    for (uint32 x = area.low_bound.x_coord; x <= area.high_bound.x_coord; ++x)
    {
        for (uint32 y = area.low_bound.y_coord; y <= area.high_bound.y_coord; ++y)
        {

            uint32 cell_id = (y * TOTAL_NUMBER_OF_CELLS_PER_MAP) + x;
            if (!isCellMarked(cell_id))
            {
                markCell(cell_id);
                CellPair pair(x, y);
                Cell cell(pair);
                cell.SetNoCreate();
                Visit(cell, gridVisitor);
                Visit(cell, worldVisitor);
            }
        }
    }
}

void Map::Update(const uint32& t_diff)
{
    metrics::PhaseClock<metrics::TickRecord> phases(m_ticks, getMSTime());

    for (MapMailbox::Entry& entry : m_mailbox.Take())
    {
        WorldSession* session = entry.session;
        if (!session)
        {
            continue;
        }

        Player* plr = session->GetPlayer();
        if (!plr || plr->GetObjectGuid() != entry.player)
        {
            continue;
        }

        if (!plr->IsInWorld() || plr->GetMap() != this)
        {
            continue;
        }

        WorkSentry watch(LookupOpcodeName(entry.packet->GetOpcode()));
        session->HandlePacket(*entry.packet);
    }

    phases.Mark(metrics::TickPhase::Mailbox, getMSTime());

    for (m_mapRefIter = m_mapRefManager.begin(); m_mapRefIter != m_mapRefManager.end(); ++m_mapRefIter)
    {
        Player* plr = m_mapRefIter->getSource();
        if (plr && plr->IsInWorld())
        {
            Occupant::UpdateHelper helper(plr);
            helper.Update(t_diff);
        }
    }

    phases.Mark(metrics::TickPhase::Players, getMSTime());

    resetMarkedCells();

    MaNGOS::ObjectUpdater updater(t_diff);

    TypeContainerVisitor<MaNGOS::ObjectUpdater, GridTypeMapContainer  > grid_object_update(updater);

    TypeContainerVisitor<MaNGOS::ObjectUpdater, WorldTypeMapContainer > world_object_update(updater);

    for (m_mapRefIter = m_mapRefManager.begin(); m_mapRefIter != m_mapRefManager.end(); ++m_mapRefIter)
    {
        Player* plr = m_mapRefIter->getSource();

        if (!plr || !plr->IsInWorld())
        {
            continue;
        }

        VisitNearbyCellsOf(plr, grid_object_update, world_object_update);

        if (!IsDungeon() && plr->IsInCombat())
        {
            std::vector<Creature*> _removeList;
            HostileRefManager& href = plr->GetHostileRefManager();
            HostileReference* ref = href.getFirst();

            while (ref)
            {
                if (Unit* unit = ref->getSource()->getOwner())
                {
                    if (static_cast<Creature*>(unit) && unit->GetMapId() == plr->GetMapId() && !InReach(*unit, *plr, GetVisibilityDistance(), false))
                    {
                        _removeList.push_back(static_cast<Creature*>(unit));
                    }
                }

                ref = ref->next();
            }

            for (std::vector<Creature*>::iterator it = _removeList.begin(); it != _removeList.end(); ++it)
            {
                (*it)->RemoveAurasByCaster(plr->GetObjectGuid());
                (*it)->_removeAttacker(plr);
                (*it)->GetHostileRefManager().deleteReference(plr);

                href.deleteReference(*it);

                VisitNearbyCellsOf(*it, grid_object_update, world_object_update);
            }
        }
    }

    phases.Mark(metrics::TickPhase::GridObjects, getMSTime());

    if (!m_activeNonPlayers.empty())
    {
        for (m_activeNonPlayersIter = m_activeNonPlayers.begin(); m_activeNonPlayersIter != m_activeNonPlayers.end();)
        {
            Occupant* obj = *m_activeNonPlayersIter;

            ++m_activeNonPlayersIter;

            if (!obj || !obj->IsInWorld())
            {
                continue;
            }

            VisitNearbyCellsOf(obj, grid_object_update, world_object_update);
        }
    }

    phases.Mark(metrics::TickPhase::ActiveObjects, getMSTime());

    m_backlog.Send();

    if (!IsBattleGround())
    {
        for (GridRefManager<NGridType>::iterator i = GridRefManager<NGridType>::begin(); i != GridRefManager<NGridType>::end();)
        {
            NGridType* grid = i->getSource();
            GridInfo* info = i->getSource()->getGridInfoRef();
            ++i;
            MANGOS_ASSERT(grid->GetGridState() >= 0 && grid->GetGridState() < MAX_GRID_STATE);
            GridStateFor(grid->GetGridState()).Update(*this, *grid, *info, grid->getX(), grid->getY(), t_diff);
        }
    }

    phases.Mark(metrics::TickPhase::ObjectUpdates, getMSTime());

    if (!m_scripts.Empty())
    {
        m_scripts.RunDue();
    }

    if (i_data)
    {
        i_data->Update(t_diff);
    }

    m_weatherSystem->UpdateWeathers(t_diff);

    phases.Mark(metrics::TickPhase::Scripts, getMSTime());

    for (Transport* vessel : sFleet.On(GetId()))
    {
        if (vessel->GetMap() == this)
        {
            Occupant::UpdateHelper helper(vessel);
            helper.Update(t_diff);
        }
    }
}

bool Map::Rebind(Player* player, float x, float y, float z, float o)
{
    Map* from = player->GetMap();
    if (!from || from == this)
    {
        return false;
    }

    CellPair was = MaNGOS::ComputeCellPair(player->Where().X(), player->Where().Y());
    if (was.x_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP || was.y_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP)
    {
        sLog.outError("Map::Rebind: %s stands nowhere on map %u", player->GetGuidStr().c_str(), from->i_id);
        return false;
    }

    Cell leaving(was);
    NGridType* oldGrid = from->getNGrid(leaving.GridX(), leaving.GridY());
    if (!oldGrid)
    {
        sLog.outError("Map::Rebind: no grid[%u,%u] on map %u", leaving.GridX(), leaving.GridY(), from->i_id);
        return false;
    }

    if (from->m_mapRefIter == player->GetMapRef())
    {
        from->m_mapRefIter = from->m_mapRefIter->nocheck_prev();
    }
    player->GetMapRef().unlink();
    from->RemoveFromGrid(player, oldGrid, leaving);

    player->Place().MoveTo(x, y, z, o);
    player->SetMap(this);
    player->GetMapRef().link(this, player);

    CellPair now = MaNGOS::ComputeCellPair(x, y);
    Cell arriving(now);
    EnsureGridLoadedAtEnter(arriving, player);

    NGridType* grid = getNGrid(arriving.GridX(), arriving.GridY());
    player->GetViewPoint().Event_GridChanged(&(*grid)(arriving.CellX(), arriving.CellY()));

    UpdateObjectVisibility(player, arriving, now);

    return true;
}

void Map::Remove(Player* player, bool remove)
{

    if (i_data)
    {
        i_data->OnPlayerLeave(player);
    }

    if (remove)
    {
        player->CleanupsBeforeDelete();
    }
    else
    {
        player->RemoveFromWorld();
    }

    if (m_mapRefIter == player->GetMapRef())
    {
        m_mapRefIter = m_mapRefIter->nocheck_prev();
    }
    player->GetMapRef().unlink();
    CellPair p = MaNGOS::ComputeCellPair(player->Where().X(), player->Where().Y());
    if (p.x_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP || p.y_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP)
    {

        if (remove)
        {
            DeleteFromWorld(player);
        }

        return;
    }

    Cell cell(p);

    if (!getNGrid(cell.data.Part.grid_x, cell.data.Part.grid_y))
    {
        sLog.outError("Map::Remove() i_grids was nullptr x:%d, y:%d", cell.data.Part.grid_x, cell.data.Part.grid_y);
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_MOVES, "Remove player %s from grid[%u,%u]", player->GetName(), cell.GridX(), cell.GridY());
    NGridType* grid = getNGrid(cell.GridX(), cell.GridY());
    MANGOS_ASSERT(grid != nullptr);

    RemoveFromGrid(player, grid, cell);

    SendRemoveTransports(player);
    UpdateObjectVisibility(player, cell, p);

    if (remove)
    {
        DeleteFromWorld(player);
    }
}

template<class T>
    void Map::Remove(T* obj, bool remove)
{
    CellPair p = MaNGOS::ComputeCellPair(obj->Where().X(), obj->Where().Y());
    if (p.x_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP || p.y_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP)
    {
        sLog.outError("Map::Remove: Object (GUID: %u TypeId:%u) have invalid coordinates X:%f Y:%f grid cell [%u:%u]", obj->GetGUIDLow(), obj->GetTypeId(), obj->Where().X(), obj->Where().Y(), p.x_coord, p.y_coord);
        return;
    }

    Cell cell(p);
    NGridType* grid = getNGrid(cell.GridX(), cell.GridY());
    if (!grid || !grid->isCellObjectDataLoaded(cell.CellX(), cell.CellY()))
    {
        return;
    }

    DEBUG_LOG("Remove object (GUID: %u TypeId:%u) from grid[%u,%u]", obj->GetGUIDLow(), obj->GetTypeId(), cell.data.Part.grid_x, cell.data.Part.grid_y);
    MANGOS_ASSERT(grid != nullptr);

    if (obj->IsActiveObject())
    {
        RemoveFromActive(obj);
    }

    if (remove)
    {
        obj->CleanupsBeforeDelete();
    }
    else
    {
        obj->RemoveFromWorld();
    }

    UpdateObjectVisibility(obj, cell, p);
    RemoveFromGrid(obj, grid, cell);

    if (remove)
    {

        if (!sWorld.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY))
        {
            SaveRespawnTime(*obj);
        }

        delete obj;
    }
}

void Map::PlayerRelocation(Player* player, float x, float y, float z, float orientation)
{
    MANGOS_ASSERT(player);

    CellPair old_val = MaNGOS::ComputeCellPair(player->Where().X(), player->Where().Y());
    CellPair new_val = MaNGOS::ComputeCellPair(x, y);

    Cell old_cell(old_val);
    Cell new_cell(new_val);
    bool same_cell = (new_cell == old_cell);

    player->Place().MoveTo(x, y, z, orientation);
    player->m_movementInfo.ChangePosition(x, y, z, orientation);

    if (old_cell.DiffGrid(new_cell) || old_cell.DiffCell(new_cell))
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_MOVES, "Player %s relocation grid[%u,%u]cell[%u,%u]->grid[%u,%u]cell[%u,%u]", player->GetName(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.GridX(), new_cell.GridY(), new_cell.CellX(), new_cell.CellY());

        NGridType* oldGrid = getNGrid(old_cell.GridX(), old_cell.GridY());
        RemoveFromGrid(player, oldGrid, old_cell);
        if (!old_cell.DiffGrid(new_cell))
        {
            AddToGrid(player, oldGrid, new_cell);
        }
        else
        {
            EnsureGridLoadedAtEnter(new_cell, player);
        }

        NGridType* newGrid = getNGrid(new_cell.GridX(), new_cell.GridY());
        player->GetViewPoint().Event_GridChanged(&(*newGrid)(new_cell.CellX(), new_cell.CellY()));
    }

    player->OnRelocated();

    NGridType* newGrid = getNGrid(new_cell.GridX(), new_cell.GridY());
    if (!same_cell && newGrid->GetGridState() != GRID_STATE_ACTIVE)
    {
        ResetGridExpiry(*newGrid, 0.1f);
        newGrid->SetGridState(GRID_STATE_ACTIVE);
    }
}

void Map::CreatureRelocation(Creature* creature, float x, float y, float z, float ang)
{
    MANGOS_ASSERT(CheckGridIntegrity(creature, false));

    Cell new_cell(MaNGOS::ComputeCellPair(x, y));

    if (CreatureCellRelocation(creature, new_cell))
    {

        creature->Place().MoveTo(x, y, z, ang);
        creature->m_movementInfo.ChangePosition(x, y, z, ang);
        creature->OnRelocated();
    }

    else if (!CreatureRespawnRelocation(creature))
    {

        DEBUG_FILTER_LOG(LOG_FILTER_CREATURE_MOVES, "Creature (GUID: %u Entry: %u ) can't be move to unloaded respawn grid.", creature->GetGUIDLow(), creature->GetEntry());
    }

    MANGOS_ASSERT(CheckGridIntegrity(creature, true));
}

bool Map::CreatureCellRelocation(Creature* c, const Cell &new_cell)
{
    Cell const& old_cell = c->GetCurrentCell();
    if (old_cell.DiffGrid(new_cell))
    {
        NGridType* destGrid = getNGrid(new_cell.GridX(), new_cell.GridY());
        bool destCellResident = destGrid && destGrid->isCellObjectDataLoaded(new_cell.CellX(), new_cell.CellY());
        if (!c->IsActiveObject() && !loaded(new_cell.gridPair()) && !destCellResident)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_CREATURE_MOVES, "Creature (GUID: %u Entry: %u) attempt move from grid[%u,%u]cell[%u,%u] to unloaded grid[%u,%u]cell[%u,%u].", c->GetGUIDLow(), c->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.GridX(), new_cell.GridY(), new_cell.CellX(), new_cell.CellY());
            return false;
        }
        if (!destCellResident)
        {
            EnsureGridLoadedAtEnter(new_cell);
        }
    }

    if (!c->IsActiveObject())
    {
        NGridType* ng = getNGrid(new_cell.GridX(), new_cell.GridY());
        if (!ng || !ng->isCellObjectDataLoaded(new_cell.CellX(), new_cell.CellY()))
        {
            return false;
        }
    }

    if (old_cell != new_cell)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_CREATURE_MOVES, "Creature (GUID: %u Entry: %u) moved in grid[%u,%u] from cell[%u,%u] to cell[%u,%u].", c->GetGUIDLow(), c->GetEntry(), old_cell.GridX(), old_cell.GridY(), old_cell.CellX(), old_cell.CellY(), new_cell.CellX(), new_cell.CellY());
        NGridType* oldGrid = getNGrid(old_cell.GridX(), old_cell.GridY());
        NGridType* newGrid = getNGrid(new_cell.GridX(), new_cell.GridY());

        RemoveFromGrid(c, oldGrid, old_cell);
        AddToGrid(c, newGrid, new_cell);
        c->GetViewPoint().Event_GridChanged(&(*newGrid)(new_cell.CellX(), new_cell.CellY()));

    }
    return true;
}

bool Map::CreatureRespawnRelocation(Creature* c)
{
    float resp_x, resp_y, resp_z, resp_o;
    resp_x = c->Spawn().X();
    resp_y = c->Spawn().Y();
    resp_z = c->Spawn().Z();
    resp_o = c->Spawn().Facing();

    CellPair resp_val = MaNGOS::ComputeCellPair(resp_x, resp_y);
    Cell resp_cell(resp_val);

    c->CombatStop();
    c->GetMotionMaster()->Clear();

    DEBUG_FILTER_LOG(LOG_FILTER_CREATURE_MOVES, "Creature (GUID: %u Entry: %u) will moved from grid[%u,%u]cell[%u,%u] to respawn grid[%u,%u]cell[%u,%u].", c->GetGUIDLow(), c->GetEntry(), c->GetCurrentCell().GridX(), c->GetCurrentCell().GridY(), c->GetCurrentCell().CellX(), c->GetCurrentCell().CellY(), resp_cell.GridX(), resp_cell.GridY(), resp_cell.CellX(), resp_cell.CellY());

    if (CreatureCellRelocation(c, resp_cell))
    {
        c->Place().MoveTo(resp_x, resp_y, resp_z, resp_o);
        c->m_movementInfo.ChangePosition(resp_x, resp_y, resp_z, resp_o);
        c->GetMotionMaster()->Initialize();
        c->OnRelocated();
        return true;
    }
    else
    {
        return false;
    }
}

bool Map::HasPlayerInOrAroundGrid(uint32 gridX, uint32 gridY) const
{
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            int gx = int(gridX) + dx;
            int gy = int(gridY) + dy;
            if (gx < 0 || gy < 0 || gx >= int(MAX_NUMBER_OF_GRIDS) || gy >= int(MAX_NUMBER_OF_GRIDS))
            {
                continue;
            }
            NGridType* g = getNGrid(uint32(gx), uint32(gy));
            if (g && g->getPlayerCount() > 0)
            {
                return true;
            }
        }
    }
    return false;
}

bool Map::IsCellLoaded(float x, float y) const
{
    Cell cell(MaNGOS::ComputeCellPair(x, y));
    NGridType* grid = getNGrid(cell.GridX(), cell.GridY());
    return grid && grid->isCellObjectDataLoaded(cell.CellX(), cell.CellY());
}

bool Map::UnloadGrid(const uint32& x, const uint32& y, bool pForce)
{
    NGridType* grid = getNGrid(x, y);
    MANGOS_ASSERT(grid != nullptr);

    {
        if (!pForce && ActiveObjectsNearGrid(x, y))
        {
            return false;
        }

        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "Unloading grid[%u,%u] for map %u", x, y, i_id);
        ObjectGridUnloader unloader(*grid);

        RemoveAllObjectsInRemoveList();

        unloader.MoveToRespawnN();

        RemoveAllObjectsInRemoveList();

        unloader.UnloadN();
        delete getNGrid(x, y);
        setNGrid(nullptr, x, y);
    }

    int gx = (MAX_NUMBER_OF_GRIDS - 1) - x;
    int gy = (MAX_NUMBER_OF_GRIDS - 1) - y;

    if (m_bLoadedGrids[gx][gy])
    {
        m_bLoadedGrids[gx][gy] = false;
        m_TerrainData->Unload(gx, gy);
    }

    DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "Unloading grid[%u,%u] for map %u finished", x, y, i_id);
    return true;
}

void Map::UnloadAll(bool pForce)
{
    for (GridRefManager<NGridType>::iterator i = GridRefManager<NGridType>::begin(); i != GridRefManager<NGridType>::end();)
    {
        NGridType& grid(*i->getSource());
        ++i;
        UnloadGrid(grid.getX(), grid.getY(), pForce);
    }
}

bool Map::CheckGridIntegrity(Creature* c, bool moved) const
{
    Cell const& cur_cell = c->GetCurrentCell();

    CellPair xy_val = MaNGOS::ComputeCellPair(c->Where().X(), c->Where().Y());
    Cell xy_cell(xy_val);
    if (xy_cell != cur_cell)
    {
        sLog.outError("Creature (GUIDLow: %u) X: %f Y: %f (%s) in grid[%u,%u]cell[%u,%u] instead grid[%u,%u]cell[%u,%u]",
            c->GetGUIDLow(),
            c->Where().X(), c->Where().Y(), (moved ? "final" : "original"),
            cur_cell.GridX(), cur_cell.GridY(), cur_cell.CellX(), cur_cell.CellY(),
            xy_cell.GridX(),  xy_cell.GridY(),  xy_cell.CellX(),  xy_cell.CellY());
        return true;
    }

    return true;
}

const char* Map::GetMapName() const
{
    return i_mapEntry ? i_mapEntry->MapName_lang[sWorld.GetDefaultDbcLocale()] : "UNNAMEDMAP\x0";
}

void Map::UpdateObjectVisibility(Occupant* obj, Cell cell, CellPair cellpair)
{
    cell.SetNoCreate();
    MaNGOS::VisibleChangesNotifier notifier(*obj);
    TypeContainerVisitor<MaNGOS::VisibleChangesNotifier, WorldTypeMapContainer > player_notifier(notifier);
    cell.Visit(cellpair, player_notifier, *this, *obj, GetVisibilityDistance());
}

void Map::SendInitSelf(Player* player, InitialWorldUpdateBatch* batch)
{
    DETAIL_LOG("Creating player data for himself %u", player->GetGUIDLow());

    UpdateData localData;
    UpdateData& data = batch ? batch->Data() : localData;

    if (Transport* transport = player->GetTransport())
    {
        if (batch)
        {
            batch->MarkTransport();
        }
        transport->BuildCreateUpdateBlockForPlayer(&data, player);

        if (TransportMap* hull = transport->AsMap())
        {
            hull->AppendCrewCreateBlocks(data, player);
        }
    }

    player->BuildCreateUpdateBlockForPlayer(&data, player);

    if (batch)
    {

        return;
    }

    WorldPacket packet;
    data.BuildPacket(&packet);
    player->GetSession()->SendPacket(&packet);
}

void Map::SendInitTransports(Player* player, InitialWorldUpdateBatch* batch)
{

    if (TransportMap* hull = AsTransport())
    {
        if (batch)
        {
            TransportMap::AppendVesselCreateBlocks(hull->Vessel(), player, batch->Data());
            batch->MarkTransport();
        }
        else
        {
            TransportMap::AnnounceVessel(hull->Vessel(), player);
        }
        return;
    }

    for (Transport* vessel : sFleet.On(i_id))
    {

        if (vessel == player->GetTransport() || vessel->GetMapId() != i_id)
        {
            continue;
        }

        if (batch)
        {
            TransportMap::AppendVesselCreateBlocks(vessel, player, batch->Data());
            batch->MarkTransport();
        }
        else
        {
            TransportMap::AnnounceVessel(vessel, player);
        }
    }
}

void Map::SendRemoveTransports(Player* player)
{
    Fleet::Vessels const& calling = sFleet.On(player->GetMapId());
    if (calling.empty())
    {
        return;
    }

    UpdateData transData;

    for (Transport* vessel : calling)
    {
        if (vessel != player->GetTransport() && vessel->GetMapId() != i_id)
        {
            vessel->BuildOutOfRangeUpdateBlock(&transData);
            player->ForgetAtClient(vessel->GetObjectGuid());
        }
    }

    WorldPacket packet;
    transData.BuildPacket(&packet);
    player->GetSession()->SendPacket(&packet);
}

inline void Map::setNGrid(NGridType* grid, uint32 x, uint32 y)
{
    if (x >= MAX_NUMBER_OF_GRIDS || y >= MAX_NUMBER_OF_GRIDS)
    {
        sLog.outError("map::setNGrid() Invalid grid coordinates found: %d, %d!", x, y);
        MANGOS_ASSERT(false);
    }
    i_grids[x][y] = grid;
}

void Map::AddObjectToRemoveList(Occupant* obj)
{
    MANGOS_ASSERT(obj->GetMapId() == GetId() && obj->GetInstanceId() == GetInstanceId());

    obj->CleanupsBeforeDelete();

    i_objectsToRemove.insert(obj);

}

void Map::RemoveAllObjectsInRemoveList()
{
    if (i_objectsToRemove.empty())
    {
        return;
    }

    while (!i_objectsToRemove.empty())
    {
        Occupant* obj = *i_objectsToRemove.begin();
        i_objectsToRemove.erase(i_objectsToRemove.begin());

        switch (obj->GetTypeId())
        {
            case TYPEID_CORPSE:
            {

                Corpse* corpse = GetCorpse(obj->GetObjectGuid());
                if (!corpse)
                {
                    sLog.outError("Try delete corpse/bones %u that not in map", obj->GetGUIDLow());
                }
                else
                {
                    Remove(corpse, true);
                }
                break;
            }
            case TYPEID_DYNAMICOBJECT:
                Remove((DynamicObject*)obj, true);
                break;
            case TYPEID_GAMEOBJECT:
                Remove((GameObject*)obj, true);
                break;
            case TYPEID_UNIT:
                Remove((Creature*)obj, true);
                break;
            default:
                sLog.outError("Non-grid object (TypeId: %u) in grid object removing list, ignored.", obj->GetTypeId());
                break;
        }
    }

}

uint32 Map::GetPlayersCountExceptGMs() const
{
    uint32 count = 0;
    for (MapRefManager::const_iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
    {
        if (!itr->getSource()->isGameMaster())
        {
            ++count;
        }
    }
    return count;
}

bool Map::ActiveObjectsNearGrid(uint32 x, uint32 y) const
{
    MANGOS_ASSERT(x < MAX_NUMBER_OF_GRIDS);
    MANGOS_ASSERT(y < MAX_NUMBER_OF_GRIDS);

    CellPair cell_min(x * MAX_NUMBER_OF_CELLS, y * MAX_NUMBER_OF_CELLS);
    CellPair cell_max(cell_min.x_coord + MAX_NUMBER_OF_CELLS, cell_min.y_coord + MAX_NUMBER_OF_CELLS);

    float viewDist = GetVisibilityDistance();
    int cell_range = (int)ceilf(viewDist / SIZE_OF_GRID_CELL) + 1;

    cell_min << cell_range;
    cell_min -= cell_range;
    cell_max >> cell_range;
    cell_max += cell_range;

    for (MapRefManager::const_iterator iter = m_mapRefManager.begin(); iter != m_mapRefManager.end(); ++iter)
    {
        Player* plr = iter->getSource();

        CellPair p = MaNGOS::ComputeCellPair(plr->Where().X(), plr->Where().Y());
        if ((cell_min.x_coord <= p.x_coord && p.x_coord <= cell_max.x_coord) &&
            (cell_min.y_coord <= p.y_coord && p.y_coord <= cell_max.y_coord))
        {
            return true;
        }
    }

    for (ActiveNonPlayers::const_iterator iter = m_activeNonPlayers.begin(); iter != m_activeNonPlayers.end(); ++iter)
    {
        Occupant* obj = *iter;

        CellPair p = MaNGOS::ComputeCellPair(obj->Where().X(), obj->Where().Y());
        if ((cell_min.x_coord <= p.x_coord && p.x_coord <= cell_max.x_coord) &&
            (cell_min.y_coord <= p.y_coord && p.y_coord <= cell_max.y_coord))
        {
            return true;
        }
    }

    return false;
}

void Map::AddToActive(Occupant* obj)
{
    m_activeNonPlayers.insert(obj);
    Cell cell = Cell(MaNGOS::ComputeCellPair(obj->Where().X(), obj->Where().Y()));
    EnsureGridLoadedAtEnter(cell);

    if (IsCreature(obj))
    {
        Creature* c = (Creature*)obj;

        if (!c->IsPet() && npcs::Listed(*c))
        {
            float x, y, z;
            x = c->Spawn().X();
    y = c->Spawn().Y();
    z = c->Spawn().Z();
            GridPair p = MaNGOS::ComputeGridPair(x, y);
            if (getNGrid(p.x_coord, p.y_coord))
            {
                getNGrid(p.x_coord, p.y_coord)->incUnloadActiveLock();
            }
            else
            {
                GridPair p2 = MaNGOS::ComputeGridPair(c->Where().X(), c->Where().Y());
                sLog.outError("Active creature (GUID: %u Entry: %u) added to grid[%u,%u] but spawn grid[%u,%u] not loaded.",
                    c->GetGUIDLow(), c->GetEntry(), p.x_coord, p.y_coord, p2.x_coord, p2.y_coord);
            }
        }
    }
}

void Map::RemoveFromActive(Occupant* obj)
{

    if (m_activeNonPlayersIter != m_activeNonPlayers.end())
    {
        ActiveNonPlayers::iterator itr = m_activeNonPlayers.find(obj);
        if (itr == m_activeNonPlayersIter)
        {
            ++m_activeNonPlayersIter;
        }

        if (itr != m_activeNonPlayers.end())
        {
            m_activeNonPlayers.erase(itr);
        }
    }
    else
    {
        m_activeNonPlayers.erase(obj);
    }

    if (IsCreature(obj))
    {
        Creature* c = (Creature*)obj;

        if (!c->IsPet() && npcs::Listed(*c))
        {
            float x, y, z;
            x = c->Spawn().X();
    y = c->Spawn().Y();
    z = c->Spawn().Z();
            GridPair p = MaNGOS::ComputeGridPair(x, y);
            if (getNGrid(p.x_coord, p.y_coord))
            {
                getNGrid(p.x_coord, p.y_coord)->decUnloadActiveLock();
            }
            else
            {
                GridPair p2 = MaNGOS::ComputeGridPair(c->Where().X(), c->Where().Y());
                sLog.outError("Active creature (GUID: %u Entry: %u) removed from grid[%u,%u] but spawn grid[%u,%u] not loaded.",
                    c->GetGUIDLow(), c->GetEntry(), p.x_coord, p.y_coord, p2.x_coord, p2.y_coord);
            }
        }
    }
}

void Map::CreateInstanceData(bool load)
{
    if (i_data != nullptr)
    {
        return;
    }

    uint32 i_script_id = 0;
    if (!i_data)
    {
        i_script_id = GetScriptId();

        if (!i_script_id)
        {
            return;
        }

        i_data = sScriptMgr.CreateInstanceData(this);
        if (!i_data)
        {
            return;
        }
    }

    if (load)
    {

        QueryResult* result;

        if (Instanceable())
        {
            result = CharacterDatabase.PQuery("SELECT `data` FROM `instance` WHERE `id` = '%u'", i_InstanceId);
        }
        else
        {
            result = CharacterDatabase.PQuery("SELECT `data` FROM `world` WHERE `map` = '%u'", GetId());
        }

        if (result)
        {
            Field* fields = result->Fetch();
            const char* data = fields[0].GetString();
            if (data)
            {
                DEBUG_LOG("Loading instance data for `%s` (Map: %u Instance: %u)", sScriptMgr.GetScriptName(i_script_id), GetId(), i_InstanceId);
                i_data->Load(data);
            }
            delete result;
        }
        else
        {

            if (!Instanceable())
            {
                CharacterDatabase.PExecute("INSERT INTO `world` VALUES ('%u', '')", GetId());
            }
        }
    }
    else
    {
        DEBUG_LOG("New instance data, \"%s\" ,initialized!", sScriptMgr.GetScriptName(i_script_id));
        i_data->Initialize();
    }
}

void Map::TeleportAllPlayersTo(TeleportLocation loc)
{
    while (HavePlayers())
    {
        if (Player* plr = m_mapRefManager.getFirst()->getSource())
        {

            switch (loc)
            {
                case TELEPORT_LOCATION_HOMEBIND:
                    plr->TeleportToHomebind();
                    break;
                case TELEPORT_LOCATION_BG_ENTRY_POINT:
                    plr->Battle().TeleportBack();
                    break;
                default:
                    break;
            }

            plr->GetMapRef().unlink();
        }
    }
}

void Map::SetWeather(uint32 zoneId, WeatherType type, float grade, bool permanently)
{
    Weather* wth = m_weatherSystem->FindOrCreateWeather(zoneId);
    wth->SetWeather(WeatherType(type), grade, this, permanently);
}

template void Map::Add(Corpse*);
template void Map::Add(Creature*);
template void Map::Add(GameObject*);
template void Map::Add(DynamicObject*);

template void Map::Remove(Corpse*, bool);
template void Map::Remove(Creature*, bool);
template void Map::Remove(GameObject*, bool);
template void Map::Remove(DynamicObject*, bool);

WorldPersistentState* WorldMap::GetPersistanceState() const
{
    return (WorldPersistentState*)Map::GetPersistentState();
}

DungeonMap::DungeonMap(uint32 id, time_t expiry, uint32 InstanceId)
    : Map(id, expiry, InstanceId),
    m_resetAfterUnload(false), m_unloadWhenEmpty(false)
{
    MANGOS_ASSERT(i_mapEntry->IsDungeon());

    DungeonMap::InitVisibilityDistance();

    m_unloadTimer = std::max(sWorld.getConfig(CONFIG_UINT32_INSTANCE_UNLOAD_DELAY), (uint32)MIN_UNLOAD_DELAY);
}

DungeonMap::~DungeonMap()
{
}

void DungeonMap::InitVisibilityDistance()
{

    m_VisibleDistance = World::GetMaxVisibleDistanceInInstances();
}

bool DungeonMap::Add(Player* player, InitialWorldEntryHook* initialEntry)
{

    if (!CanEnter(player))
    {
        return false;
    }

    DungeonHold* playerBind = player->Binds().To(GetId());
    if (playerBind && playerBind->permanent)
    {

        if (playerBind->state != GetPersistanceState())
        {
            sLog.outError("InstanceMap::Add: player %s(%d) is permanently bound to instance %d,%d,%d,%d,%d but he is being put in instance %d,%d,%d,%d,%d",
                player->GetName(), player->GetGUIDLow(), playerBind->state->GetMapId(),
                playerBind->state->GetInstanceId(),
                playerBind->state->GetPlayerCount(), playerBind->state->GetGroupCount(),
                playerBind->state->CanReset(),
                GetPersistanceState()->GetMapId(), GetPersistanceState()->GetInstanceId(),
                GetPersistanceState()->GetPlayerCount(),
                GetPersistanceState()->GetGroupCount(), GetPersistanceState()->CanReset());
            MANGOS_ASSERT(false);
        }
    }
    else
    {
        Group* pGroup = player->GetGroup();
        if (pGroup)
        {

            DungeonHold* groupBind = pGroup->Binds().To(GetId());
            if (playerBind)
            {
                sLog.outError("InstanceMap::Add: %s is being put in instance %d,%d,%d,%d,%d but he is in group (Id: %d) and is bound to instance %d,%d,%d,%d,%d!",
                    GuidString(player->GetObjectGuid()).c_str(), playerBind->state->GetMapId(), playerBind->state->GetInstanceId(),
                    playerBind->state->GetPlayerCount(), playerBind->state->GetGroupCount(),
                    playerBind->state->CanReset(), pGroup->GetId(),
                    playerBind->state->GetMapId(), playerBind->state->GetInstanceId(),
                    playerBind->state->GetPlayerCount(), playerBind->state->GetGroupCount(), playerBind->state->CanReset());

                if (groupBind)
                {
                    sLog.outError("InstanceMap::Add: the group (Id: %d) is bound to instance %d,%d,%d,%d,%d",
                        pGroup->GetId(),
                        groupBind->state->GetMapId(), groupBind->state->GetInstanceId(),
                        groupBind->state->GetPlayerCount(), groupBind->state->GetGroupCount(), groupBind->state->CanReset());
                }

                player->Binds().Release(GetId());
            }

            if (!groupBind)
            {
                pGroup->Binds().BindTo(GetPersistanceState(), false);
            }
            else
            {

                if (groupBind->state != GetPersistentState())
                {
                    sLog.outError("InstanceMap::Add: %s is being put in instance %d,%d but he is in group (Id: %d) which is bound to instance %d,%d!",
                        GuidString(player->GetObjectGuid()).c_str(), GetPersistentState()->GetMapId(),
                        GetPersistentState()->GetInstanceId(),
                        pGroup->GetId(), groupBind->state->GetMapId(),
                        groupBind->state->GetInstanceId());

                    sLog.outError("MapSave players: %d, group count: %d",
                        GetPersistanceState()->GetPlayerCount(), GetPersistanceState()->GetGroupCount());

                    if (groupBind->state)
                    {
                        sLog.outError("GroupBind save players: %d, group count: %d", groupBind->state->GetPlayerCount(), groupBind->state->GetGroupCount());
                    }
                    else
                    {
                        sLog.outError("GroupBind save nullptr");
                    }
                    MANGOS_ASSERT(false);
                }

                if (groupBind->permanent)
                {
                    WorldPacket data(SMSG_INSTANCE_SAVE_CREATED, 4);
                    data << uint32(0);
                    player->GetSession()->SendPacket(&data);
                    player->Binds().BindTo(GetPersistanceState(), true);
                }
            }
        }
        else
        {

            if (!playerBind)
            {
                player->Binds().BindTo(GetPersistanceState(), false);
            }
            else

            {
                MANGOS_ASSERT(playerBind->state == GetPersistentState());
            }
        }
    }

    SetResetSchedule(false);

    DETAIL_LOG("MAP: Player '%s' is entering instance '%u' of map '%s'", player->GetName(), GetInstanceId(), GetMapName());

    m_unloadTimer = 0;
    m_resetAfterUnload = false;
    m_unloadWhenEmpty = false;

    Map::Add(player, initialEntry);

    return true;
}

void DungeonMap::Update(const uint32& t_diff)
{
    Map::Update(t_diff);
}

void DungeonMap::Remove(Player* player, bool remove)
{
    DETAIL_LOG("MAP: Removing player '%s' from instance '%u' of map '%s' before relocating to other map", player->GetName(), GetInstanceId(), GetMapName());

    if (!m_unloadTimer && m_mapRefManager.getSize() == 1)
    {
        m_unloadTimer = m_unloadWhenEmpty ? MIN_UNLOAD_DELAY : std::max(sWorld.getConfig(CONFIG_UINT32_INSTANCE_UNLOAD_DELAY), (uint32)MIN_UNLOAD_DELAY);
    }

    Map::Remove(player, remove);

    SetResetSchedule(true);
}

bool DungeonMap::Reset(InstanceResetMethod method)
{

    if (HavePlayers())
    {
        if (method == INSTANCE_RESET_ALL)
        {

            for (MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
            {
                itr->getSource()->SendResetFailedNotify(GetId());
            }
        }
        else
        {
            if (method == INSTANCE_RESET_GLOBAL)
            {

                for (MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
                {
                    itr->getSource()->Binds().StillWelcome(false);
                }
            }

            m_unloadWhenEmpty = true;
            m_resetAfterUnload = true;
        }
    }
    else
    {

        m_unloadTimer = MIN_UNLOAD_DELAY;
        m_resetAfterUnload = true;
    }

    return m_mapRefManager.isEmpty();
}

void DungeonMap::PermBindAllPlayers(Player* player)
{
    Group* group = player->GetGroup();

    for (MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
    {
        Player* plr = itr->getSource();

        DungeonHold* bind = plr->Binds().To(GetId());
        if (!bind || !bind->permanent)
        {
            plr->Binds().BindTo(GetPersistanceState(), true);
            WorldPacket data(SMSG_INSTANCE_SAVE_CREATED, 4);
            data << uint32(0);
            plr->GetSession()->SendPacket(&data);
        }

        if (group && group->GetLeaderGuid() == plr->GetObjectGuid())
        {
            group->Binds().BindTo(GetPersistanceState(), true);
        }
    }
}

void DungeonMap::UnloadAll(bool pForce)
{
    TeleportAllPlayersTo(TELEPORT_LOCATION_HOMEBIND);

    if (m_resetAfterUnload == true)
    {
        GetPersistanceState()->DeleteRespawnTimes();
    }

    Map::UnloadAll(pForce);
}

void DungeonMap::SendResetWarnings(uint32 timeLeft) const
{
    for (MapRefManager::const_iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
    {
        itr->getSource()->SendInstanceResetWarning(GetId(), timeLeft);
    }
}

void DungeonMap::SetResetSchedule(bool on)
{

    if (!HavePlayers() && !IsRaid())
    {
        sMapPersistentStateMgr.GetScheduler().ScheduleReset(on, GetPersistanceState()->GetResetTime(), DungeonResetEvent(RESET_EVENT_NORMAL_DUNGEON, GetId(), GetInstanceId()));
    }
}

uint32 DungeonMap::GetMaxPlayers() const
{
    InstanceTemplate const* iTemplate = ObjectMgr::GetInstanceTemplate(GetId());
    if (!iTemplate)
    {
        return 0;
    }
    return iTemplate->maxPlayers;
}

DungeonPersistentState* DungeonMap::GetPersistanceState() const
{
    return (DungeonPersistentState*)Map::GetPersistentState();
}

BattleGroundMap::BattleGroundMap(uint32 id, time_t expiry, uint32 InstanceId)
    : Map(id, expiry, InstanceId)
{

    BattleGroundMap::InitVisibilityDistance();
}

BattleGroundMap::~BattleGroundMap()
{
}

void BattleGroundMap::Update(const uint32& diff)
{
    Map::Update(diff);

    GetBG()->Update(diff);
}

BattleGroundPersistentState* BattleGroundMap::GetPersistanceState() const
{
    return (BattleGroundPersistentState*)Map::GetPersistentState();
}

void BattleGroundMap::InitVisibilityDistance()
{

    m_VisibleDistance = World::GetMaxVisibleDistanceInBGArenas();
}

bool BattleGroundMap::CanEnter(Player* player)
{
    if (!Map::CanEnter(player))
    {
        return false;
    }

    if (player->Battle().Id() != GetInstanceId())
    {
        return false;
    }

    return true;
}

bool BattleGroundMap::Add(Player* player, InitialWorldEntryHook* initialEntry)
{
    if (!CanEnter(player))
    {
        return false;
    }

    player->Binds().StillWelcome(true);

    return Map::Add(player, initialEntry);
}

void BattleGroundMap::Remove(Player* player, bool remove)
{
    DETAIL_LOG("MAP: Removing player '%s' from bg '%u' of map '%s' before relocating to other map", player->GetName(), GetInstanceId(), GetMapName());
    Map::Remove(player, remove);
}

void BattleGroundMap::SetUnload()
{
    m_unloadTimer = MIN_UNLOAD_DELAY;
}

void BattleGroundMap::UnloadAll(bool pForce)
{
    TeleportAllPlayersTo(TELEPORT_LOCATION_BG_ENTRY_POINT);

    Map::UnloadAll(pForce);
}

bool Map::CanEnter(Player* player)
{
    if (player->GetMapRef().getTarget() == this)
    {
        if (player->GetTransport())
        {
            return true;
        }
        else
        {
            sLog.outError("Map::CanEnter -%s already in map!", player->GetGuidStr().c_str());
            MANGOS_ASSERT(false);
            return false;
        }
    }

    return true;
}

Player* Map::GetPlayer(ObjectGuid guid)
{
    Player* plr = sPlayerRegistry.Find(guid);
    return plr && plr->GetMap() == this ? plr : nullptr;
}

Creature* Map::GetCreature(ObjectGuid guid)
{
    return m_objectsStore.find<Creature>(guid, (Creature*)nullptr);
}

Pet* Map::GetPet(ObjectGuid guid)
{
    return m_objectsStore.find<Pet>(guid, (Pet*)nullptr);
}

Corpse* Map::GetCorpse(ObjectGuid guid)
{
    Corpse* ret = sCorpseManager.FindInMap(guid, GetId());
    return ret && ret->GetInstanceId() == GetInstanceId() ? ret : nullptr;
}

Creature* Map::GetAnyTypeCreature(ObjectGuid guid)
{
    switch (GuidHigh(guid))
    {
        case HIGHGUID_UNIT:         return GetCreature(guid);
        case HIGHGUID_PET:          return GetPet(guid);
        default:                    break;
    }

    return nullptr;
}

GameObject* Map::GetGameObject(ObjectGuid guid)
{
    return m_objectsStore.find<GameObject>(guid, (GameObject*)nullptr);
}

DynamicObject* Map::GetDynamicObject(ObjectGuid guid)
{
    return m_objectsStore.find<DynamicObject>(guid, (DynamicObject*)nullptr);
}

Unit* Map::GetUnit(ObjectGuid guid)
{
    if ((guid != 0 && GuidHigh(guid) == HIGHGUID_PLAYER))
    {
        return GetPlayer(guid);
    }

    return GetAnyTypeCreature(guid);
}

Occupant* Map::GetOccupant(ObjectGuid guid)
{
    switch (GuidHigh(guid))
    {
        case HIGHGUID_PLAYER:       return GetPlayer(guid);
        case HIGHGUID_GAMEOBJECT:   return GetGameObject(guid);
        case HIGHGUID_UNIT:         return GetCreature(guid);
        case HIGHGUID_PET:          return GetPet(guid);
        case HIGHGUID_DYNAMICOBJECT: return GetDynamicObject(guid);
        case HIGHGUID_CORPSE:
        {

            Corpse* corpse = GetCorpse(guid);
            return corpse && corpse->IsInWorld() ? corpse : nullptr;
        }
        case HIGHGUID_MO_TRANSPORT:
        case HIGHGUID_TRANSPORT:
        default:                    break;
    }

    return nullptr;
}

uint32 Map::GenerateLocalLowGuid(HighGuid guidhigh)
{

    switch (guidhigh)
    {
        case HIGHGUID_UNIT:
            return m_CreatureGuids.Generate();
        case HIGHGUID_GAMEOBJECT:
            return m_GameObjectGuids.Generate();
        case HIGHGUID_DYNAMICOBJECT:
            return m_DynObjectGuids.Generate();
        case HIGHGUID_PET:
            return m_PetGuids.Generate();
        default:
            MANGOS_ASSERT(false);
            return 0;
    }
}

bool Map::IsInLineOfSight(float srcX, float srcY, float srcZ, float destX, float destY, float destZ) const
{
    return m_TerrainData->IsInLineOfSight(srcX, srcY, srcZ, destX, destY, destZ)
           && m_dyn_tree.IsInLineOfSight(srcX, srcY, srcZ, destX, destY, destZ, PHASE_ANY);
}

bool Map::GetHitPosition(float srcX, float srcY, float srcZ, float& destX, float& destY, float& destZ, float modifyDist) const
{

    const float staticFrac = m_TerrainData->NearestHitFraction(srcX, srcY, srcZ, destX, destY, destZ);
    const float dynFrac = m_dyn_tree.NearestHitFraction(srcX, srcY, srcZ, destX, destY, destZ, PHASE_ANY);
    const float frac = std::min(staticFrac, dynFrac);
    bool result0 = (frac <= 1.0f);
    if (result0)
    {
        const float dx = destX - srcX, dy = destY - srcY, dz = destZ - srcZ;
        const float len = sqrt(dx * dx + dy * dy + dz * dz);

        float travel = frac * len - modifyDist;
        if (travel < 0.0f)
        {
            travel = 0.0f;
        }
        const float t = (len > 0.0f) ? (travel / len) : 0.0f;
        destX = srcX + dx * t;
        destY = srcY + dy * t;
        destZ = srcZ + dz * t;
    }
    return result0;
}

world::terrain::Column Map::ColumnAt(float x, float y, float zTop, float zBottom) const
{
    return m_TerrainData->ColumnAt(x, y, zTop, zBottom, &m_dyn_tree, PHASE_ANY);
}

bool Map::GetHeightInRange(float x, float y, float& z, float maxSearchDist ) const
{
    const auto floor = FloorNear(x, y, z, maxSearchDist);
    if (!floor)
    {
        return false;
    }
    z = *floor;
    return true;
}

std::optional<float> Map::Floor(float x, float y, float z) const
{
    return ColumnAt(x, y, z + FLOOR_BURIED_LIFT, z - FLOOR_SEARCH_DOWN)
           .Floor(z, FLOOR_SEARCH_UP);
}

std::optional<float> Map::FloorNear(float x, float y, float z,
                                    float maxSearchDist ) const
{
    const auto floor = Floor(x, y, z);
    if (!floor || fabs(z - *floor) > maxSearchDist)
    {
        return std::nullopt;
    }
    return floor;
}

float Map::GetHeight(float x, float y, float z) const
{
    const auto floor = Floor(x, y, z);
    return floor ? *floor : INVALID_HEIGHT;
}

void Map::InsertGameObjectModel(const GameObjectModel& mdl)
{
    m_dyn_tree.Insert(const_cast<GameObjectModel&>(mdl));
}

void Map::RemoveGameObjectModel(const GameObjectModel& mdl)
{
    m_dyn_tree.Remove(const_cast<GameObjectModel&>(mdl));
}

bool Map::GetRandomPointInTheAir(float& x, float& y, float& z, float radius)
{
    const float angle = rand_norm_f() * (M_PI_F * 2.0f);
    const float range = rand_norm_f() * radius;

    float i_x = x + range * cos(angle);
    float i_y = y + range * sin(angle);

    float ground = GetHeight(i_x, i_y, z);
    if (ground > INVALID_HEIGHT)
    {
        float min_z = z - 0.7f * radius;
        if (min_z < ground)
        {
            min_z = ground + 2.5f;
        }
        float max_z = std::max(z + 0.7f * radius, min_z);
        x = i_x;
        y = i_y;
        z = min_z + rand_norm_f() * (max_z - min_z);
        return true;
    }
    return false;
}

bool Map::GetReachableRandomPointOnGround(float& x, float& y, float& z, float radius)
{

    const float angle = rand_norm_f() * (M_PI_F * 2.0f);
    const float range = rand_norm_f() * radius;

    float i_x = x + range * cos(angle);
    float i_y = y + range * sin(angle);
    float i_z = z + 1.0f;

    GetHitPosition(x, y, z + 1.0f, i_x, i_y, i_z, -0.5f);
    i_z = z;
    if (!GetHeightInRange(i_x, i_y, i_z))
    {
        return false;
    }

    float ac = fabs(z - i_z);
    float delta = 0;

    float slope = 0;
    const float MAX_SLOPE_IN_RADIAN = 50.0f / 180.0f * M_PI_F;

    delta = fabs(x - i_x);
    if (delta > 0.0f)
    {

        float slope = atan(ac / delta);
        if (slope < MAX_SLOPE_IN_RADIAN)
        {
            x = i_x;
            y = i_y;
            z = i_z;
            return true;
        }
    }

    delta = fabs(y - i_y);
    if (delta > 0.0f)
    {

        slope = atan(ac / delta);
        if (slope < MAX_SLOPE_IN_RADIAN)
        {
            x = i_x;
            y = i_y;
            z = i_z;
            return true;
        }
    }

    return false;
}

bool Map::ContainsGameObjectModel(const GameObjectModel& mdl) const
{
    return m_dyn_tree.Contains(mdl);
}

void Map::RefreshGameObjectModel(GameObjectModel& mdl)
{
    mdl.UpdatePose();
    m_dyn_tree.Refresh(mdl);
}

bool Map::GetRandomPointUnderWater(float& x, float& y, float& z, float radius, GridMapLiquidData& liquid_status)
{
    const float angle = rand_norm_f() * (M_PI_F * 2.0f);
    const float range = rand_norm_f() * radius;

    float i_x = x + range * cos(angle);
    float i_y = y + range * sin(angle);

    float ground = GetHeight(i_x, i_y, z);
    if (ground > INVALID_HEIGHT)
    {
        float min_z = z - 0.7f * radius;
        if (min_z < ground)
        {
            min_z = ground + 0.5f;
        }

        float liquidLevel = liquid_status.level - 2.0f;

        if (min_z > liquidLevel)
        {
            return false;
        }

        float max_z = std::max(z + 0.7f * radius, min_z);
        max_z = std::min(max_z, liquidLevel);
        x = i_x;
        y = i_y;
        z = min_z + rand_norm_f() * (max_z - min_z);
        return true;
    }
    return false;
}

bool Map::GetReachableRandomPosition(Unit* unit, float& x, float& y, float& z, float radius)
{
    float i_x = x;
    float i_y = y;
    float i_z = z;

    bool isFlying;
    bool isSwimming = true;
    switch (unit->GetTypeId())
    {
        case TYPEID_PLAYER:
            isFlying = static_cast<Player*>(unit)->IsFlying();
            break;
        case TYPEID_UNIT:
            isFlying = static_cast<Creature*>(unit)->IsFlying();
            isSwimming = static_cast<Creature*>(unit)->IsSwimming();
            break;
        default:
            sLog.outError("Map::GetReachableRandomPosition> Unsupported unit (%s) is passed!", unit->GetGuidStr().c_str());
            return false;
    }

    if (radius < 0.1f)
    {
        sLog.outError("Map::GetReachableRandomPosition> Invalid radius (%f) for %s", radius, unit->GetGuidStr().c_str());
        return false;
    }

    bool newDestAssigned;
    if (isFlying)
    {
        newDestAssigned = GetRandomPointInTheAir(i_x, i_y, i_z, radius);
    }
    else
    {
        GridMapLiquidData liquid_status;
        GridMapLiquidStatus res = m_TerrainData->getLiquidStatus(i_x, i_y, i_z, MAP_ALL_LIQUIDS, &liquid_status);
        if (isSwimming && (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER)))
        {
            newDestAssigned = GetRandomPointUnderWater(i_x, i_y, i_z, radius, liquid_status);
        }
        else
        {
            newDestAssigned = GetReachableRandomPointOnGround(i_x, i_y, i_z, radius);
        }
    }

    if (newDestAssigned)
    {
        x = i_x;
        y = i_y;
        z = i_z;
        return true;
    }

    return false;
}
