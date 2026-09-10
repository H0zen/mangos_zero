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

#include "CharacterRows.h"
#include <string>
#include <set>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "Mint.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Chat.h"
#include "SocialMgr.h"
#include "Util.h"
#include "Language.h"
#include "World.h"
#include "PlayerRegistry.h"

void MemberSlot::SetMemberStats(Player* player)
{
    Name   = player->GetName();
    Level  = player->getLevel();
    Class  = player->getClass();
    ZoneId = player->IsInWorld() ? player->GetTerrain()->GetZoneId(player->Where().X(), player->Where().Y(), player->Where().Z()) : player->GetCachedZoneId();
}

void MemberSlot::UpdateLogoutTime()
{
    LogoutTime = time(nullptr);
}

void MemberSlot::SetPNOTE(std::string pnote)
{
    Pnote = pnote;

    CharacterDatabase.escape_string(pnote);
    CharacterDatabase.PExecute("UPDATE `guild_member` SET `pnote` = '%s' WHERE `guid` = '%u'", pnote.c_str(), GuidCounter(guid));
}

void MemberSlot::SetOFFNOTE(std::string offnote)
{
    OFFnote = offnote;

    CharacterDatabase.escape_string(offnote);
    CharacterDatabase.PExecute("UPDATE `guild_member` SET `offnote` = '%s' WHERE `guid` = '%u'", offnote.c_str(), GuidCounter(guid));
}

void MemberSlot::ChangeRank(uint32 newRank)
{
    RankId = newRank;

    Player* player = sObjectMgr.GetPlayer(guid);

    if (player)
    {
        player->SetRank(newRank);
    }

    CharacterDatabase.PExecute("UPDATE `guild_member` SET `rank`='%u' WHERE `guid`='%u'", newRank, GuidCounter(guid));
}

Guild::Guild()
{
    m_Id = 0;
    m_Name.clear();
    GINFO.clear();
    MOTD.clear();
    m_EmblemStyle = 0;
    m_EmblemColor = 0;
    m_BorderStyle = 0;
    m_BorderColor = 0;
    m_BackgroundColor = 0;
    m_accountsNumber = 0;

    m_CreatedYear = 0;
    m_CreatedMonth = 0;
    m_CreatedDay = 0;

    m_GuildEventLogNextGuid = 0;
}

Guild::~Guild()
{
}

bool Guild::Create(Player* leader, std::string gname)
{
    if (sGuildMgr.GetGuildByName(gname))
    {
        return false;
    }

    WorldSession* lSession = leader->GetSession();
    if (!lSession)
    {
        return false;
    }

    m_LeaderGuid = leader->GetObjectGuid();
    m_Name = gname;
    GINFO.clear();
    MOTD = "No message set.";
    m_Id = sMint.GuildIds().Next();

    time_t now = time(0);
    std::tm local = safe_localtime(now);
    m_CreatedDay   = local.tm_mday;
    m_CreatedMonth = local.tm_mon + 1;
    m_CreatedYear  = local.tm_year + 1900;

    DEBUG_LOG("GUILD: creating guild %s to leader: %s", gname.c_str(), GuidString(m_LeaderGuid).c_str());

    CharacterDatabase.escape_string(gname);

    std::string dbGINFO = GINFO;
    std::string dbMOTD = MOTD;
    CharacterDatabase.escape_string(dbGINFO);
    CharacterDatabase.escape_string(dbMOTD);

    CharacterDatabase.BeginTransaction();

    CharacterDatabase.PExecute("DELETE FROM `guild_member` WHERE `guildid`='%u'", m_Id);
    CharacterDatabase.PExecute("INSERT INTO `guild` (`guildid`,`name`,`leaderguid`,`info`,`motd`,`createdate`,`EmblemStyle`,`EmblemColor`,`BorderStyle`,`BorderColor`,`BackgroundColor`) "
        "VALUES('%u','%s','%u', '%s', '%s','" UI64FMTD "','%u','%u','%u','%u','%u')",
        m_Id, gname.c_str(), GuidCounter(m_LeaderGuid), dbGINFO.c_str(), dbMOTD.c_str(), uint64(now), m_EmblemStyle, m_EmblemColor, m_BorderStyle, m_BorderColor, m_BackgroundColor);
    CharacterDatabase.CommitTransaction();

    CreateDefaultGuildRanks(lSession->GetSessionDbLocaleIndex());

    return AddMember(m_LeaderGuid, (uint32)GR_GUILDMASTER);
}

