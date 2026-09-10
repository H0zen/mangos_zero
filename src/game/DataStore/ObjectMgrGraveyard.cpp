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

#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "DBCStores.h"
#include "Log.h"
#include "ProgressBar.h"
#include "LivingWorldAnchorPolicy.h"
#include "Movement/Generators/MotionMaster.h"
#include "SQLStorages.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "World.h"
#include "Group.h"
#include "Transports.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Chat.h"
#include "MapPersistentStateMgr.h"
#include "SpellAuras.h"
#include "Util.h"
#include "GossipDef.h"
#include "Mail.h"
#include "Formulas.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "DisableMgr.h"
#include "ItemEnchantmentMgr.h"

void ObjectMgr::LoadGraveyardZones()
{
    mGraveYardMap.clear();

    QueryResult* result = WorldDatabase.Query("SELECT `id`,`ghost_zone`,`faction` FROM `game_graveyard_zone`");

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u graveyard-zone links", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 safeLocId = fields[0].GetUInt32();
        uint32 zoneId = fields[1].GetUInt32();
        uint32 team   = fields[2].GetUInt32();

        WorldSafeLocsEntry const* entry = sWorldSafeLocsStore.LookupEntry(safeLocId);
        if (!entry)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record for not existing graveyard (WorldSafeLocs.dbc id) %u, skipped.", safeLocId);
            continue;
        }

        AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(zoneId);
        if (!areaEntry)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record for not existing zone id (%u), skipped.", zoneId);
            continue;
        }

        if (areaEntry->ParentAreaID != 0)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record subzone id (%u) instead of zone, skipped.", zoneId);
            continue;
        }

        if (team != TEAM_BOTH_ALLOWED && team != HORDE && team != ALLIANCE)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record for non player faction (%u), skipped.", team);
            continue;
        }

        if (!AddGraveYardLink(safeLocId, zoneId, Team(team), false))
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has a duplicate record for Graveyard (ID: %u) and Zone (ID: %u), skipped.", safeLocId, zoneId);
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u graveyard-zone links", count);
    sLog.outString();
}

WorldSafeLocsEntry const* ObjectMgr::GetClosestGraveYard(float x, float y, float z, uint32 MapId, Team team)
{

    uint32 zoneId = sTerrainMgr.GetZoneId(MapId, x, y, z);

    GraveYardMapBounds bounds = mGraveYardMap.equal_range(zoneId);

    if (bounds.first == bounds.second)
    {
        sLog.outErrorDb("Table `game_graveyard_zone` incomplete: Zone %u Team %u does not have a linked graveyard.", zoneId, uint32(team));
        return nullptr;
    }

    bool foundNear = false;
    float distNear;
    WorldSafeLocsEntry const* entryNear = nullptr;

    bool foundEntr = false;
    float distEntr;
    WorldSafeLocsEntry const* entryEntr = nullptr;

    WorldSafeLocsEntry const* entryFar = nullptr;

    InstanceTemplate const* tempEntry = GetInstanceTemplate(MapId);

    for (GraveYardMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        GraveYardData const& data = itr->second;

        WorldSafeLocsEntry const* entry = sWorldSafeLocsStore.LookupEntry(data.safeLocId);

        if (data.team != TEAM_BOTH_ALLOWED && data.team != team && team != TEAM_BOTH_ALLOWED)
        {
            continue;
        }

        if (MapId != entry->map_id)
        {

            if (!tempEntry ||
                tempEntry->ghostEntranceMap < 0 ||
                uint32(tempEntry->ghostEntranceMap) != entry->map_id ||
                (tempEntry->ghostEntranceX == 0.0f && tempEntry->ghostEntranceY == 0.0f))
            {

                entryFar = entry;
                continue;
            }

            float dist2 = (entry->x - tempEntry->ghostEntranceX) * (entry->x - tempEntry->ghostEntranceX) +
                (entry->y - tempEntry->ghostEntranceY) * (entry->y - tempEntry->ghostEntranceY);
            if (foundEntr)
            {
                if (dist2 < distEntr)
                {
                    distEntr = dist2;
                    entryEntr = entry;
                }
            }
            else
            {
                foundEntr = true;
                distEntr = dist2;
                entryEntr = entry;
            }
        }

        else
        {
            float dist2 = (entry->x - x) * (entry->x - x) + (entry->y - y) * (entry->y - y) + (entry->z - z) * (entry->z - z);
            if (foundNear)
            {
                if (dist2 < distNear)
                {
                    distNear = dist2;
                    entryNear = entry;
                }
            }
            else
            {
                foundNear = true;
                distNear = dist2;
                entryNear = entry;
            }
        }
    }

    if (entryNear)
    {
        return entryNear;
    }

    if (entryEntr)
    {
        return entryEntr;
    }

    return entryFar;
}

GraveYardData const* ObjectMgr::FindGraveYardData(uint32 id, uint32 zoneId) const
{
    GraveYardMapBounds bounds = mGraveYardMap.equal_range(zoneId);

    for (GraveYardMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second.safeLocId == id)
        {
            return &itr->second;
        }
    }

    return nullptr;
}

bool ObjectMgr::AddGraveYardLink(uint32 id, uint32 zoneId, Team team, bool inDB)
{
    if (FindGraveYardData(id, zoneId))
    {
        return false;
    }

    GraveYardData data;
    data.safeLocId = id;
    data.team = team;

    mGraveYardMap.insert(GraveYardMap::value_type(zoneId, data));

    if (inDB)
    {
        WorldDatabase.PExecuteLog("INSERT INTO `game_graveyard_zone` (`id`,`ghost_zone`,`faction`) VALUES ('%u', '%u','%u')", id, zoneId, uint32(team));
    }

    return true;
}

void ObjectMgr::SetGraveYardLinkTeam(uint32 id, uint32 zoneId, Team team)
{
    std::pair<GraveYardMap::iterator, GraveYardMap::iterator> bounds = mGraveYardMap.equal_range(zoneId);

    for (GraveYardMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        GraveYardData& data = itr->second;

        if (data.safeLocId != id)
        {
            continue;
        }

        data.team = team;
        return;
    }

    if (team == TEAM_INVALID)
    {
        return;
    }

    sLog.outErrorDb("ObjectMgr::SetGraveYardLinkTeam called for safeLoc %u, zoneId %u, but no graveyard link for this found in database.", id, zoneId);
    AddGraveYardLink(id, zoneId, team);
}
