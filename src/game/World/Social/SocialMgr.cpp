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

#include "SocialMgr.h"
#include "Policies/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "Player.h"
#include "World.h"
#include "Util.h"
#include "PlayerRegistry.h"

PlayerSocial::PlayerSocial()
{
}

PlayerSocial::~PlayerSocial()
{
    m_playerSocialMap.clear();
}

uint32 PlayerSocial::GetNumberOfSocialsWithFlag(SocialFlag flag)
{

    uint32 counter = 0;

    for (PlayerSocialMap::const_iterator itr = m_playerSocialMap.begin(); itr != m_playerSocialMap.end(); ++itr)
    {
        if (itr->second.Flags & flag)
        {
            ++counter;
        }
    }

    return counter;
}

bool PlayerSocial::AddToSocialList(ObjectGuid friend_guid, bool ignore)
{

    if (ignore)
    {
        if (GetNumberOfSocialsWithFlag(SOCIAL_FLAG_IGNORED) >= SOCIALMGR_IGNORE_LIMIT)
        {
            return false;
        }
    }
    else
    {
        if (GetNumberOfSocialsWithFlag(SOCIAL_FLAG_FRIEND) >= SOCIALMGR_FRIEND_LIMIT)
        {
            return false;
        }
    }

    uint32 flag = SOCIAL_FLAG_FRIEND;
    if (ignore)
    {
        flag = SOCIAL_FLAG_IGNORED;
    }

    PlayerSocialMap::const_iterator itr = m_playerSocialMap.find(GuidCounter(friend_guid));
    if (itr != m_playerSocialMap.end())
    {
        CharacterDatabase.PExecute("UPDATE `character_social` SET `flags` = (`flags` | %u) WHERE `guid` = '%u' AND `friend` = '%u'", flag, m_playerLowGuid, GuidCounter(friend_guid));
        m_playerSocialMap[GuidCounter(friend_guid)].Flags |= flag;
    }
    else
    {
        CharacterDatabase.PExecute("INSERT INTO `character_social` (`guid`, `friend`, `flags`) VALUES ('%u', '%u', '%u')", m_playerLowGuid, GuidCounter(friend_guid), flag);
        FriendInfo fi;
        fi.Flags |= flag;
        m_playerSocialMap[GuidCounter(friend_guid)] = fi;
    }
    return true;
}

