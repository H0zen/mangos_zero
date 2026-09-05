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
 * @file ChannelHandler.cpp
 * @brief Channel-related opcode handlers
 *
 * This file handles channel-related opcodes including:
 * - CMSG_JOIN_CHANNEL: Join a channel
 * - CMSG_LEAVE_CHANNEL: Leave a channel
 * - CMSG_CHANNEL_LIST: List channels
 * - CMSG_CHANNEL_PASSWORD: Set channel password
 * - CMSG_CHANNEL_OWNER: Set channel owner
 * - CMSG_CHANNEL_MODERATOR: Set channel moderator
 * - CMSG_CHANNEL_MUTE: Mute channel member
 * - CMSG_CHANNEL_UNMUTE: Unmute channel member
 * - CMSG_CHANNEL_INVITE: Invite to channel
 * - CMSG_CHANNEL_KICK: Kick from channel
 * - CMSG_CHANNEL_BAN: Ban from channel
 * - CMSG_CHANNEL_UNBAN: Unban from channel
 */

#include <string>
#include "ChannelAnswers.h"
#include "ObjectMgr.h"                                      // for normalizePlayerName
#include "ChannelMgr.h"
#include "OpcodeTable.h"

/**
 * @brief Handles a client's request to join a chat channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::JoinChannel(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

    std::string channelName, pass;

    recvPacket >> channelName;

    if (channelName.empty())
    {
        return;
    }

    recvPacket >> pass;

    uint32 channelId = 0;
    char tmpStr[255];

    // Current player area id
    const uint32 playerZoneId = session.GetPlayer()->GetTerrain()->GetZoneId(session.GetPlayer()->Where().X(), session.GetPlayer()->Where().Y(), session.GetPlayer()->Where().Z());
    const uint32 stormwindZoneID = 1519;
    const uint32 ironforgeZoneID = 1537;
    const uint32 darnassusZoneID = 1657;
    const uint32 orgrimmarZoneID = 1637;
    const uint32 thunderbluffZoneID = 1638;
    const uint32 undercityZoneID = 1497;
    uint32 cityLookupAreaID = playerZoneId;    // Used to lookup for channels which support cross-city-chat

    // Area id of "Cities"
    const uint32 citiesZoneID = 3459;

    // Channel ID of the trade channel since this only applies to it
    const uint32 tradeChannelID = 2;
    const uint32 guildRecruitmentChannelID = 25;

    // Check if we are inside of a city
    if (playerZoneId == stormwindZoneID ||
        playerZoneId == ironforgeZoneID ||
        playerZoneId == darnassusZoneID ||
        playerZoneId == orgrimmarZoneID ||
        playerZoneId == thunderbluffZoneID ||
        playerZoneId == undercityZoneID)
    {
        // Use cities instead of the player id
        cityLookupAreaID = citiesZoneID;
    }

    //TODO: This doesn't seem like the right way to do it, but the client doesn't send any ID of the channel, and it's needed
    for (uint32 i = 0; i < sChatChannelsStore.GetNumRows(); ++i)
    {
        ChatChannelsEntry const* channel = sChatChannelsStore.LookupEntry(i);
        AreaTableEntry const* area = channel ? sAreaStore.LookupEntry(
            (channel->ID == tradeChannelID || channel->ID == guildRecruitmentChannelID) ? cityLookupAreaID : playerZoneId) : nullptr;

        if (area && channel)
        {
            snprintf(tmpStr, 255, channel->Name_lang[session.GetSessionDbcLocale()], area->AreaName_lang[session.GetSessionDbcLocale()]);
            //With a format string
            if (strcmp(tmpStr, channelName.c_str()) == 0 ||
                strcmp(channel->Name_lang[0], channelName.c_str()) == 0)
            {
                // Without one, used for ie: World Defense
                channelId = channel->ID;
                break;
            }
        }
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
    {
        //the channel id needs to be checkd for lfg (explanation?)
        if (Channel* chn = cMgr->GetJoinChannel(channelName))
        {
            chn->Join(session.GetPlayer(), pass.c_str());
        }
    }
}

/**
 * @brief Handles a client's request to leave a chat channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::LeaveChannel(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    // uint32 unk;
    std::string channelname;
    // recvPacket >> unk;                                   // channel id?
    recvPacket >> channelname;

    if (channelname.empty())
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->Leave(&who, true);
        }
        cMgr->LeftChannel(channelname);
    }
}

/**
 * @brief Sends the member list for a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelList(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname;
    recvPacket >> channelname;

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->List(&who);
        }
    }
}

/**
 * @brief Changes the password for a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelPassword(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname, pass;
    recvPacket >> channelname;

    recvPacket >> pass;

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->Password(&who, pass.c_str());
        }
    }
}

/**
 * @brief Sets a new owner for a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelSetOwner(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();

    std::string channelname, newp;
    recvPacket >> channelname;

    recvPacket >> newp;

    if (!normalizePlayerName(newp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->SetOwner(&who, newp.c_str());
        }
    }
}

/**
 * @brief Requests the current owner of a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelOwner(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname;
    recvPacket >> channelname;
    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->SendWhoOwner(&who);
        }
    }
}

/**
 * @brief Grants moderator privileges to a channel member.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelModerator(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname, otp;
    recvPacket >> channelname;

    recvPacket >> otp;

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->SetModerator(&who, otp.c_str());
        }
    }
}

/**
 * @brief Removes moderator privileges from a channel member.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelUnmoderator(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname, otp;
    recvPacket >> channelname;

    recvPacket >> otp;

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->UnsetModerator(&who, otp.c_str());
        }
    }
}

/**
 * @brief Mutes a member in a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelMute(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname, otp;
    recvPacket >> channelname;

    recvPacket >> otp;

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->SetMute(&who, otp.c_str());
        }
    }
}

/**
 * @brief Unmutes a member in a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelUnmute(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();

    std::string channelname, otp;
    recvPacket >> channelname;

    recvPacket >> otp;

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->UnsetMute(&who, otp.c_str());
        }
    }
}

/**
 * @brief Invites another player to a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelInvite(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname, otp;
    recvPacket >> channelname;

    recvPacket >> otp;

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->Invite(&who, otp.c_str());
        }
    }
}

/**
 * @brief Kicks a member from a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelKick(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname, otp;
    recvPacket >> channelname;

    recvPacket >> otp;
    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->Kick(&who, otp.c_str());
        }
    }
}

/**
 * @brief Bans a member from a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelBan(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname, otp;
    recvPacket >> channelname;

    recvPacket >> otp;

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->Ban(&who, otp.c_str());
        }
    }
}

/**
 * @brief Removes a ban for a member in a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelUnban(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();

    std::string channelname, otp;
    recvPacket >> channelname;

    recvPacket >> otp;

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->UnBan(&who, otp.c_str());
        }
    }
}

/**
 * @brief Toggles channel join and leave announcements.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelAnnouncements(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname;
    recvPacket >> channelname;
    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->Announce(&who);
        }
    }
}

/**
 * @brief Toggles moderated mode for a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelModerate(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname;
    recvPacket >> channelname;
    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->Moderate(&who);
        }
    }
}

/**
 * @brief Handles a channel display list query.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::ChannelDisplayListQuery(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname;
    recvPacket >> channelname;
    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            chn->List(&who);
        }
    }
}

/**
 * @brief Sends the current member count for a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::GetChannelMemberCount(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname;
    recvPacket >> channelname;
    if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, &who))
        {
            WorldPacket data(SMSG_CHANNEL_MEMBER_COUNT, chn->GetName().size() + 1 + 1 + 4);
            data << chn->GetName();
            data << uint8(chn->GetFlags());
            data << uint32(chn->GetNumPlayers());
            who.GetSession()->SendPacket(&data);
        }
    }
}

/**
 * @brief Handles a channel watch request from the client.
 *
 * @param recvPacket The received opcode packet.
 */
void channels::SetChannelWatch(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname;
    recvPacket >> channelname;
    /** if (ChannelMgr* cMgr = channelMgr(who.GetTeam()))
     *  if (Channel *chn = cMgr->GetChannel(channelname, &who))
     *  {
     *      chn->JoinNotify(who.GetGUID());
     *  }
     */
}
