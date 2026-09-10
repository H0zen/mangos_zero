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
#include "Guild.h"
#include "GuildMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "SocialMgr.h"
#include "Util.h"
#include "Language.h"
#include "World.h"

void Guild::CreateRank(std::string name_, uint32 rights)
{
    if (m_Ranks.size() >= GUILD_RANKS_MAX_COUNT)
    {
        return;
    }

    uint32 new_rank_id = m_Ranks.size();

    AddRank(name_, rights);

    CharacterDatabase.escape_string(name_);
    CharacterDatabase.PExecute("INSERT INTO `guild_rank` (`guildid`,`rid`,`rname`,`rights`) VALUES ('%u', '%u', '%s', '%u')", m_Id, new_rank_id, name_.c_str(), rights);
}

void Guild::AddRank(const std::string& name_, uint32 rights)
{
    m_Ranks.push_back(RankInfo(name_, rights));
}

void Guild::DelRank()
{

    if (m_Ranks.size() <= GUILD_RANKS_MIN_COUNT)
    {
        return;
    }

    uint32 rank = GetLowestRank();
    CharacterDatabase.PExecute("DELETE FROM `guild_rank` WHERE `rid`>='%u' AND `guildid`='%u'", rank, m_Id);

    m_Ranks.pop_back();
}

std::string Guild::GetRankName(uint32 rankId)
{
    if (rankId >= m_Ranks.size())
    {
        return "<unknown>";
    }

    return m_Ranks[rankId].Name;
}

uint32 Guild::GetRankRights(uint32 rankId)
{
    if (rankId >= m_Ranks.size())
    {
        return 0;
    }

    return m_Ranks[rankId].Rights;
}

void Guild::SetRankName(uint32 rankId, std::string name_)
{
    if (rankId >= m_Ranks.size())
    {
        return;
    }

    m_Ranks[rankId].Name = name_;

    CharacterDatabase.escape_string(name_);
    CharacterDatabase.PExecute("UPDATE `guild_rank` SET `rname`='%s' WHERE `rid`='%u' AND `guildid`='%u'", name_.c_str(), rankId, m_Id);
}

void Guild::SetRankRights(uint32 rankId, uint32 rights)
{
    if (rankId >= m_Ranks.size())
    {
        return;
    }

    m_Ranks[rankId].Rights = rights;

    CharacterDatabase.PExecute("UPDATE `guild_rank` SET `rights`='%u' WHERE `rid`='%u' AND `guildid`='%u'", rights, rankId, m_Id);
}
