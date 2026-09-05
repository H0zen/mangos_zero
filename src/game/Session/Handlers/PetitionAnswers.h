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

/// What the client asks about a charter being signed.
namespace petitions
{
    void PetitionBuy(WorldSession& session, WorldPacket& packet);
    void PetitionShowSign(Player& who, WorldPacket& packet);
    void PetitionQuery(Player& who, WorldPacket& packet);
    void PetitionRename(WorldSession& session, WorldPacket& packet);
    void PetitionSign(WorldSession& session, WorldPacket& packet);
    void PetitionDecline(Player& who, WorldPacket& packet);
    void OfferPetition(WorldSession& session, WorldPacket& packet);
    void TurnInPetition(WorldSession& session, WorldPacket& packet);
    void PetitionShowList(Player& who, WorldPacket& packet);
}