void Guild::CreateDefaultGuildRanks(int locale_idx)
{
    CharacterDatabase.PExecute("DELETE FROM `guild_rank` WHERE `guildid`='%u'", m_Id);

    CreateRank(sObjectMgr.GetMangosString(LANG_GUILD_MASTER, locale_idx),   GR_RIGHT_ALL);
    CreateRank(sObjectMgr.GetMangosString(LANG_GUILD_OFFICER, locale_idx),  GR_RIGHT_ALL);
    CreateRank(sObjectMgr.GetMangosString(LANG_GUILD_VETERAN, locale_idx),  GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);
    CreateRank(sObjectMgr.GetMangosString(LANG_GUILD_MEMBER, locale_idx),   GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);
    CreateRank(sObjectMgr.GetMangosString(LANG_GUILD_INITIATE, locale_idx), GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);
}

bool Guild::AddMember(ObjectGuid plGuid, uint32 plRank)
{
    Player* pl = sObjectMgr.GetPlayer(plGuid);
    if (pl)
    {
        if (pl->GetGuildId() != 0)
        {
            return false;
        }
    }
    else
    {
        if (CharacterRows::GuildOf(plGuid) != 0)
        {
            return false;
        }
    }

    Player::RemovePetitionsAndSigns(plGuid);

    uint32 lowguid = GuidCounter(plGuid);

    MemberSlot newmember;

    newmember.guid = plGuid;

    if (pl)
    {
        newmember.accountId = pl->GetSession()->GetAccountId();
        newmember.Name   = pl->GetName();
        newmember.Level  = pl->getLevel();
        newmember.Class  = pl->getClass();
        newmember.ZoneId = pl->GetTerrain()->GetZoneId(pl->Where().X(), pl->Where().Y(), pl->Where().Z());
    }
    else
    {

        QueryResult* result = CharacterDatabase.PQuery("SELECT `name`,`level`,`class`,`zone`,`account` FROM `characters` WHERE `guid` = '%u'", lowguid);
        if (!result)
        {
            return false;
        }

        Field* fields    = result->Fetch();
        newmember.Name   = fields[0].GetCppString();
        newmember.Level  = fields[1].GetUInt8();
        newmember.Class  = fields[2].GetUInt8();
        newmember.ZoneId = fields[3].GetUInt32();
        newmember.accountId = fields[4].GetInt32();
        delete result;

        if (newmember.Level < 1 || newmember.Level > STRONG_MAX_LEVEL ||
            !((1 << (newmember.Class - 1)) & CLASSMASK_ALL_PLAYABLE))
        {
            sLog.outError("%s has a broken data in field `characters` table, can not add him to guild.", GuidString(plGuid).c_str());
            return false;
        }
    }

    newmember.RankId  = plRank;
    newmember.OFFnote = (std::string)"";
    newmember.Pnote   = (std::string)"";
    newmember.LogoutTime = time(nullptr);
    members[lowguid] = newmember;

    std::string dbPnote   = newmember.Pnote;
    std::string dbOFFnote = newmember.OFFnote;
    CharacterDatabase.escape_string(dbPnote);
    CharacterDatabase.escape_string(dbOFFnote);

    CharacterDatabase.PExecute("INSERT INTO `guild_member` (`guildid`,`guid`,`rank`,`pnote`,`offnote`) VALUES ('%u', '%u', '%u','%s','%s')",
        m_Id, lowguid, newmember.RankId, dbPnote.c_str(), dbOFFnote.c_str());

    if (pl)
    {
        pl->SetInGuild(m_Id);
        pl->SetRank(newmember.RankId);
        pl->Invites().ToGuild(0);
    }

    UpdateAccountsNumber();

    return true;
}

void Guild::SetMOTD(std::string motd)
{
    MOTD = motd;

    CharacterDatabase.escape_string(motd);
    CharacterDatabase.PExecute("UPDATE `guild` SET `motd`='%s' WHERE `guildid`='%u'", motd.c_str(), m_Id);

}

