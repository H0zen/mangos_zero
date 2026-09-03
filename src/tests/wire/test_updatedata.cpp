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

// SMSG_UPDATE_OBJECT assembly: the header, the transport flag, and the
// out-of-range block's place in the packet.
//
// has_transport is a property of the PACKET, not of any one block, so it is the
// OR over what went in. A caller deciding it at build time can only be wrong in
// one of two directions, and both break a boarded unit.

#include "doctest.h"

#include "UpdateData.h"
#include "WorldPacket.h"
#include "Opcodes.h"

#include <cstdint>
#include <vector>

namespace
{
    /// One filler byte standing in for a block's payload. The assembly under
    /// test does not read block contents, only counts and flags them.
    void AddBlock(UpdateData& data, uint8 filler = 0x7F)
    {
        data.GetBuffer() << filler;
        data.AddUpdateBlock();
    }

    uint32 BlockCount(const WorldPacket& packet)
    {
        REQUIRE(packet.size() >= 5);
        const uint8* p = packet.contents();
        return uint32(p[0]) | (uint32(p[1]) << 8) | (uint32(p[2]) << 16) | (uint32(p[3]) << 24);
    }

    uint8 HasTransportByte(const WorldPacket& packet)
    {
        REQUIRE(packet.size() >= 5);
        return packet.contents()[4];
    }
}

TEST_CASE("The header is a block count and then the transport flag")
{
    UpdateData data;
    AddBlock(data);
    AddBlock(data);

    WorldPacket packet;
    REQUIRE(data.BuildPacket(&packet));

    CHECK(packet.GetOpcode() == SMSG_UPDATE_OBJECT);
    CHECK(BlockCount(packet) == 2);
    CHECK(HasTransportByte(packet) == 0);
}

TEST_CASE("A packet with no transport block says so")
{
    UpdateData data;
    AddBlock(data);

    WorldPacket packet;
    REQUIRE(data.BuildPacket(&packet));

    CHECK_FALSE(data.HasTransport());
    CHECK(HasTransportByte(packet) == 0);
}

TEST_CASE("One transport block sets the flag for the whole packet")
{
    UpdateData data;
    AddBlock(data);
    data.MarkTransport();
    AddBlock(data);
    AddBlock(data);

    CHECK(data.HasTransport());

    WorldPacket packet;
    REQUIRE(data.BuildPacket(&packet));

    CHECK(BlockCount(packet) == 3);
    CHECK(HasTransportByte(packet) == 1);
}

TEST_CASE("Marking twice is the same as marking once")
{
    // It is an OR, not a count: two hulls in one packet still set one flag.
    UpdateData data;
    data.MarkTransport();
    data.MarkTransport();
    AddBlock(data);

    WorldPacket packet;
    REQUIRE(data.BuildPacket(&packet));

    CHECK(HasTransportByte(packet) == 1);
}

TEST_CASE("Clear resets the transport flag with everything else")
{
    // The map reuses one UpdateData per observer across ticks. A flag that
    // survived Clear would claim a hull in every later packet for that viewer.
    UpdateData data;
    data.MarkTransport();
    AddBlock(data);
    REQUIRE(data.HasTransport());

    data.Clear();

    CHECK_FALSE(data.HasTransport());
    CHECK_FALSE(data.HasData());

    AddBlock(data);
    WorldPacket packet;
    REQUIRE(data.BuildPacket(&packet));
    CHECK(HasTransportByte(packet) == 0);
}

TEST_CASE("A fresh UpdateData carries no data and no flag")
{
    UpdateData data;

    CHECK_FALSE(data.HasData());
    CHECK_FALSE(data.HasTransport());
    CHECK(data.GetOutOfRangeGUIDs().empty());
}

TEST_CASE("An out-of-range guid alone is enough to build a packet")
{
    UpdateData data;
    data.AddOutOfRangeGUID(ObjectGuid(HIGHGUID_UNIT, uint32(1), uint32(42)));

    CHECK(data.HasData());

    WorldPacket packet;
    REQUIRE(data.BuildPacket(&packet));

    // The out-of-range section is itself one block, counted in the header.
    CHECK(BlockCount(packet) == 1);
    CHECK(HasTransportByte(packet) == 0);
}

TEST_CASE("The out-of-range section comes before the ordinary blocks")
{
    // Wire rule. The block that follows the header must be the out-of-range one,
    // so its type byte sits at offset 5 with the filler blocks after it.
    UpdateData data;
    AddBlock(data, 0xAB);
    data.AddOutOfRangeGUID(ObjectGuid(HIGHGUID_UNIT, uint32(1), uint32(42)));

    WorldPacket packet;
    REQUIRE(data.BuildPacket(&packet));

    // One filler block plus the out-of-range section.
    CHECK(BlockCount(packet) == 2);
    CHECK(packet.contents()[5] == UPDATETYPE_OUT_OF_RANGE_OBJECTS);

    // And the filler really did land after it, not before.
    bool fillerFound = false;
    for (size_t i = 6; i < packet.size(); ++i)
    {
        if (packet.contents()[i] == 0xAB)
        {
            fillerFound = true;
            break;
        }
    }
    CHECK(fillerFound);
}

TEST_CASE("A packet past the compression threshold changes opcode")
{
    // Over 100 bytes the payload is deflated and the opcode changes with it, so
    // a client that reads the uncompressed form never sees the compressed one.
    UpdateData data;
    for (int i = 0; i < 200; ++i)
    {
        AddBlock(data, uint8(i));
    }

    WorldPacket packet;
    REQUIRE(data.BuildPacket(&packet));

    CHECK(packet.GetOpcode() == SMSG_COMPRESSED_UPDATE_OBJECT);
}

TEST_CASE("A small packet is left uncompressed")
{
    UpdateData data;
    AddBlock(data);

    WorldPacket packet;
    REQUIRE(data.BuildPacket(&packet));

    CHECK(packet.GetOpcode() == SMSG_UPDATE_OBJECT);
}
