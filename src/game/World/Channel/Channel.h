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

#pragma once

#include "Platform/Define.h"
#include <list>
#include <string>
#include "ObjectGuid.h"
#include "WorldPacket.h"
#include "Player.h"

#include <map>

enum ChatNotify
{
    CHAT_JOINED_NOTICE                = 0x00,
    CHAT_LEFT_NOTICE                  = 0x01,

    CHAT_YOU_JOINED_NOTICE            = 0x02,

    CHAT_YOU_LEFT_NOTICE              = 0x03,
    CHAT_WRONG_PASSWORD_NOTICE        = 0x04,
    CHAT_NOT_MEMBER_NOTICE            = 0x05,
    CHAT_NOT_MODERATOR_NOTICE         = 0x06,
    CHAT_PASSWORD_CHANGED_NOTICE      = 0x07,
    CHAT_OWNER_CHANGED_NOTICE         = 0x08,
    CHAT_PLAYER_NOT_FOUND_NOTICE      = 0x09,
    CHAT_NOT_OWNER_NOTICE             = 0x0A,
    CHAT_CHANNEL_OWNER_NOTICE         = 0x0B,
    CHAT_MODE_CHANGE_NOTICE           = 0x0C,
    CHAT_ANNOUNCEMENTS_ON_NOTICE      = 0x0D,
    CHAT_ANNOUNCEMENTS_OFF_NOTICE     = 0x0E,
    CHAT_MODERATION_ON_NOTICE         = 0x0F,
    CHAT_MODERATION_OFF_NOTICE        = 0x10,
    CHAT_MUTED_NOTICE                 = 0x11,
    CHAT_PLAYER_KICKED_NOTICE         = 0x12,
    CHAT_BANNED_NOTICE                = 0x13,
    CHAT_PLAYER_BANNED_NOTICE         = 0x14,
    CHAT_PLAYER_UNBANNED_NOTICE       = 0x15,
    CHAT_PLAYER_NOT_BANNED_NOTICE     = 0x16,
    CHAT_PLAYER_ALREADY_MEMBER_NOTICE = 0x17,
    CHAT_INVITE_NOTICE                = 0x18,
    CHAT_INVITE_WRONG_FACTION_NOTICE  = 0x19,
    CHAT_WRONG_FACTION_NOTICE         = 0x1A,
    CHAT_INVALID_NAME_NOTICE          = 0x1B,
    CHAT_NOT_MODERATED_NOTICE         = 0x1C,
    CHAT_PLAYER_INVITED_NOTICE        = 0x1D,
    CHAT_PLAYER_INVITE_BANNED_NOTICE  = 0x1E,
    CHAT_THROTTLED_NOTICE             = 0x1F,
};

enum ChannelIds
{
    CHANNEL_ID_GENERAL           = 1,
    CHANNEL_ID_TRADE             = 2,
    CHANNEL_ID_LOCAL_DEFENSE     = 22,
    CHANNEL_ID_WORLD_DEFENSE     = 23,
    CHANNEL_ID_LOOKING_FOR_GROUP = 24,
    CHANNEL_ID_GUILD_RECRUITMENT = 25
};

class Channel
{
    enum ChannelFlags
    {
        CHANNEL_FLAG_NONE       = 0x00,
        CHANNEL_FLAG_CUSTOM     = 0x01,

        CHANNEL_FLAG_TRADE      = 0x04,
        CHANNEL_FLAG_NOT_LFG    = 0x08,
        CHANNEL_FLAG_GENERAL    = 0x10,
        CHANNEL_FLAG_CITY       = 0x20,
        CHANNEL_FLAG_LFG        = 0x40,
        CHANNEL_FLAG_VOICE      = 0x80

    };

    enum ChannelDBCFlags
    {
        CHANNEL_DBC_FLAG_NONE       = 0x00000,
        CHANNEL_DBC_FLAG_INITIAL    = 0x00001,
        CHANNEL_DBC_FLAG_ZONE_DEP   = 0x00002,
        CHANNEL_DBC_FLAG_GLOBAL     = 0x00004,
        CHANNEL_DBC_FLAG_TRADE      = 0x00008,
        CHANNEL_DBC_FLAG_CITY_ONLY  = 0x00010,
        CHANNEL_DBC_FLAG_CITY_ONLY2 = 0x00020,
        CHANNEL_DBC_FLAG_DEFENSE    = 0x10000,
        CHANNEL_DBC_FLAG_GUILD_REQ  = 0x20000,
        CHANNEL_DBC_FLAG_LFG        = 0x40000
    };

