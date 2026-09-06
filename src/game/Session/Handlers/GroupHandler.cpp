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

/**
 * @file GroupHandler.cpp
 * @brief Group/party opcode handlers
 *
 * This file handles group-related opcodes including:
 * - CMSG_GROUP_INVITE: Invite player to group
 * - CMSG_GROUP_ACCEPT: Accept group invitation
 * - CMSG_GROUP_DECLINE: Decline group invitation
 * - CMSG_GROUP_UNINVITE: Remove member from group
 * - CMSG_GROUP_LEAVE: Leave group
 * - CMSG_GROUP_DISBAND: Disband group
 * - CMSG_GROUP_CHANGE_LEADER: Transfer leadership
 * - CMSG_GROUP_SET_LEADER: Set new leader
 * - CMSG_LOOT_METHOD: Set loot method
 * - CMSG_MINIMAP_PING: Send minimap ping
 *
 * Group operations require proper permission checks and state validation.
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

/** differences from off:
 *  - you can uninvite yourself - it is useful
 *  - you can accept invitation even if leader went offline
 */

/** todo:
 * - group_destroyed msg is sent but not shown
 * - reduce xp gaining when in raid group
 * - quest sharing has to be corrected
 * - FIX sending PartyMemberStats
 */

/**
 * @brief Sends a party operation result packet to the client.
 *
 * @param operation The party operation being reported.
 * @param member The related member name.
 * @param res The result code to send.
 */
void WorldSession::SendPartyResult(PartyOperation operation, const std::string& member, PartyResult res)
{
    WorldPacket data(SMSG_PARTY_COMMAND_RESULT, (4 + member.size() + 1 + 4));
    data << uint32(operation);
    data << member;                                         // max len 48
    data << uint32(res);

    SendPacket(&data);
}

/**
 * @brief Handles a request to invite a player into a party.
 *
 * @param recv_data The received opcode packet.
 */
void groups::GroupInvite(Player& who, WorldPacket& recv_data)
{
    std::string membername;
    recv_data >> membername;

    // attempt add selected player

    // cheating
    if (!normalizePlayerName(membername))
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_BAD_PLAYER_NAME_S);
        return;
    }

    Player* player = sObjectMgr.GetPlayer(membername.c_str());

    // no player or cheat self-invite
    if (!player || player == &who)
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_BAD_PLAYER_NAME_S);
        return;
    }

    // can't group with
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP) && who.GetTeam() != player->GetTeam())
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_PLAYER_WRONG_FACTION);
        return;
    }

    if (who.GetInstanceId() != 0 && player->GetInstanceId() != 0 && who.GetInstanceId() != player->GetInstanceId() && who.GetMapId() == player->GetMapId())
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_ALREADY_IN_GROUP_S); // error message is not so appropriated but no other option for classic
        return;
    }

    // just ignore us
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
    // player already in another group or invited
    if (group2 || player->Invites().ToParty())
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_ALREADY_IN_GROUP_S);
        return;
    }

    if (group)
    {
        // not have permissions for invite
        if (!group->IsLeader(who.GetObjectGuid()) && !group->IsAssistant(who.GetObjectGuid()))
        {
            who.GetSession()->SendPartyResult(PARTY_OP_INVITE, "", ERR_NOT_LEADER);
            return;
        }
        // not have place
        if (group->IsFull())
        {
            who.GetSession()->SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
            return;
        }
    }

    // ok, but group not exist, start a new group
    // but don't create and save the group to the DB until
    // at least one person joins
    if (!group)
    {
        group = new Group;
        // new group: if can't add then delete
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
        // already existing group: if can't add then just leave
        if (!group->AddInvite(player))
        {
            return;
        }
    }

    // ok, we do it
    WorldPacket data(SMSG_GROUP_INVITE, 10);                // guess size
    data << who.GetName();                         // max length: 48
    player->GetSession()->SendPacket(&data);

    who.GetSession()->SendPartyResult(PARTY_OP_INVITE, membername, ERR_PARTY_RESULT_OK);
}

/**
 * @brief Accepts a pending group invite.
 *
 * @param recv_data The received opcode packet.
 */