void Guild::SetGINFO(std::string ginfo)
{
    GINFO = ginfo;

    CharacterDatabase.escape_string(ginfo);
    CharacterDatabase.PExecute("UPDATE `guild` SET `info`='%s' WHERE `guildid`='%u'", ginfo.c_str(), m_Id);

}

bool Guild::LoadGuildFromDB(QueryResult* guildDataResult)
{
    if (!guildDataResult)
    {
        return false;
    }

    Field* fields = guildDataResult->Fetch();

    m_Id              = fields[0].GetUInt32();
    m_Name            = fields[1].GetCppString();
    m_LeaderGuid      = MakeGuid(HIGHGUID_PLAYER, fields[2].GetUInt32());
    m_EmblemStyle     = fields[3].GetUInt32();
    m_EmblemColor     = fields[4].GetUInt32();
    m_BorderStyle     = fields[5].GetUInt32();
    m_BorderColor     = fields[6].GetUInt32();
    m_BackgroundColor = fields[7].GetUInt32();
    GINFO             = fields[8].GetCppString();
    MOTD              = fields[9].GetCppString();
    time_t time       = time_t(fields[10].GetUInt64());

    if (time > 0)
    {
        std::tm local  = safe_localtime(time);
        m_CreatedDay   = local.tm_mday;
        m_CreatedMonth = local.tm_mon + 1;
        m_CreatedYear  = local.tm_year + 1900;
    }

    return true;
}

bool Guild::CheckGuildStructure()
{

    int32 GM_rights = GetRank(m_LeaderGuid);
    if (GM_rights == -1)
    {
        if (DelMember(m_LeaderGuid))
        {
            return false;
        }
    }
    else if (GM_rights != GR_GUILDMASTER)
    {
        SetLeader(m_LeaderGuid);
    }

    for (MemberList::iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        MemberSlot &member = itr->second;
        if (member.RankId == GR_GUILDMASTER && m_LeaderGuid != member.guid)
        {
            member.ChangeRank(GR_OFFICER);
        }
    }

    return true;
}

bool Guild::LoadRanksFromDB(QueryResult* guildRanksResult)
{
    if (!guildRanksResult)
    {
        sLog.outError("Guild %u has broken `guild_rank` data, creating new...", m_Id);
        CreateDefaultGuildRanks(0);
        return true;
    }

    Field* fields;
    bool broken_ranks = false;

    do
    {
        fields = guildRanksResult->Fetch();

        if (!fields)
        {
            break;
        }

        uint32 guildId       = fields[0].GetUInt32();
        if (guildId < m_Id)
        {

            sLog.outErrorDb("Guild %u does not exist but it has a record in guild_rank table, deleting it!", guildId);
            CharacterDatabase.PExecute("DELETE FROM `guild_rank` WHERE `guildid` = '%u'", guildId);
            continue;
        }

        if (guildId > m_Id)
        {
            break;
        }

        uint32 rankID        = fields[1].GetUInt32();
        std::string rankName = fields[2].GetCppString();
        uint32 rankRights    = fields[3].GetUInt32();

        if (rankID != m_Ranks.size())
        {
            broken_ranks =  true;
        }

        if (m_Ranks.empty())
        {
            rankRights |= GR_RIGHT_ALL;
        }

        AddRank(rankName, rankRights);
    }
    while (guildRanksResult->NextRow());

    if (m_Ranks.size() < GUILD_RANKS_MIN_COUNT)
    {
        m_Ranks.clear();
        sLog.outError("Guild %u has broken `guild_rank` data, creating new...", m_Id);
        CreateDefaultGuildRanks(0);
        broken_ranks = false;
    }

    if (broken_ranks)
    {
        sLog.outError("Guild %u has broken `guild_rank` data, repairing...", m_Id);
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `guild_rank` WHERE `guildid`='%u'", m_Id);
        for (size_t i = 0; i < m_Ranks.size(); ++i)
        {
            std::string name = m_Ranks[i].Name;
            uint32 rights = m_Ranks[i].Rights;
            CharacterDatabase.escape_string(name);
            CharacterDatabase.PExecute("INSERT INTO `guild_rank` (`guildid`,`rid`,`rname`,`rights`) VALUES ('%u', '%u', '%s', '%u')", m_Id, uint32(i), name.c_str(), rights);
        }
        CharacterDatabase.CommitTransaction();
    }

    return true;
}

