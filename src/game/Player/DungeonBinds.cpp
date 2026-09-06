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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "DungeonBinds.h"

#include "Database/DatabaseEnv.h"
#include "Group.h"
#include "GroupBinds.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

DungeonBinds::~DungeonBinds()
{
    // Whether the copies or their holders are taken down first is undefined, so
    // each copy is told he is gone rather than left with a dangling name.
    for (auto& held : m_held)
    {
        held.second.state->RemovePlayer(&m_owner);
    }
}

void DungeonBinds::Load(QueryResult* result)
{
    m_held.clear();

    Group* group = m_owner.GetGroup();

    if (!result)
    {
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        bool permanent = fields[1].GetBool();
        uint32 mapId = fields[2].GetUInt32();
        uint32 instanceId = fields[0].GetUInt32();
        time_t resetTime = static_cast<time_t>(fields[3].GetUInt64());
        // the reset time of an ordinary copy is only written when the copy is
        // unloaded, so what is read here may be stale -- but then the copy is
        // loaded and this value goes unused

        MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
        if (!mapEntry || !mapEntry->IsDungeon())
        {
            sLog.outError("DungeonBinds::Load: %s is held to map %u, which is not a dungeon", m_owner.GetGuidStr().c_str(), mapId);
            CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u' AND `instance` = '%u'",
                m_owner.GetGUIDLow(), instanceId);
            continue;
        }

        if (!permanent && group)
        {
            sLog.outError("DungeonBinds::Load: %s is in group (Id: %d) but holds map %u,%u only temporarily",
                m_owner.GetGuidStr().c_str(), group->GetId(), mapId, instanceId);
            CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u' AND `instance` = '%u'",
                m_owner.GetGUIDLow(), instanceId);
            continue;
        }

        // a temporary hold is always a solo one, so its copy can always reset
        DungeonPersistentState* state = static_cast<DungeonPersistentState*>(
            sMapPersistentStateMgr.AddPersistentState(mapEntry, instanceId, resetTime, !permanent, true));
        if (state)
        {
            BindTo(state, permanent, true);
        }
    }
    while (result->NextRow());

    delete result;
}

void DungeonBinds::Reset(InstanceResetMethod method)
{
    // method can be INSTANCE_RESET_ALL, INSTANCE_RESET_GROUP_JOIN

    for (DungeonHolds::iterator itr = m_held.begin(); itr != m_held.end();)
    {
        DungeonPersistentState* state = itr->second.state;
        MapEntry const* entry = sMapStore.LookupEntry(itr->first);
        if (!entry || !state->CanReset())
        {
            ++itr;
            continue;
        }

        // resetting them all reaches ordinary dungeons only, never a raid
        if (method == INSTANCE_RESET_ALL && entry->InstanceType == MAP_RAID)
        {
            ++itr;
            continue;
        }

        if (Map* map = sMapMgr.FindMap(state->GetMapId(), state->GetInstanceId()))
        {
            if (map->IsDungeon())
            {
                static_cast<DungeonMap*>(map)->Reset(method);
            }
        }

        // a copy he holds alone has nobody inside it to be told
        if (method == INSTANCE_RESET_ALL)
        {
            m_owner.SendResetInstanceSuccess(state->GetMapId());
        }

        state->DeleteFromDB();
        m_held.erase(itr++);

        // this drops the copy from the manager unless someone else still holds it
        state->RemovePlayer(&m_owner);
    }
}

DungeonHold* DungeonBinds::To(uint32 mapId)
{
    DungeonHolds::iterator itr = m_held.find(mapId);
    return itr != m_held.end() ? &itr->second : nullptr;
}

void DungeonBinds::Release(uint32 mapId, bool unload)
{
    DungeonHolds::iterator itr = m_held.find(mapId);
    Release(itr, unload);
}

