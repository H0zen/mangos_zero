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

/// What the client asks about the things he carries.
namespace items
{
    void SplitItem(Player& who, WorldPacket& packet);
    void SwapInvItem(Player& who, WorldPacket& packet);
    void AutoEquipItemSlot(Player& who, WorldPacket& packet);
    void SwapItem(Player& who, WorldPacket& packet);
    void AutoEquipItem(Player& who, WorldPacket& packet);
    void DestroyItem(Player& who, WorldPacket& packet);
    void ItemQuerySingle(WorldSession& session, WorldPacket& packet);
    void ReadItem(Player& who, WorldPacket& packet);
    void BuyItemInSlot(Player& who, WorldPacket& packet);
    void ItemNameQuery(WorldSession& session, WorldPacket& packet);
}