bool Guild::LoadMembersFromDB(QueryResult* guildMembersResult)
{
    if (!guildMembersResult)
    {
        return false;
    }

    do
    {
        Field* fields = guildMembersResult->Fetch();

        if (!fields)
        {
            break;
        }
        uint32 guildId       = fields[0].GetUInt32();
        if (guildId < m_Id)
        {

            sLog.outErrorDb("Guild %u does not exist but it has a record in guild_member table, deleting it!", guildId);
            CharacterDatabase.PExecute("DELETE FROM `guild_member` WHERE `guildid` = '%u'", guildId);
            continue;
        }

        if (guildId > m_Id)

        {
            break;
        }

        MemberSlot newmember;
        uint32 lowguid = fields[1].GetUInt32();
        newmember.guid = MakeGuid(HIGHGUID_PLAYER, lowguid);
        newmember.RankId = fields[2].GetUInt32();

        if (newmember.RankId >= m_Ranks.size())
        {
            newmember.RankId = GetLowestRank();
        }

        newmember.Pnote                 = fields[3].GetCppString();
        newmember.OFFnote               = fields[4].GetCppString();

        newmember.Name                  = fields[5].GetCppString();
        newmember.Level                 = fields[6].GetUInt8();
        newmember.Class                 = fields[7].GetUInt8();
        newmember.ZoneId                = fields[8].GetUInt32();
        newmember.LogoutTime            = fields[9].GetUInt64();
        newmember.accountId             = fields[10].GetInt32();

        if (newmember.Level < 1 || newmember.Level > STRONG_MAX_LEVEL)
        {
            sLog.outError("%s has a broken data in field `characters`.`data`, deleting him from guild!", GuidString(newmember.guid).c_str());
            CharacterDatabase.PExecute("DELETE FROM `guild_member` WHERE `guid` = '%u'", lowguid);
            continue;
        }
        if (!newmember.ZoneId)
        {
            sLog.outError("%s has broken zone-data", GuidString(newmember.guid).c_str());

            newmember.ZoneId = CharacterRows::ZoneOf(newmember.guid);
        }
        if (!((1 << (newmember.Class - 1)) & CLASSMASK_ALL_PLAYABLE))
        {
            sLog.outError("%s has a broken data in field `characters`.`class`, deleting him from guild!", GuidString(newmember.guid).c_str());
            CharacterDatabase.PExecute("DELETE FROM `guild_member` WHERE `guid` = '%u'", lowguid);
            continue;
        }

        members[lowguid]      = newmember;
    }
    while (guildMembersResult->NextRow());

    if (members.empty())
    {
        return false;
    }

    UpdateAccountsNumber();

    return true;
}

void Guild::SetLeader(ObjectGuid guid)
{
    MemberSlot* slot = GetMemberSlot(guid);
    if (!slot)
    {
        return;
    }

    m_LeaderGuid = guid;
    slot->ChangeRank(GR_GUILDMASTER);

    CharacterDatabase.PExecute("UPDATE `guild` SET `leaderguid`='%u' WHERE `guildid`='%u'", GuidCounter(guid), m_Id);
}

bool Guild::DelMember(ObjectGuid guid, bool isDisbanding)
{
    uint32 lowguid = GuidCounter(guid);

    if (m_LeaderGuid == guid && !isDisbanding)
    {
        MemberSlot* oldLeader = nullptr;
        MemberSlot* best = nullptr;
        ObjectGuid newLeaderGUID = 0;
        for (Guild::MemberList::iterator i = members.begin(); i != members.end(); ++i)
        {
            if (i->first == lowguid)
            {
                oldLeader = &(i->second);
                continue;
            }

            if (!best || best->RankId > i->second.RankId)
            {
                best = &(i->second);
                newLeaderGUID = MakeGuid(HIGHGUID_PLAYER, i->first);
            }
        }

        if (!best)
        {
            return true;
        }

        SetLeader(newLeaderGUID);

        if (Player* newLeader = sObjectMgr.GetPlayer(newLeaderGUID))
        {
            newLeader->SetRank(GR_GUILDMASTER);
        }

        if (oldLeader)
        {
            BroadcastEvent(GE_LEADER_CHANGED, oldLeader->Name.c_str(), best->Name.c_str());
            BroadcastEvent(GE_LEFT, guid, oldLeader->Name.c_str());
        }
    }

    members.erase(lowguid);

    Player* player = sObjectMgr.GetPlayer(guid);

    if (player)
    {
        player->SetInGuild(0);
        player->SetRank(0);
    }

    CharacterDatabase.PExecute("DELETE FROM `guild_member` WHERE `guid` = '%u'", lowguid);

    if (!isDisbanding)
    {
        UpdateAccountsNumber();
    }

    return members.empty();
}

