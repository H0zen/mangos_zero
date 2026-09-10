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

#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <string>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "GuildAnswers.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "SocialMgr.h"
#include "PlayerRegistry.h"

void guilds::GuildQuery(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_QUERY");

    uint32 guildId;
    recvPacket >> guildId;

    if (Guild* guild = sGuildMgr.GetGuildById(guildId))
    {
        guild->Query(&session);
        return;
    }

    session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
}

void guilds::GuildCreate(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_CREATE");

    std::string gname;
    recvPacket >> gname;

    if (who.GetGuildId())
    {
        return;
    }

    Guild* guild = new Guild;
    if (!guild->Create(&who, gname))
    {
        delete guild;
        return;
    }

    sGuildMgr.AddGuild(guild);
}

void WorldSession::SendGuildInvite(Player* player, bool  )
{
    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    player->Invites().ToGuild(GetPlayer()->GetGuildId());

    WorldPacket data(SMSG_GUILD_INVITE, (8 + 10));
    data << GetPlayer()->GetName();
    data << guild->GetName();
    player->GetSession()->SendPacket(&data);
}

void guilds::GuildInvite(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_INVITE");

    std::string Invitedname, plname;
    Player* player = nullptr;

    recvPacket >> Invitedname;

    if (normalizePlayerName(Invitedname))
    {
        player = sPlayerRegistry.FindByName(Invitedname.c_str());
    }

    if (!player)
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, Invitedname, ERR_GUILD_PLAYER_NOT_FOUND_S);
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());
    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (player->GetSocial()->HasIgnore(session.GetPlayer()->GetObjectGuid()))
    {
        return;
    }

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && player->GetTeam() != session.GetPlayer()->GetTeam())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, Invitedname, ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (player->GetGuildId())
    {
        plname = player->GetName();
        session.SendGuildCommandResult(GUILD_INVITE_S, plname, ERR_ALREADY_IN_GUILD_S);
        return;
    }

    if (player->Invites().ToGuild())
    {
        plname = player->GetName();
        session.SendGuildCommandResult(GUILD_INVITE_S, plname, ERR_ALREADY_INVITED_TO_GUILD_S);
        return;
    }

    if (!guild->HasRankRight(session.GetPlayer()->GetRank(), GR_RIGHT_INVITE))
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    DEBUG_LOG("Player %s Invited %s to Join his Guild", session.GetPlayer()->GetName(), Invitedname.c_str());

    player->Invites().ToGuild(session.GetPlayer()->GetGuildId());

    guild->LogGuildEvent(GUILD_EVENT_LOG_INVITE_PLAYER, session.GetPlayer()->GetObjectGuid(), player->GetObjectGuid());

    WorldPacket data(SMSG_GUILD_INVITE, (8 + 10));
    data << session.GetPlayer()->GetName();
    data << guild->GetName();
    player->GetSession()->SendPacket(&data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_INVITE)");
}

void guilds::GuildRemove(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_REMOVE");

    std::string plName;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
    {
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());
    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(session.GetPlayer()->GetRank(), GR_RIGHT_REMOVE))
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);
    if (!slot)
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->RankId == GR_GUILDMASTER)
    {
        session.SendGuildCommandResult(GUILD_QUIT_S, "", ERR_GUILD_LEADER_LEAVE);
        return;
    }

    if (session.GetPlayer()->GetRank() >= slot->RankId)
    {
        session.SendGuildCommandResult(GUILD_QUIT_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    if (guild->DelMember(slot->guid))
    {
        guild->Disband();
        delete guild;
        return;
    }

    guild->LogGuildEvent(GUILD_EVENT_LOG_UNINVITE_PLAYER, session.GetPlayer()->GetObjectGuid(), slot->guid);

    guild->BroadcastEvent(GE_REMOVED, plName.c_str(), session.GetPlayer()->GetName());
}

void guilds::GuildAccept(Player& who, WorldPacket& )
{
    Guild* guild;
    Player* player = &who;

    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_ACCEPT");

    guild = sGuildMgr.GetGuildById(player->Invites().ToGuild());
    if (!guild || player->GetGuildId())
    {
        return;
    }

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && player->GetTeam() != sObjectMgr.GetPlayerTeamByGUID(guild->GetLeaderGuid()))
    {
        return;
    }

    if (!guild->AddMember(who.GetObjectGuid(), guild->GetLowestRank()))
    {
        return;
    }

    guild->LogGuildEvent(GUILD_EVENT_LOG_JOIN_GUILD, who.GetObjectGuid());

    guild->BroadcastEvent(GE_JOINED, player->GetObjectGuid(), player->GetName());
}

void guilds::GuildDecline(Player& who, WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_DECLINE");
    if (who.GetGuildId())
    {
        return;
    }

    who.Invites().ToGuild(0);
}

