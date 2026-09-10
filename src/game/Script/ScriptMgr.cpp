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
#include <set>
#include "ScriptMgr.h"
#include "Policies/Singleton.h"
#include "Log.h"
#include "ProgressBar.h"
#include "ObjectMgr.h"
#include "WaypointManager.h"
#include "World.h"
#include <DBCStores.h>
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "SQLStorages.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "WaypointMovementGenerator.h"
#include "Mail.h"
#include "LFGMgr.h"

#ifdef ENABLE_SD3
#include "system/ScriptDevMgr.h"
#endif

#include <cstring>

ScriptMgr::ScriptMgr() : m_scheduledScripts(0)
{
    m_dbScripts.resize(DBS_END);

    ScriptChainMap emptyMap;

    for (int t = DBS_START; t < DBS_END; ++t)
    {
        m_dbScripts[t] = emptyMap;
    }
}

ScriptMgr::~ScriptMgr()
{
    m_dbScripts.clear();
}

ScriptLoadResult ScriptMgr::LoadScriptLibrary(const char* libName)
{
#ifdef ENABLE_SD3
    if (std::strcmp(libName, "mangosscript") == 0)
    {
        SD3::FreeScriptLibrary();
        SD3::InitScriptLibrary();
        return SCRIPT_LOAD_OK;
    }
#endif

    return SCRIPT_LOAD_ERR_NOT_FOUND;
}

void ScriptMgr::UnloadScriptLibrary()
{
#ifdef ENABLE_SD3
    SD3::FreeScriptLibrary();
#else
    return;
#endif
}

void ScriptMgr::CollectPossibleEventIds(std::set<uint32>& eventIds)
{

    for (SQLStorageBase::SQLSIterator<GameObjectInfo> itr = sGOStorage.getDataBegin<GameObjectInfo>(); itr < sGOStorage.getDataEnd<GameObjectInfo>(); ++itr)
    {
        switch (itr->type)
        {
            case GAMEOBJECT_TYPE_GOOBER:
                eventIds.insert(itr->goober.eventId);
                break;
            case GAMEOBJECT_TYPE_CHEST:
                eventIds.insert(itr->chest.eventId);
                break;
            case GAMEOBJECT_TYPE_CAMERA:
                eventIds.insert(itr->camera.eventID);
                break;
            case GAMEOBJECT_TYPE_CAPTURE_POINT:
                eventIds.insert(itr->capturePoint.neutralEventID1);
                eventIds.insert(itr->capturePoint.neutralEventID2);
                eventIds.insert(itr->capturePoint.contestedEventID1);
                eventIds.insert(itr->capturePoint.contestedEventID2);
                eventIds.insert(itr->capturePoint.progressEventID1);
                eventIds.insert(itr->capturePoint.progressEventID2);
                eventIds.insert(itr->capturePoint.winEventID1);
                eventIds.insert(itr->capturePoint.winEventID2);
                break;
            default:
                break;
        }
    }

    for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(i);
        if (spell)
        {
            for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
            {
                if (spell->Effect[j] == SPELL_EFFECT_SEND_EVENT)
                {
                    if (spell->EffectMiscValue[j])
                    {
                        eventIds.insert(spell->EffectMiscValue[j]);
                    }
                }
            }
        }
    }
}

bool StartEvents_Event(Map* map, uint32 id, Object* source, Object* target, bool isStart, Unit* forwardToPvp)
{
    MANGOS_ASSERT(source);

    if (sScriptMgr.OnProcessEvent(id, source, target, isStart))
    {
        return true;
    }

    if (forwardToPvp &&IsGameObject(source))
    {
        BattleGround* bg = nullptr;
        OutdoorPvP* opvp = nullptr;
        if (IsPlayer(forwardToPvp))
        {
            bg = ((Player*)forwardToPvp)->Battle().Ground();
            if (!bg)
            {
                opvp = sOutdoorPvPMgr.GetScript(((Player*)forwardToPvp)->GetCachedZoneId());
            }
        }
        else
        {
            if (map->IsBattleGround())
            {
                bg = ((BattleGroundMap*)map)->GetBG();
            }
            else
            {
                GameObject const* go = static_cast<GameObject*>(source);
                opvp = sOutdoorPvPMgr.GetScript(go->GetTerrain()->GetZoneId(
                           go->Where().X(), go->Where().Y(), go->Where().Z()));
            }
        }

        if (bg && bg->HandleEvent(id, static_cast<GameObject*>(source)))
        {
            return true;
        }

        if (opvp && opvp->HandleEvent(id, static_cast<GameObject*>(source)))
        {
            return true;
        }
    }

    ScriptExecutionParam execParam = SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE_TARGET;
    if (IsType(source, TYPEMASK_CREATURE_OR_GAMEOBJECT))
    {
        execParam = SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE;
    }
    else if (target && IsType(target, TYPEMASK_CREATURE_OR_GAMEOBJECT))
    {
        execParam = SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET;
    }

    return map->Scripts().Start(DBS_ON_EVENT, id, source, target, execParam);
}

uint32 GetScriptId(const char* name)
{
    return sScriptMgr.GetScriptId(name);
}

char const* GetScriptName(uint32 id)
{
    return sScriptMgr.GetScriptName(id);
}

uint32 GetScriptIdsCount()
{
    return sScriptMgr.GetScriptIdsCount();
}

void SetExternalWaypointTable(char const* tableName)
{
    sWaypointMgr.SetExternalWPTable(tableName);
}

bool AddWaypointFromExternal(uint32 entry, int32 pathId, uint32 pointId, float x, float y, float z, float o, uint32 waittime)
{
    return sWaypointMgr.AddExternalNode(entry, pathId, pointId, x, y, z, o, waittime);
}
