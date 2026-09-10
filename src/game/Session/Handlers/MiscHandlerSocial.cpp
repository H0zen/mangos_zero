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
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "World.h"
#include "CinematicFlyover.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "SocialAnswers.h"
#include "Auth/BigNumber.h"
#include "Auth/Sha1.h"
#include "UpdateData.h"
#include "LootMgr.h"
#include "Chat.h"
#include "ScriptMgr.h"
#include "PlayerRegistry.h"
#include "Object.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Pet.h"
#include "SocialMgr.h"
#include "DBCEnums.h"
#include <zlib.h>

void social::AddFriend(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ADD_FRIEND");

    std::string friendName = session.GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN);

    recv_data >> friendName;

    if (!normalizePlayerName(friendName))
    {
        return;
    }

    CharacterDatabase.escape_string(friendName);

    DEBUG_LOG("WORLD: %s asked to add friend : '%s'",
        session.GetPlayer()->GetName(), friendName.c_str());

    uint32 accountId = session.GetAccountId();
    CharacterDatabase.AsyncPQuery([accountId](QueryResult* result)
                                  {
                                      WorldSession::HandleAddFriendOpcodeCallBack(result, accountId);
                                  }, "SELECT `guid`, `race` FROM `characters` WHERE `name` = '%s'", friendName.c_str());
}

void WorldSession::HandleAddFriendOpcodeCallBack(QueryResult* result, uint32 accountId)
{
    if (!result)
    {
        return;
    }

    uint32 friendLowGuid = (*result)[0].GetUInt32();
    ObjectGuid friendGuid = MakeGuid(HIGHGUID_PLAYER, friendLowGuid);
    Team team = Player::TeamForRace((*result)[1].GetUInt8());

    delete result;

    WorldSession* session = sWorld.FindSession(accountId);
    if (!session)
    {
        return;
    }

    Player* player = session->GetPlayer();
    if (!player)
    {
        return;
    }

    FriendsResult friendResult = FRIEND_NOT_FOUND;
    if (friendGuid)
    {
        if (friendGuid == player->GetObjectGuid())
        {
            friendResult = FRIEND_SELF;
        }
        else if (player->GetTeam() != team && !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ADD_FRIEND) && session->GetSecurity() < SEC_MODERATOR)
        {
            friendResult = FRIEND_ENEMY;
        }
        else if (player->GetSocial()->HasFriend(friendGuid))
        {
            friendResult = FRIEND_ALREADY;
        }
        else
        {
            Player* pFriend = sPlayerRegistry.Find(friendGuid);
            if (pFriend && pFriend->IsInWorld() && pFriend->IsVisibleGloballyFor(player))
            {
                friendResult = FRIEND_ADDED_ONLINE;
            }
            else
            {
                friendResult = FRIEND_ADDED_OFFLINE;
            }

            if (!player->GetSocial()->AddToSocialList(friendGuid, false))
            {
                friendResult = FRIEND_LIST_FULL;
                DEBUG_LOG("WORLD: %s's friend list is full.", player->GetName());
            }
        }
    }

    sSocialMgr.SendFriendStatus(player, friendResult, friendGuid, false);

    DEBUG_LOG("WORLD: Sent (SMSG_FRIEND_STATUS)");
}

void social::DelFriend(Player& who, WorldPacket& recv_data)
{
    ObjectGuid friendGuid = 0;

    DEBUG_LOG("WORLD: Received opcode CMSG_DEL_FRIEND");

    recv_data >> friendGuid;

    who.GetSocial()->RemoveFromSocialList(friendGuid, false);

    sSocialMgr.SendFriendStatus(&who, FRIEND_REMOVED, friendGuid, false);

    DEBUG_LOG("WORLD: Sent motd (SMSG_FRIEND_STATUS)");
}

void social::AddIgnore(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ADD_IGNORE");

    std::string IgnoreName = session.GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN);

    recv_data >> IgnoreName;

    if (!normalizePlayerName(IgnoreName))
    {
        return;
    }

    CharacterDatabase.escape_string(IgnoreName);

    DEBUG_LOG("WORLD: %s asked to Ignore: '%s'",
        session.GetPlayer()->GetName(), IgnoreName.c_str());

    uint32 accountId = session.GetAccountId();
    CharacterDatabase.AsyncPQuery([accountId](QueryResult* result)
                                  {
                                      WorldSession::HandleAddIgnoreOpcodeCallBack(result, accountId);
                                  }, "SELECT `guid` FROM `characters` WHERE `name` = '%s'", IgnoreName.c_str());
}

void WorldSession::HandleAddIgnoreOpcodeCallBack(QueryResult* result, uint32 accountId)
{
    if (!result)
    {
        return;
    }

    uint32 ignoreLowGuid = (*result)[0].GetUInt32();
    ObjectGuid ignoreGuid = MakeGuid(HIGHGUID_PLAYER, ignoreLowGuid);

    delete result;

    WorldSession* session = sWorld.FindSession(accountId);
    if (!session)
    {
        return;
    }

    Player* player = session->GetPlayer();
    if (!player)
    {
        return;
    }

    FriendsResult ignoreResult = FRIEND_IGNORE_NOT_FOUND;
    if (ignoreGuid)
    {
        if (ignoreGuid == player->GetObjectGuid())
        {
            ignoreResult = FRIEND_IGNORE_SELF;
        }
        else if (player->GetSocial()->HasIgnore(ignoreGuid))
        {
            ignoreResult = FRIEND_IGNORE_ALREADY;
        }
        else
        {
            ignoreResult = FRIEND_IGNORE_ADDED;

            if (!player->GetSocial()->AddToSocialList(ignoreGuid, true))
            {
                ignoreResult = FRIEND_IGNORE_FULL;
            }
        }
    }

    sSocialMgr.SendFriendStatus(player, ignoreResult, ignoreGuid, false);

    DEBUG_LOG("WORLD: Sent (SMSG_FRIEND_STATUS)");
}

void social::DelIgnore(Player& who, WorldPacket& recv_data)
{
    ObjectGuid ignoreGuid = 0;

    DEBUG_LOG("WORLD: Received opcode CMSG_DEL_IGNORE");

    recv_data >> ignoreGuid;

    who.GetSocial()->RemoveFromSocialList(ignoreGuid, true);

    sSocialMgr.SendFriendStatus(&who, FRIEND_IGNORE_REMOVED, ignoreGuid, false);

    DEBUG_LOG("WORLD: Sent motd (SMSG_FRIEND_STATUS)");
}
