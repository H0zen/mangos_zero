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

/// What the client asks about his party or raid.
namespace groups
{
    void GroupInvite(Player& who, WorldPacket& packet);
    void GroupAccept(Player& who, WorldPacket& packet);
    void GroupDecline(Player& who, WorldPacket& packet);
    void GroupUninviteGuid(Player& who, WorldPacket& packet);
    void GroupUninvite(Player& who, WorldPacket& packet);
    void GroupSetLeader(Player& who, WorldPacket& packet);
    void GroupDisband(Player& who, WorldPacket& packet);
    void LootRules(Player& who, WorldPacket& packet);
    void LootRoll(Player& who, WorldPacket& packet);
    void MinimapPing(Player& who, WorldPacket& packet);
    void RandomRoll(Player& who, WorldPacket& packet);
    void RaidTargetUpdate(Player& who, WorldPacket& packet);
    void GroupRaidConvert(Player& who, WorldPacket& packet);
    void GroupChangeSubGroup(Player& who, WorldPacket& packet);
    void GroupAssistantLeader(Player& who, WorldPacket& packet);
    void PartyAssignment(Player& who, WorldPacket& packet);
    void RaidReadyCheck(Player& who, WorldPacket& packet);
    void RaidReadyCheckFinished(Player& who, WorldPacket& packet);
    void RequestPartyMemberStats(Player& who, WorldPacket& packet);
    void RequestRaidInfo(Player& who, WorldPacket& packet);
    void OptOutOfLoot(WorldSession& session, WorldPacket& packet);
}
