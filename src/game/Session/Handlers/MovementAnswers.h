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

class MovementInfo;
class ObjectGuid;
class Player;
class WorldPacket;
class WorldSession;

/// What the client says about where he is and how he moves.
namespace movement
{
    /// Whether what the client claims about where he is can be believed.
    bool Verify(Player& who, MovementInfo const& movementInfo, ObjectGuid const& guid);
    bool Verify(Player& who, MovementInfo const& movementInfo);

    /// Puts the mover where the verified packet says he is.
    void Relocate(Player& who, MovementInfo& movementInfo);

    void MoveWorldportAck(WorldSession& session, WorldPacket& packet);
    void MoveTeleportAck(Player& who, WorldPacket& packet);
    void MovementOpcodes(WorldSession& session, WorldPacket& packet);
    void ForceSpeedChangeAckOpcodes(Player& who, WorldPacket& packet);
    void SetActiveMover(Player& who, WorldPacket& packet);
    void MoveNotActiveMover(Player& who, WorldPacket& packet);
    void MountSpecialAnim(Player& who, WorldPacket& packet);
    void MoveKnockBackAck(WorldSession& session, WorldPacket& packet);
    void MoveHoverAck(Player& who, WorldPacket& packet);
    void MoveWaterWalkAck(Player& who, WorldPacket& packet);
    void SummonResponse(Player& who, WorldPacket& packet);
    void MoveTimeSkipped(Player& who, WorldPacket& packet);
}
