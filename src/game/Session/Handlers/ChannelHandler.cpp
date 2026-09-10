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

#include <string>
#include "ChannelAnswers.h"
#include "ObjectMgr.h"
#include "ChannelMgr.h"
#include "OpcodeTable.h"

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

    const uint32 playerZoneId = session.GetPlayer()->GetTerrain()->GetZoneId(session.GetPlayer()->Where().X(), session.GetPlayer()->Where().Y(), session.GetPlayer()->Where().Z());
    const uint32 stormwindZoneID = 1519;
    const uint32 ironforgeZoneID = 1537;
    const uint32 darnassusZoneID = 1657;
    const uint32 orgrimmarZoneID = 1637;
    const uint32 thunderbluffZoneID = 1638;
    const uint32 undercityZoneID = 1497;
    uint32 cityLookupAreaID = playerZoneId;

    const uint32 citiesZoneID = 3459;

    const uint32 tradeChannelID = 2;
    const uint32 guildRecruitmentChannelID = 25;

    if (playerZoneId == stormwindZoneID ||
        playerZoneId == ironforgeZoneID ||
        playerZoneId == darnassusZoneID ||
        playerZoneId == orgrimmarZoneID ||
        playerZoneId == thunderbluffZoneID ||
        playerZoneId == undercityZoneID)
    {

        cityLookupAreaID = citiesZoneID;
    }

    for (uint32 i = 0; i < sChatChannelsStore.GetNumRows(); ++i)
    {
        ChatChannelsEntry const* channel = sChatChannelsStore.LookupEntry(i);
        AreaTableEntry const* area = channel ? sAreaStore.LookupEntry(
            (channel->ID == tradeChannelID || channel->ID == guildRecruitmentChannelID) ? cityLookupAreaID : playerZoneId) : nullptr;

        if (area && channel)
        {
            snprintf(tmpStr, 255, channel->Name_lang[session.GetSessionDbcLocale()], area->AreaName_lang[session.GetSessionDbcLocale()]);

            if (strcmp(tmpStr, channelName.c_str()) == 0 ||
                strcmp(channel->Name_lang[0], channelName.c_str()) == 0)
            {

                channelId = channel->ID;
                break;
            }
        }
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
    {

        if (Channel* chn = cMgr->GetJoinChannel(channelName))
        {
            chn->Join(session.GetPlayer(), pass.c_str());
        }
    }
}

void channels::LeaveChannel(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

    std::string channelname;

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

void channels::ChannelList(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelPassword(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelSetOwner(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelOwner(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelModerator(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelUnmoderator(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelMute(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelUnmute(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelInvite(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelKick(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelBan(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelUnban(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelAnnouncements(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelModerate(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::ChannelDisplayListQuery(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::GetChannelMemberCount(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

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

void channels::SetChannelWatch(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

    std::string channelname;
    recvPacket >> channelname;

}
