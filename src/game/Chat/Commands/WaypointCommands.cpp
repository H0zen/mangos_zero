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

#include <string>
#include <list>
#include "Chat.h"
#include "SpawnRecord.h"
#include "Language.h"
#include "PointMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "TemporarySummon.h"
#include "MoveMap.h"
#include "PathFinder.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Movement/Spline/MoveSplineInit.h"
#include <fstream>
#include <map>
#include <typeinfo>

inline Creature* Helper_CreateWaypointFor(Creature* wpOwner, WaypointPathOrigin wpOrigin, int32 pathId, uint32 wpId, WaypointNode const* wpNode, CreatureInfo const* waypointInfo)
{
    TemporarySummonWaypoint* wpCreature = new TemporarySummonWaypoint(wpOwner->GetObjectGuid(), wpId, pathId, (uint32)wpOrigin);

    CreatureCreatePos pos(wpOwner->GetMap(), wpNode->x, wpNode->y, wpNode->z, wpNode->orientation);

    if (!wpCreature->Create(wpOwner->GetMap()->GenerateLocalLowGuid(HIGHGUID_UNIT), pos, waypointInfo))
    {
        delete wpCreature;
        return nullptr;
    }

    wpCreature->SetVisibility(VISIBILITY_OFF);
    wpCreature->SetSpawn(pos);

    wpCreature->SetActiveObjectState(true);

    wpCreature->Summon(TEMPSPAWN_TIMED_DESPAWN, 5 * MINUTE * IN_MILLISECONDS);
    return wpCreature;
}

inline void UnsummonVisualWaypoints(Player const* player, ObjectGuid ownerGuid)
{
    std::list<Creature*> waypoints;
    MaNGOS::AllCreaturesOfEntryInRangeCheck checkerForWaypoint(player, VISUAL_WAYPOINT, SIZE_OF_GRIDS);
    MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(waypoints, checkerForWaypoint);
    Cell::VisitGridObjects(player, searcher, SIZE_OF_GRIDS);

    for (std::list<Creature*>::iterator itr = waypoints.begin(); itr != waypoints.end(); ++itr)
    {
        if ((*itr)->GetSubtype() != CREATURE_SUBTYPE_TEMPORARY_SUMMON)
        {
            continue;
        }

        TemporarySummonWaypoint* wpTarget = dynamic_cast<TemporarySummonWaypoint*>(*itr);
        if (!wpTarget)
        {
            continue;
        }

        if (wpTarget->GetSummonerGuid() == ownerGuid)
        {
            wpTarget->UnSummon();
        }
    }
}