bool Guild::ChangeMemberRank(ObjectGuid guid, uint8 newRank)
{
    if (newRank <= GetLowestRank())
    {
        if (MemberSlot* member = GetMemberSlot(guid))
        {
            member->ChangeRank(newRank);
            return true;
        }
    }
    return false;
}

void Guild::BroadcastToGuild(WorldSession* session, const std::string& msg, uint32 language)
{
    if (!session)
    {
        return;
    }

    Player* player = session->GetPlayer();
    if (!player || !HasRankRight(player->GetRank(), GR_RIGHT_GCHATSPEAK))
    {
        return;
    }

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_GUILD, msg.c_str(), Language(language), player->GetChatTag(), player->GetObjectGuid(), player->GetName());

    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        Player* pl = sPlayerRegistry.Find(MakeGuid(HIGHGUID_PLAYER, itr->first));

        if (pl && pl->GetSession() && HasRankRight(pl->GetRank(), GR_RIGHT_GCHATLISTEN) && !pl->GetSocial()->HasIgnore(player->GetObjectGuid()))
        {
            pl->GetSession()->SendPacket(&data);
        }
    }
}

void Guild::BroadcastToOfficers(WorldSession* session, const std::string& msg, uint32 language)
{
    if (!session)
    {
        return;
    }

    Player* player = session->GetPlayer();
    if (!player || !HasRankRight(player->GetRank(), GR_RIGHT_OFFCHATSPEAK))
    {
        return;
    }

    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_OFFICER, msg.c_str(), Language(language), player->GetChatTag(), player->GetObjectGuid(), player->GetName());

        Player* pl = sPlayerRegistry.Find(MakeGuid(HIGHGUID_PLAYER, itr->first));

        if (pl && pl->GetSession() && HasRankRight(pl->GetRank(), GR_RIGHT_OFFCHATLISTEN) && !pl->GetSocial()->HasIgnore(player->GetObjectGuid()))
        {
            pl->GetSession()->SendPacket(&data);
        }
    }
}

void Guild::BroadcastPacket(WorldPacket* packet)
{
    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        Player* player = sPlayerRegistry.Find(MakeGuid(HIGHGUID_PLAYER, itr->first));
        if (player)
        {
            player->GetSession()->SendPacket(packet);
        }
    }
}

void Guild::BroadcastPacketToRank(WorldPacket* packet, uint32 rankId)
{
    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        if (itr->second.RankId == rankId)
        {
            Player* player = sPlayerRegistry.Find(MakeGuid(HIGHGUID_PLAYER, itr->first));
            if (player)
            {
                player->GetSession()->SendPacket(packet);
            }
        }
    }
}

