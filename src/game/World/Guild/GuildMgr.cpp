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
#include "GuildMgr.h"
#include "Guild.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "ProgressBar.h"
#include "World.h"

GuildMgr::GuildMgr()
{
}

GuildMgr::~GuildMgr()
{
    for (GuildMap::iterator itr = m_GuildMap.begin(); itr != m_GuildMap.end(); ++itr)
    {
        delete itr->second;
    }
}

void GuildMgr::AddGuild(Guild* guild)
{
    m_GuildMap[guild->GetId()] = guild;
}

void GuildMgr::RemoveGuild(uint32 guildId)
{
    m_GuildMap.erase(guildId);
}

Guild* GuildMgr::GetGuildById(uint32 guildId) const
{
    GuildMap::const_iterator itr = m_GuildMap.find(guildId);
    if (itr != m_GuildMap.end())
    {
        return itr->second;
    }

    return nullptr;
}

Guild* GuildMgr::GetGuildByName(std::string const& name) const
{
    for (GuildMap::const_iterator itr = m_GuildMap.begin(); itr != m_GuildMap.end(); ++itr)
    {
        if (itr->second->GetName() == name)
        {
            return itr->second;
        }
    }
    return nullptr;
}

Guild* GuildMgr::GetGuildByLeader(ObjectGuid const& guid) const
{
    for (GuildMap::const_iterator itr = m_GuildMap.begin(); itr != m_GuildMap.end(); ++itr)
    {
        if (itr->second->GetLeaderGuid() == guid)
        {
            return itr->second;
        }
    }
    return nullptr;
}

std::string GuildMgr::GetGuildNameById(uint32 guildId) const
{
    GuildMap::const_iterator itr = m_GuildMap.find(guildId);
    if (itr != m_GuildMap.end())
    {
        return itr->second->GetName();
    }

    return "";
}

void GuildMgr::LoadGuilds()
{
    Guild* newGuild;
    uint32 count = 0;

    QueryResult* result = CharacterDatabase.Query(

            "SELECT `guild`.`guildid`,`guild`.`name`,`leaderguid`,`EmblemStyle`,`EmblemColor`,`BorderStyle`,`BorderColor`,"

            "`BackgroundColor`,`info`,`motd`,`createdate` FROM `guild` ORDER BY `guildid` ASC");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u guild definitions", count);
        sLog.outString();
        return;
    }

    QueryResult* guildRanksResult   = CharacterDatabase.Query("SELECT `guildid`,`rid`,`rname`,`rights` FROM `guild_rank` ORDER BY `guildid` ASC, `rid` ASC");

    QueryResult* guildMembersResult = CharacterDatabase.Query(

            "SELECT `guildid`,`guild_member`.`guid`,`rank`,`pnote`,`offnote`,"

            "`characters`.`name`, `characters`.`level`, `characters`.`class`, `characters`.`zone`, `characters`.`logout_time`, `characters`.`account` "
            "FROM `guild_member` LEFT JOIN `characters` ON `characters`.`guid` = `guild_member`.`guid` ORDER BY `guildid` ASC");

    BarGoLink bar(result->GetRowCount());

    do
    {

        bar.step();
        ++count;

        newGuild = new Guild;
        if (!newGuild->LoadGuildFromDB(result) ||
            !newGuild->LoadRanksFromDB(guildRanksResult) ||
            !newGuild->LoadMembersFromDB(guildMembersResult) ||
            !newGuild->CheckGuildStructure())
        {
            newGuild->Disband();
            delete newGuild;
            continue;
        }
        newGuild->LoadGuildEventLogFromDB();
        AddGuild(newGuild);
    }
    while (result->NextRow());

    delete result;
    delete guildRanksResult;
    delete guildMembersResult;

    CharacterDatabase.PExecute("DELETE FROM `guild_eventlog` WHERE `LogGuid` > '%u'", sWorld.getConfig(CONFIG_UINT32_GUILD_EVENT_LOG_COUNT));

    sLog.outString(">> Loaded %u guild definitions", count);
    sLog.outString();
}