void groups::GroupAccept(Player& who, WorldPacket& /*recv_data*/)
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

    // remove in from invites in any case
    group->RemoveInvite(&who);

    /** error handling **/

    /********************/

    // not have place
    if (group->IsFull())
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
        return;
    }

    Player* leader = sObjectMgr.GetPlayer(group->GetLeaderGuid());

    // forming a new group, create it
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

    // everything is fine, do it, PLAYER'S GROUP IS SET IN ADDMEMBER!!!
    if (!group->AddMember(who.GetObjectGuid(), who.GetName()))
    {
        return;
    }
}

/**
 * @brief Declines a pending group invite.
 *
 * @param recv_data The received opcode packet.
 */
void groups::GroupDecline(Player& who, WorldPacket& /*recv_data*/)
{
    Group*  group  = who.Invites().ToParty();
    if (!group)
    {
        return;
    }

    // remember leader if online
    Player* leader = sObjectMgr.GetPlayer(group->GetLeaderGuid());

    // uninvite, group can be deleted
    who.UninviteFromGroup();

    if (!leader || !leader->GetSession())
    {
        return;
    }

    // report
    WorldPacket data(SMSG_GROUP_DECLINE, 10);               // guess size
    data << who.GetName();
    leader->GetSession()->SendPacket(&data);
}

/**
 * @brief Uninvites a group member or invitee by guid.
 *
 * @param recv_data The received opcode packet.
 */
void groups::GroupUninviteGuid(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    // can't uninvite yourself
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

/**
 * @brief Uninvites a group member or invitee by player name.
 *
 * @param recv_data The received opcode packet.
 */
void groups::GroupUninvite(Player& who, WorldPacket& recv_data)
{
    std::string membername;
    recv_data >> membername;

    // player not found
    if (!normalizePlayerName(membername))
    {
        return;
    }

    // can't uninvite yourself
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

/**
 * @brief Changes the leader of the current group.
 *
 * @param recv_data The received opcode packet.
 */
void groups::GroupSetLeader(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    Player* player = sObjectMgr.GetPlayer(guid);

    /** error handling **/
    if (!player || !group->IsLeader(who.GetObjectGuid()) || player->GetGroup() != group)
    {
        return;
    }

    /********************/

    // everything is fine, do it
    group->ChangeLeader(guid);
}

/**
 * @brief Handles a request to leave or disband the current group.
 *
 * @param recv_data The received opcode packet.
 */
void groups::GroupDisband(Player& who, WorldPacket& /*recv_data*/)
{
    if (!who.GetGroup())
    {
        return;
    }

    if (who.Battle().InOne())
    {
        who.GetSession()->SendPartyResult(PARTY_OP_INVITE, "", ERR_NOT_LEADER);  // error message is not so appropriated but no other option for classic
        return;
    }

    /** error handling **/

    /********************/

    // everything is fine, do it
    who.GetSession()->SendPartyResult(PARTY_OP_LEAVE, who.GetName(), ERR_PARTY_RESULT_OK);

    who.RemoveFromGroup();
}

/**
 * @brief Updates the group's loot rules.
 *
 * @param recv_data The received opcode packet.
 */
void groups::LootRules(Player& who, WorldPacket& recv_data)
{
    uint32 lootMethod;
    ObjectGuid lootMaster;
    uint32 lootThreshold;
    recv_data >> lootMethod >> lootMaster >> lootThreshold;

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(who.GetObjectGuid()))
    {
        return;
    }

    /********************/

    // everything is fine, do it
    group->SetLootMethod((LootMethod)lootMethod);
    group->SetLooterGuid(lootMaster);
    group->SetLootThreshold((ItemQualities)lootThreshold);
    group->SendUpdate();
}

/**
 * @brief Handles a player's loot roll choice.
 *
 * @param recv_data The received opcode packet.
 */
void groups::LootRoll(Player& who, WorldPacket& recv_data)
{
    ObjectGuid lootedTarget;
    uint32 itemSlot;
    uint8  rollType;
    recv_data >> lootedTarget;                              // guid of the item rolled
    recv_data >> itemSlot;
    recv_data >> rollType;

    // DEBUG_LOG("WORLD RECIEVE CMSG_LOOT_ROLL, From:%u, Numberofplayers:%u, rollType:%u", (uint32)Guid, NumberOfPlayers, rollType);

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    if (rollType >= MAX_ROLL_FROM_CLIENT)
    {
        return;
    }

    // everything is fine, do it, if false then some cheating problem found (result not used in pre-3.0)
    group->CountRollVote(&who, lootedTarget, itemSlot, RollVote(rollType));
}

/**
 * @brief Broadcasts a minimap ping to the player's group.
 *
 * @param recv_data The received opcode packet.
 */
void groups::MinimapPing(Player& who, WorldPacket& recv_data)
{
    float x, y;
    recv_data >> x;
    recv_data >> y;

    if (!who.GetGroup())
    {
        return;
    }

    // DEBUG_LOG("Received opcode MSG_MINIMAP_PING X: %f, Y: %f", x, y);

    /** error handling **/

    /********************/

    // everything is fine, do it
    WorldPacket data(MSG_MINIMAP_PING, (8 + 4 + 4));
    data << who.GetObjectGuid();
    data << float(x);
    data << float(y);
    who.GetGroup()->BroadcastPacket(&data, true, -1, who.GetObjectGuid());
}

/**
 * @brief Rolls a random value and broadcasts it to the party if applicable.
 *
 * @param recv_data The received opcode packet.
 */
void groups::RandomRoll(Player& who, WorldPacket& recv_data)
{
    uint32 minimum, maximum, roll;
    recv_data >> minimum;
    recv_data >> maximum;

    /** error handling **/
    if (minimum > maximum || maximum > 10000)               // < 32768 for urand call
    {
        return;
    }

    /********************/

    // everything is fine, do it
    roll = urand(minimum, maximum);

    // DEBUG_LOG("ROLL: MIN: %u, MAX: %u, ROLL: %u", minimum, maximum, roll);

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

/**
 * @brief Handles raid target icon queries and updates.
 *
 * @param recv_data The received opcode packet.
 */
void groups::RaidTargetUpdate(Player& who, WorldPacket& recv_data)
{
    uint8  x;
    recv_data >> x;

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/

    /********************/

    // everything is fine, do it
    if (x == 0xFF)                                          // target icon request
    {
        group->SendTargetIconList(who.GetSession());
    }
    else                                                    // target icon update
    {
        if (!group->IsLeader(who.GetObjectGuid()) &&
            !group->IsAssistant(who.GetObjectGuid()))
        {
            return;
        }

        ObjectGuid guid;
        recv_data >> guid;
        group->SetTargetIcon(x, guid);
    }
}

/**
 * @brief Converts the current party into a raid group.
 *
 * @param recv_data The received opcode packet.
 */
void groups::GroupRaidConvert(Player& who, WorldPacket& /*recv_data*/)
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

    /** error handling **/
    if (!group->IsLeader(who.GetObjectGuid()) || group->GetMembersCount() < 2)
    {
        return;
    }

    /********************/

    // everything is fine, do it (is it 0 (PARTY_OP_INVITE) correct code)
    who.GetSession()->SendPartyResult(PARTY_OP_INVITE, "", ERR_PARTY_RESULT_OK);
    group->ConvertToRaid();
}

/**
 * @brief Moves a raid member into another subgroup.
 *
 * @param recv_data The received opcode packet.
 */
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

    // we will get correct pointer for group here, so we don't have to check if group is BG raid
    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(who.GetObjectGuid()) &&
        !group->IsAssistant(who.GetObjectGuid()))
    {
        return;
    }

    if (!group->HasFreeSlotSubGroup(groupNr))
    {
        return;
    }

    /********************/

    // everything is fine, do it
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