void PlayerSocial::RemoveFromSocialList(ObjectGuid friend_guid, bool ignore)
{
    PlayerSocialMap::iterator itr = m_playerSocialMap.find(GuidCounter(friend_guid));
    if (itr == m_playerSocialMap.end())
    {
        return;
    }

    uint32 flag = SOCIAL_FLAG_FRIEND;
    if (ignore)
    {
        flag = SOCIAL_FLAG_IGNORED;
    }

    itr->second.Flags &= ~flag;
    if (itr->second.Flags == 0)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_social` WHERE `guid` = '%u' AND `friend` = '%u'", m_playerLowGuid, GuidCounter(friend_guid));
        m_playerSocialMap.erase(itr);
    }
    else
    {
        CharacterDatabase.PExecute("UPDATE `character_social` SET `flags` = (`flags` & ~%u) WHERE `guid` = '%u' AND `friend` = '%u'", flag, m_playerLowGuid, GuidCounter(friend_guid));
    }
}

struct friend_
{
    uint64 guid;
    uint8 status;

    uint32 area;
    uint32 level;
    uint32 class_;
};

void PlayerSocial::SendFriendList(Player* plr)
{

    if (!plr || plr->GetGUIDLow() != m_playerLowGuid)
    {
        return;
    }

    uint32 size = GetNumberOfSocialsWithFlag(SOCIAL_FLAG_FRIEND);

    WorldPacket data(SMSG_FRIEND_LIST, (1 + size * 25));
    data << uint8(size);

    for (PlayerSocialMap::iterator itr = m_playerSocialMap.begin(); itr != m_playerSocialMap.end(); ++itr)
    {
        if (itr->second.Flags & SOCIAL_FLAG_FRIEND)
        {
            sSocialMgr.GetFriendInfo(plr, itr->first, itr->second);

            data << MakeGuid(HIGHGUID_PLAYER, itr->first);
            data << uint8(itr->second.Status);
            if (itr->second.Status)
            {
                data << uint32(itr->second.Area);
                data << uint32(itr->second.Level);
                data << uint32(itr->second.Class);
            }
        }
    }

    plr->GetSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_FRIEND_LIST");
}

void PlayerSocial::SendIgnoreList(Player* plr)
{
    if (!plr || plr->GetGUIDLow() != m_playerLowGuid)
    {
        return;
    }

    uint32 size = GetNumberOfSocialsWithFlag(SOCIAL_FLAG_IGNORED);

    WorldPacket data(SMSG_IGNORE_LIST, (1 +
        size * 8));
    data << uint8(size);

    for (PlayerSocialMap::iterator itr = m_playerSocialMap.begin(); itr != m_playerSocialMap.end(); ++itr)
    {
        if (itr->second.Flags & SOCIAL_FLAG_IGNORED)
        {
            data << MakeGuid(HIGHGUID_PLAYER, itr->first);
        }
    }

    plr->GetSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_IGNORE_LIST");
}

bool PlayerSocial::HasFriend(ObjectGuid friend_guid)
{
    PlayerSocialMap::const_iterator itr = m_playerSocialMap.find(GuidCounter(friend_guid));
    if (itr != m_playerSocialMap.end())
    {
        return itr->second.Flags & SOCIAL_FLAG_FRIEND;
    }
    return false;
}

bool PlayerSocial::HasIgnore(ObjectGuid ignore_guid)
{
    PlayerSocialMap::const_iterator itr = m_playerSocialMap.find(GuidCounter(ignore_guid));
    if (itr != m_playerSocialMap.end())
    {
        return itr->second.Flags & SOCIAL_FLAG_IGNORED;
    }
    return false;
}

SocialMgr::SocialMgr()
{
}

SocialMgr::~SocialMgr()
{
}

void SocialMgr::GetFriendInfo(Player* player, uint32 friend_lowguid, FriendInfo& friendInfo)
{

    if (!player || !player->GetSocial())
    {
        return;
    }

    Player* pFriend = sPlayerRegistry.Find(MakeGuid(HIGHGUID_PLAYER, friend_lowguid));

    Team team = player->GetTeam();
    AccountTypes security = player->GetSession()->GetSecurity();
    bool allowTwoSideWhoList = sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST);
    AccountTypes gmLevelInWhoList = AccountTypes(sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST));

    PlayerSocialMap::iterator itr = player->GetSocial()->m_playerSocialMap.find(friend_lowguid);
    if (itr != player->GetSocial()->m_playerSocialMap.end())
    {

        if (pFriend && pFriend->GetName() &&
            (security > SEC_PLAYER ||
            ((pFriend->GetTeam() == team || allowTwoSideWhoList) && (pFriend->GetSession()->GetSecurity() <= gmLevelInWhoList))) &&
            pFriend->IsVisibleGloballyFor(player))
        {
            friendInfo.Status = FRIEND_STATUS_ONLINE;
            if (pFriend->isAFK())
            {
                friendInfo.Status = FRIEND_STATUS_AFK;
            }
            if (pFriend->isDND())
            {
                friendInfo.Status = FRIEND_STATUS_DND;
            }
            friendInfo.Area = pFriend->GetTerrain()->GetZoneId(pFriend->Where().X(), pFriend->Where().Y(), pFriend->Where().Z());
            friendInfo.Level = pFriend->getLevel();
            friendInfo.Class = pFriend->getClass();
        }
        else
        {
            friendInfo.Status = FRIEND_STATUS_OFFLINE;
            friendInfo.Area = 0;
            friendInfo.Level = 0;
            friendInfo.Class = 0;
        }
    }
}

void SocialMgr::MakeFriendStatusPacket(FriendsResult result, uint32 guid, WorldPacket* data)
{
    data->Initialize(SMSG_FRIEND_STATUS, 5);
    *data << uint8(result);
    *data << MakeGuid(HIGHGUID_PLAYER, guid);
}

void SocialMgr::SendFriendStatus(Player* player, FriendsResult result, ObjectGuid friend_guid, bool broadcast)
{
    uint32 friend_lowguid = GuidCounter(friend_guid);

    FriendInfo fi;

    WorldPacket data;
    MakeFriendStatusPacket(result, friend_lowguid, &data);
    GetFriendInfo(player, friend_lowguid, fi);

    switch (result)
    {
        case FRIEND_ADDED_ONLINE:
        case FRIEND_ONLINE:
            data << uint8(fi.Status);
            data << uint32(fi.Area);
            data << uint32(fi.Level);
            data << uint32(fi.Class);
            break;
        default:
            break;
    }

    if (broadcast)
    {
        BroadcastToFriendListers(player, &data);
    }
    else
    {
        player->GetSession()->SendPacket(&data);
    }
}

void SocialMgr::BroadcastToFriendListers(Player* player, WorldPacket* packet)
{
    if (!player)
    {
        return;
    }

    Team team = player->GetTeam();
    AccountTypes security = player->GetSession()->GetSecurity();
    uint32 guid     = player->GetGUIDLow();
    AccountTypes gmLevelInWhoList = AccountTypes(sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST));
    bool allowTwoSideWhoList = sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST);

    for (SocialMap::const_iterator itr = m_socialMap.begin(); itr != m_socialMap.end(); ++itr)
    {
        PlayerSocialMap::const_iterator itr2 = itr->second.m_playerSocialMap.find(guid);
        if (itr2 != itr->second.m_playerSocialMap.end() && (itr2->second.Flags & SOCIAL_FLAG_FRIEND))
        {
            Player* pFriend = sPlayerRegistry.Find(MakeGuid(HIGHGUID_PLAYER, itr->first));

            if (pFriend && pFriend->IsInWorld() &&
                (pFriend->GetSession()->GetSecurity() > SEC_PLAYER ||
                ((pFriend->GetTeam() == team || allowTwoSideWhoList) && security <= gmLevelInWhoList)) &&
                player->IsVisibleGloballyFor(pFriend))
            {
                pFriend->GetSession()->SendPacket(packet);
            }
        }
    }
}

PlayerSocial* SocialMgr::LoadFromDB(QueryResult* result, ObjectGuid guid)
{
    PlayerSocial* social = &m_socialMap[GuidCounter(guid)];
    social->SetPlayerGuid(guid);

    if (!result)
    {
        return social;
    }

    uint32 friendCounter = 0, ignoreCounter = 0;

    do
    {
        Field* fields  = result->Fetch();

        uint32 friend_guid = fields[0].GetUInt32();
        uint32 flags = fields[1].GetUInt32();

        if ((flags & SOCIAL_FLAG_IGNORED) && ignoreCounter >= SOCIALMGR_IGNORE_LIMIT)
        {
            continue;
        }
        if ((flags & SOCIAL_FLAG_FRIEND) && friendCounter >= SOCIALMGR_FRIEND_LIMIT)
        {
            continue;
        }

        social->m_playerSocialMap[friend_guid] = FriendInfo(flags);

        if (flags & SOCIAL_FLAG_IGNORED)
        {
            ++ignoreCounter;
        }
        else
        {
            ++friendCounter;
        }
    }
    while (result->NextRow());
    delete result;
    return social;
}
