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

#include "InstanceLedger.h"

#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "GridMap.h"
#include "LivingWorld.h"
#include "Map.h"
#include "MapFoundry.h"
#include "MapPersistentStateMgr.h"
#include "MapRoster.h"
#include "Player.h"

void InstanceLedger::PrimeMaxId()
{
    m_maxId = 0;

    QueryResult* result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `instance`");
    if (result)
    {
        m_maxId = result->Fetch()[0].GetUInt32();
        delete result;
    }
}

Map* InstanceLedger::OpenFor(Player& player, uint32 mapId)
{
    const MapEntry* entry = sMapStore.LookupEntry(mapId);
    if (!entry)
    {
        return nullptr;
    }

    // A battleground copy is cast when the match is made, long before anyone walks in, so
    // here it is only ever looked up. His stay names it; without one he has no business here.
    if (entry->IsBattleGround())
    {
        const uint32 match = player.Battle().Id();
        MANGOS_ASSERT(match);

        Map* arena = sMapRoster.Find(mapId, match);
        MANGOS_ASSERT(arena);

        return arena;
    }

    uint32 copy = 0;
    DungeonPersistentState* save = player.Binds().CopyForHimOrHisGroup(mapId);

    if (save)
    {
        copy = save->GetInstanceId();

        if (Map* open = sMapRoster.Find(mapId, copy))
        {
            return open;
        }
    }
    else
    {
        // Nothing binds him and nobody in his group has been inside: the first time this
        // dungeon exists for him is now.
        copy = MintId();
    }

    Map* dungeon = sMapFoundry.CastDungeon(mapId, copy, save);
    if (!dungeon)
    {
        return nullptr;
    }

    sMapRoster.Enrol(MapKey(mapId, copy), dungeon);

    sLivingWorld.PinActiveGrids(*dungeon);

    return dungeon;
}

Map* InstanceLedger::OpenBattleGround(uint32 mapId, BattleGround* bg)
{
    sTerrainMgr.LoadTerrain(mapId);

    const uint32 copy = MintId();

    Map* arena = sMapFoundry.CastBattleGround(mapId, copy, bg);
    if (!arena)
    {
        return nullptr;
    }

    sMapRoster.Enrol(MapKey(mapId, copy), arena);

    return arena;
}

uint32 InstanceLedger::OpenDungeons() const
{
    uint32 open = 0;

    for (auto const& filed : sMapRoster.All())
    {
        if (filed.second->IsDungeon())
        {
            ++open;
        }
    }

    return open;
}

uint32 InstanceLedger::PlayersInside() const
{
    uint32 inside = 0;

    for (auto const& filed : sMapRoster.All())
    {
        if (filed.second->IsDungeon())
        {
            inside += filed.second->GetPlayers().getSize();
        }
    }

    return inside;
}
