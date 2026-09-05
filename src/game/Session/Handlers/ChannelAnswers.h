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

class Player;
class WorldPacket;
class WorldSession;

/// What the client asks about chat channels.
namespace channels
{
    void JoinChannel(WorldSession& session, WorldPacket& packet);
    void LeaveChannel(Player& who, WorldPacket& packet);
    void ChannelList(Player& who, WorldPacket& packet);
    void ChannelPassword(Player& who, WorldPacket& packet);
    void ChannelSetOwner(Player& who, WorldPacket& packet);
    void ChannelOwner(Player& who, WorldPacket& packet);
    void ChannelModerator(Player& who, WorldPacket& packet);
    void ChannelUnmoderator(Player& who, WorldPacket& packet);
    void ChannelMute(Player& who, WorldPacket& packet);
    void ChannelUnmute(Player& who, WorldPacket& packet);
    void ChannelInvite(Player& who, WorldPacket& packet);
    void ChannelKick(Player& who, WorldPacket& packet);
    void ChannelBan(Player& who, WorldPacket& packet);
    void ChannelUnban(Player& who, WorldPacket& packet);
    void ChannelAnnouncements(Player& who, WorldPacket& packet);
    void ChannelModerate(Player& who, WorldPacket& packet);
    void ChannelDisplayListQuery(Player& who, WorldPacket& packet);
    void GetChannelMemberCount(Player& who, WorldPacket& packet);
    void SetChannelWatch(Player& who, WorldPacket& packet);
}