bool ChatHandler::HandleWpAddCommand(char* args)
{
    DEBUG_LOG("DEBUG: HandleWpAddCommand");

    CreatureInfo const* waypointInfo = ObjectMgr::GetCreatureTemplate(VISUAL_WAYPOINT);
    if (!waypointInfo || waypointInfo->GetHighGuid() != HIGHGUID_UNIT)
    {
        return false;
    }

    Creature* targetCreature = getSelectedCreature();
    WaypointPathOrigin wpDestination = PATH_NO_PATH;
    int32 wpPathId = 0;
    uint32 wpPointId = 0;
    Creature* wpOwner;

    if (targetCreature)
    {

        if (targetCreature->GetEntry() == VISUAL_WAYPOINT && targetCreature->GetSubtype() == CREATURE_SUBTYPE_TEMPORARY_SUMMON)
        {
            TemporarySummonWaypoint* wpTarget = dynamic_cast<TemporarySummonWaypoint*>(targetCreature);
            if (!wpTarget)
            {
                PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
                SetSentErrorMessage(true);
                return false;
            }

            wpOwner = targetCreature->GetMap()->GetAnyTypeCreature(wpTarget->GetSummonerGuid());
            if (!wpOwner)
            {
                PSendSysMessage(LANG_WAYPOINT_NOTFOUND_NPC, GuidString(wpTarget->GetSummonerGuid()).c_str());
                SetSentErrorMessage(true);
                return false;
            }
            wpDestination = (WaypointPathOrigin)wpTarget->GetPathOrigin();
            wpPathId = wpTarget->GetPathId();
            wpPointId = wpTarget->GetWaypointId() + 1;
        }
        else
        {
            wpOwner = targetCreature;
        }
    }
    else
    {
        uint32 dbGuid;
        if (!ExtractUInt32(&args, dbGuid))
        {
            PSendSysMessage(LANG_WAYPOINT_NOGUID);
            SetSentErrorMessage(true);
            return false;
        }

        CreatureData const* data = sObjectMgr.GetCreatureData(dbGuid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        if (m_session->GetPlayer()->GetMapId() != data->mapid)
        {
            PSendSysMessage(LANG_COMMAND_CREATUREATSAMEMAP, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        wpOwner = m_session->GetPlayer()->GetMap()->GetAnyTypeCreature(data->GetObjectGuid(dbGuid));
        if (!wpOwner)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }
    }

    if (wpDestination == PATH_NO_PATH)
    {
        if (ExtractOptInt32(&args, wpPathId, 0))
        {
            uint32 src = (uint32)PATH_NO_PATH;
            if (ExtractOptUInt32(&args, src, src))
            {
                wpDestination = (WaypointPathOrigin)src;
            }
            else
            {
                if (wpPathId != 0)
                {
                    wpDestination = PATH_FROM_ENTRY;
                }
            }
        }

        if (wpDestination == PATH_NO_PATH)
        {
            if (wpOwner->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
            {
                if (WaypointMovementGenerator const* wpMMGen = dynamic_cast<WaypointMovementGenerator const*>(wpOwner->GetMotionMaster()->GetCurrent()))
                {
                    wpMMGen->GetPathInformation(wpPathId, wpDestination);
                }
            }

            if (wpDestination == PATH_NO_PATH && !sWaypointMgr.GetDefaultPath(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), &wpDestination))
            {
                wpDestination = PATH_FROM_ENTRY;
                if (npcs::Listed(*wpOwner))
                {
                    QueryResult* result = WorldDatabase.PQuery("SELECT COUNT(`id`) FROM `creature` WHERE `id` = %u", wpOwner->GetEntry());
                    if (result && result->Fetch()[0].GetUInt32() != 1)
                    {
                        wpDestination = PATH_FROM_GUID;
                    }
                    delete result;
                }
            }
        }
    }

    float x, y, z;
    x = m_session->GetPlayer()->Where().X();
    y = m_session->GetPlayer()->Where().Y();
    z = m_session->GetPlayer()->Where().Z();
    if (!sWaypointMgr.AddNode(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPointId, wpDestination, x, y, z))
    {
        PSendSysMessage(LANG_WAYPOINT_NOTCREATED, wpPointId, wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpDestination).c_str());
        SetSentErrorMessage(true);
        return false;
    }

    UnsummonVisualWaypoints(m_session->GetPlayer(), wpOwner->GetObjectGuid());
    WaypointPath const* wpPath = sWaypointMgr.GetPathFromOrigin(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPathId, wpDestination);
    for (WaypointPath::const_iterator itr = wpPath->begin(); itr != wpPath->end(); ++itr)
    {
        if (!Helper_CreateWaypointFor(wpOwner, wpDestination, wpPathId, itr->first, &itr->second, waypointInfo))
        {
            PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
            SetSentErrorMessage(true);
            return false;
        }
    }

    PSendSysMessage(LANG_WAYPOINT_ADDED, wpPointId, wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpDestination).c_str());

    return true;
}

