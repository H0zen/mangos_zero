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

#include "Platform/Define.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "BattleGroundAnswers.h"
#include "Object.h"
#include "Chat.h"
#include "BattleGroundMgr.h"
#include "BattleGroundWS.h"
#include "BattleGround.h"
#include "Language.h"
#include "ScriptMgr.h"
#include "World.h"
#include "DisableMgr.h"
#include "GameTime.h"

void battlegrounds::BattlemasterHello(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    recv_data >> guid;

    DEBUG_LOG("WORLD: Received opcode CMSG_BATTLEMASTER_HELLO from %s", GuidString(guid).c_str());

    Creature* pCreature = session.GetPlayer()->GetMap()->GetCreature(guid);

    if (!pCreature)
    {
        return;
    }

    if (!pCreature->IsBattleMaster())
    {
        return;
    }

    pCreature->StopMoving();

    BattleGroundTypeId bgTypeId = sBattleGroundMgr.GetBattleMasterBG(pCreature->GetEntry());

    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
    {
        return;
    }

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bgTypeId))
    {
        session.SendNotification(LANG_BG_IS_DISABLED);
        return;
    }

    if (!session.GetPlayer()->GetBGAccessByLevel(bgTypeId))
    {

        session.SendNotification(LANG_YOUR_BG_LEVEL_REQ_ERROR);
        return;
    }

    session.SendBattlegGroundList(guid, bgTypeId);
}

void WorldSession::SendBattlegGroundList(ObjectGuid guid, BattleGroundTypeId bgTypeId)
{
    WorldPacket data;
    sBattleGroundMgr.BuildBattleGroundListPacket(&data, guid, _player, bgTypeId);
    SendPacket(&data);
}

void battlegrounds::BattlemasterJoin(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    uint32 instanceId;
    uint32 mapId;
    uint8 joinAsGroup;
    bool isPremade = false;
    Group* grp;

    recv_data >> guid;
    recv_data >> mapId;
    recv_data >> instanceId;
    recv_data >> joinAsGroup;

    BattleGroundTypeId bgTypeId = GetBattleGroundTypeIdByMapId(mapId);

    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
    {
        sLog.outError("Battleground: invalid bgtype (%u) received. possible cheater? player guid %u", bgTypeId, who.GetGUIDLow());
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_BATTLEMASTER_JOIN from %s", GuidString(guid).c_str());

    BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(bgTypeId);

    if (who.Battle().InOne())
    {
        return;
    }

    Creature* unit = who.GetMap()->GetCreature(guid);
    if (!unit)
    {
        return;
    }

    if (!unit->IsBattleMaster())
    {
        return;
    }

    BattleGround* bg = nullptr;
    if (instanceId)
    {
        bg = sBattleGroundMgr.GetBattleGroundThroughClientInstance(instanceId, bgTypeId);
    }

    if (!bg && !(bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId)))
    {
        sLog.outError("Battleground: no available bg / template found");
        return;
    }

    BattleGroundBracketId bgBracketId = who.GetBattleGroundBracketIdFromLevel(bgTypeId);

    if (!joinAsGroup)
    {

        if (!who.Battle().MayJoin())
        {
            WorldPacket data(SMSG_GROUP_JOINED_BATTLEGROUND, 4);
            data << uint32(0xFFFFFFFE);
            who.GetSession()->SendPacket(&data);
            return;
        }

        if (who.Queues().SlotOf(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        {

            return;
        }

        if (!who.Queues().AnyFree())
        {
            return;
        }
    }
    else
    {
        grp = who.GetGroup();

        if (!grp)
        {
            return;
        }
        uint32 err = grp->CanJoinBattleGroundQueue(bgTypeId, bgQueueTypeId, 0, bg->GetMaxPlayersPerTeam());
        isPremade = sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH) &&
            (grp->GetMembersCount() >= bg->GetMinPlayersPerTeam());
        if (err != BG_JOIN_ERR_OK)
        {
            who.GetSession()->SendBattleGroundJoinError(err);
            return;
        }
    }

    BattleGroundQueue& bgQueue = sBattleGroundMgr.m_BattleGroundQueues[bgQueueTypeId];
    if (joinAsGroup)
    {
        DEBUG_LOG("Battleground: the following players are joining as group:");
        GroupQueueInfo* ginfo = bgQueue.AddGroup(&who, grp, bgTypeId, bgBracketId, isPremade);
        uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, who.GetBattleGroundBracketIdFromLevel(bgTypeId));
        for (GroupReference* itr = grp->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->getSource();
            if (!member)
            {
                continue;
            }

            uint32 queueSlot = member->Queues().Take(bgQueueTypeId);

            member->Battle().RecordTheWayBack(&who);

            WorldPacket data;

            sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0);
            member->GetSession()->SendPacket(&data);
            sBattleGroundMgr.BuildGroupJoinedBattlegroundPacket(&data, int32(bg->GetMapId()));
            member->GetSession()->SendPacket(&data);
            DEBUG_LOG("Battleground: player joined queue for bg queue type %u bg type %u: GUID %u, NAME %s", bgQueueTypeId, bgTypeId, member->GetGUIDLow(), member->GetName());
        }
        DEBUG_LOG("Battleground: group end");
    }
    else
    {
        GroupQueueInfo* ginfo = bgQueue.AddGroup(&who, nullptr, bgTypeId, bgBracketId, isPremade);
        uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo, who.GetBattleGroundBracketIdFromLevel(bgTypeId));

        uint32 queueSlot = who.Queues().Take(bgQueueTypeId);

        who.Battle().RecordTheWayBack();

        WorldPacket data;

        sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0);
        who.GetSession()->SendPacket(&data);
        DEBUG_LOG("Battleground: player joined queue for bg queue type %u bg type %u: GUID %u, NAME %s", bgQueueTypeId, bgTypeId, who.GetGUIDLow(), who.GetName());
    }
    sBattleGroundMgr.ScheduleQueueUpdate(bgQueueTypeId, bgTypeId, who.GetBattleGroundBracketIdFromLevel(bgTypeId));
}

