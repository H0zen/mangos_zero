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

#include <algorithm>
#include <set>
#include <mutex>
#include <shared_mutex>
#include "ScriptMgr.h"
#include "Log.h"
#include "ProgressBar.h"
#include "ObjectMgr.h"
#include "SQLStorages.h"
#include "Policies/Singleton.h"
#include "WaypointManager.h"
#include "World.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "WaypointMovementGenerator.h"
#include "Mail.h"
#include <DBCStores.h>
#ifdef ENABLE_SD3
#include "system/ScriptDevMgr.h"
#endif
#include "LFGMgr.h"

void ScriptMgr::LoadScriptBinding()
{
#ifdef ENABLE_SD3
    for (int i = 0; i < SCRIPTED_MAX_TYPE; ++i)
    {
        m_scriptBind[i].clear();
    }

    QueryResult* result = WorldDatabase.PQuery("SELECT `type`, `bind`, `ScriptName`, `data` FROM `script_binding`");
    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded no script binding.");
        sLog.outString();
        return;
    }

    std::set<uint32> eventIds;
    CollectPossibleEventIds(eventIds);

    BarGoLink bar(result->GetRowCount());
    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint8 type = fields[0].GetUInt8();
        int32 id = fields[1].GetInt32();
        const char* scriptName = fields[2].GetString();
        uint8 data = fields[3].GetUInt8();

        if (type >= SCRIPTED_MAX_TYPE)
        {
            sLog.outErrorScriptLib("script_binding table contains a script for non-existent type %u (bind %d), ignoring.", type, id);
            continue;
        }
        uint32 scriptId = GetScriptId(scriptName);
        if (!scriptId)
        {
            sLog.outErrorScriptLib("something is very bad with your script_binding table!");
            continue;
        }

        bool exists = false;
        switch (type)
        {
            case SCRIPTED_UNIT:
                exists = id > 0 ? bool(sCreatureStorage.LookupEntry<CreatureInfo>(uint32(id))) : bool(sObjectMgr.GetCreatureData(uint32(-id)));
                break;
            case SCRIPTED_GAMEOBJECT:
                exists = id > 0 ? bool(sGOStorage.LookupEntry<GameObjectInfo>(uint32(id))) : bool(sObjectMgr.GetGOData(uint32(-id)));
                break;
            case SCRIPTED_ITEM:
                exists = bool(sItemStorage.LookupEntry<ItemPrototype>(uint32(id)));
                break;
            case SCRIPTED_AREATRIGGER:
                exists = bool(sAreaTriggerStore.LookupEntry(uint32(id)));
                break;
            case SCRIPTED_SPELL:
            case SCRIPTED_AURASPELL:
                exists = bool(sSpellStore.LookupEntry(uint32(id)));
                break;
            case SCRIPTED_MAPEVENT:
                exists = eventIds.count(uint32(id));
                break;
            case SCRIPTED_MAP:
                exists = bool(sMapStore.LookupEntry(uint32(id)));
                break;
            case SCRIPTED_PVP_ZONE:
                exists = bool(sAreaStore.LookupEntry(uint32(id)));
                break;
            case SCRIPTED_BATTLEGROUND:
                if (MapEntry const* mapEntry = sMapStore.LookupEntry(uint32(id)))
                {
                    exists = mapEntry->IsBattleGround();
                }
                break;
            case SCRIPTED_INSTANCE:
                if (MapEntry const* mapEntry = sMapStore.LookupEntry(uint32(id)))
                {
                    exists = mapEntry->IsDungeon();
                }
                break;
            case SCRIPTED_CONDITION:
                exists = sConditionStorage.LookupEntry<PlayerCondition>(uint32(id));
                break;
            case SCRIPTED_ACHIEVEMENT:
                break;
        }

        if (!exists)
        {
            sLog.outErrorScriptLib("script type %u (%s) is bound to non-existing entry %d, ignoring.", type, scriptName, id);
            continue;
        }

        if (type == SCRIPTED_SPELL || type == SCRIPTED_AURASPELL)
        {
            id |= uint32(data) << 24;
        }

        m_scriptBind[type][id] = scriptId;
    }
    while (result->NextRow());

    delete result;
    sLog.outString("Of the total %u script bindings, loaded succesfully:", count);
    for (uint8 i = 0; i < SCRIPTED_MAX_TYPE; ++i)
    {
        if (m_scriptBind[i].size())
        {
            sLog.outString(".. type %u: %u binds", i, uint32(m_scriptBind[i].size()));
            count -= m_scriptBind[i].size();
        }
    }
    sLog.outString("Thus, %u script binds are found bad.", count);

    sLog.outString();
#endif
    return;
}

bool ScriptMgr::ReloadScriptBinding()
{
#ifdef _DEBUG
    std::unique_lock<std::shared_mutex> guard(m_bindMutex);
    LoadScriptBinding();
    return true;
#else
    return false;
#endif
}

void ScriptMgr::LoadScriptNames()
{
    m_scriptNames.push_back("");
    QueryResult* result = WorldDatabase.Query("SELECT DISTINCT(`ScriptName`) FROM `script_binding`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded empty set of Script Names!");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());
    uint32 count = 0;

    do
    {
        bar.step();
        m_scriptNames.push_back((*result)[0].GetString());
        ++count;
    }
    while (result->NextRow());
    delete result;

    std::sort(m_scriptNames.begin(), m_scriptNames.end());

    sLog.outString(">> Loaded %d unique Script Names", count);
    sLog.outString();
}

uint32 ScriptMgr::GetScriptId(const char* name) const
{

    if (!name)
    {
        return 0;
    }

    ScriptNameMap::const_iterator itr =
        std::lower_bound(m_scriptNames.begin(), m_scriptNames.end(), name);

    if (itr == m_scriptNames.end() || *itr != name)
    {
        return 0;
    }

    return uint32(itr - m_scriptNames.begin());
}

uint32 ScriptMgr::GetBoundScriptId(ScriptedObjectType entity, int32 entry)
{
#ifdef _DEBUG
    std::shared_lock<std::shared_mutex> guard(m_bindMutex);
#endif
    uint32 id = 0;
    if (entity < SCRIPTED_MAX_TYPE)
    {
        EntryToScriptIdMap::iterator it = m_scriptBind[entity].find(entry);
        if (it != m_scriptBind[entity].end())
        {
            id = it->second;
        }
    }
    else
    {
        sLog.outErrorScriptLib("asking a script for non-existing entity type %u!", entity);
    }
    return id;
}

char const* ScriptMgr::GetScriptLibraryVersion() const
{
#ifdef ENABLE_SD3
    return SD3::GetScriptLibraryVersion();
#else
    return nullptr;
#endif
}
