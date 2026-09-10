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
#include "Utilities/MathDefines.h"
#include "CreatureLinkingMgr.h"
#include "Policies/Singleton.h"
#include "ProgressBar.h"
#include "Database/DatabaseEnv.h"
#include "ObjectMgr.h"
#include "SharedDefines.h"
#include "Creature.h"
#include "CreatureAI.h"

#define INVALID_MAP_ID      0xFFFFFFFF

void CreatureLinkingMgr::LoadFromDB()
{

    m_creatureLinkingMap.clear();
    m_creatureLinkingGuidMap.clear();
    m_eventTriggers.clear();
    m_eventGuidTriggers.clear();

    sLog.outString("> Loading table `creature_linking_template`");
    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `map`, `master_entry`, `flag`, `search_range` FROM `creature_linking_template`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Table creature_linking_template is empty.");
        sLog.outString();
    }
    else
    {
        BarGoLink bar((int)result->GetRowCount());
        do
        {
            bar.step();

            Field* fields = result->Fetch();
            CreatureLinkingInfo tmp;

            uint32 entry            = fields[0].GetUInt32();
            tmp.mapId               = fields[1].GetUInt32();
            tmp.masterId            = fields[2].GetUInt32();
            tmp.linkingFlag         = fields[3].GetUInt16();
            tmp.searchRange         = fields[4].GetUInt16();
            tmp.masterDBGuid        = 0;

            if (!IsLinkingEntryValid(entry, &tmp, true))
            {
                continue;
            }

            ++count;

            m_creatureLinkingMap.insert(CreatureLinkingMap::value_type(entry, tmp));

            m_eventTriggers.insert(tmp.masterId);
        }
        while (result->NextRow());

        sLog.outString(">> Loaded creature linking for %u creature-entries", count);
        sLog.outString();

        delete result;
    }

    sLog.outString("> Loading table `creature_linking`");
    count = 0;
    result = WorldDatabase.Query("SELECT `guid`, `master_guid`, `flag` FROM `creature_linking`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString(">> Table creature_linking is empty.");
        sLog.outString();

        return;
    }

    BarGoLink guidBar((int)result->GetRowCount());
    do
    {
        guidBar.step();

        Field* fields = result->Fetch();
        CreatureLinkingInfo tmp;

        uint32 guid             = fields[0].GetUInt32();
        tmp.mapId               = INVALID_MAP_ID;
        tmp.masterId            = fields[1].GetUInt32();
        tmp.linkingFlag         = fields[2].GetUInt16();
        tmp.masterDBGuid        = tmp.masterId;
        tmp.searchRange         = 0;

        if (!IsLinkingEntryValid(guid, &tmp, false))
        {
            continue;
        }

        ++count;

        m_creatureLinkingGuidMap.insert(CreatureLinkingMap::value_type(guid, tmp));

        m_eventGuidTriggers.insert(tmp.masterId);
    }
    while (result->NextRow());

    sLog.outString(">> Loaded creature linking for %u creature-Guids", count);
    sLog.outString();

    delete result;
}