void WorldSession::HandleBattleGroundPlayerPositionsOpcode(WorldPacket & )
{

    DEBUG_LOG("WORLD: Received opcode MSG_BATTLEGROUND_PLAYER_POSITIONS");

    BattleGround* bg = _player->Battle().Ground();
    if (!bg)
    {
        return;
    }

    switch (bg->GetTypeID())
    {
        case BATTLEGROUND_WS:
        {
            uint32 flagCarrierCount = 0;

            Player* flagCarrierAlliance = sObjectMgr.GetPlayer(((BattleGroundWS*)bg)->GetAllianceFlagCarrierGuid());
            if (flagCarrierAlliance)
            {
                ++flagCarrierCount;
            }

            Player* flagCarrierHorde = sObjectMgr.GetPlayer(((BattleGroundWS*)bg)->GetHordeFlagCarrierGuid());
            if (flagCarrierHorde)
            {
                ++flagCarrierCount;
            }

            WorldPacket data(MSG_BATTLEGROUND_PLAYER_POSITIONS, 4 + 4 + 16 * flagCarrierCount);
            data << uint32(0);
            data << uint32(flagCarrierCount);

            if (flagCarrierAlliance)
            {
                data << flagCarrierAlliance->GetObjectGuid();
                data << float(flagCarrierAlliance->Where().X());
                data << float(flagCarrierAlliance->Where().Y());
            }
            if (flagCarrierHorde)
            {
                data << flagCarrierHorde->GetObjectGuid();
                data << float(flagCarrierHorde->Where().X());
                data << float(flagCarrierHorde->Where().Y());
            }

            SendPacket(&data);
            break;
        }
        case BATTLEGROUND_AB:
        case BATTLEGROUND_AV:
        {

            WorldPacket data(MSG_BATTLEGROUND_PLAYER_POSITIONS, 4 + 4);
            data << uint32(0);
            data << uint32(0);
            SendPacket(&data);
            break;
        }
        default:

            break;
    }
}

void WorldSession::HandlePVPLogDataOpcode(WorldPacket & )
{
    DEBUG_LOG("WORLD: Received opcode MSG_PVP_LOG_DATA");

    BattleGround* bg = _player->Battle().Ground();
    if (!bg)
    {
        return;
    }

    WorldPacket data;
    sBattleGroundMgr.BuildPvpLogDataPacket(&data, bg);
    SendPacket(&data);

    DEBUG_LOG("WORLD: Sent MSG_PVP_LOG_DATA Message");
}

void battlegrounds::BattlefieldList(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_BATTLEFIELD_LIST");

    uint32 mapId;
    recv_data >> mapId;

    BattleGroundTypeId bgTypeId = GetBattleGroundTypeIdByMapId(mapId);

    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
    {
        sLog.outError("Battleground: invalid bgtype received.");
        return;
    }

    WorldPacket data;
    sBattleGroundMgr.BuildBattleGroundListPacket(&data, who.GetObjectGuid(), &who, BattleGroundTypeId(bgTypeId));
    who.GetSession()->SendPacket(&data);
}