void guilds::GuildInfo(WorldSession& session, WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_INFO");

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());
    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    WorldPacket data(SMSG_GUILD_INFO, (5 * 4 + guild->GetName().size() + 1));
    data << guild->GetName();
    data << uint32(guild->GetCreatedDay());
    data << uint32(guild->GetCreatedMonth());
    data << uint32(guild->GetCreatedYear());
    data << uint32(guild->GetMemberSize());
    data << uint32(guild->GetAccountsNumber());
    session.SendPacket(&data);
}

void guilds::GuildRoster(Player& who, WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_ROSTER");

    if (Guild* guild = sGuildMgr.GetGuildById(who.GetGuildId()))
    {
        guild->Roster(who.GetSession());
    }
}

void guilds::GuildPromote(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_PROMOTE");

    std::string plName;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
    {
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());
    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(session.GetPlayer()->GetRank(), GR_RIGHT_PROMOTE))
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);
    if (!slot)
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->guid == session.GetPlayer()->GetObjectGuid())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_NAME_INVALID);
        return;
    }

    if (session.GetPlayer()->GetRank() + 1 >= slot->RankId)
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    uint32 newRankId = slot->RankId - 1;

    slot->ChangeRank(newRankId);

    guild->LogGuildEvent(GUILD_EVENT_LOG_PROMOTE_PLAYER, session.GetPlayer()->GetObjectGuid(), slot->guid, newRankId);

    guild->BroadcastEvent(GE_PROMOTION, session.GetPlayer()->GetName(), plName.c_str(), guild->GetRankName(newRankId).c_str());
}

void guilds::GuildDemote(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_DEMOTE");

    std::string plName;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
    {
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());

    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(session.GetPlayer()->GetRank(), GR_RIGHT_DEMOTE))
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);

    if (!slot)
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->guid == session.GetPlayer()->GetObjectGuid())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_NAME_INVALID);
        return;
    }

    if (session.GetPlayer()->GetRank() >= slot->RankId)
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    if (slot->RankId >= guild->GetLowestRank())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_LOW_S);
        return;
    }

    uint32 newRankId = slot->RankId + 1;

    slot->ChangeRank(newRankId);

    guild->LogGuildEvent(GUILD_EVENT_LOG_DEMOTE_PLAYER, session.GetPlayer()->GetObjectGuid(), slot->guid, newRankId);

    guild->BroadcastEvent(GE_DEMOTION, session.GetPlayer()->GetName(), plName.c_str(), guild->GetRankName(slot->RankId).c_str());
}

void guilds::GuildLeave(WorldSession& session, WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_LEAVE");

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());
    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (session.GetPlayer()->GetObjectGuid() == guild->GetLeaderGuid() && guild->GetMemberSize() > 1)
    {
        session.SendGuildCommandResult(GUILD_QUIT_S, "", ERR_GUILD_LEADER_LEAVE);
        return;
    }

    if (session.GetPlayer()->GetObjectGuid() == guild->GetLeaderGuid())
    {
        guild->Disband();
        delete guild;
        return;
    }

    session.SendGuildCommandResult(GUILD_QUIT_S, guild->GetName(), ERR_PLAYER_NO_MORE_IN_GUILD);

    if (guild->DelMember(session.GetPlayer()->GetObjectGuid()))
    {
        guild->Disband();
        delete guild;
        return;
    }

    guild->LogGuildEvent(GUILD_EVENT_LOG_LEAVE_GUILD, session.GetPlayer()->GetObjectGuid());

    guild->BroadcastEvent(GE_LEFT, session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
}

void guilds::GuildDisband(WorldSession& session, WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_DISBAND");

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());
    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (session.GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->Disband();
    delete guild;

    DEBUG_LOG("WORLD: Guild Successfully Disbanded");
}

void guilds::GuildLeader(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_LEADER");

    std::string name;
    recvPacket >> name;

    Player* oldLeader = session.GetPlayer();

    if (!normalizePlayerName(name))
    {
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(oldLeader->GetGuildId());

    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (oldLeader->GetObjectGuid() != guild->GetLeaderGuid())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* oldSlot = guild->GetMemberSlot(oldLeader->GetObjectGuid());
    if (!oldSlot)
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(name);
    if (!slot)
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, name, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    guild->SetLeader(slot->guid);
    oldSlot->ChangeRank(GR_OFFICER);

    guild->BroadcastEvent(GE_LEADER_CHANGED, oldLeader->GetName(), name.c_str());
}

void guilds::GuildMOTD(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_MOTD");

    std::string MOTD;

    if (!recvPacket.empty())
    {
        recvPacket >> MOTD;
    }
    else
    {
        MOTD.clear();
    }

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());
    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(session.GetPlayer()->GetRank(), GR_RIGHT_SETMOTD))
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->SetMOTD(MOTD);

    guild->BroadcastEvent(GE_MOTD, MOTD.c_str());
}

void guilds::GuildSetPublicNote(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_SET_PUBLIC_NOTE");

    std::string name, PNOTE;
    recvPacket >> name;

    if (!normalizePlayerName(name))
    {
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());

    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(session.GetPlayer()->GetRank(), GR_RIGHT_EPNOTE))
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(name);
    if (!slot)
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, name, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    recvPacket >> PNOTE;

    slot->SetPNOTE(PNOTE);

    guild->Roster(&session);
}

