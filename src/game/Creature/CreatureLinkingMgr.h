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

#include <unordered_set>
#include <utility>
#include "Platform/Define.h"
#include <map>
#include "Policies/Singleton.h"
#include "ObjectGuid.h"
#include <functional>

class Unit;
class Creature;
class Map;

enum CreatureLinkingEvent
{
    LINKING_EVENT_AGGRO         = 0,
    LINKING_EVENT_EVADE         = 1,
    LINKING_EVENT_DIE           = 2,
    LINKING_EVENT_RESPAWN       = 3,
    LINKING_EVENT_DESPAWN       = 4,
};

enum CreatureLinkingFlags
{

    FLAG_AGGRO_ON_AGGRO             = 0x0001,
    FLAG_TO_AGGRO_ON_AGGRO          = 0x0002,
    FLAG_RESPAWN_ON_EVADE           = 0x0004,
    FLAG_TO_RESPAWN_ON_EVADE        = 0x0008,
    FLAG_DESPAWN_ON_EVADE           = 0x1000,
    FLAG_DESPAWN_ON_DEATH           = 0x0010,
    FLAG_SELFKILL_ON_DEATH          = 0x0020,
    FLAG_RESPAWN_ON_DEATH           = 0x0040,
    FLAG_RESPAWN_ON_RESPAWN         = 0x0080,
    FLAG_DESPAWN_ON_RESPAWN         = 0x0100,

    FLAG_FOLLOW                     = 0x0200,
    FLAG_DESPAWN_ON_DESPAWN         = 0x2000,

    FLAG_CANT_SPAWN_IF_BOSS_DEAD    = 0x0400,
    FLAG_CANT_SPAWN_IF_BOSS_ALIVE   = 0x0800,

    LINKING_FLAG_INVALID            = 0x4000,
};

struct CreatureLinkingInfo
{
    uint32 mapId;
    uint32 masterId;
    uint32 masterDBGuid;
    uint16 linkingFlag: 16;
    uint16 searchRange: 16;
};

class CreatureLinkingMgr
{
    public:
        CreatureLinkingMgr() {}

    public:
        void LoadFromDB();

    public:

        bool IsLinkedEventTrigger(Creature* pCreature) const;

        bool IsLinkedMaster(Creature* pCreature) const;

        bool IsSpawnedByLinkedMob(Creature* pCreature) const;
        bool IsSpawnedByLinkedMob(CreatureLinkingInfo const* pInfo) const;

        CreatureLinkingInfo const* GetLinkedTriggerInformation(Creature* pCreature) const;
        CreatureLinkingInfo const* GetLinkedTriggerInformation(uint32 entry, uint32 lowGuid, uint32 mapId) const;

    private:
        typedef std::multimap < uint32 , CreatureLinkingInfo > CreatureLinkingMap;
        typedef std::pair<CreatureLinkingMap::const_iterator, CreatureLinkingMap::const_iterator> CreatureLinkingMapBounds;

        CreatureLinkingMap m_creatureLinkingMap;

        CreatureLinkingMap m_creatureLinkingGuidMap;

        std::unordered_set<uint32> m_eventTriggers;
        std::unordered_set<uint32> m_eventGuidTriggers;

        static bool IsLinkingEntryValid(uint32 slaveEntry, CreatureLinkingInfo* pInfo, bool byEntry);
};

class CreatureLinkingHolder
{
    public:
        CreatureLinkingHolder() {}

    public:

        void AddSlaveToHolder(Creature* pCreature);

        void AddMasterToHolder(Creature* pCreature);

        void DoCreatureLinkingEvent(CreatureLinkingEvent eventType, Creature* pSource, Unit* pEnemy = nullptr);

        bool CanSpawn(Creature* pCreature) const;

        bool TryFollowMaster(Creature* pCreature);

    private:

        struct InfoAndGuids
        {
            uint16 linkingFlag: 16;
            uint16 searchRange: 16;
            GuidList linkedGuids;
        };

        struct InfoAndGuid
        {
            uint16 linkingFlag;
            ObjectGuid linkedGuid = 0;
        };

        typedef std::multimap < uint32 , InfoAndGuids > HolderMap;
        typedef std::pair<HolderMap::iterator, HolderMap::iterator> HolderMapBounds;
        typedef std::multimap < uint32 , ObjectGuid > BossGuidMap;
        typedef std::pair<BossGuidMap::const_iterator, BossGuidMap::const_iterator> BossGuidMapBounds;

        void ProcessSlaveGuidList(CreatureLinkingEvent eventType, Creature* pSource, uint32 flag, uint16 searchRange, GuidList& slaveGuidList, Unit* pEnemy);

        void ProcessSlave(CreatureLinkingEvent eventType, Creature* pSource, uint32 flag, Creature* pSlave, Unit* pEnemy);

        void SetFollowing(Creature* pWho, Creature* pWhom);

        bool IsSlaveInRangeOfBoss(Creature const* pSlave, Creature const* pBoss, uint16 searchRange) const;
        bool IsSlaveInRangeOfBoss(Creature const* pBoss, float slaveX, float slaveY, uint16 searchRange) const;

        bool IsRespawnReady(uint32 dbLowGuid, Map* _map) const;

        bool CanSpawn(uint32 lowGuid, Map* _map, CreatureLinkingInfo const*  pInfo, float sx, float sy) const;

        HolderMap m_holderMap;

        HolderMap m_holderGuidMap;

        BossGuidMap m_masterGuid;
};

#define sCreatureLinkingMgr MaNGOS::Singleton<CreatureLinkingMgr>::Instance()