bool ChatHandler::HandleWpModifyCommand(char* args)
{
    DEBUG_LOG("DEBUG: HandleWpModifyCommand");

    if (!*args)
    {
        return false;
    }

    CreatureInfo const* waypointInfo = ObjectMgr::GetCreatureTemplate(VISUAL_WAYPOINT);
    if (!waypointInfo || waypointInfo->GetHighGuid() != HIGHGUID_UNIT)
    {
        return false;
    }

    char* subCmd_str = ExtractLiteralArg(&args);
    if (!subCmd_str)
    {
        return false;
    }

    std::string subCmd = subCmd_str;

    if ((subCmd != "waittime") && (subCmd != "scriptid") && (subCmd != "orientation") && (subCmd != "del") && (subCmd != "move"))
    {
        return false;
    }

    Creature* targetCreature = getSelectedCreature();
    Creature* wpOwner;
    uint32 wpId = 0;
    WaypointPathOrigin wpSource = PATH_NO_PATH;
    int32 wpPathId = 0;

    if (targetCreature)
    {
        DEBUG_LOG("DEBUG: HandleWpModifyCommand - User did select an NPC");

        if (targetCreature->GetEntry() != VISUAL_WAYPOINT || targetCreature->GetSubtype() != CREATURE_SUBTYPE_TEMPORARY_SUMMON)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
            SetSentErrorMessage(true);
            return false;
        }
        TemporarySummonWaypoint* wpTarget = dynamic_cast<TemporarySummonWaypoint*>(targetCreature);
        if (!wpTarget)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
            SetSentErrorMessage(true);
            return false;
        }

        wpOwner = targetCreature->GetMap()->GetAnyTypeCreature(wpTarget->GetSummonerGuid());
        if (!wpOwner)
        {
            PSendSysMessage(LANG_WAYPOINT_NOTFOUND_NPC, GuidString(wpTarget->GetSummonerGuid()).c_str());
            SetSentErrorMessage(true);
            return false;
        }
        wpId = wpTarget->GetWaypointId();

        wpPathId = wpTarget->GetPathId();
        wpSource = (WaypointPathOrigin)wpTarget->GetPathOrigin();
    }
    else
    {
        uint32 dbGuid = 0;

        if (!ExtractUInt32(&args, dbGuid))
        {
            SendSysMessage(LANG_WAYPOINT_NOGUID);
            SetSentErrorMessage(true);
            return false;
        }

        if (!ExtractUInt32(&args, wpId))
        {
            SendSysMessage(LANG_WAYPOINT_NOWAYPOINTGIVEN);
            SetSentErrorMessage(true);
            return false;
        }

        CreatureData const* data = sObjectMgr.GetCreatureData(dbGuid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        wpOwner = m_session->GetPlayer()->GetMap()->GetAnyTypeCreature(data->GetObjectGuid(dbGuid));
        if (!wpOwner)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }
    }

    if (wpSource == PATH_NO_PATH)
    {
        if (wpOwner->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            if (WaypointMovementGenerator const* wpMMGen = dynamic_cast<WaypointMovementGenerator const*>(wpOwner->GetMotionMaster()->GetCurrent()))
            {
                wpMMGen->GetPathInformation(wpPathId, wpSource);
            }
        }
        if (wpSource == PATH_NO_PATH)
        {
            sWaypointMgr.GetDefaultPath(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), &wpSource);
        }
    }

    WaypointPath const* wpPath = sWaypointMgr.GetPathFromOrigin(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPathId, wpSource);
    if (!wpPath)
    {
        PSendSysMessage(LANG_WAYPOINT_NOTFOUNDPATH, wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpSource).c_str());
        SetSentErrorMessage(true);
        return false;
    }

    WaypointPath::const_iterator point = wpPath->find(wpId);
    if (point == wpPath->end())
    {
        PSendSysMessage(LANG_WAYPOINT_NOTFOUND, wpId, wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpSource).c_str());
        SetSentErrorMessage(true);
        return false;
    }

    if (!targetCreature && subCmd != "del")
    {
        targetCreature = Helper_CreateWaypointFor(wpOwner, wpSource, wpPathId, wpId, &(point->second), waypointInfo);
        if (!targetCreature)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
            SetSentErrorMessage(true);
            return false;
        }
    }

    if (subCmd == "del")
    {
        sWaypointMgr.DeleteNode(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpId, wpPathId, wpSource);

        if (TemporarySummonWaypoint* wpCreature = dynamic_cast<TemporarySummonWaypoint*>(targetCreature))
        {
            wpCreature->UnSummon();
        }

        if (wpPath->empty())
        {
            wpOwner->SetDefaultMovementType(RANDOM_MOTION_TYPE);
            wpOwner->GetMotionMaster()->Initialize();
            if (wpOwner->IsAlive())
            {
                wpOwner->SetDeathState(JUST_DIED);
                wpOwner->Respawn();
            }
            npcs::Save(*wpOwner);
        }

        PSendSysMessage(LANG_WAYPOINT_REMOVED);
        return true;
    }
    else if (subCmd == "move")
    {
        float x, y, z;
        x = m_session->GetPlayer()->Where().X();
        y = m_session->GetPlayer()->Where().Y();
        z = m_session->GetPlayer()->Where().Z();

        targetCreature->NearTeleportTo(x, y, z, targetCreature->Where().Facing());

        sWaypointMgr.SetNodePosition(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpId, wpPathId, wpSource, x, y, z);

        PSendSysMessage(LANG_WAYPOINT_CHANGED);
        return true;
    }
    else if (subCmd == "waittime")
    {
        uint32 waittime;
        if (!ExtractUInt32(&args, waittime))
        {
            return false;
        }

        sWaypointMgr.SetNodeWaittime(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpId, wpPathId, wpSource, waittime);
    }
    else if (subCmd == "scriptid")
    {
        uint32 scriptId;
        if (!ExtractUInt32(&args, scriptId))
        {
            return false;
        }

        if (!sWaypointMgr.SetNodeScriptId(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpId, wpPathId, wpSource, scriptId))
        {
            PSendSysMessage(LANG_WAYPOINT_INFO_UNK_SCRIPTID, scriptId);
        }
    }
    else if (subCmd == "orientation")
    {
        float ori;
        if (!ExtractFloat(&args, ori))
        {
            return false;
        }

        sWaypointMgr.SetNodeOrientation(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpId, wpPathId, wpSource, ori);
    }

    PSendSysMessage(LANG_WAYPOINT_CHANGED_NO, subCmd_str);
    return true;
}

