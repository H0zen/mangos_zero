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

#include "LoginEffectPackets.h"

#include "Opcodes.h"
#include "WorldPacket.h"

WorldPacket LoginEffectPackets::BuildCastResult()
{
    WorldPacket packet(SMSG_CAST_FAILED, 5);
    packet << uint32(SpellId);
    packet << uint8(0);
    return packet;
}

WorldPacket LoginEffectPackets::BuildStart(uint64 casterGuid)
{
    WorldPacket packet(SMSG_SPELL_START, 22);
    packet.appendPackGUID(casterGuid);
    packet.appendPackGUID(casterGuid);
    packet << uint32(SpellId);
    packet << uint16(2);
    packet << uint32(0);
    packet << uint16(0);
    return packet;
}

WorldPacket LoginEffectPackets::BuildGo(uint64 casterGuid)
{
    WorldPacket packet(SMSG_SPELL_GO, 29);
    packet.appendPackGUID(casterGuid);
    packet.appendPackGUID(casterGuid);
    packet << uint32(SpellId);
    packet << uint16(256);
    packet << uint8(1);
    packet << uint64(casterGuid);
    packet << uint8(0);
    packet << uint16(2);
    packet << uint8(0);
    return packet;
}
