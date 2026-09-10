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
#include <string>
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "GroupAnswers.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Group.h"
#include "SocialMgr.h"
#include "Util.h"
#include "PlayerRegistry.h"

void WorldSession::SendPartyResult(PartyOperation operation, const std::string& member, PartyResult res)
{
    WorldPacket data(SMSG_PARTY_COMMAND_RESULT, (4 + member.size() + 1 + 4));
    data << uint32(operation);
    data << member;
    data << uint32(res);

    SendPacket(&data);
}

void groups::GroupInvite(Player& who, WorldPacket& recv_data)
{
    std::string membername;
    recv_data >> membername;

    if (!normalizePlayerName(membername))
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_BAD_PLAYER_NAME_S);
        return;
    }

    Player* player = sObjectMgr.GetPlayer(membername.c_str());

    if (!player || player == &who)
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_BAD_PLAYER_NAME_S);
        return;
    }

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP) && who.GetTeam() != player->GetTeam())
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_PLAYER_WRONG_FACTION);
        return;
    }

    if (who.GetInstanceId() != 0 && player->GetInstanceId() != 0 && who.GetInstanceId() != player->GetInstanceId() && who.GetMapId() == player->GetMapId())
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_ALREADY_IN_GROUP_S);
        return;
    }

    if (player->GetSocial()->HasIgnore(who.GetObjectGuid()))
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_IGNORING_YOU_S);
        return;
    }

    Group* group = who.GetGroup();
    if (group && group->isBGGroup())
    {
        group = who.GetOriginalGroup();
    }

    Group* group2 = player->GetGroup();
    if (group2 && group2->isBGGroup())
    {
        group2 = player->GetOriginalGroup();
    }

    if (group2 || player->Invites().ToParty())
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_ALREADY_IN_GROUP_S);
        return;
    }

    if (group)
    {

        if (!group->IsLeader(who.GetObjectGuid()) && !group->IsAssistant(who.GetObjectGuid()))
        {
            who.GetSession()->SendPartyResult(PARTY_OP_INVITE, "", ERR_NOT_LEADER);
            return;
        }

        if (group->IsFull())
        {
            who.GetSession()->SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
            return;
        }
    }

    if (!group)
    {
        group = new Group;

        if (!group->AddLeaderInvite(&who))
        {
            delete group;
            return;
        }
        if (!group->AddInvite(player))
        {
            delete group;
            return;
        }
    }
    else
    {

        if (!group->AddInvite(player))
        {
            return;
        }
    }

    WorldPacket data(SMSG_GROUP_INVITE, 10);
    data << who.GetName();
    player->GetSession()->SendPacket(&data);

    who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_PARTY_RESULT_OK);
}

void groups::GroupAccept(Player& who, WorldPacket& )
{
    Group* group = who.Invites().ToParty();
    if (!group)
    {
        return;
    }

    if (group->GetLeaderGuid() == who.GetObjectGuid())
    {
        sLog.outError("HandleGroupAcceptOpcode: %s tried to accept an invite to his own group",
            who.GetGuidStr().c_str());
        return;
    }

    group->RemoveInvite(&who);

    if (group->IsFull())
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
        return;
    }

    Player* leader = sObjectMgr.GetPlayer(group->GetLeaderGuid());

    if (!group->IsCreated())
    {
        if (leader)
        {
            group->RemoveInvite(leader);
        }
        if (group->Create(group->GetLeaderGuid(), group->GetLeaderName()))
        {
            sObjectMgr.AddGroup(group);
        }
        else
        {
            return;
        }
    }

    if (!group->AddMember(who.GetObjectGuid(), who.GetName()))
    {
        return;
    }
}

void groups::GroupDecline(Player& who, WorldPacket& )
{
    Group*  group  = who.Invites().ToParty();
    if (!group)
    {
        return;
    }

    Player* leader = sObjectMgr.GetPlayer(group->GetLeaderGuid());

    who.UninviteFromGroup();

    if (!leader || !leader->GetSession())
    {
        return;
    }

    WorldPacket data(SMSG_GROUP_DECLINE, 10);
    data << who.GetName();
    leader->GetSession()->SendPacket(&data);
}