bool CreatureLinkingMgr::IsLinkingEntryValid(uint32 slaveEntry, CreatureLinkingInfo* pTmp, bool byEntry)
{

    if (byEntry)
    {
        CreatureInfo const* pInfo = ObjectMgr::GetCreatureTemplate(slaveEntry);
        CreatureInfo const* pMasterInfo = ObjectMgr::GetCreatureTemplate(pTmp->masterId);

        if (!pInfo)
        {
            sLog.outErrorDb("`creature_linking_template` has a non existing slave_entry (slave: %u, master %u), skipped.", slaveEntry, pTmp->masterId);
            return false;
        }
        if (!pMasterInfo)
        {
            sLog.outErrorDb("`creature_linking_template` has a non existing master_entry (slave: %u, master %u), skipped", slaveEntry, pTmp->masterId);
            return false;
        }
        if (pTmp->mapId && !sMapStore.LookupEntry(pTmp->mapId))
        {
            sLog.outErrorDb("`creature_linking_template` has a non existing map %u (slave %u, master %u), skipped", pTmp->mapId, slaveEntry, pTmp->masterId);
            return false;
        }
    }
    else
    {
        CreatureData const* slaveData = sObjectMgr.GetCreatureData(slaveEntry);
        CreatureData const* masterData = sObjectMgr.GetCreatureData(pTmp->masterId);

        if (!slaveData)
        {
            sLog.outErrorDb("`creature_linking` has a non existing slave (guid: %u, master_guid %u), skipped", slaveEntry, pTmp->masterId);
            return false;
        }
        if (!masterData)
        {
            sLog.outErrorDb("`creature_linking` has a non existing master (guid: %u,, master_guid: %u), skipped", slaveEntry, pTmp->masterId);
            return false;
        }
        if (slaveData->mapid != masterData->mapid)
        {
            sLog.outErrorDb("`creature_linking` has a slave and master on different maps (guid: %u, master_guid: %u), skipped", slaveEntry, pTmp->masterId);
            return false;
        }
    }

    if (pTmp->linkingFlag & ~(LINKING_FLAG_INVALID - 1)  || pTmp->linkingFlag == 0)
    {
        sLog.outErrorDb("`creature_linking%s` has invalid flag, (entry: %u, map: %u, flags: %u), skipped", byEntry ? "_template" : "", slaveEntry, pTmp->mapId, pTmp->linkingFlag);
        return false;
    }

    if (pTmp->linkingFlag & FLAG_DESPAWN_ON_RESPAWN && slaveEntry == pTmp->masterId)
    {
        sLog.outErrorDb("`creature_linking%s` has pointless FLAG_DESPAWN_ON_RESPAWN for self, (entry: %u, map: %u), skipped", byEntry ? "_template" : "", slaveEntry, pTmp->mapId);
        return false;
    }

    if (byEntry)
    {

        if (pTmp->searchRange == 0 && pTmp->linkingFlag & (FLAG_FOLLOW | FLAG_CANT_SPAWN_IF_BOSS_DEAD | FLAG_CANT_SPAWN_IF_BOSS_ALIVE))
        {
            QueryResult* result = WorldDatabase.PQuery("SELECT `guid` FROM `creature` WHERE `id`=%u AND `map`=%u LIMIT 2", pTmp->masterId, pTmp->mapId);
            if (!result)
            {
                sLog.outErrorDb("`creature_linking_template` has FLAG_FOLLOW, but no master, (entry: %u, map: %u, master: %u)", slaveEntry, pTmp->mapId, pTmp->masterId);
                return false;
            }

            if (result->GetRowCount() > 1)
            {
                sLog.outErrorDb("`creature_linking_template` has FLAG_FOLLOW, but non unique master, (entry: %u, map: %u, master: %u)", slaveEntry, pTmp->mapId, pTmp->masterId);
                delete result;
                return false;
            }
            Field* fields = result->Fetch();
            pTmp->masterDBGuid = fields[0].GetUInt32();
            delete result;
        }
    }

    return true;
}

enum EventMask
{
    EVENT_MASK_ON_AGGRO     = FLAG_AGGRO_ON_AGGRO,
    EVENT_MASK_ON_EVADE     = FLAG_RESPAWN_ON_EVADE | FLAG_DESPAWN_ON_EVADE,
    EVENT_MASK_ON_DIE       = FLAG_DESPAWN_ON_DEATH | FLAG_SELFKILL_ON_DEATH | FLAG_RESPAWN_ON_DEATH | FLAG_FOLLOW,
    EVENT_MASK_ON_RESPAWN   = FLAG_RESPAWN_ON_RESPAWN | FLAG_DESPAWN_ON_RESPAWN | FLAG_FOLLOW,
    EVENT_MASK_TRIGGER_TO   = FLAG_TO_AGGRO_ON_AGGRO | FLAG_TO_RESPAWN_ON_EVADE | FLAG_FOLLOW,
    EVENT_MASK_ON_DESPAWN   = FLAG_DESPAWN_ON_DESPAWN,
};

bool CreatureLinkingMgr::IsLinkedEventTrigger(Creature* pCreature) const
{

    if (m_eventTriggers.find(pCreature->GetEntry()) != m_eventTriggers.end())
    {
        return true;
    }

    if (m_eventGuidTriggers.find(pCreature->GetGUIDLow()) != m_eventGuidTriggers.end())
    {
        return true;
    }

    if (CreatureLinkingInfo const* pInfo = GetLinkedTriggerInformation(pCreature))
    {
        return pInfo->linkingFlag & EVENT_MASK_TRIGGER_TO;
    }

    return false;
}