/**
 * @brief Sets or clears the assistant leader flag for a raid member.
 *
 * @param recv_data The received opcode packet.
 */
void groups::GroupAssistantLeader(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint8 flag;
    recv_data >> guid;
    recv_data >> flag;

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(who.GetObjectGuid()))
    {
        return;
    }

    /********************/

    // everything is fine, do it
    group->SetAssistant(guid, (flag == 0 ? false : true));
}

/**
 * @brief Updates main tank or main assist raid assignments.
 *
 * @param recv_data The received opcode packet.
 */
void groups::PartyAssignment(Player& who, WorldPacket& recv_data)
{
    uint8 flag1, flag2;
    ObjectGuid guid;
    recv_data >> flag1 >> flag2;
    recv_data >> guid;

    DEBUG_LOG("MSG_PARTY_ASSIGNMENT");

    Group* group = who.GetGroup();
    if (!group)
    {
        return;
    }

    // if (flag1) Main Assist
    //     0x4
    // if (flag2) Main Tank
    //     0x2

    /** error handling **/
    if (!group->IsLeader(who.GetObjectGuid()))
    {
        return;
    }

    /********************/

    // everything is fine, do it
    if (flag1 == 1)
    {
        group->SetMainAssistant(guid);
    }
    if (flag2 == 1)
    {
        group->SetMainTank(guid);
    }
}

