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

// The counting client.
//
// What it has to get right is the accounting: a measurement that undercounts is
// worse than none, because it says the server is fine.

#include "doctest.h"

#include "Synthetic/SyntheticLink.h"
#include "Opcodes.h"

using synthetic::SyntheticLink;

namespace
{
    WorldPacket Packet(uint16 opcode, size_t payload)
    {
        WorldPacket p(opcode, payload);
        for (size_t i = 0; i < payload; ++i)
        {
            p << uint8(0x5A);
        }
        return p;
    }
}

TEST_CASE("A fresh link has sent nothing and is open")
{
    SyntheticLink link("synthetic:1");

    CHECK(link.Bytes() == 0);
    CHECK(link.Packets() == 0);
    CHECK_FALSE(link.IsClosed());
    CHECK(link.GetRemoteAddress() == "synthetic:1");
}

TEST_CASE("A packet counts its payload plus the wire header")
{
    // Counting the payload alone would understate a burst of small packets,
    // which is the exact shape movement relay has.
    SyntheticLink link("synthetic:1");
    link.SendPacket(Packet(SMSG_MONSTER_MOVE, 30));

    CHECK(link.Packets() == 1);
    CHECK(link.Bytes() == 30 + SyntheticLink::SERVER_HEADER_BYTES);
}

TEST_CASE("An empty packet still costs its header")
{
    SyntheticLink link("synthetic:1");
    link.SendPacket(Packet(SMSG_ATTACKSTOP, 0));

    CHECK(link.Bytes() == SyntheticLink::SERVER_HEADER_BYTES);
}

TEST_CASE("Bytes accumulate across packets")
{
    SyntheticLink link("synthetic:1");
    for (int i = 0; i < 20; ++i)
    {
        link.SendPacket(Packet(SMSG_MONSTER_MOVE, 41));
    }

    CHECK(link.Packets() == 20);
    CHECK(link.Bytes() == 20 * (41 + SyntheticLink::SERVER_HEADER_BYTES));
}

TEST_CASE("Taking the counts starts the next window at zero")
{
    SyntheticLink link("synthetic:1");
    link.SendPacket(Packet(SMSG_MONSTER_MOVE, 10));

    CHECK(link.TakeBytes() == 10 + SyntheticLink::SERVER_HEADER_BYTES);
    CHECK(link.Bytes() == 0);

    CHECK(link.TakePackets() == 1);
    CHECK(link.Packets() == 0);
}

TEST_CASE("A closed link counts nothing more")
{
    // It has to behave like a dead socket, or a bot that logs out keeps
    // inflating the total it was measuring.
    SyntheticLink link("synthetic:1");
    link.SendPacket(Packet(SMSG_MONSTER_MOVE, 10));
    const uint64 before = link.Bytes();

    link.Close();
    CHECK(link.IsClosed());

    link.SendPacket(Packet(SMSG_MONSTER_MOVE, 10));
    CHECK(link.Bytes() == before);
}
