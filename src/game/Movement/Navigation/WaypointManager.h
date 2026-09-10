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
#include "Platform/Define.h"
#include <map>
#include <set>
#include <vector>
#include <string>

enum WaypointPathOrigin
{
    PATH_NO_PATH            = 0,
    PATH_FROM_GUID          = 1,
    PATH_FROM_ENTRY         = 2,
    PATH_FROM_EXTERNAL      = 3
};

#define MAX_WAYPOINT_TEXT 5
struct WaypointBehavior
{
    uint32 emote;
    uint32 spell;
    int32  textid[MAX_WAYPOINT_TEXT];
    uint32 model1;
    uint32 model2;

    bool isEmpty();
    WaypointBehavior()
    {
        emote = 0;
        spell = 0;
        model1 = 0;
        model2 = 0;
        for (uint32 i = 0; i < MAX_WAYPOINT_TEXT; ++i)
        {
            textid[i] = 0;
        }
    }
    WaypointBehavior(const WaypointBehavior& b);
};

struct WaypointNode
{
    float x;
    float y;
    float z;
    float orientation;
    uint32 delay;
    uint32 script_id;
    WaypointBehavior* behavior;
    WaypointNode() : x(0.0f), y(0.0f), z(0.0f), orientation(0.0f), delay(0), script_id(0), behavior(nullptr) {}
    WaypointNode(float _x, float _y, float _z, float _o, uint32 _delay, uint32 _script_id, WaypointBehavior* _behavior)
        : x(_x), y(_y), z(_z), orientation(_o), delay(_delay), script_id(_script_id), behavior(_behavior) {}
};

typedef std::map < uint32 , WaypointNode > WaypointPath;

class WaypointManager
{
    public:
        WaypointManager() : m_externalTable("external.waypointTable") {}
        ~WaypointManager()
        {
            Unload();
        }

        void Load();
        void Unload();

        WaypointPath* GetDefaultPath(uint32 entry, uint32 lowGuid, WaypointPathOrigin* wpOrigin = nullptr)
        {
            WaypointPath* path = nullptr;
            path = GetPath(lowGuid);
            if (path && wpOrigin)
            {
                *wpOrigin = PATH_FROM_GUID;
            }

            if (!path)
            {
                path = GetPathTemplate(entry);
                if (path && wpOrigin)
                {
                    *wpOrigin = PATH_FROM_ENTRY;
                }
            }

            return path;
        }

        WaypointPath* GetPathFromOrigin(uint32 entry, uint32 lowGuid, int32 pathId, WaypointPathOrigin wpOrigin)
        {
            WaypointPathMap* wpMap = nullptr;
            uint32 key = 0;

            switch (wpOrigin)
            {
                case PATH_FROM_GUID:
                    key = lowGuid;
                    wpMap = &m_pathMap;
                    break;
                case PATH_FROM_ENTRY:
                    if (pathId >= 0xFF || pathId < 0)
                    {
                        return nullptr;
                    }
                    key = (entry << 8) + pathId;
                    wpMap = &m_pathTemplateMap;
                    break;
                case PATH_FROM_EXTERNAL:
                    if (pathId >= 0xFF || pathId < 0)
                    {
                        return nullptr;
                    }
                    key = (entry << 8) + pathId;
                    wpMap = &m_externalPathTemplateMap;
                    break;
                case PATH_NO_PATH:
                default:
                    return nullptr;
            }
            WaypointPathMap::iterator find = wpMap->find(key);
            return find != wpMap->end() ? &find->second : nullptr;
        }

        void DeletePath(uint32 id);
        void CheckTextsExistance(std::set<int32>& ids);

        void SetExternalWPTable(char const* tableName) { m_externalTable = std::string(tableName); }
        std::string GetExternalWPTable() const { return m_externalTable; }

        bool AddExternalNode(uint32 entry, int32 pathId, uint32 pointId, float x, float y, float z, float o, uint32 waittime);

        WaypointNode const* AddNode(uint32 entry, uint32 dbGuid, uint32& pointId, WaypointPathOrigin wpDest, float x, float y, float z);

        void DeleteNode(uint32 entry, uint32 dbGuid, uint32 point, int32 pathId, WaypointPathOrigin wpOrigin);
        void SetNodePosition(uint32 entry, uint32 dbGuid, uint32 point, int32 pathId, WaypointPathOrigin wpOrigin, float x, float y, float z);
        void SetNodeWaittime(uint32 entry, uint32 dbGuid, uint32 point, int32 pathId, WaypointPathOrigin wpOrigin, uint32 waittime);
        void SetNodeOrientation(uint32 entry, uint32 dbGuid, uint32 point, int32 pathId, WaypointPathOrigin wpOrigin, float orientation);
        bool SetNodeScriptId(uint32 entry, uint32 dbGuid, uint32 point, int32 pathId, WaypointPathOrigin wpOrigin, uint32 scriptId);

        static std::string GetOriginString(WaypointPathOrigin origin)
        {
            switch (origin)
            {
                case PATH_NO_PATH:          return "<no path>";
                case PATH_FROM_GUID:        return "guid";
                case PATH_FROM_ENTRY:       return "entry";
                case PATH_FROM_EXTERNAL:    return "external";
                default:                    return "invalid origin";
            }
        }

    private:
        WaypointPath* GetPath(uint32 id)
        {
            WaypointPathMap::iterator itr = m_pathMap.find(id);
            return itr != m_pathMap.end() ? &itr->second : nullptr;
        }

        WaypointPath* GetPathTemplate(uint32 entry)
        {
            WaypointPathMap::iterator itr = m_pathTemplateMap.find((entry << 8) );
            return itr != m_pathTemplateMap.end() ? &itr->second : nullptr;
        }

        void _clearPath(WaypointPath& path);

        typedef std::unordered_map < uint32 , WaypointPath > WaypointPathMap;
        WaypointPathMap m_pathMap;
        WaypointPathMap m_pathTemplateMap;
        WaypointPathMap m_externalPathTemplateMap;
        std::string m_externalTable;
};

#define sWaypointMgr MaNGOS::Singleton<WaypointManager>::Instance()

bool AddWaypointFromExternal(uint32 entry, int32 pathId, uint32 pointId, float x, float y, float z, float o, uint32 waittime);