/**
 * @brief Starts or answers a raid ready check.
 *
 * @param recv_data The received opcode packet.
 */
void groups::RaidReadyCheck(Player& who, WorldPacket& recv_data)
{
    if (recv_data.empty())                                  // request
    {
        Group* group = who.GetGroup();
        if (!group)
        {
            return;
        }

        /** error handling **/
        if (!group->IsLeader(who.GetObjectGuid()) &&
            !group->IsAssistant(who.GetObjectGuid()))
        {
            return;
        }

        /********************/

        // everything is fine, do it
        WorldPacket data(MSG_RAID_READY_CHECK, 0);
        group->BroadcastPacket(&data, false, -1, who.GetObjectGuid());

        group->OfflineReadyCheck();
    }
    else                                                    // answer
    {
        uint8 state;
        recv_data >> state;

        Group* group = who.GetGroup();
        if (!group)
        {
            return;
        }

        // everything is fine, do it
        WorldPacket data(MSG_RAID_READY_CHECK, 9);
        data << who.GetObjectGuid();
        data << uint8(state);
        group->BroadcastReadyCheck(&data);
    }
}

/**
 * @brief Handles the completion of a raid ready check.
 *
 * @param recv_data The received opcode packet.
 */
void groups::RaidReadyCheckFinished(Player& who, WorldPacket& /*recv_data*/)
{
    // Group* group = who.GetGroup();
    // if (!group)
    //    return;

    // if (!group->IsLeader(who.GetGUID()) && !group->IsAssistant(who.GetGUID()))
    //    return;

    // Is any reaction need?
}

/**
 * @brief Builds a party member stats update packet.
 *
 * @param player The player whose stats are being serialized.
 * @param data The packet receiving the serialized fields.
 */
void WorldSession::BuildPartyMemberStatsChangedPacket(Player* player, WorldPacket* data)
{
    uint32 mask = player->GetGroupUpdateFlag();

    if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)                // if update power type, update current/max power also
    {
        mask |= (GROUP_UPDATE_FLAG_CUR_POWER | GROUP_UPDATE_FLAG_MAX_POWER);
    }

    if (mask & GROUP_UPDATE_FLAG_PET_POWER_TYPE)            // same for pets
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
        // In all checked pre-2.x data of packets included only positive auras
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
        *data << (pet ? pet->GetObjectGuid() : ObjectGuid());
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
            // In all checked pre-2.x data of packets included only positive auras
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