void groups::GroupUninviteGuid(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    recv_data >> guid;

    if (guid == who.GetObjectGuid())
    {
        sLog.outError("WorldSession::HandleGroupUninviteGuidOpcode: leader %s tried to uninvite himself from the group.", who.GetGuidStr().c_str());
        return;
    }

    PartyResult res = who.CanUninviteFromGroup();
    if (res != ERR_PARTY_RESULT_OK)
    {
        who.GetSession()->SendPartyResult(PARTY_OP_LEAVE, "", res);
        return;
    }

    Group* grp = who.GetGroup();
    if (!grp)
    {
        return;
    }

    if (grp->IsMember(guid))
    {
        Player::RemoveFromGroup(grp, guid, GROUP_KICK);
        return;
    }

    if (Player* plr = grp->GetInvited(guid))
    {
        plr->UninviteFromGroup();
        return;
    }

    who.GetSession()->SendPartyResult(PARTY_OP_LEAVE, "", ERR_TARGET_NOT_IN_GROUP_S);
}

void groups::GroupUninvite(Player& who, WorldPacket& recv_data)
{
    std::string membername;
    recv_data >> membername;

    if (!normalizePlayerName(membername))
    {
        return;
    }

    if (who.GetName() == membername)
    {
        sLog.outError("WorldSession::HandleGroupUninviteOpcode: leader %s tried to uninvite himself from the group.", who.GetGuidStr().c_str());
        return;
    }

    PartyResult res = who.CanUninviteFromGroup();
    if (res != ERR_PARTY_RESULT_OK)
    {
        who.GetSession()->SendPartyResult(PARTY_OP_LEAVE, "", res);
        return;
    }

    Group* grp = who.GetGroup();
    if (!grp)
    {
        return;
    }

    if (ObjectGuid guid = grp->GetMemberGuid(membername))
    {
        Player::RemoveFromGroup(grp, guid, GROUP_KICK);
        return;
    }

    if (Player* plr = grp->GetInvited(membername))
    {
        plr->UninviteFromGroup();
        return;
    }

    who.GetSession()->SendPartyResult(PARTY_OP_LEAVE, membername, ERR_TARGET_NOT_IN_GROUP_S);
}

void groups::GroupSetLeader(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    recv_data >> guid;

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    Player* player = sObjectMgr.GetPlayer(guid);

    if (!player || !group->IsLeader(who.GetObjectGuid()) || player->GetGroup() != group)
    {
        return;
    }

    group->ChangeLeader(guid);
}

void groups::GroupDisband(Player& who, WorldPacket& )
{
    if (!who.GetGroup())
    {
        return;
    }

    if (who.Battle().InOne())
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, "", ERR_NOT_LEADER);
        return;
    }

    who.GetSession()->SendPartyResult(PARTY_OP_LEAVE, who.GetName(), ERR_PARTY_RESULT_OK);

    who.RemoveFromGroup();
}

void groups::LootRules(Player& who, WorldPacket& recv_data)
{
    uint32 lootMethod;
    ObjectGuid lootMaster = 0;
    uint32 lootThreshold;
    recv_data >> lootMethod >> lootMaster >> lootThreshold;

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    if (!group->IsLeader(who.GetObjectGuid()))
    {
        return;
    }

    group->SetLootMethod((LootMethod)lootMethod);
    group->SetLooterGuid(lootMaster);
    group->SetLootThreshold((ItemQualities)lootThreshold);
    group->SendUpdate();
}

void groups::LootRoll(Player& who, WorldPacket& recv_data)
{
    ObjectGuid lootedTarget = 0;
    uint32 itemSlot;
    uint8  rollType;
    recv_data >> lootedTarget;
    recv_data >> itemSlot;
    recv_data >> rollType;

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    if (rollType >= MAX_ROLL_FROM_CLIENT)
    {
        return;
    }

    group->CountRollVote(&who, lootedTarget, itemSlot, RollVote(rollType));
}

void groups::MinimapPing(Player& who, WorldPacket& recv_data)
{
    float x, y;
    recv_data >> x;
    recv_data >> y;

    if (!who.GetGroup())
    {
        return;
    }

    WorldPacket data(MSG_MINIMAP_PING, (8 + 4 + 4));
    data << who.GetObjectGuid();
    data << float(x);
    data << float(y);
    who.GetGroup()->BroadcastPacket(&data, true, -1, who.GetObjectGuid());
}

void groups::RandomRoll(Player& who, WorldPacket& recv_data)
{
    uint32 minimum, maximum, roll;
    recv_data >> minimum;
    recv_data >> maximum;

    if (minimum > maximum || maximum > 10000)
    {
        return;
    }

    roll = urand(minimum, maximum);

    WorldPacket data(MSG_RANDOM_ROLL, 4 + 4 + 4 + 8);
    data << uint32(minimum);
    data << uint32(maximum);
    data << uint32(roll);
    data << who.GetObjectGuid();
    if (who.GetGroup())
    {
        who.GetGroup()->BroadcastPacket(&data, false);
    }
    else
    {
        who.GetSession()->SendPacket(&data);
    }
}

