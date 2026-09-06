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

#include "GroupBinds.h"

#include "Database/DatabaseEnv.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "Player.h"

GroupBinds::~GroupBinds()
{
    // Whether the copies or their holders are taken down first is undefined, so
    // each copy is told the group is gone rather than left with a dangling name.
    for (auto& held : m_held)
    {
        held.second.state->RemoveGroup(&m_owner);
    }
}

DungeonHold* GroupBinds::To(uint32 mapId)
{
    if (!sMapStore.LookupEntry(mapId))
    {
        return nullptr;
    }

    DungeonHolds::iterator itr = m_held.find(mapId);
    return itr != m_held.end() ? &itr->second : nullptr;
}

DungeonHold* GroupBinds::BindTo(DungeonPersistentState* state, bool permanent, bool load)
{
    if (!state || m_owner.isBGGroup())
    {
        return nullptr;
    }

    DungeonHold& hold = m_held[state->GetMapId()];
    if (hold.state)
    {
        // a boss has fallen, or a member's own holds are being copied over
        if (!load && (permanent != hold.permanent || state != hold.state))
        {
            CharacterDatabase.PExecute("UPDATE `group_instance` SET `instance` = '%u', `permanent` = '%u' WHERE `leaderGuid` = '%u' AND `instance` = '%u'",
                state->GetInstanceId(), permanent, m_owner.GetLeaderGuid().GetCounter(), hold.state->GetInstanceId());
        }
    }
    else if (!load)
    {
        CharacterDatabase.PExecute("INSERT INTO `group_instance` (`leaderGuid`, `instance`, `permanent`) VALUES ('%u', '%u', '%u')",
            m_owner.GetLeaderGuid().GetCounter(), state->GetInstanceId(), permanent);
    }

    if (hold.state != state)
    {
        if (hold.state)
        {
            hold.state->RemoveGroup(&m_owner);
        }
        state->AddGroup(&m_owner);
    }

    hold.state = state;
    hold.permanent = permanent;

    if (!load)
    {
        DEBUG_LOG("GroupBinds::BindTo: group (Id: %d) is now held to map %d, copy %d",
            m_owner.GetId(), state->GetMapId(), state->GetInstanceId());
    }

    return &hold;
}

void GroupBinds::Release(uint32 mapId, bool unload)
{
    DungeonHolds::iterator itr = m_held.find(mapId);
    if (itr == m_held.end())
    {
        return;
    }

    if (!unload)
    {
        CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `leaderGuid` = '%u' AND `instance` = '%u'",
            m_owner.GetLeaderGuid().GetCounter(), itr->second.state->GetInstanceId());
    }

    itr->second.state->RemoveGroup(&m_owner);               // the copy can go here
    m_held.erase(itr);
}

void GroupBinds::ReleasePermanent()
{
    for (DungeonHolds::iterator itr = m_held.begin(); itr != m_held.end();)
    {
        if (itr->second.permanent)
        {
            itr->second.state->RemoveGroup(&m_owner);
            itr = m_held.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void GroupBinds::Reset(InstanceResetMethod method, Player* tellHim)
{
    if (m_owner.isBGGroup())
    {
        return;
    }

    // method can be INSTANCE_RESET_ALL, INSTANCE_RESET_GROUP_DISBAND

    for (DungeonHolds::iterator itr = m_held.begin(); itr != m_held.end();)
    {
        DungeonPersistentState* state = itr->second.state;
        MapEntry const* entry = sMapStore.LookupEntry(itr->first);
        if (!entry || (!state->CanReset() && method != INSTANCE_RESET_GROUP_DISBAND))
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

        bool isEmpty = true;
        if (Map* map = sMapMgr.FindMap(state->GetMapId(), state->GetInstanceId()))
        {
            if (map->IsDungeon() && !(method == INSTANCE_RESET_GROUP_DISBAND && !state->CanReset()))
            {
                isEmpty = static_cast<DungeonMap*>(map)->Reset(method);
            }
        }

        if (tellHim)
        {
            if (isEmpty)
            {
                tellHim->SendResetInstanceSuccess(state->GetMapId());
            }
            else
            {
                tellHim->SendResetInstanceFailed(INSTANCERESET_FAIL_ZONING, state->GetMapId());
            }
        }

        if (!isEmpty && method != INSTANCE_RESET_GROUP_DISBAND)
        {
            ++itr;
            continue;
        }

        // someone else may hold the copy permanently, in which case it is not
        // reset -- the group merely lets go of it
        if (state->CanReset())
        {
            state->DeleteFromDB();
        }
        else
        {
            CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `instance` = '%u'", state->GetInstanceId());
        }

        itr = m_held.erase(itr);

        // this drops the copy unless someone online still holds it
        state->RemoveGroup(&m_owner);
    }
}