void Guild::Disband()
{
    BroadcastEvent(GE_DISBANDED);

    while (!members.empty())
    {
        MemberList::const_iterator itr = members.begin();
        DelMember(MakeGuid(HIGHGUID_PLAYER, itr->first), true);
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `guild` WHERE `guildid` = '%u'", m_Id);
    CharacterDatabase.PExecute("DELETE FROM `guild_rank` WHERE `guildid` = '%u'", m_Id);
    CharacterDatabase.PExecute("DELETE FROM `guild_eventlog` WHERE `guildid` = '%u'", m_Id);
    CharacterDatabase.CommitTransaction();

    sGuildMgr.RemoveGuild(m_Id);
}

void Guild::Roster(WorldSession* session )
{

    WorldPacket data(SMSG_GUILD_ROSTER, (4 + MOTD.length() + 1 + GINFO.length() + 1 + 4 + m_Ranks.size() * 4 + members.size() * 50));
    data << uint32(members.size());
    data << MOTD;
    data << GINFO;

    data << uint32(m_Ranks.size());
    for (RankList::const_iterator ritr = m_Ranks.begin(); ritr != m_Ranks.end(); ++ritr)
    {
        data << uint32(ritr->Rights);
    }

    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        if (Player* pl = sPlayerRegistry.Find(MakeGuid(HIGHGUID_PLAYER, itr->first)))
        {
            data << pl->GetObjectGuid();
            data << uint8(1);
            data << pl->GetName();
            data << uint32(itr->second.RankId);
            data << uint8(pl->getLevel());
            data << uint8(pl->getClass());
            data << uint32(pl->GetTerrain()->GetZoneId(pl->Where().X(), pl->Where().Y(), pl->Where().Z()));
            data << itr->second.Pnote;
            data << itr->second.OFFnote;
        }
        else
        {
            data << MakeGuid(HIGHGUID_PLAYER, itr->first);
            data << uint8(0);
            data << itr->second.Name;
            data << uint32(itr->second.RankId);
            data << uint8(itr->second.Level);
            data << uint8(itr->second.Class);
            data << uint32(itr->second.ZoneId);
            data << float(float(time(nullptr) - itr->second.LogoutTime) / DAY);
            data << itr->second.Pnote;
            data << itr->second.OFFnote;
        }
    }
    if (session)
    {
        session->SendPacket(&data);
    }
    else
    {
        BroadcastPacket(&data);
    }
    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_ROSTER)");
}

void Guild::Query(WorldSession* session)
{
    WorldPacket data(SMSG_GUILD_QUERY_RESPONSE, (4 + 48 + 10 * 32 + 5 * 4));

    data << uint32(m_Id);
    data << m_Name;

    for (size_t i = 0 ; i < GUILD_RANKS_MAX_COUNT; ++i)
    {
        if (i < m_Ranks.size())
        {
            data << m_Ranks[i].Name;
        }
        else
        {
            data << uint8(0);
        }
    }

    data << uint32(m_EmblemStyle);
    data << uint32(m_EmblemColor);
    data << uint32(m_BorderStyle);
    data << uint32(m_BorderColor);
    data << uint32(m_BackgroundColor);

    session->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_QUERY_RESPONSE)");
}

void Guild::SetEmblem(uint32 emblemStyle, uint32 emblemColor, uint32 borderStyle, uint32 borderColor, uint32 backgroundColor)
{
    m_EmblemStyle = emblemStyle;
    m_EmblemColor = emblemColor;
    m_BorderStyle = borderStyle;
    m_BorderColor = borderColor;
    m_BackgroundColor = backgroundColor;

    CharacterDatabase.PExecute("UPDATE `guild` SET `EmblemStyle`=%u, `EmblemColor`=%u, `BorderStyle`=%u, `BorderColor`=%u, `BackgroundColor`=%u WHERE `guildid` = %u", m_EmblemStyle, m_EmblemColor, m_BorderStyle, m_BorderColor, m_BackgroundColor, m_Id);
}

uint32 Guild::GetAccountsNumber()
{

    if (m_accountsNumber)
    {
        return m_accountsNumber;
    }

    std::set<uint32> accountsIdSet;
    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        accountsIdSet.insert(itr->second.accountId);
    }

    m_accountsNumber = accountsIdSet.size();

    return m_accountsNumber;
}

void Guild::DisplayGuildEventLog(WorldSession* session)
{

    WorldPacket data(MSG_GUILD_EVENT_LOG_QUERY, 0);

    data << uint8(m_GuildEventLog.size());
    for (GuildEventLog::const_iterator itr = m_GuildEventLog.begin(); itr != m_GuildEventLog.end(); ++itr)
    {

        data << uint8(itr->EventType);

        data << MakeGuid(HIGHGUID_PLAYER, itr->PlayerGuid1);

        if (itr->EventType != GUILD_EVENT_LOG_JOIN_GUILD && itr->EventType != GUILD_EVENT_LOG_LEAVE_GUILD)
        {
            data << MakeGuid(HIGHGUID_PLAYER, itr->PlayerGuid2);
        }

        if (itr->EventType == GUILD_EVENT_LOG_PROMOTE_PLAYER || itr->EventType == GUILD_EVENT_LOG_DEMOTE_PLAYER)
        {
            data << uint8(itr->NewRank);
        }

        data << uint32(time(nullptr) - itr->TimeStamp);
    }
    session->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent (MSG_GUILD_EVENT_LOG_QUERY)");
}