    enum ChannelMemberFlags
    {
        MEMBER_FLAG_NONE        = 0x00,
        MEMBER_FLAG_OWNER       = 0x01,
        MEMBER_FLAG_MODERATOR   = 0x02,
        MEMBER_FLAG_VOICED      = 0x04,
        MEMBER_FLAG_MUTED       = 0x08,
        MEMBER_FLAG_CUSTOM      = 0x10,
        MEMBER_FLAG_MIC_MUTED   = 0x20,

    };

    struct PlayerInfo
    {
        ObjectGuid player = 0;
        uint8 flags;

        bool HasFlag(uint8 flag) { return flags & flag; }
        void SetFlag(uint8 flag) { if (!HasFlag(flag)) { flags |= flag; } }
        bool IsOwner()
        {
            return flags & MEMBER_FLAG_OWNER;
        }
        void SetOwner(bool state)
        {
            if (state)
            {
                flags |= MEMBER_FLAG_OWNER;
            }
            else
            {
                flags &= ~MEMBER_FLAG_OWNER;
            }
        }
        bool IsModerator()
        {
            return flags & MEMBER_FLAG_MODERATOR;
        }
        void SetModerator(bool state)
        {
            if (state)
            {
                flags |= MEMBER_FLAG_MODERATOR;
            }
            else
            {
                flags &= ~MEMBER_FLAG_MODERATOR;
            }
        }
        bool IsMuted()
        {
            return flags & MEMBER_FLAG_MUTED;
        }
        void SetMuted(bool state)
        {
            if (state)
            {
                flags |= MEMBER_FLAG_MUTED;
            }
            else
            {
                flags &= ~MEMBER_FLAG_MUTED;
            }
        }
    };

    public:
        Channel(const std::string& name);
        std::string GetName() const { return m_name; }
        uint32 GetChannelId() const { return m_channelId; }
        bool IsConstant() const { return m_channelId != 0; }
        bool IsAnnounce() const { return m_announce; }
        bool IsLFG() const { return GetFlags() & CHANNEL_FLAG_LFG; }
        std::string GetPassword() const { return m_password; }
        void SetPassword(const std::string& npassword) { m_password = npassword; }
        void SetAnnounce(bool nannounce) { m_announce = nannounce; }
        uint32 GetNumPlayers() const { return m_players.size(); }
        uint8 GetFlags() const { return m_flags; }
        bool HasFlag(uint8 flag) { return m_flags & flag; }

        void Join(Player* player, const char* password);
        void Leave(Player* player, bool send = true);
        void KickOrBan(Player* player, const char* targetName, bool ban);
        void Kick(Player* player, const char* targetName) { KickOrBan(player, targetName, false); }
        void Ban(Player* player, const char* targetName) { KickOrBan(player, targetName, true); }
        void UnBan(Player* player, const char* targetName);
        void Password(Player* player, const char* password);
        void SetMode(Player* player, const char* targetName, bool moderator, bool set);
        void SetOwner(ObjectGuid guid, bool exclaim = true);
        void SetOwner(Player* player, const char* targetName);
        void SendWhoOwner(Player* player);
        void SetModerator(Player* player, const char* targetName) { SetMode(player, targetName, true, true); }
        void UnsetModerator(Player* player, const char* targetName) { SetMode(player, targetName, true, false); }
        void SetMute(Player* player, const char* targetName) { SetMode(player, targetName, false, true); }
        void UnsetMute(Player* player, const char* targetName) { SetMode(player, targetName, false, false); }
        void List(Player* player);
        void Announce(Player* player);
        void Moderate(Player* player);
        void Say(Player* player, const char* text, uint32 lang);
        void Invite(Player* player, const char* targetName);
        void Voice(ObjectGuid guid1, ObjectGuid guid2);
        void DeVoice(ObjectGuid guid1, ObjectGuid guid2);
        void JoinNotify(ObjectGuid guid);
        void LeaveNotify(ObjectGuid guid);