bool ChatHandler::HandleWpShowCommand(char* args)
{
    DEBUG_LOG("DEBUG: HandleWpShowCommand");

    if (!*args)
    {
        return false;
    }

    CreatureInfo const* waypointInfo = ObjectMgr::GetCreatureTemplate(VISUAL_WAYPOINT);
    if (!waypointInfo || waypointInfo->GetHighGuid() != HIGHGUID_UNIT)
    {
        return false;
    }

    char* subCmd_str = ExtractLiteralArg(&args);
    if (!subCmd_str)
    {
        return false;
    }
    std::string subCmd = subCmd_str;

    uint32 dbGuid = 0;
    int32 wpPathId = 0;
    WaypointPathOrigin wpOrigin = PATH_NO_PATH;

    Creature* targetCreature = getSelectedCreature();
    if (targetCreature)
    {
        if (ExtractOptInt32(&args, wpPathId, 0))
        {
            uint32 src;
            if (ExtractOptUInt32(&args, src, (uint32)PATH_NO_PATH))
            {
                wpOrigin = (WaypointPathOrigin)src;
            }
        }
    }
    else
    {
        if (!ExtractUInt32(&args, dbGuid))
        {
            return false;
        }

        if (ExtractOptInt32(&args, wpPathId, 0))
        {
            uint32 src = (uint32)PATH_NO_PATH;
            if (ExtractOptUInt32(&args, src, src))
            {
                wpOrigin = (WaypointPathOrigin)src;
            }
        }

        CreatureData const* data = sObjectMgr.GetCreatureData(dbGuid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        targetCreature = m_session->GetPlayer()->GetMap()->GetCreature(data->GetObjectGuid(dbGuid));
        if (!targetCreature)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }
    }

    Creature* wpOwner;
    TemporarySummonWaypoint* wpTarget = nullptr;

    if (subCmd == "info")
    {

        if (targetCreature->GetEntry() != VISUAL_WAYPOINT || targetCreature->GetSubtype() != CREATURE_SUBTYPE_TEMPORARY_SUMMON)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
            SetSentErrorMessage(true);
            return false;
        }
        wpTarget = dynamic_cast<TemporarySummonWaypoint*>(targetCreature);
        if (!wpTarget)
        {
            PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
            SetSentErrorMessage(true);
            return false;
        }

        wpOwner = targetCreature->GetMap()->GetAnyTypeCreature(wpTarget->GetSummonerGuid());
        if (!wpOwner)
        {
            PSendSysMessage(LANG_WAYPOINT_NOTFOUND_NPC, GuidString(wpTarget->GetSummonerGuid()).c_str());
            SetSentErrorMessage(true);
            return false;
        }

        wpOrigin = (WaypointPathOrigin)wpTarget->GetPathOrigin();
        wpPathId = wpTarget->GetPathId();
    }
    else
    {
        wpOwner = targetCreature;
    }

    WaypointPath* wpPath = nullptr;
    if (wpOrigin != PATH_NO_PATH)
    {
        wpPath = sWaypointMgr.GetPathFromOrigin(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPathId, wpOrigin);
    }
    else
    {
        if (wpOwner->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            if (WaypointMovementGenerator const* wpMMGen = dynamic_cast<WaypointMovementGenerator const*>(wpOwner->GetMotionMaster()->GetCurrent()))
            {
                wpMMGen->GetPathInformation(wpPathId, wpOrigin);
                wpPath = sWaypointMgr.GetPathFromOrigin(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPathId, wpOrigin);
            }
        }
        if (wpOrigin == PATH_NO_PATH)
        {
            wpPath = sWaypointMgr.GetDefaultPath(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), &wpOrigin);
        }
    }

    if (!wpPath || wpPath->empty())
    {
        PSendSysMessage(LANG_WAYPOINT_NOTFOUNDPATH, wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpOrigin).c_str());
        SetSentErrorMessage(true);
        return false;
    }

    if (subCmd == "info")
    {

        WaypointPath::const_iterator point = wpPath->find(wpTarget->GetWaypointId());
        if (point == wpPath->end())
        {
            PSendSysMessage(LANG_WAYPOINT_NOTFOUND, wpTarget->GetWaypointId(), wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpOrigin).c_str());
            SetSentErrorMessage(true);
            return false;
        }

        PSendSysMessage(LANG_WAYPOINT_INFO_TITLE, wpTarget->GetWaypointId(), wpOwner->GetGuidStr().c_str(), wpPathId, WaypointManager::GetOriginString(wpOrigin).c_str());
        PSendSysMessage(LANG_WAYPOINT_INFO_WAITTIME, point->second.delay);
        PSendSysMessage(LANG_WAYPOINT_INFO_ORI, point->second.orientation);
        PSendSysMessage(LANG_WAYPOINT_INFO_SCRIPTID, point->second.script_id);
        if (wpOrigin == PATH_FROM_EXTERNAL)
        {
            PSendSysMessage(LANG_WAYPOINT_INFO_AISCRIPT, wpOwner->GetScriptName().c_str());
        }
        if (WaypointBehavior* behaviour = point->second.behavior)
        {
            PSendSysMessage(" ModelId1: %u", behaviour->model1);
            PSendSysMessage(" ModelId2: %u", behaviour->model2);
            PSendSysMessage(" Emote: %u", behaviour->emote);
            PSendSysMessage(" Spell: %u", behaviour->spell);
            for (int i = 0; i < MAX_WAYPOINT_TEXT; ++i)
            {
                PSendSysMessage(" TextId%i: %i \'%s\'", i + 1, behaviour->textid[i], (behaviour->textid[i] ? GetMangosString(behaviour->textid[i]) : ""));
            }
        }

        return true;
    }

    if (subCmd == "on")
    {
        UnsummonVisualWaypoints(m_session->GetPlayer(), wpOwner->GetObjectGuid());

        for (WaypointPath::const_iterator pItr = wpPath->begin(); pItr != wpPath->end(); ++pItr)
        {
            if (!Helper_CreateWaypointFor(wpOwner, wpOrigin, wpPathId, pItr->first, &(pItr->second), waypointInfo))
            {
                PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
                SetSentErrorMessage(true);
                return false;
            }
        }

        return true;
    }

    if (subCmd == "first")
    {
        if (!Helper_CreateWaypointFor(wpOwner, wpOrigin, wpPathId, wpPath->begin()->first, &(wpPath->begin()->second), waypointInfo))
        {
            PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
            SetSentErrorMessage(true);
            return false;
        }

        return true;
    }

    if (subCmd == "last")
    {
        if (!Helper_CreateWaypointFor(wpOwner, wpOrigin, wpPathId, wpPath->rbegin()->first, &(wpPath->rbegin()->second), waypointInfo))
        {
            PSendSysMessage(LANG_WAYPOINT_VP_NOTCREATED, VISUAL_WAYPOINT);
            SetSentErrorMessage(true);
            return false;
        }

        return true;
    }

    if (subCmd == "off")
    {
        UnsummonVisualWaypoints(m_session->GetPlayer(), wpOwner->GetObjectGuid());
        PSendSysMessage(LANG_WAYPOINT_VP_ALLREMOVED);
        return true;
    }

    return false;
}