void Guild::LoadGuildEventLogFromDB()
{

    QueryResult* result = CharacterDatabase.PQuery("SELECT `LogGuid`, `EventType`, `PlayerGuid1`, `PlayerGuid2`, `NewRank`, `TimeStamp` FROM `guild_eventlog` WHERE `guildid`=%u ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u", m_Id, GUILD_EVENTLOG_MAX_RECORDS);
    if (!result)
    {
        return;
    }
    bool isNextLogGuidSet = false;

    do
    {
        Field* fields = result->Fetch();
        if (!isNextLogGuidSet)
        {
            m_GuildEventLogNextGuid = fields[0].GetUInt32();
            isNextLogGuidSet = true;
        }

        GuildEventLogEntry NewEvent;
        NewEvent.EventType = fields[1].GetUInt8();
        NewEvent.PlayerGuid1 = fields[2].GetUInt32();
        NewEvent.PlayerGuid2 = fields[3].GetUInt32();
        NewEvent.NewRank = fields[4].GetUInt8();
        NewEvent.TimeStamp = fields[5].GetUInt64();

        m_GuildEventLog.push_front(NewEvent);
    }
    while (result->NextRow());
    delete result;
}

void Guild::LogGuildEvent(uint8 EventType, ObjectGuid playerGuid1, ObjectGuid playerGuid2, uint8 newRank)
{
    GuildEventLogEntry NewEvent;

    NewEvent.EventType = EventType;
    NewEvent.PlayerGuid1 = GuidCounter(playerGuid1);
    NewEvent.PlayerGuid2 = GuidCounter(playerGuid2);
    NewEvent.NewRank = newRank;
    NewEvent.TimeStamp = uint32(time(nullptr));

    m_GuildEventLogNextGuid = (m_GuildEventLogNextGuid + 1) % sWorld.getConfig(CONFIG_UINT32_GUILD_EVENT_LOG_COUNT);

    if (m_GuildEventLog.size() >= GUILD_EVENTLOG_MAX_RECORDS)
    {
        m_GuildEventLog.pop_front();
    }

    m_GuildEventLog.push_back(NewEvent);

    CharacterDatabase.PExecute("DELETE FROM `guild_eventlog` WHERE `guildid`='%u' AND `LogGuid`='%u'", m_Id, m_GuildEventLogNextGuid);
    CharacterDatabase.PExecute("INSERT INTO `guild_eventlog` (`guildid`, `LogGuid`, `EventType`, `PlayerGuid1`, `PlayerGuid2`, `NewRank`, `TimeStamp`) VALUES ('%u','%u','%u','%u','%u','%u','" UI64FMTD "')",
        m_Id, m_GuildEventLogNextGuid, uint32(NewEvent.EventType), NewEvent.PlayerGuid1, NewEvent.PlayerGuid2, uint32(NewEvent.NewRank), NewEvent.TimeStamp);
}

void Guild::BroadcastEvent(GuildEvents event, ObjectGuid guid, char const* str1 , char const* str2 , char const* str3 )
{
    uint8 strCount = !str1 ? 0 : (!str2 ? 1 : (!str3 ? 2 : 3));

    WorldPacket data(SMSG_GUILD_EVENT, 1 + 1 + 1 * strCount + ((guid == 0) ? 0 : 8));
    data << uint8(event);
    data << uint8(strCount);

    if (str3)
    {
        data << str1;
        data << str2;
        data << str3;
    }
    else if (str2)
    {
        data << str1;
        data << str2;
    }
    else if (str1)
    {
        data << str1;
    }

    if (!(guid == 0))
    {
        data << guid;
    }

    BroadcastPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_GUILD_EVENT");
}