/*this procedure handles clients CMSG_REQUEST_PARTY_MEMBER_STATS request*/
void groups::RequestPartyMemberStats(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REQUEST_PARTY_MEMBER_STATS");
    ObjectGuid guid;
    recv_data >> guid;

    Player* player = sPlayerRegistry.Find(guid, false);
    if (!player)
    {
        WorldPacket data(SMSG_PARTY_MEMBER_STATS_FULL, 3 + 4 + 1);
        data << guid.WriteAsPacked();
        data << uint32(GROUP_UPDATE_FLAG_STATUS);
        data << uint8(MEMBER_STATUS_OFFLINE);
        who.GetSession()->SendPacket(&data);
        return;
    }

    Pet* pet = player->GetPet();

    // expected number of auras on player: 12, on pet: 8. This is only for packet size estimation
    WorldPacket data(SMSG_PARTY_MEMBER_STATS_FULL, 3 + 4 + 1 + (23 + 12 * 2) + (pet ? (29 + 8 * 2) : 0));
    data << player->GetPackGUID();

    //uint32 mask1 = 0x00040BFF;                              // common mask, real flags used 0x000040BFF
    uint32 mask1 = GROUP_UPDATE_PLAYER;                     // actually, 0x000007FF
    if (pet)
    {
        mask1 |= GROUP_UPDATE_PET;                       // for hunters and other classes with pets
    }

    Powers powerType = player->GetPowerType();
    data << uint32(mask1);                                  // group update mask
    data << uint8(MEMBER_STATUS_ONLINE);                    // member's online status
    data << uint16(player->GetHealth());                    // GROUP_UPDATE_FLAG_CUR_HP
    data << uint16(player->GetMaxHealth());                 // GROUP_UPDATE_FLAG_MAX_HP
    data << uint8(powerType);                               // GROUP_UPDATE_FLAG_POWER_TYPE
    data << uint16(player->GetPower(powerType));            // GROUP_UPDATE_FLAG_CUR_POWER
    data << uint16(player->GetMaxPower(powerType));         // GROUP_UPDATE_FLAG_MAX_POWER
    data << uint16(player->getLevel());                     // GROUP_UPDATE_FLAG_LEVEL

    // verify player coordinates and zoneid to send to teammates
    uint16 iZoneId = 0;
    uint16 iCoordX = 0;
    uint16 iCoordY = 0;

    if (player->IsInWorld())
    {
        iZoneId = player->GetTerrain()->GetZoneId(player->Where().X(), player->Where().Y(), player->Where().Z());
        iCoordX = player->Where().X();
        iCoordY = player->Where().Y();
    }
    else if (player->IsBeingTeleported())               // Player is in teleportation
    {
        Geometry::Placement& loc = player->GetTeleportDest(); // So take teleportation destination
        iZoneId = sTerrainMgr.GetZoneId(loc.MapId(), loc.X(), loc.Y(), loc.Z());
        iCoordX = loc.X();
        iCoordY = loc.Y();
    }
    else
    {
        // unknown player status.
    }

    data << uint16(iZoneId);                              // GROUP_UPDATE_FLAG_ZONE
    data << uint16(iCoordX);                              // GROUP_UPDATE_FLAG_POSITION
    data << uint16(iCoordY);                              // GROUP_UPDATE_FLAG_POSITION

    uint32 auramask = 0;
    size_t maskPos = data.wpos();
    data << uint32(auramask);                               // placeholder
    for (uint8 i = 0; i < 32; ++i)
    {
        if (uint32 aura = player->GetUInt32Value(UNIT_FIELD_AURA + i))
        {
            auramask |= (uint32(1) << i);
            data << uint16(aura);
        }
    }
    data.put<uint32>(maskPos, auramask);                    // GROUP_UPDATE_FLAG_AURAS
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
    data.put<uint16>(maskPos, auramask1);                    // GROUP_UPDATE_FLAG_AURAS_2

    if (pet)
    {
        Powers petpowertype = pet->GetPowerType();
        data << pet->GetObjectGuid();                       // GROUP_UPDATE_FLAG_PET_GUID
        data << pet->GetName();                             // GROUP_UPDATE_FLAG_PET_NAME
        data << uint16(pet->GetDisplayId());                // GROUP_UPDATE_FLAG_PET_MODEL_ID
        data << uint16(pet->GetHealth());                   // GROUP_UPDATE_FLAG_PET_CUR_HP
        data << uint16(pet->GetMaxHealth());                // GROUP_UPDATE_FLAG_PET_MAX_HP
        data << uint8(petpowertype);                        // GROUP_UPDATE_FLAG_PET_POWER_TYPE
        data << uint16(pet->GetPower(petpowertype));        // GROUP_UPDATE_FLAG_PET_CUR_POWER
        data << uint16(pet->GetMaxPower(petpowertype));     // GROUP_UPDATE_FLAG_PET_MAX_POWER

        uint32 petauramask = 0;
        size_t petMaskPos = data.wpos();
        data << uint32(petauramask);                        // placeholder
        for (uint8 i = 0; i < 32; ++i)
        {
            if (uint32 petaura = pet->GetUInt32Value(UNIT_FIELD_AURA + i))
            {
                petauramask |= (uint32(1) << i);
                data << uint16(petaura);
            }
        }
        data.put<uint32>(petMaskPos, petauramask);          // GROUP_UPDATE_FLAG_PET_AURAS
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
        data.put<uint16>(petMaskPos, petauramask1);         // GROUP_UPDATE_FLAG_PET_AURAS_2
    }

    who.GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the saved raid instance information to the client.
 *
 * @param recv_data The received opcode packet.
 */
void groups::RequestRaidInfo(Player& who, WorldPacket& /*recv_data*/)
{
    // every time the player checks the character screen
    who.Binds().TellRaidInfo();
}

/**
 * @brief Handles the client's opt-out-of-loot setting.
 *
 * @param recv_data The received opcode packet.
 */
void groups::OptOutOfLoot(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_OPT_OUT_OF_LOOT");

    uint32 unkn;
    recv_data >> unkn;

    // ignore if player not loaded
    if (!session.GetPlayer())                                       // needed because STATUS_AUTHED
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