void groups::RaidTargetUpdate(Player& who, WorldPacket& recv_data)
{
    uint8  x;
    recv_data >> x;

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    if (x == 0xFF)
    {
        group->SendTargetIconList(who.GetSession());
    }
    else
    {
        if (!group->IsLeader(who.GetObjectGuid()) &&
            !group->IsAssistant(who.GetObjectGuid()))
        {
            return;
        }

        ObjectGuid guid = 0;
        recv_data >> guid;
        group->SetTargetIcon(x, guid);
    }
}

void groups::GroupRaidConvert(Player& who, WorldPacket& )
{
    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    if (who.Battle().InOne())
    {
        return;
    }

    if (!group->IsLeader(who.GetObjectGuid()) || group->GetMembersCount() < 2)
    {
        return;
    }

    who.GetSession()->SendPartyResult(PARTY_OP_INVITE, "", ERR_PARTY_RESULT_OK);
    group->ConvertToRaid();
}

void groups::GroupChangeSubGroup(Player& who, WorldPacket& recv_data)
{
    std::string name;
    uint8 groupNr;
    recv_data >> name;

    recv_data >> groupNr;

    if (groupNr >= MAX_RAID_SUBGROUPS)
    {
        return;
    }

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    if (!group->IsLeader(who.GetObjectGuid()) &&
        !group->IsAssistant(who.GetObjectGuid()))
    {
        return;
    }

    if (!group->HasFreeSlotSubGroup(groupNr))
    {
        return;
    }

    if (Player* player = sObjectMgr.GetPlayer(name.c_str()))
    {
        group->ChangeMembersGroup(player, groupNr);
    }
    else
    {
        if (ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(name.c_str()))
        {
            group->ChangeMembersGroup(guid, groupNr);
        }
    }
}

void groups::GroupAssistantLeader(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    uint8 flag;
    recv_data >> guid;
    recv_data >> flag;

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    if (!group->IsLeader(who.GetObjectGuid()))
    {
        return;
    }

    group->SetAssistant(guid, (flag == 0 ? false : true));
}

void groups::PartyAssignment(Player& who, WorldPacket& recv_data)
{
    uint8 flag1, flag2;
    ObjectGuid guid = 0;
    recv_data >> flag1 >> flag2;
    recv_data >> guid;

    DEBUG_LOG("MSG_PARTY_ASSIGNMENT");

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    if (!group->IsLeader(who.GetObjectGuid()))
    {
        return;
    }

    if (flag1 == 1)
    {
        group->SetMainAssistant(guid);
    }
    if (flag2 == 1)
    {
        group->SetMainTank(guid);
    }
}

void groups::RaidReadyCheck(Player& who, WorldPacket& recv_data)
{
    if (recv_data.empty())
    {
        Group* group = who.GetGroup();
        if (!group)
        {
            return;
        }

        if (!group->IsLeader(who.GetObjectGuid()) &&
            !group->IsAssistant(who.GetObjectGuid()))
        {
            return;
        }

        WorldPacket data(MSG_RAID_READY_CHECK, 0);
        group->BroadcastPacket(&data, false, -1, who.GetObjectGuid());

        group->OfflineReadyCheck();
    }
    else
    {
        uint8 state;
        recv_data >> state;

        Group* group = who.GetGroup();
        if (!group)
        {
            return;
        }

        WorldPacket data(MSG_RAID_READY_CHECK, 9);
        data << who.GetObjectGuid();
        data << uint8(state);
        group->BroadcastReadyCheck(&data);
    }
}

void groups::RaidReadyCheckFinished(Player& who, WorldPacket& )
{

}