void guilds::GuildSetOfficerNote(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_SET_OFFICER_NOTE");

    std::string plName, OFFNOTE;
    recvPacket >> plName;

    if (!normalizePlayerName(plName))
    {
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());

    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(session.GetPlayer()->GetRank(), GR_RIGHT_EOFFNOTE))
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(plName);
    if (!slot)
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    recvPacket >> OFFNOTE;

    slot->SetOFFNOTE(OFFNOTE);

    guild->Roster(&session);
}

void guilds::GuildRank(WorldSession& session, WorldPacket& recvPacket)
{
    std::string rankname;
    uint32 rankId;
    uint32 rights;

    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_RANK");

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());
    if (!guild)
    {
        recvPacket.rpos(recvPacket.wpos());
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (session.GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        recvPacket.rpos(recvPacket.wpos());
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    recvPacket >> rankId;
    recvPacket >> rights;
    recvPacket >> rankname;
    DEBUG_LOG("WORLD: Changed RankName to %s , Rights to 0x%.4X", rankname.c_str(), rights);

    guild->SetRankName(rankId, rankname);

    if (rankId == GR_GUILDMASTER)
    {
        rights = GR_RIGHT_ALL;
    }

    guild->SetRankRights(rankId, rights);

    guild->Query(&session);
    guild->Roster();
}

void guilds::GuildAddRank(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_ADD_RANK");

    std::string rankname;
    recvPacket >> rankname;

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());
    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (session.GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    if (guild->GetRanksSize() >= GUILD_RANKS_MAX_COUNT)
    {
        return;
    }

    guild->CreateRank(rankname, GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);

    guild->Query(&session);
    guild->Roster();
}

void guilds::GuildDelRank(WorldSession& session, WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_DEL_RANK");

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());
    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (session.GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->DelRank();

    guild->Query(&session);
    guild->Roster();
}

void WorldSession::SendGuildCommandResult(uint32 typecmd, const std::string& str, uint32 cmdresult)
{
    WorldPacket data(SMSG_GUILD_COMMAND_RESULT, (8 + str.size() + 1));
    data << typecmd;
    data << str;
    data << cmdresult;
    SendPacket(&data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_COMMAND_RESULT)");
}

void guilds::GuildChangeInfoText(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_INFO_TEXT");

    std::string GINFO;
    recvPacket >> GINFO;

    Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId());
    if (!guild)
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(session.GetPlayer()->GetRank(), GR_RIGHT_MODIFY_GUILD_INFO))
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->SetGINFO(GINFO);
}

void guilds::SaveGuildEmblem(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode MSG_SAVE_GUILD_EMBLEM");

    ObjectGuid vendorGuid = 0;
    uint32 EmblemStyle, EmblemColor, BorderStyle, BorderColor, BackgroundColor;

    recvPacket >> vendorGuid;
    recvPacket >> EmblemStyle >> EmblemColor >> BorderStyle >> BorderColor >> BackgroundColor;

    Creature* pCreature = who.GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_TABARDDESIGNER);
    if (!pCreature)
    {

        who.GetSession()->SendSaveGuildEmblem(ERR_GUILDEMBLEM_FAIL_NO_MESSAGE);
        DEBUG_LOG("WORLD: HandleSaveGuildEmblemOpcode - %s not found or you can't interact with him.", GuidString(vendorGuid).c_str());
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    Guild* guild = sGuildMgr.GetGuildById(who.GetGuildId());
    if (!guild)
    {

        who.GetSession()->SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOGUILD);
        return;
    }

    if (guild->GetLeaderGuid() != who.GetObjectGuid())
    {

        who.GetSession()->SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOTGUILDMASTER);
        return;
    }

    if (who.GetMoney() < 10 * GOLD)
    {

        who.GetSession()->SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOTENOUGHMONEY);
        return;
    }

    who.ModifyMoney(-10 * GOLD);
    guild->SetEmblem(EmblemStyle, EmblemColor, BorderStyle, BorderColor, BackgroundColor);

    who.GetSession()->SendSaveGuildEmblem(ERR_GUILDEMBLEM_SUCCESS);

    guild->Query(who.GetSession());
}

void guilds::GuildEventLogQuery(Player& who, WorldPacket& )
{

    DEBUG_LOG("WORLD: Received (MSG_GUILD_EVENT_LOG_QUERY)");

    if (uint32 GuildId = who.GetGuildId())
    {
        if (Guild* pGuild = sGuildMgr.GetGuildById(GuildId))
        {
            pGuild->DisplayGuildEventLog(who.GetSession());
        }
    }
}

void WorldSession::SendSaveGuildEmblem(uint32 msg)
{
    WorldPacket data(MSG_SAVE_GUILD_EMBLEM, 4);
    data << uint32(msg);
    SendPacket(&data);
}