bool ChatHandler::HandleWpExportCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Creature* wpOwner;
    WaypointPathOrigin wpOrigin = PATH_NO_PATH;
    int32 wpPathId = 0;

    if (Creature* targetCreature = getSelectedCreature())
    {

        if (targetCreature->GetEntry() == VISUAL_WAYPOINT && targetCreature->GetSubtype() == CREATURE_SUBTYPE_TEMPORARY_SUMMON)
        {
            TemporarySummonWaypoint* wpTarget = dynamic_cast<TemporarySummonWaypoint*>(targetCreature);
            if (!wpTarget)
            {
                PSendSysMessage(LANG_WAYPOINT_VP_SELECT);
                SetSentErrorMessage(true);
                return false;
            }

            wpOwner = targetCreature->GetMap()->GetAnyTypeCreature(wpTarget->GetSummonerGuid());
            if (!wpOwner)
            {
                PSendSysMessage(LANG_WAYPOINT_NOTFOUND_NPC, GuidString(wpTarget->GetSummonerGuid()).c_str());
                SetSentErrorMessage(true);
                return false;
            }
            wpOrigin = (WaypointPathOrigin)wpTarget->GetPathOrigin();
            wpPathId = wpTarget->GetPathId();
        }
        else
        {
            wpOwner = targetCreature;
        }
    }
    else
    {
        uint32 dbGuid;
        if (!ExtractUInt32(&args, dbGuid))
        {
            PSendSysMessage(LANG_WAYPOINT_NOGUID);
            SetSentErrorMessage(true);
            return false;
        }

        CreatureData const* data = sObjectMgr.GetCreatureData(dbGuid);
        if (!data)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        if (m_session->GetPlayer()->GetMapId() != data->mapid)
        {
            PSendSysMessage(LANG_COMMAND_CREATUREATSAMEMAP, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }

        wpOwner = m_session->GetPlayer()->GetMap()->GetAnyTypeCreature(data->GetObjectGuid(dbGuid));
        if (!wpOwner)
        {
            PSendSysMessage(LANG_WAYPOINT_CREATNOTFOUND, dbGuid);
            SetSentErrorMessage(true);
            return false;
        }
    }

    char* export_str = ExtractLiteralArg(&args);
    if (!export_str)
    {
        PSendSysMessage(LANG_WAYPOINT_ARGUMENTREQ, "export");
        SetSentErrorMessage(true);
        return false;
    }

    if (wpOrigin == PATH_NO_PATH)
    {
        if (ExtractOptInt32(&args, wpPathId, 0))
        {
            uint32 src = (uint32)PATH_NO_PATH;
            if (ExtractOptUInt32(&args, src, src))
            {
                wpOrigin = (WaypointPathOrigin)src;
            }
            else
            {
                if (wpPathId != 0)
                {
                    wpOrigin = PATH_FROM_ENTRY;
                }
            }
        }

        if (wpOrigin == PATH_NO_PATH)
        {
            if (wpOwner->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
            {
                if (WaypointMovementGenerator const* wpMMGen = dynamic_cast<WaypointMovementGenerator const*>(wpOwner->GetMotionMaster()->GetCurrent()))
                {
                    wpMMGen->GetPathInformation(wpPathId, wpOrigin);
                }
            }
            if (wpOrigin == PATH_NO_PATH)
            {
                sWaypointMgr.GetDefaultPath(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), &wpOrigin);
            }
        }
    }

    WaypointPath const* wpPath = sWaypointMgr.GetPathFromOrigin(wpOwner->GetEntry(), wpOwner->GetGUIDLow(), wpPathId, wpOrigin);
    if (!wpPath || wpPath->empty())
    {
        PSendSysMessage(LANG_WAYPOINT_NOTHINGTOEXPORT);
        SetSentErrorMessage(true);
        return false;
    }

    std::ofstream outfile;
    outfile.open(export_str);

    std::string table;
    char const* key_field;
    uint32 key;
    switch (wpOrigin)
    {
        case PATH_FROM_ENTRY: key = wpOwner->GetEntry();    key_field = "entry";    table = "creature_movement_template"; break;
        case PATH_FROM_GUID: key = wpOwner->GetGUIDLow();   key_field = "id";       table = "creature_movement"; break;
        case PATH_FROM_EXTERNAL: key = wpOwner->GetEntry(); key_field = "entry";    table = sWaypointMgr.GetExternalWPTable(); break;
        case PATH_NO_PATH:
            return false;
    }

    outfile << "DELETE FROM `" << table << "` WHERE `" << key_field << "`=" << key << ";\n";
    if (wpOrigin != PATH_FROM_EXTERNAL)
    {
        outfile << "INSERT INTO `" << table << "` (`" << key_field << "`, `point`, `position_x`, `position_y`, `position_z`, `orientation`, `waittime`, `script_id`) VALUES\n";
    }
    else
    {
        outfile << "INSERT INTO `" << table << "` (`" << key_field << "`, `point`, `position_x`, `position_y`, `position_z`, `orientation`, `waittime`) VALUES\n";
    }

    WaypointPath::const_iterator itr = wpPath->begin();
    uint32 countDown = wpPath->size();
    for (; itr != wpPath->end(); ++itr, --countDown)
    {
        outfile << "(" << key << ",";
        outfile << itr->first << ",";
        outfile << itr->second.x << ",";
        outfile << itr->second.y << ",";
        outfile << itr->second.z << ",";
        outfile << itr->second.orientation << ",";
        outfile << itr->second.delay << ",";
        if (wpOrigin != PATH_FROM_EXTERNAL)
        {
            outfile << itr->second.script_id << ")";
        }
        if (countDown > 1)
        {
            outfile << ",\n";
        }
        else
        {
            outfile << ";\n";
        }
    }

    PSendSysMessage(LANG_WAYPOINT_EXPORTED);
    outfile.close();

    return true;
}
