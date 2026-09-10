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
#include "CorpseManager.h"

#include "CellImpl.h"
#include "Corpse.h"
#include "GridNotifiers.h"
#include "Log.h"
#include "Map.h"
#include "MapRoster.h"
#include "ObjectMgr.h"
#include "World.h"

#include <ctime>
#include <forward_list>

CorpseManager::~CorpseManager()
{

    m_byOwner.WithExclusive([](MaNGOS::ConcurrentRegistry<ObjectGuid, Corpse>::MapType& map)
    {
        for (auto& itr : map)
        {
            itr.second->RemoveFromWorld();
            delete itr.second;
        }
        map.clear();
    });
}

Corpse* CorpseManager::Find(ObjectGuid corpseGuid) const
{
    return m_corpses.Find(corpseGuid);
}

Corpse* CorpseManager::FindInMap(ObjectGuid corpseGuid, uint32 mapId) const
{
    Corpse* corpse = m_corpses.Find(corpseGuid);
    if (!corpse || corpse->GetMapId() != mapId)
    {
        return nullptr;
    }
    return corpse;
}

Corpse* CorpseManager::FindForPlayer(ObjectGuid playerGuid) const
{
    Corpse* corpse = m_byOwner.Find(playerGuid);
    if (!corpse)
    {
        return nullptr;
    }

    MANGOS_ASSERT(corpse->GetType() != CORPSE_BONES);
    return corpse;
}

void CorpseManager::AddObject(Corpse* corpse)
{
    m_corpses.Insert(corpse->GetObjectGuid(), corpse);
}

void CorpseManager::RemoveObject(Corpse* corpse)
{
    m_corpses.Remove(corpse->GetObjectGuid());
}

void CorpseManager::RecordCell(Corpse* corpse)
{
    const CellPair cellPair =
        MaNGOS::ComputeCellPair(corpse->Where().X(), corpse->Where().Y());
    const uint32 cellId =
        (cellPair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cellPair.x_coord;

    sObjectMgr.AddCorpseCellData(corpse->GetMapId(), cellId,
                                 GuidCounter(corpse->GetOwnerGuid()),
                                 corpse->GetInstanceId());
}

void CorpseManager::ForgetCell(Corpse* corpse)
{
    const CellPair cellPair =
        MaNGOS::ComputeCellPair(corpse->Where().X(), corpse->Where().Y());
    const uint32 cellId =
        (cellPair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cellPair.x_coord;

    sObjectMgr.DeleteCorpseCellData(corpse->GetMapId(), cellId,
                                    GuidCounter(corpse->GetObjectGuid()));
}

void CorpseManager::Add(Corpse* corpse)
{
    MANGOS_ASSERT(corpse && corpse->GetType() != CORPSE_BONES);

    m_byOwner.Insert(corpse->GetOwnerGuid(), corpse);
    RecordCell(corpse);
}

void CorpseManager::Remove(Corpse* corpse)
{
    MANGOS_ASSERT(corpse && corpse->GetType() != CORPSE_BONES);

    if (!m_byOwner.Find(corpse->GetOwnerGuid()))
    {
        return;
    }

    ForgetCell(corpse);
    corpse->RemoveFromWorld();
    m_byOwner.Remove(corpse->GetOwnerGuid());
}

void CorpseManager::AddCorpsesToGrid(GridPair const& gridpair, GridType& grid, Map* map)
{
    m_byOwner.ForEach([&gridpair, &grid, map](Corpse* corpse)
    {
        if (corpse->GetGrid() != gridpair)
        {
            return;
        }

        if (map->Instanceable())
        {
            if (corpse->GetInstanceId() == map->GetInstanceId())
            {
                grid.AddWorldObject(corpse);
            }
        }
        else
        {
            grid.AddWorldObject(corpse);
        }
    });
}

Corpse* CorpseManager::ConvertCorpseForPlayer(ObjectGuid playerGuid, bool insignia)
{
    Corpse* corpse = FindForPlayer(playerGuid);
    if (!corpse)
    {

        return nullptr;
    }

    DEBUG_LOG("Deleting Corpse and spawning bones.");

    Remove(corpse);

    Map* map = sMapRoster.Find(corpse->GetMapId(), corpse->GetInstanceId());
    if (map)
    {
        map->Remove(corpse, false);
    }

    corpse->DeleteFromDB();

    Corpse* bones = nullptr;

    const bool bonesAllowed = insignia
        || (map && map->IsBattleGround()
                ? sWorld.getConfig(CONFIG_BOOL_DEATH_BONES_BG)
                : sWorld.getConfig(CONFIG_BOOL_DEATH_BONES_WORLD));

    if (map && bonesAllowed
        && !map->IsRemovalGrid(corpse->Where().X(), corpse->Where().Y()))
    {
        bones = new Corpse;
        bones->Create(corpse->GetGUIDLow());

        for (int i = 3; i < CORPSE_END; ++i)
        {
            bones->SetUInt32Value(i, corpse->GetUInt32Value(i));
        }

        bones->SetGrid(corpse->GetGrid());
        bones->Place().MoveTo(corpse->Where().X(), corpse->Where().Y(), corpse->Where().Z(), corpse->Where().Facing());

        bones->SetUInt32Value(CORPSE_FIELD_FLAGS, CORPSE_FLAG_UNK2 | CORPSE_FLAG_BONES);
        bones->SetGuidValue(CORPSE_FIELD_OWNER, 0);

        for (int i = 0; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (corpse->GetUInt32Value(CORPSE_FIELD_ITEM + i))
            {
                bones->SetUInt32Value(CORPSE_FIELD_ITEM + i, 0);
            }
        }

        map->Add(bones);
    }

    delete corpse;

    return bones;
}

void CorpseManager::RemoveOldCorpses()
{
    const time_t now = time(nullptr);

    std::forward_list<ObjectGuid> expired;

    m_byOwner.ForEach([&now, &expired](Corpse* corpse)
    {
        if (corpse->IsExpired(now))
        {
            expired.emplace_front(corpse->GetOwnerGuid());
        }
    });

    for (ObjectGuid owner : expired)
    {
        ConvertCorpseForPlayer(owner);
    }
}