bool CreatureLinkingMgr::IsLinkedMaster(Creature* pCreature) const
{
    return m_eventTriggers.find(pCreature->GetEntry()) != m_eventTriggers.end();
}

bool CreatureLinkingMgr::IsSpawnedByLinkedMob(Creature* pCreature) const
{
    return IsSpawnedByLinkedMob(GetLinkedTriggerInformation(pCreature));
}

bool CreatureLinkingMgr::IsSpawnedByLinkedMob(CreatureLinkingInfo const* pInfo) const
{
    return pInfo && pInfo->linkingFlag & (FLAG_CANT_SPAWN_IF_BOSS_DEAD | FLAG_CANT_SPAWN_IF_BOSS_ALIVE) && (pInfo->masterDBGuid || pInfo->searchRange);
}

CreatureLinkingInfo const* CreatureLinkingMgr::GetLinkedTriggerInformation(Creature* pCreature) const
{
    return GetLinkedTriggerInformation(pCreature->GetEntry(), pCreature->GetGUIDLow(), pCreature->GetMapId());
}

CreatureLinkingInfo const* CreatureLinkingMgr::GetLinkedTriggerInformation(uint32 entry, uint32 lowGuid, uint32 mapId) const
{

    CreatureLinkingMapBounds bounds = m_creatureLinkingGuidMap.equal_range(lowGuid);
    for (CreatureLinkingMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
    {
        return &(iter->second);
    }

    bounds = m_creatureLinkingMap.equal_range(entry);
    for (CreatureLinkingMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
    {
        if (iter->second.mapId == mapId)
        {
            return &(iter->second);
        }
    }

    return nullptr;
}

void CreatureLinkingHolder::AddSlaveToHolder(Creature* pCreature)
{
    CreatureLinkingInfo const* pInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(pCreature);
    if (!pInfo)
    {
        return;
    }

    if (pInfo->mapId == INVALID_MAP_ID)
    {
        HolderMapBounds bounds = m_holderGuidMap.equal_range(pInfo->masterId);
        for (HolderMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
        {
            if (itr->second.linkingFlag == pInfo->linkingFlag)
            {
                itr->second.linkedGuids.push_back(pCreature->GetObjectGuid());
                pCreature = nullptr;
                break;
            }
        }

        if (pCreature)
        {
            InfoAndGuids tmp;
            tmp.linkedGuids.push_back(pCreature->GetObjectGuid());
            tmp.linkingFlag = pInfo->linkingFlag;
            tmp.searchRange = 0;
            m_holderGuidMap.insert(HolderMap::value_type(pInfo->masterId, tmp));
        }
        return;
    }

    HolderMapBounds bounds = m_holderMap.equal_range(pInfo->masterId);
    for (HolderMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second.linkingFlag == pInfo->linkingFlag && itr->second.searchRange == pInfo->searchRange)
        {
            itr->second.linkedGuids.push_back(pCreature->GetObjectGuid());
            pCreature = nullptr;
            break;
        }
    }

    if (pCreature)
    {
        InfoAndGuids tmp;
        tmp.linkedGuids.push_back(pCreature->GetObjectGuid());
        tmp.linkingFlag = pInfo->linkingFlag;
        tmp.searchRange = pInfo->searchRange;
        m_holderMap.insert(HolderMap::value_type(pInfo->masterId, tmp));
    }
}

void CreatureLinkingHolder::AddMasterToHolder(Creature* pCreature)
{
    if (pCreature->IsPet())
    {
        return;
    }

    if (!sCreatureLinkingMgr.IsLinkedMaster(pCreature))
    {
        return;
    }

    BossGuidMapBounds bounds = m_masterGuid.equal_range(pCreature->GetEntry());
    for (BossGuidMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second == pCreature->GetObjectGuid())
        {
            return;
        }
    }
    m_masterGuid.insert(BossGuidMap::value_type(pCreature->GetEntry(), pCreature->GetObjectGuid()));
}

void CreatureLinkingHolder::DoCreatureLinkingEvent(CreatureLinkingEvent eventType, Creature* pSource, Unit* pEnemy )
{

    if (!sCreatureLinkingMgr.IsLinkedEventTrigger(pSource))
    {
        return;
    }

    if (pSource->IsControlledByPlayer())
    {
        return;
    }

    if (eventType == LINKING_EVENT_AGGRO && !pEnemy)
    {
        return;
    }

    uint32 eventFlagFilter = 0;
    uint32 reverseEventFlagFilter = 0;

    switch (eventType)
    {
        case LINKING_EVENT_AGGRO:   eventFlagFilter = EVENT_MASK_ON_AGGRO;   reverseEventFlagFilter = FLAG_TO_AGGRO_ON_AGGRO;   break;
        case LINKING_EVENT_EVADE:   eventFlagFilter = EVENT_MASK_ON_EVADE;   reverseEventFlagFilter = FLAG_TO_RESPAWN_ON_EVADE; break;
        case LINKING_EVENT_DIE:     eventFlagFilter = EVENT_MASK_ON_DIE;     reverseEventFlagFilter = 0;                        break;
        case LINKING_EVENT_RESPAWN: eventFlagFilter = EVENT_MASK_ON_RESPAWN; reverseEventFlagFilter = FLAG_FOLLOW;              break;
        case LINKING_EVENT_DESPAWN: eventFlagFilter = EVENT_MASK_ON_DESPAWN; reverseEventFlagFilter = 0;                        break;
    }

    HolderMapBounds bounds = m_holderMap.equal_range(pSource->GetEntry());
    for (HolderMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        ProcessSlaveGuidList(eventType, pSource, itr->second.linkingFlag & eventFlagFilter, itr->second.searchRange, itr->second.linkedGuids, pEnemy);
    }

    bounds = m_holderGuidMap.equal_range(pSource->GetGUIDLow());
    for (HolderMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        ProcessSlaveGuidList(eventType, pSource, itr->second.linkingFlag & eventFlagFilter, itr->second.searchRange, itr->second.linkedGuids, pEnemy);
    }

    if (CreatureLinkingInfo const* pInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(pSource))
    {
        if (pInfo->linkingFlag & reverseEventFlagFilter)
        {
            Creature* pMaster = nullptr;
            if (pInfo->mapId != INVALID_MAP_ID)
            {
                BossGuidMapBounds finds = m_masterGuid.equal_range(pInfo->masterId);
                for (BossGuidMap::const_iterator itr = finds.first; itr != finds.second; ++itr)
                {
                    pMaster = pSource->GetMap()->GetCreature(itr->second);
                    if (pMaster && IsSlaveInRangeOfBoss(pSource, pMaster, pInfo->searchRange))
                    {
                        break;
                    }
                }
            }
            else
            {
                CreatureData const* masterData = sObjectMgr.GetCreatureData(pInfo->masterDBGuid);
                CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(masterData->id);
                pMaster = pSource->GetMap()->GetCreature(MakeGuid(cInfo->GetHighGuid(), cInfo->Entry, pInfo->masterDBGuid));
            }

            if (pMaster)
            {
                switch (eventType)
                {
                    case LINKING_EVENT_AGGRO:
                        if (pMaster->IsControlledByPlayer())
                        {
                            return;
                        }

                        if (pMaster->IsInCombat())
                        {
                            pMaster->SetInCombatWith(pEnemy);
                        }
                        else
                        {
                            pMaster->AI()->AttackStart(pEnemy);
                        }
                        break;
                    case LINKING_EVENT_EVADE:
                        if (!pMaster->IsAlive())
                        {
                            pMaster->Respawn();
                        }
                        break;
                    case LINKING_EVENT_RESPAWN:
                        if (pMaster->IsAlive())
                        {
                            SetFollowing(pSource, pMaster);
                        }
                        break;
                    case LINKING_EVENT_DIE:
                    case LINKING_EVENT_DESPAWN:
                        break;
                }
            }
        }
    }
}

void CreatureLinkingHolder::ProcessSlaveGuidList(CreatureLinkingEvent eventType, Creature* pSource, uint32 flag, uint16 searchRange, GuidList& slaveGuidList, Unit* pEnemy)
{
    if (!flag)
    {
        return;
    }

    for (GuidList::iterator slave_itr = slaveGuidList.begin(); slave_itr != slaveGuidList.end();)
    {
        Creature* pSlave = pSource->GetMap()->GetCreature(*slave_itr);
        if (!pSlave)
        {

            slaveGuidList.erase(slave_itr++);
            continue;
        }

        ++slave_itr;

        if (pSlave->IsPet())
        {
            continue;
        }

        if (IsSlaveInRangeOfBoss(pSlave, pSource, searchRange))
        {
            ProcessSlave(eventType, pSource, flag, pSlave, pEnemy);
        }
    }
}

void CreatureLinkingHolder::ProcessSlave(CreatureLinkingEvent eventType, Creature* pSource, uint32 flag, Creature* pSlave, Unit* pEnemy)
{
    switch (eventType)
    {
        case LINKING_EVENT_AGGRO:
            if (flag & FLAG_AGGRO_ON_AGGRO)
            {
                if (pSlave->IsControlledByPlayer())
                {
                    return;
                }

                if (pSlave->IsInCombat())
                {
                    pSlave->SetInCombatWith(pEnemy);
                }
                else
                {
                    pSlave->AI()->AttackStart(pEnemy);
                }
            }
            break;
        case LINKING_EVENT_EVADE:
            if (flag & FLAG_DESPAWN_ON_EVADE && pSlave->IsAlive())
            {
                pSlave->ForcedDespawn();
            }
            if (flag & FLAG_RESPAWN_ON_EVADE && !pSlave->IsAlive())
            {
                pSlave->Respawn();
            }
            break;
        case LINKING_EVENT_DIE:
            if (flag & FLAG_SELFKILL_ON_DEATH && pSlave->IsAlive())
            {
                pSlave->DealDamage(pSlave, pSlave->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
            }
            if (flag & FLAG_DESPAWN_ON_DEATH && pSlave->IsAlive())
            {
                pSlave->ForcedDespawn();
            }
            if (flag & FLAG_RESPAWN_ON_DEATH && !pSlave->IsAlive())
            {
                pSlave->Respawn();
            }
            break;
        case LINKING_EVENT_RESPAWN:
            if (flag & FLAG_RESPAWN_ON_RESPAWN)
            {

                if (!pSlave->IsAlive() && pSlave->GetRespawnTime() > time(nullptr))
                {
                    pSlave->Respawn();
                }
            }
            else if (flag & FLAG_DESPAWN_ON_RESPAWN && pSlave->IsAlive())
            {
                pSlave->ForcedDespawn();
            }

            if (flag & FLAG_FOLLOW && pSlave->IsAlive() && !pSlave->IsInCombat())
            {
                SetFollowing(pSlave, pSource);
            }

            break;
        case LINKING_EVENT_DESPAWN:
            if (flag & FLAG_DESPAWN_ON_DESPAWN && !pSlave->IsDespawned())
            {
                pSlave->ForcedDespawn();
            }

            break;
    }
}

void CreatureLinkingHolder::SetFollowing(Creature* pWho, Creature* pWhom)
{

    float sX, sY, sZ, mX, mY, mZ, mO;
    sX = pWho->Spawn().X();
    sY = pWho->Spawn().Y();
    sZ = pWho->Spawn().Z();
    mX = pWhom->Spawn().X();
    mY = pWhom->Spawn().Y();
    mZ = pWhom->Spawn().Z();
    mO = pWhom->Spawn().Facing();

    float dx, dy, dz;
    dx = sX - mX;
    dy = sY - mY;
    dz = sZ - mZ;

    float dist = sqrt(dx * dx + dy * dy + dz * dz);

    dist = dist - pWho->Where().Extent() - pWhom->Where().Extent();
    if (dist < 0.0f)
    {
        dist = 0.0f;
    }

    float angle = atan2(dy, dx) - mO;
    angle = (angle >= 0) ? angle : 2 * M_PI_F + angle;

    pWho->GetMotionMaster()->MoveFollow(pWhom, dist, angle);
}

bool CreatureLinkingHolder::IsSlaveInRangeOfBoss(Creature const* pSlave, Creature const* pBoss, uint16 searchRange) const
{
    float sX, sY, sZ;
    sX = pSlave->Spawn().X();
    sY = pSlave->Spawn().Y();
    sZ = pSlave->Spawn().Z();
    return IsSlaveInRangeOfBoss(pBoss, sX, sY, searchRange);
}

bool CreatureLinkingHolder::IsSlaveInRangeOfBoss(Creature const* pBoss, float sX, float sY, uint16 searchRange) const
{
    if (!searchRange)
    {
        return true;
    }

    float mX, mY, mZ, dx, dy;
    mX = pBoss->Spawn().X();
    mY = pBoss->Spawn().Y();
    mZ = pBoss->Spawn().Z();

    dx = sX - mX;
    dy = sY - mY;

    return dx * dx + dy * dy < searchRange * searchRange;
}

bool CreatureLinkingHolder::IsRespawnReady(uint32 dbLowGuid, Map* _map) const
{
    time_t respawnTime = _map->GetPersistentState()->GetCreatureRespawnTime(dbLowGuid);
    return (!respawnTime || respawnTime <= time(nullptr)) && CanSpawn(dbLowGuid, _map, nullptr, 0.0f, 0.0f);
}

bool CreatureLinkingHolder::CanSpawn(Creature* pCreature) const
{
    CreatureLinkingInfo const*  pInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(pCreature);
    if (!pInfo)
    {
        return true;
    }

    float sx, sy, sz;
    sx = pCreature->Spawn().X();
    sy = pCreature->Spawn().Y();
    sz = pCreature->Spawn().Z();
    return CanSpawn(0, pCreature->GetMap(), pInfo, sx, sy);
}

bool CreatureLinkingHolder::CanSpawn(uint32 lowGuid, Map* _map, CreatureLinkingInfo const*  pInfo, float sx, float sy) const
{
    if (!pInfo)
    {
        CreatureData const* data = sObjectMgr.GetCreatureData(lowGuid);
        if (!data)
        {
            return true;
        }
        pInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(data->id, lowGuid, data->mapid);
        if (!pInfo)
        {
            return true;
        }

        if (!sCreatureLinkingMgr.IsSpawnedByLinkedMob(pInfo))
        {
            return true;
        }

        sx = data->posX;
        sy = data->posY;
    }

    if (pInfo->searchRange == 0)
    {
        if (!pInfo->masterDBGuid)
        {
            return false;
        }

        if (pInfo->linkingFlag & FLAG_CANT_SPAWN_IF_BOSS_DEAD)
        {
            return IsRespawnReady(pInfo->masterDBGuid, _map);
        }
        else if (pInfo->linkingFlag & FLAG_CANT_SPAWN_IF_BOSS_ALIVE)
        {
            return !IsRespawnReady(pInfo->masterDBGuid, _map);
        }
        else
        {
            return true;
        }
    }

    BossGuidMapBounds finds = m_masterGuid.equal_range(pInfo->masterId);
    for (BossGuidMap::const_iterator itr = finds.first; itr != finds.second; ++itr)
    {
        Creature* pMaster = _map->GetCreature(itr->second);
        if (pMaster && IsSlaveInRangeOfBoss(pMaster, sx, sy, pInfo->searchRange))
        {
            if (pInfo->linkingFlag & FLAG_CANT_SPAWN_IF_BOSS_DEAD)
            {
                return pMaster->IsAlive();
            }
            else if (pInfo->linkingFlag & FLAG_CANT_SPAWN_IF_BOSS_ALIVE)
            {
                return !pMaster->IsAlive();
            }
            else
            {
                return true;
            }
        }
    }

    return true;
}

bool CreatureLinkingHolder::TryFollowMaster(Creature* pCreature)
{
    CreatureLinkingInfo const*  pInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(pCreature);
    if (!pInfo || !(pInfo->linkingFlag & FLAG_FOLLOW))
    {
        return false;
    }

    Creature* pMaster = nullptr;
    if (pInfo->mapId != INVALID_MAP_ID)
    {
        BossGuidMapBounds finds = m_masterGuid.equal_range(pInfo->masterId);
        for (BossGuidMap::const_iterator itr = finds.first; itr != finds.second; ++itr)
        {
            pMaster = pCreature->GetMap()->GetCreature(itr->second);
            if (pMaster && IsSlaveInRangeOfBoss(pCreature, pMaster, pInfo->searchRange))
            {
                break;
            }
        }
    }
    else
    {
        CreatureData const* masterData = sObjectMgr.GetCreatureData(pInfo->masterDBGuid);
        CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(masterData->id);
        pMaster = pCreature->GetMap()->GetCreature(MakeGuid(cInfo->GetHighGuid(), cInfo->Entry, pInfo->masterDBGuid));
    }

    if (pMaster && pMaster->IsAlive())
    {
        SetFollowing(pCreature, pMaster);
        return true;
    }

    return false;
}