void DungeonBinds::Release(DungeonHolds::iterator& itr, bool unload)
{
    if (itr == m_held.end())
    {
        return;
    }

    if (!unload)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u' AND `instance` = '%u'",
            m_owner.GetGUIDLow(), itr->second.state->GetInstanceId());
    }

    itr->second.state->RemovePlayer(&m_owner);              // the copy can go here
    m_held.erase(itr++);
}

DungeonHold* DungeonBinds::BindTo(DungeonPersistentState* state, bool permanent, bool load)
{
    if (!state)
    {
        return nullptr;
    }

    DungeonHold& hold = m_held[state->GetMapId()];
    if (hold.state)
    {
        // the group killed a boss, so a temporary hold becomes permanent
        if (!load && (permanent != hold.permanent || state != hold.state))
        {
            CharacterDatabase.PExecute("UPDATE `character_instance` SET `instance` = '%u', `permanent` = '%u' WHERE `guid` = '%u' AND `instance` = '%u'",
                state->GetInstanceId(), permanent, m_owner.GetGUIDLow(), hold.state->GetInstanceId());
        }
    }
    else if (!load)
    {
        CharacterDatabase.PExecute("INSERT INTO `character_instance` (`guid`, `instance`, `permanent`) VALUES ('%u', '%u', '%u')",
            m_owner.GetGUIDLow(), state->GetInstanceId(), permanent);
    }

    if (hold.state != state)
    {
        if (hold.state)
        {
            hold.state->RemovePlayer(&m_owner);
        }
        state->AddPlayer(&m_owner);
    }

    if (permanent)
    {
        state->SetCanReset(false);
    }

    hold.state = state;
    hold.permanent = permanent;

    if (!load)
    {
        DEBUG_LOG("DungeonBinds::BindTo: %s(%d) is now held to map %d, copy %d",
            m_owner.GetName(), m_owner.GetGUIDLow(), state->GetMapId(), state->GetInstanceId());
    }

    return &hold;
}

DungeonPersistentState* DungeonBinds::CopyForHimOrHisGroup(uint32 mapId)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
    if (!mapEntry)
    {
        return nullptr;
    }

    DungeonHold* hold = To(mapId);
    DungeonPersistentState* state = hold ? hold->state : nullptr;

    // a permanent hold of his own comes first; failing that, his group's, and
    // only then the temporary one he took alone
    if (!hold || !hold->permanent)
    {
        if (Group* group = m_owner.GetGroup())
        {
            if (DungeonHold* groupHold = group->Binds().To(mapId))
            {
                state = groupHold->state;
            }
        }
    }

    return state;
}

void DungeonBinds::TellRaidInfo()
{
    uint32 counter = 0;

    WorldPacket data(SMSG_RAID_INSTANCE_INFO, 4);

    size_t p_counter = data.wpos();
    data << uint32(counter);                                // placeholder

    for (auto const& held : m_held)
    {
        if (!held.second.permanent)
        {
            continue;
        }

        DungeonPersistentState* state = held.second.state;
        data << uint32(state->GetMapId());
        data << uint32(state->GetResetTime() - time(nullptr));
        data << uint32(state->GetInstanceId());
        ++counter;
    }

    data.put<uint32>(p_counter, counter);
    m_owner.GetSession()->SendPacket(&data);
}

void DungeonBinds::TellSaved()
{
    bool hasBeenSaved = false;
    for (auto const& held : m_held)
    {
        if (held.second.permanent)                          // only permanent holds are sent
        {
            hasBeenSaved = true;
            break;
        }
    }

    WorldPacket data;
    data.Initialize(SMSG_UPDATE_INSTANCE_OWNERSHIP, 4);
    data << uint32(hasBeenSaved);
    m_owner.GetSession()->SendPacket(&data);

    if (!hasBeenSaved)
    {
        return;
    }

    for (auto const& held : m_held)
    {
        if (held.second.permanent)
        {
            data.Initialize(SMSG_UPDATE_LAST_INSTANCE, 4);
            data << uint32(held.second.state->GetMapId());
            m_owner.GetSession()->SendPacket(&data);
        }
    }
}