void battlegrounds::BattleFieldPort(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_BATTLEFIELD_PORT");

    uint8 action;
    uint32 mapId;

    recv_data >> mapId >> action;

    BattleGroundTypeId bgTypeId = GetBattleGroundTypeIdByMapId(mapId);

    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
    {
        sLog.outError("BattlegroundHandler: invalid bg map (%u) received.", mapId);
        return;
    }
    if (!who.Queues().AnyHeld())
    {
        sLog.outError("BattlegroundHandler: Invalid CMSG_BATTLEFIELD_PORT received from player (%u), he is not in bg_queue.", who.GetGUIDLow());
        return;
    }

    BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(bgTypeId);
    BattleGroundQueue& bgQueue = sBattleGroundMgr.m_BattleGroundQueues[bgQueueTypeId];

    GroupQueueInfo ginfo;
    if (!bgQueue.GetPlayerGroupInfoData(who.GetObjectGuid(), &ginfo))
    {
        sLog.outError("BattlegroundHandler: itrplayerstatus not found.");
        return;
    }

    if (!ginfo.IsInvitedToBGInstanceGUID && action == 1)
    {
        sLog.outError("BattlegroundHandler: instance not found.");
        return;
    }

    BattleGround* bg = sBattleGroundMgr.GetBattleGround(ginfo.IsInvitedToBGInstanceGUID, bgTypeId);

    if (!bg && action == 0)
    {
        bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
    }
    if (!bg)
    {
        sLog.outError("BattlegroundHandler: bg_template not found for type id %u.", bgTypeId);
        return;
    }

    if (action == 1)
    {

        if (!who.Battle().MayJoin())
        {

            WorldPacket data2(SMSG_GROUP_JOINED_BATTLEGROUND, 4);
            data2 << uint32(0xFFFFFFFE);
            who.GetSession()->SendPacket(&data2);
            action = 0;
            DEBUG_LOG("Battleground: player %s (%u) has a deserter debuff, do not port him to battleground!", who.GetName(), who.GetGUIDLow());
        }

        if (who.getLevel() > bg->GetMaxLevel())
        {
            sLog.outError("Battleground: Player %s (%u) has level (%u) higher than maxlevel (%u) of battleground (%u)! Do not port him to battleground!",
                who.GetName(), who.GetGUIDLow(), who.getLevel(), bg->GetMaxLevel(), bg->GetTypeID());
            action = 0;
        }
    }
    uint32 queueSlot = who.Queues().SlotOf(bgQueueTypeId);
    WorldPacket data;
    switch (action)
    {
        case 1:
            if (!who.Queues().Called(bgQueueTypeId))
            {
                return;
            }

            if (!who.IsAlive())
            {
                who.ResurrectPlayer(1.0f);
                who.SpawnCorpseBones();
            }

            if (who.IsTaxiFlying())
            {
                who.GetMotionMaster()->MovementExpired();
                who.m_taxi.ClearTaxiDestinations();
            }

            sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_IN_PROGRESS, 0, bg->GetStartTime());
            who.GetSession()->SendPacket(&data);

            bgQueue.RemovePlayer(who.GetObjectGuid(), false);

            if (BattleGround* currentBg = who.Battle().Ground())
            {
                currentBg->RemovePlayerAtLeave(who.GetObjectGuid(), false, true);
            }

            who.Battle().In(bg->GetInstanceID(), bgTypeId);

            who.Battle().Side(ginfo.GroupTeam);

            sBattleGroundMgr.SendToBattleGround(&who, ginfo.IsInvitedToBGInstanceGUID, bgTypeId);

            DEBUG_LOG("Battleground: player %s (%u) joined battle for bg %u, bgtype %u, queue type %u.", who.GetName(), who.GetGUIDLow(), bg->GetInstanceID(), bg->GetTypeID(), bgQueueTypeId);
            break;
        case 0:
            who.Queues().Give(bgQueueTypeId);
            sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_NONE, 0, 0);
            bgQueue.RemovePlayer(who.GetObjectGuid(), true);

            sBattleGroundMgr.ScheduleQueueUpdate(bgQueueTypeId, bgTypeId, who.GetBattleGroundBracketIdFromLevel(bgTypeId));
            who.GetSession()->SendPacket(&data);
            DEBUG_LOG("Battleground: player %s (%u) left queue for bgtype %u, queue type %u.", who.GetName(), who.GetGUIDLow(), bg->GetTypeID(), bgQueueTypeId);
            break;
        default:
            sLog.outError("Battleground port: unknown action %u", action);
            break;
    }
}

void battlegrounds::LeaveBattlefield(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_LEAVE_BATTLEFIELD");

    recv_data.read_skip<uint8>();
    recv_data.read_skip<uint8>();
    recv_data.read_skip<uint16>();

    if (who.IsInCombat())
    {
        if (BattleGround* bg = who.Battle().Ground())
        {
            if (bg->GetStatus() != STATUS_WAIT_LEAVE)
            {
                return;
            }
        }
    }

    who.Battle().Leave();
}

