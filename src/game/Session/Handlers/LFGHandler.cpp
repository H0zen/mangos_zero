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

#include <set>
#include <string>
#include "Platform/Define.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "MeetingstoneAnswers.h"
#include "Object.h"
#include "Chat.h"
#include "Language.h"
#include "ScriptMgr.h"
#include "World.h"
#include "Group.h"
#include "LFGHandler.h"
#include "LFGMgr.h"

void meetingstones::MeetingStoneJoin(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;

    recv_data >> guid;

    DEBUG_LOG("WORLD: Recvd CMSG_MEETINGSTONE_JOIN Message guid: %s", GuidString(guid).c_str());

    if (!who.IsSelfMover())
    {
        return;
    }

    GameObject *obj = who.GetMap()->GetGameObject(guid);

    if (!obj)
    {
        return;
    }

    if (obj->GetGoType() != GAMEOBJECT_TYPE_MEETINGSTONE)
    {
        sLog.outError("HandleMeetingStoneJoinOpcode: CMSG_MEETINGSTONE_JOIN for not allowed GameObject type %u (Entry %u), didn't expect this to happen.", obj->GetGoType(), obj->GetEntry());
        return;
    }

    if (Group* grp = who.GetGroup())
    {
        if (!grp->IsLeader(who.GetObjectGuid()))
        {
            who.GetSession()->SendMeetingstoneFailed(MEETINGSTONE_FAIL_PARTYLEADER);

            return;
        }

        if (grp->isRaidGroup())
        {
            who.GetSession()->SendMeetingstoneFailed(MEETINGSTONE_FAIL_RAID_GROUP);
            return;
        }

        if (grp->IsFull())
        {
            who.GetSession()->SendMeetingstoneFailed(MEETINGSTONE_FAIL_FULL_GROUP);
            return;
        }
    }

    GameObjectInfo const* gInfo = ObjectMgr::GetGameObjectInfo(obj->GetEntry());

    sLFGMgr.AddToQueue(&who, gInfo->meetingstone.areaID);
}

void meetingstones::MeetingStoneLeave(Player& who, WorldPacket& )
{
    DEBUG_LOG("WORLD: Recvd CMSG_MEETINGSTONE_LEAVE");
    if (Group *grp = who.GetGroup())
    {
        if (grp->IsLeader(who.GetObjectGuid()) && grp->isInLFG())
        {
            sLFGMgr.RemoveGroupFromQueue(grp->GetId());
        }
        else
        {
            who.GetSession()->SendMeetingstoneSetqueue(0, MEETINGSTONE_STATUS_NONE);
        }
    }
    else
    {
        sLFGMgr.RemovePlayerFromQueue(who.GetObjectGuid());
    }
}

void WorldSession::HandleMeetingStoneInfoOpcode(WorldPacket & )
{
    DEBUG_LOG("WORLD: Received CMSG_MEETING_STONE_INFO");

    if (Group *grp = _player->GetGroup())
    {
        if (grp->isInLFG())
        {
            SendMeetingstoneSetqueue(grp->GetLFGAreaId(), MEETINGSTONE_STATUS_JOINED_QUEUE);
        }
        else
        {
            SendMeetingstoneSetqueue(0, MEETINGSTONE_STATUS_NONE);
        }
    }
    else
    {
        sLFGMgr.RestoreOfflinePlayer(_player->GetObjectGuid());
    }
}

void WorldSession::SendMeetingstoneFailed(uint8 status)
{
    WorldPacket data(SMSG_MEETINGSTONE_JOINFAILED, 1);
    data << uint8(status);
    SendPacket(&data);
}

void WorldSession::SendMeetingstoneSetqueue(uint32 areaid, uint8 status)
{
    WorldPacket data(SMSG_MEETINGSTONE_SETQUEUE, 5);
    data << uint32(areaid);
    data << uint8(status);
    SendPacket(&data);
}
