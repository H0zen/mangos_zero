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

#include "World.h"
#include "Database/DatabaseEnv.h"
#include "Config/Config.h"
#include "Platform/Define.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Player.h"
#include "AccountMgr.h"
#include "AuctionHouseMgr.h"
#include "ObjectMgr.h"
#include "CreatureEventAIMgr.h"
#include "GuildMgr.h"
#include "SpellMgr.h"
#include "Chat.h"
#include "DBCStores.h"
#include "MassMailMgr.h"
#include "LootMgr.h"
#include "ItemEnchantmentMgr.h"
#include "ScriptMgr.h"
#include "CreatureAIRegistry.h"
#include "ProgressBar.h"
#include "Policies/Singleton.h"
#include "BattleGround/BattleGroundMgr.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "TemporarySummon.h"
#include "MoveMap.h"
#include "GameEventMgr.h"
#include "PoolManager.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "MapPersistentStateMgr.h"
#include "WaypointManager.h"
#include "GMTicketMgr.h"
#include "Util.h"
#include "CharacterDatabaseCleaner.h"
#include "CreatureLinkingMgr.h"
#include "Weather.h"
#include "LFGMgr.h"
#include "DisableMgr.h"
#include "Language.h"
#include "CommandMgr.h"
#include "UpdateTime.h"
#include "GameTime.h"
#include "ScheduledExit.h"
#include <iostream>

WorldSession* World::FindSession(uint32 id) const
{
    SessionMap::const_iterator itr = m_sessions.find(id);

    if (itr != m_sessions.end())
    {
        return itr->second;
    }
    else
    {
        return nullptr;
    }
}

bool World::RemoveSession(uint32 id)
{

    SessionMap::const_iterator itr = m_sessions.find(id);

    if (itr != m_sessions.end() && itr->second)
    {
        if (itr->second->PlayerLoading())
        {
            return false;
        }
        itr->second->KickPlayer();
    }

    return true;
}

void World::AddSession(WorldSession* s)
{
    addSessQueue.add(s);
}

int32 World::GetQueuedSessionPos(WorldSession* sess)
{
    uint32 position = 1;

    for (Queue::const_iterator iter = m_QueuedSessions.begin(); iter != m_QueuedSessions.end(); ++iter, ++position)
    {
        if ((*iter) == sess)
        {
            return position;
        }
    }
    return 0;
}

void World::AddQueuedSession(WorldSession* sess)
{
    sess->SetInQueue(true);
    m_QueuedSessions.push_back(sess);

    WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4 + 1 + 4 + 4);
    packet << uint8(AUTH_WAIT_QUEUE);
    packet << uint32(0);
    packet << uint8(0);
    packet << uint32(0);
    packet << uint32(GetQueuedSessionPos(sess));
    sess->SendPacket(&packet);
}

bool World::RemoveQueuedSession(WorldSession* sess)
{

    uint32 sessions = GetActiveSessionCount();

    uint32 position = 1;
    Queue::iterator iter = m_QueuedSessions.begin();

    bool found = false;

    for (; iter != m_QueuedSessions.end(); ++iter, ++position)
    {
        if (*iter == sess)
        {
            sess->SetInQueue(false);
            iter = m_QueuedSessions.erase(iter);
            found = true;
            break;
        }
    }

    if (!found && sessions)
    {
        --sessions;
    }

    if ((!m_playerLimit || (int32)sessions < m_playerLimit) && !m_QueuedSessions.empty())
    {
        WorldSession* pop_sess = m_QueuedSessions.front();
        pop_sess->SetInQueue(false);
        pop_sess->SendAuthWaitQue(0);
        m_QueuedSessions.pop_front();

        iter = m_QueuedSessions.begin();
        position = 1;
    }

    for (; iter != m_QueuedSessions.end(); ++iter, ++position)
    {
        (*iter)->SendAuthWaitQue(position);
    }

    return found;
}