void WorldSession::BuildPartyMemberStatsChangedPacket(Player* player, WorldPacket* data)
{
    uint32 mask = player->GetGroupUpdateFlag();

    if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)
    {
        mask |= (GROUP_UPDATE_FLAG_CUR_POWER | GROUP_UPDATE_FLAG_MAX_POWER);
    }

    if (mask & GROUP_UPDATE_FLAG_PET_POWER_TYPE)
    {
        mask |= (GROUP_UPDATE_FLAG_PET_CUR_POWER | GROUP_UPDATE_FLAG_PET_MAX_POWER);
    }

    uint32 byteCount = 0;
    for (int i = 0; i < GROUP_UPDATE_FLAGS_COUNT; ++i)
    {
        if (mask & (1 << i))
        {
            byteCount += GroupUpdateLength[i];
        }
    }

    data->Initialize(SMSG_PARTY_MEMBER_STATS, 8 + 4 + byteCount);
    *data << player->GetPackGUID();
    *data << uint32(mask);

    if (mask & GROUP_UPDATE_FLAG_STATUS)
    {
        if (player)
        {
            if (player->IsPvP())
            {
                *data << uint8(MEMBER_STATUS_ONLINE | MEMBER_STATUS_PVP);
            }
            else
            {
                *data << uint8(MEMBER_STATUS_ONLINE);
            }
        }
        else
        {
            *data << uint8(MEMBER_STATUS_OFFLINE);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_CUR_HP)
    {
        *data << uint16(player->GetHealth());
    }

    if (mask & GROUP_UPDATE_FLAG_MAX_HP)
    {
        *data << uint16(player->GetMaxHealth());
    }

    Powers powerType = player->GetPowerType();
    if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)
    {
        *data << uint8(powerType);
    }

    if (mask & GROUP_UPDATE_FLAG_CUR_POWER)
    {
        *data << uint16(player->GetPower(powerType));
    }

    if (mask & GROUP_UPDATE_FLAG_MAX_POWER)
    {
        *data << uint16(player->GetMaxPower(powerType));
    }

    if (mask & GROUP_UPDATE_FLAG_LEVEL)
    {
        *data << uint16(player->getLevel());
    }

    if (mask & GROUP_UPDATE_FLAG_ZONE)
    {
        *data << uint16(player->GetTerrain()->GetZoneId(player->Where().X(), player->Where().Y(), player->Where().Z()));
    }

    if (mask & GROUP_UPDATE_FLAG_POSITION)
    {
        *data << uint16(player->Where().X()) << uint16(player->Where().Y());
    }

    if (mask & GROUP_UPDATE_FLAG_AURAS)
    {
        const uint64& auramask = player->GetAuraUpdateMask();
        *data << uint32(auramask);

        for (uint32 i = 0; i < MAX_POSITIVE_AURAS; ++i)
        {
            if (auramask & (uint64(1) << i))
            {
                *data << uint16(player->GetUInt32Value(UNIT_FIELD_AURA + i));
            }
        }
    }

    Pet* pet = player->GetPet();
    if (mask & GROUP_UPDATE_FLAG_PET_GUID)
    {
        *data << (pet ? pet->GetObjectGuid() : 0);
    }

    if (mask & GROUP_UPDATE_FLAG_PET_NAME)
    {
        if (pet)
        {
            *data << pet->GetName();
        }
        else
        {
            *data << uint8(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_MODEL_ID)
    {
        if (pet)
        {
            *data << uint16(pet->GetDisplayId());
        }
        else
        {
            *data << uint16(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_CUR_HP)
    {
        if (pet)
        {
            *data << uint16(pet->GetHealth());
        }
        else
        {
            *data << uint16(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_MAX_HP)
    {
        if (pet)
        {
            *data << uint16(pet->GetMaxHealth());
        }
        else
        {
            *data << uint16(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_POWER_TYPE)
    {
        if (pet)
        {
            *data << uint8(pet->GetPowerType());
        }
        else
        {
            *data << uint8(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_CUR_POWER)
    {
        if (pet)
        {
            *data << uint16(pet->GetPower(pet->GetPowerType()));
        }
        else
        {
            *data << uint16(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_MAX_POWER)
    {
        if (pet)
        {
            *data << uint16(pet->GetMaxPower(pet->GetPowerType()));
        }
        else
        {
            *data << uint16(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_AURAS)
    {
        if (pet)
        {
            const uint64& auramask = pet->GetAuraUpdateMask();
            *data << uint32(auramask);

            for (uint32 i = 0; i < MAX_POSITIVE_AURAS; ++i)
            {
                if (auramask & (uint64(1) << i))
                {
                    *data << uint16(pet->GetUInt32Value(UNIT_FIELD_AURA + i));
                }
            }
        }
        else
        {
            *data << uint32(0);
        }
    }
}

void groups::RequestPartyMemberStats(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REQUEST_PARTY_MEMBER_STATS");
    ObjectGuid guid = 0;
    recv_data >> guid;

    Player* player = sPlayerRegistry.Find(guid, false);
    if (!player)
    {
        WorldPacket data(SMSG_PARTY_MEMBER_STATS_FULL, 3 + 4 + 1);
        data << PackGuid(guid);
        data << uint32(GROUP_UPDATE_FLAG_STATUS);
        data << uint8(MEMBER_STATUS_OFFLINE);
        who.GetSession()->SendPacket(&data);
        return;
    }

    Pet* pet = player->GetPet();

    WorldPacket data(SMSG_PARTY_MEMBER_STATS_FULL, 3 + 4 + 1 + (23 + 12 * 2) + (pet ? (29 + 8 * 2) : 0));
    data << player->GetPackGUID();

    uint32 mask1 = GROUP_UPDATE_PLAYER;
    if (pet)
    {
        mask1 |= GROUP_UPDATE_PET;
    }

    Powers powerType = player->GetPowerType();
    data << uint32(mask1);
    data << uint8(MEMBER_STATUS_ONLINE);
    data << uint16(player->GetHealth());
    data << uint16(player->GetMaxHealth());
    data << uint8(powerType);
    data << uint16(player->GetPower(powerType));
    data << uint16(player->GetMaxPower(powerType));
    data << uint16(player->getLevel());

    uint16 iZoneId = 0;
    uint16 iCoordX = 0;
    uint16 iCoordY = 0;

    if (player->IsInWorld())
    {
        iZoneId = player->GetTerrain()->GetZoneId(player->Where().X(), player->Where().Y(), player->Where().Z());
        iCoordX = player->Where().X();
        iCoordY = player->Where().Y();
    }
    else if (player->IsBeingTeleported())
    {
        Geometry::Placement& loc = player->GetTeleportDest();
        iZoneId = sTerrainMgr.GetZoneId(loc.MapId(), loc.X(), loc.Y(), loc.Z());
        iCoordX = loc.X();
        iCoordY = loc.Y();
    }
    else
    {

    }

    data << uint16(iZoneId);
    data << uint16(iCoordX);
    data << uint16(iCoordY);

    uint32 auramask = 0;
    size_t maskPos = data.wpos();
    data << uint32(auramask);
    for (uint8 i = 0; i < 32; ++i)
    {
        if (uint32 aura = player->GetUInt32Value(UNIT_FIELD_AURA + i))
        {
            auramask |= (uint32(1) << i);
            data << uint16(aura);
        }
    }
    data.put<uint32>(maskPos, auramask);
    uint16 auramask1 = 0;
    maskPos = data.wpos();
    data << uint16(auramask1);
    for (uint8 i = 32; i < MAX_AURAS; ++i)
    {
        if (uint32 aura = player->GetUInt32Value(UNIT_FIELD_AURA + i))
        {
            auramask1 |= (uint16(1) << (i-32));
            data << uint16(aura);
        }
    }
    data.put<uint16>(maskPos, auramask1);

    if (pet)
    {
        Powers petpowertype = pet->GetPowerType();
        data << pet->GetObjectGuid();
        data << pet->GetName();
        data << uint16(pet->GetDisplayId());
        data << uint16(pet->GetHealth());
        data << uint16(pet->GetMaxHealth());
        data << uint8(petpowertype);
        data << uint16(pet->GetPower(petpowertype));
        data << uint16(pet->GetMaxPower(petpowertype));

        uint32 petauramask = 0;
        size_t petMaskPos = data.wpos();
        data << uint32(petauramask);
        for (uint8 i = 0; i < 32; ++i)
        {
            if (uint32 petaura = pet->GetUInt32Value(UNIT_FIELD_AURA + i))
            {
                petauramask |= (uint32(1) << i);
                data << uint16(petaura);
            }
        }
        data.put<uint32>(petMaskPos, petauramask);
        uint16 petauramask1 = 0;
        petMaskPos = data.wpos();
        data << uint16(petauramask1);
        for (uint8 i = 32; i < MAX_AURAS; ++i)
        {
            if (uint32 petaura = pet->GetUInt32Value(UNIT_FIELD_AURA + i))
            {
                petauramask1 |= (uint16(1) << (i-32));
                data << uint16(petaura);
            }
        }
        data.put<uint16>(petMaskPos, petauramask1);
    }

    who.GetSession()->SendPacket(&data);
}

void groups::RequestRaidInfo(Player& who, WorldPacket& )
{

    who.Binds().TellRaidInfo();
}

void groups::OptOutOfLoot(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_OPT_OUT_OF_LOOT");

    uint32 unkn;
    recv_data >> unkn;

    if (!session.GetPlayer())
    {
        if (unkn != 0)
        {
            sLog.outError("CMSG_GROUP_PASS_ON_LOOT value<>0 for not-loaded character!");
        }
        return;
    }

    if (unkn != 0)
    {
        sLog.outError("CMSG_GROUP_PASS_ON_LOOT: activation not implemented!");
    }
}