        static const uint8 SPEAK_IN_LOCALDEFENSE_RANK = 4 + 9;

        static const uint8 SPEAK_IN_WORLDDEFENSE_RANK = 4 + 10;

        static void MakeNotOnPacket(WorldPacket* data, const std::string &name);
    private:

        void MakeNotifyPacket(WorldPacket* data, uint8 notify_type);

        void MakeJoined(WorldPacket* data, ObjectGuid guid);
        void MakeLeft(WorldPacket* data, ObjectGuid guid);
        void MakeYouJoined(WorldPacket* data);
        void MakeYouLeft(WorldPacket* data);
        void MakeWrongPassword(WorldPacket* data);
        void MakeNotMember(WorldPacket* data);
        void MakeNotModerator(WorldPacket* data);
        void MakePasswordChanged(WorldPacket* data, ObjectGuid guid);
        void MakeOwnerChanged(WorldPacket* data, ObjectGuid guid);
        void MakePlayerNotFound(WorldPacket* data, const std::string& name);
        void MakeNotOwner(WorldPacket* data);
        void MakeChannelOwner(WorldPacket* data);
        void MakeModeChange(WorldPacket* data, ObjectGuid guid, uint8 oldflags);
        void MakeAnnouncementsOn(WorldPacket* data, ObjectGuid guid);
        void MakeAnnouncementsOff(WorldPacket* data, ObjectGuid guid);
        void MakeModerationOn(WorldPacket* data, ObjectGuid guid);
        void MakeModerationOff(WorldPacket* data, ObjectGuid guid);
        void MakeMuted(WorldPacket* data);
        void MakePlayerKicked(WorldPacket* data, ObjectGuid target, ObjectGuid source);
        void MakeBanned(WorldPacket* data);
        void MakePlayerBanned(WorldPacket* data, ObjectGuid target, ObjectGuid source);
        void MakePlayerUnbanned(WorldPacket* data, ObjectGuid target, ObjectGuid source);
        void MakePlayerNotBanned(WorldPacket* data, const std::string& name);
        void MakePlayerAlreadyMember(WorldPacket* data, ObjectGuid guid);
        void MakeInvite(WorldPacket* data, ObjectGuid guid);
        void MakeInviteWrongFaction(WorldPacket* data);
        void MakeWrongFaction(WorldPacket* data);
        void MakeInvalidName(WorldPacket* data);
        void MakeNotModerated(WorldPacket* data);
        void MakePlayerInvited(WorldPacket* data, const std::string& name);
        void MakePlayerInviteBanned(WorldPacket* data, const std::string& name);
        void MakeThrottled(WorldPacket* data);

        void SendToAll(WorldPacket* data, ObjectGuid guid = 0);
        void SendToOne(WorldPacket* data, ObjectGuid who);

        bool IsOn(ObjectGuid who) const { return m_players.find(who) != m_players.end(); }
        bool IsBanned(ObjectGuid guid) const { return m_banned.find(guid) != m_banned.end(); }

        uint8 GetPlayerFlags(ObjectGuid guid) const
        {
            PlayerList::const_iterator p_itr = m_players.find(guid);
            if (p_itr == m_players.end())
            {
                return 0;
            }

            return p_itr->second.flags;
        }

        void SetModerator(ObjectGuid guid, bool set)
        {
            if (m_players[guid].IsModerator() != set)
            {
                uint8 oldFlag = GetPlayerFlags(guid);
                m_players[guid].SetModerator(set);

                WorldPacket data;
                MakeModeChange(&data, guid, oldFlag);
                SendToAll(&data);
            }
        }

        void SetMute(ObjectGuid guid, bool set)
        {
            if (m_players[guid].IsMuted() != set)
            {
                uint8 oldFlag = GetPlayerFlags(guid);
                m_players[guid].SetMuted(set);

                WorldPacket data;
                MakeModeChange(&data, guid, oldFlag);
                SendToAll(&data);
            }
        }

    private:
        bool        m_announce;
        bool        m_moderate;
        std::string m_name;
        std::string m_password;
        uint8       m_flags;
        uint32      m_channelId;
        ObjectGuid  m_ownerGuid = 0;

        typedef     std::map<ObjectGuid, PlayerInfo> PlayerList;
        PlayerList  m_players;
        GuidSet m_banned;
};