void WorldSession::HandleBattlefieldStatusOpcode(WorldPacket & )
{

    DEBUG_LOG("WORLD: Battleground status");

    WorldPacket data;

    BattleGround* bg;
    for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
    {
        BattleGroundQueueTypeId bgQueueTypeId = _player->Queues().Kind(i);
        if (!bgQueueTypeId)
        {
            continue;
        }

        BattleGroundTypeId bgTypeId = BattleGroundMgr::BGTemplateId(bgQueueTypeId);
        if (bgTypeId == _player->Battle().Kind())
        {
            bg = _player->Battle().Ground();

            if (bg)
            {

                sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, i, STATUS_IN_PROGRESS, bg->GetEndTime(), bg->GetStartTime());
                SendPacket(&data);
                continue;
            }
        }

        BattleGroundQueue& bgQueue = sBattleGroundMgr.m_BattleGroundQueues[bgQueueTypeId];
        GroupQueueInfo ginfo;
        if (!bgQueue.GetPlayerGroupInfoData(_player->GetObjectGuid(), &ginfo))
        {
            continue;
        }
        if (ginfo.IsInvitedToBGInstanceGUID)
        {
            bg = sBattleGroundMgr.GetBattleGround(ginfo.IsInvitedToBGInstanceGUID, bgTypeId);
            if (!bg)
            {
                continue;
            }
            uint32 remainingTime = getMSTimeDiff(GameTime::GetGameTimeMS(), ginfo.RemoveInviteTime);

            sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, i, STATUS_WAIT_JOIN, remainingTime, 0);
            SendPacket(&data);
        }
        else
        {
            bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
            if (!bg)
            {
                continue;
            }

            uint32 avgTime = bgQueue.GetAverageQueueWaitTime(&ginfo, _player->GetBattleGroundBracketIdFromLevel(bgTypeId));

            sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, i, STATUS_WAIT_QUEUE, avgTime, getMSTimeDiff(ginfo.JoinTime, GameTime::GetGameTimeMS()));
            SendPacket(&data);
        }
    }
}

void battlegrounds::AreaSpiritHealerQuery(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_AREA_SPIRIT_HEALER_QUERY");

    BattleGround* bg = who.Battle().Ground();
    if (!bg)
    {
        return;
    }

    ObjectGuid guid = 0;
    recv_data >> guid;

    Creature* unit = who.GetMap()->GetCreature(guid);
    if (!unit)
    {
        return;
    }

    if (!unit->IsSpiritService())
    {
        return;
    }

    unit->SendAreaSpiritHealerQueryOpcode(&who);
}

void battlegrounds::AreaSpiritHealerQueue(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_AREA_SPIRIT_HEALER_QUEUE");

    BattleGround* bg = who.Battle().Ground();
    if (!bg)
    {
        return;
    }

    ObjectGuid guid = 0;
    recv_data >> guid;

    Creature* unit = who.GetMap()->GetCreature(guid);
    if (!unit)
    {
        return;
    }

    if (!unit->IsSpiritService())
    {
        return;
    }

    sScriptMgr.OnGossipHello(&who, unit);
}

void WorldSession::SendBattleGroundJoinError(uint8 err)
{
    WorldPacket data;
    int32 msg;
    switch (err)
    {
        case BG_JOIN_ERR_OFFLINE_MEMBER:
            msg = LANG_BG_GROUP_OFFLINE_MEMBER;
            break;
        case BG_JOIN_ERR_GROUP_TOO_MANY:
            msg = LANG_BG_GROUP_TOO_LARGE;
            break;
        case BG_JOIN_ERR_MIXED_FACTION:
            msg = LANG_BG_GROUP_MIXED_FACTION;
            break;
        case BG_JOIN_ERR_MIXED_LEVELS:
            msg = LANG_BG_GROUP_MIXED_LEVELS;
            break;
        case BG_JOIN_ERR_GROUP_MEMBER_ALREADY_IN_QUEUE:
            msg = LANG_BG_GROUP_MEMBER_ALREADY_IN_QUEUE;
            break;
        case BG_JOIN_ERR_GROUP_DESERTER:
            msg = LANG_BG_GROUP_MEMBER_DESERTER;
            break;
        case BG_JOIN_ERR_ALL_QUEUES_USED:
            msg = LANG_BG_GROUP_MEMBER_NO_FREE_QUEUE_SLOTS;
            break;
        case BG_JOIN_ERR_GROUP_NOT_ENOUGH:

        default:
            return;
            break;
    }
    ChatHandler::BuildChatPacket(data, CHAT_MSG_BG_SYSTEM_NEUTRAL, GetMangosString(msg), LANG_UNIVERSAL);
    SendPacket(&data);
    return;
}
