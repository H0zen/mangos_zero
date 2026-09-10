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

#include "PerKind.h"
#include "Utilities/Errors.h"
#include "ObjectGridLoader.h"
#include "CorpseManager.h"
#include "ObjectMgr.h"
#include "MapPersistentStateMgr.h"
#include "Creature.h"
#include "DynamicObject.h"
#include "Corpse.h"
#include "World.h"
#include "CellImpl.h"
#include "BattleGround/BattleGround.h"

class ObjectGridRespawnMover
{
    public:
        explicit ObjectGridRespawnMover(bool cellGranular = false)
            : i_cellGranular(cellGranular)
        {
        }

        void Move(GridType& grid);

        template<class T> void Visit(GridRefManager<T>&) {}

        void Visit(CreatureMapType& m);

    private:
        bool i_cellGranular;
};

void
ObjectGridRespawnMover::Move(GridType& grid)
{
    TypeContainerVisitor<ObjectGridRespawnMover, GridTypeMapContainer > mover(*this);
    grid.Visit(mover);
}

void
ObjectGridRespawnMover::Visit(CreatureMapType& m)
{

    for (CreatureMapType::iterator iter = m.begin(), next; iter != m.end(); iter = next)
    {
        next = iter; ++next;

        Creature* c = iter->getSource();

        MANGOS_ASSERT(!c->IsPet());

        Cell const& cur_cell  = c->GetCurrentCell();

        float resp_x, resp_y, resp_z;
        resp_x = c->Spawn().X();
    resp_y = c->Spawn().Y();
    resp_z = c->Spawn().Z();
        CellPair resp_val = MaNGOS::ComputeCellPair(resp_x, resp_y);
        Cell resp_cell(resp_val);

        bool needsRelocation = i_cellGranular ? (cur_cell != resp_cell) : cur_cell.DiffGrid(resp_cell);
        if (needsRelocation)
        {
            c->GetMap()->CreatureRespawnRelocation(c);

        }
    }
}

class ObjectWorldLoader
{
    public:

        explicit ObjectWorldLoader(ObjectGridLoader& gloader)
            : i_cell(gloader.i_cell), i_grid(gloader.i_grid), i_map(gloader.i_map), i_corpses(0)
        {}

        void Visit(CorpseMapType& m);

        template<class T> void Visit(GridRefManager<T>&) {}

    private:
        Cell i_cell;
        NGridType& i_grid;
        Map* i_map;
    public:
        uint32 i_corpses;
};

template<class T> void addUnitState(T* , CellPair const& )
{
}

template<> void addUnitState(Creature* obj, CellPair const& cell_pair)
{
    Cell cell(cell_pair);

    obj->SetCurrentCell(cell);
}

template <class T>

void LoadHelper(CellGuidSet const& guid_set, CellPair& cell, GridRefManager<T>& , uint32& count, Map* map, GridType& grid)
{
    BattleGround* bg = map->IsBattleGround() ? ((BattleGroundMap*)map)->GetBG() : nullptr;

    for (CellGuidSet::const_iterator i_guid = guid_set.begin(); i_guid != guid_set.end(); ++i_guid)
    {
        uint32 guid = *i_guid;

        T* obj = new T;

        if (!obj->LoadFromDB(guid, map))
        {
            delete obj;
            continue;
        }

        grid.AddGridObject(obj);

        addUnitState(obj, cell);
        obj->SetMap(map);
        obj->AddToWorld();
        if (obj->IsActiveObject())
        {
            map->AddToActive(obj);
        }

        obj->GetViewPoint().Event_AddedToWorld(&grid);

        if (bg)
        {
            bg->OnObjectDBLoad(obj);
        }

        ++count;
    }
}

void LoadHelper(CellCorpseSet const& cell_corpses, CellPair& cell, CorpseMapType& , uint32& count, Map* map, GridType& grid)
{
    if (cell_corpses.empty())
    {
        return;
    }

    for (CellCorpseSet::const_iterator itr = cell_corpses.begin(); itr != cell_corpses.end(); ++itr)
    {
        if (itr->second != map->GetInstanceId())
        {
            continue;
        }

        uint32 player_lowguid = itr->first;

        Corpse* obj = sCorpseManager.FindForPlayer(MakeGuid(HIGHGUID_PLAYER, player_lowguid));
        if (!obj)
        {
            continue;
        }

        grid.AddWorldObject(obj);

        addUnitState(obj, cell);
        obj->SetMap(map);
        obj->AddToWorld();
        if (obj->IsActiveObject())
        {
            map->AddToActive(obj);
        }

        ++count;
    }
}

void
ObjectGridLoader::Visit(GameObjectMapType& m)
{
    uint32 x = (i_cell.GridX() * MAX_NUMBER_OF_CELLS) + i_cell.CellX();
    uint32 y = (i_cell.GridY() * MAX_NUMBER_OF_CELLS) + i_cell.CellY();
    CellPair cell_pair(x, y);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    CellObjectGuids const& cell_guids = sObjectMgr.GetCellObjectGuids(i_map->GetId(), cell_id);

    GridType& grid = (*i_map->getNGrid(i_cell.GridX(), i_cell.GridY()))(i_cell.CellX(), i_cell.CellY());
    LoadHelper(cell_guids.gameobjects, cell_pair, m, i_gameObjects, i_map, grid);
    LoadHelper(i_map->GetPersistentState()->GetCellObjectGuids(cell_id).gameobjects, cell_pair, m, i_gameObjects, i_map, grid);
}

void
ObjectGridLoader::Visit(CreatureMapType& m)
{
    uint32 x = (i_cell.GridX() * MAX_NUMBER_OF_CELLS) + i_cell.CellX();
    uint32 y = (i_cell.GridY() * MAX_NUMBER_OF_CELLS) + i_cell.CellY();
    CellPair cell_pair(x, y);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    CellObjectGuids const& cell_guids = sObjectMgr.GetCellObjectGuids(i_map->GetId(), cell_id);

    GridType& grid = (*i_map->getNGrid(i_cell.GridX(), i_cell.GridY()))(i_cell.CellX(), i_cell.CellY());
    LoadHelper(cell_guids.creatures, cell_pair, m, i_creatures, i_map, grid);
    LoadHelper(i_map->GetPersistentState()->GetCellObjectGuids(cell_id).creatures, cell_pair, m, i_creatures, i_map, grid);
}

void
ObjectWorldLoader::Visit(CorpseMapType& m)
{
    uint32 x = (i_cell.GridX() * MAX_NUMBER_OF_CELLS) + i_cell.CellX();
    uint32 y = (i_cell.GridY() * MAX_NUMBER_OF_CELLS) + i_cell.CellY();
    CellPair cell_pair(x, y);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    CellObjectGuids const& cell_guids = sObjectMgr.GetCellObjectGuids(i_map->GetId(), cell_id);
    GridType& grid = (*i_map->getNGrid(i_cell.GridX(), i_cell.GridY()))(i_cell.CellX(), i_cell.CellY());
    LoadHelper(cell_guids.corpses, cell_pair, m, i_corpses, i_map, grid);
}

void
ObjectGridLoader::Load(GridType& grid)
{
    {
        TypeContainerVisitor<ObjectGridLoader, GridTypeMapContainer > loader(*this);
        grid.Visit(loader);
    }

    {
        ObjectWorldLoader wloader(*this);
        TypeContainerVisitor<ObjectWorldLoader, WorldTypeMapContainer > loader(wloader);
        grid.Visit(loader);
        i_corpses = wloader.i_corpses;
    }
}

void ObjectGridLoader::LoadCell(uint32 cellX, uint32 cellY)
{
    if (i_grid.isCellObjectDataLoaded(cellX, cellY))
    {
        return;
    }
    i_grid.setCellObjectDataLoaded(cellX, cellY, true);

    i_cell.data.Part.cell_x = cellX;
    i_cell.data.Part.cell_y = cellY;

    Load(i_grid(cellX, cellY));
}

void ObjectGridLoader::LoadN(void)
{
    i_gameObjects = 0; i_creatures = 0; i_corpses = 0;
    i_cell.data.Part.cell_y = 0;

    for (unsigned int x = 0; x < MAX_NUMBER_OF_CELLS; ++x)
    {
        for (unsigned int y = 0; y < MAX_NUMBER_OF_CELLS; ++y)
        {
            LoadCell(x, y);
        }
    }
    DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "%u GameObjects, %u Creatures, and %u Corpses/Bones loaded for grid %u on map %u", i_gameObjects, i_creatures, i_corpses, i_grid.GetGridId(), i_map->GetId());
}

void ObjectGridUnloader::MoveToRespawnN()
{
    for (unsigned int x = 0; x < MAX_NUMBER_OF_CELLS; ++x)
    {
        for (unsigned int y = 0; y < MAX_NUMBER_OF_CELLS; ++y)
        {
            ObjectGridRespawnMover mover;
            mover.Move(i_grid(x, y));
        }
    }
}

void ObjectGridUnloader::MoveToRespawnCell(uint32 cellX, uint32 cellY)
{
    ObjectGridRespawnMover mover(true);
    mover.Move(i_grid(cellX, cellY));
}

void
ObjectGridUnloader::Unload(GridType& grid)
{
    TypeContainerVisitor<ObjectGridUnloader, GridTypeMapContainer > unloader(*this);
    grid.Visit(unloader);
}

template<class T>
    void ObjectGridUnloader::Visit(GridRefManager<T>& m)
{

    for (typename GridRefManager<T>::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        iter->getSource()->CleanupsBeforeDelete();
    }

    while (!m.isEmpty())
    {
        T* obj = m.getFirst()->getSource();

        if (!sWorld.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY))
        {
            SaveRespawnTime(*obj);
        }

        obj->RemoveFromWorld();

        delete obj;
    }
}

void
ObjectGridStoper::Stop(GridType& grid)
{
    TypeContainerVisitor<ObjectGridStoper, GridTypeMapContainer > stoper(*this);
    grid.Visit(stoper);
}

void
ObjectGridStoper::Visit(CreatureMapType& m)
{

    for (CreatureMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        iter->getSource()->CombatStop();
        iter->getSource()->DeleteThreatList();
        iter->getSource()->Conjured().RemoveAllAreas();
    }
}

template void ObjectGridUnloader::Visit(GameObjectMapType&);
template void ObjectGridUnloader::Visit(DynamicObjectMapType&);
