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

// 1.12 framing. The codec owns no socket and performs no I/O, which is what lets
// these tests hand it a TCP stream a byte at a time -- the split that breaks a
// framing layer in production is never the convenient one.

#include "doctest.h"

#include "Packet/PacketCodec.h"

#include <cstdint>
#include <vector>

using proto::PacketCodec;
using proto::DecodeStatus;

namespace
{
    /// Build an inbound header: size big-endian counting the four opcode bytes,
    /// then the opcode little-endian over 32 bits.
    std::vector<uint8> ClientFrame(uint32 opcode, const std::vector<uint8>& payload)
    {
        const uint32 size = static_cast<uint32>(payload.size()) + 4;

        std::vector<uint8> bytes;
        bytes.push_back(static_cast<uint8>((size >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8>(size & 0xFF));
        bytes.push_back(static_cast<uint8>(opcode & 0xFF));
        bytes.push_back(static_cast<uint8>((opcode >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8>((opcode >> 16) & 0xFF));
        bytes.push_back(static_cast<uint8>((opcode >> 24) & 0xFF));
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        return bytes;
    }

    /// A header whose size field is written verbatim, so a test can state an
    /// impossible one that ClientFrame() would never produce.
    std::vector<uint8> RawHeader(uint32 size, uint32 opcode)
    {
        std::vector<uint8> bytes;
        bytes.push_back(static_cast<uint8>((size >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8>(size & 0xFF));
        bytes.push_back(static_cast<uint8>(opcode & 0xFF));
        bytes.push_back(static_cast<uint8>((opcode >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8>((opcode >> 16) & 0xFF));
        bytes.push_back(static_cast<uint8>((opcode >> 24) & 0xFF));
        return bytes;
    }
}

TEST_CASE("A whole packet in one feed decodes to one packet")
{
    PacketCodec codec;
    std::vector<WorldPacket> out;

    const std::vector<uint8> payload = { 0xDE, 0xAD, 0xBE, 0xEF };
    const std::vector<uint8> wire = ClientFrame(0x01FD, payload);

    CHECK(codec.Feed(wire.data(), wire.size(), out) == DecodeStatus::Ok);
    REQUIRE(out.size() == 1);
    CHECK(out[0].GetOpcode() == 0x01FD);
    REQUIRE(out[0].size() == payload.size());
    CHECK(std::vector<uint8>(out[0].contents(), out[0].contents() + out[0].size()) == payload);
}

TEST_CASE("The same packet fed one byte at a time decodes identically")
{
    PacketCodec codec;
    std::vector<WorldPacket> out;

    const std::vector<uint8> payload = { 0xDE, 0xAD, 0xBE, 0xEF };
    const std::vector<uint8> wire = ClientFrame(0x01FD, payload);

    for (size_t i = 0; i + 1 < wire.size(); ++i)
    {
        CHECK(codec.Feed(&wire[i], 1, out) == DecodeStatus::Ok);
        CHECK(out.empty()); // nothing may surface before the last byte
    }

    CHECK(codec.Feed(&wire[wire.size() - 1], 1, out) == DecodeStatus::Ok);
    REQUIRE(out.size() == 1);
    CHECK(out[0].GetOpcode() == 0x01FD);
    CHECK(out[0].size() == payload.size());
}

TEST_CASE("A feed that stops mid-header resumes correctly")
{
    PacketCodec codec;
    std::vector<WorldPacket> out;

    const std::vector<uint8> wire = ClientFrame(0x00DC, { 0x11, 0x22 });

    CHECK(codec.Feed(wire.data(), 3, out) == DecodeStatus::Ok);
    CHECK(out.empty());

    CHECK(codec.Feed(wire.data() + 3, wire.size() - 3, out) == DecodeStatus::Ok);
    REQUIRE(out.size() == 1);
    CHECK(out[0].GetOpcode() == 0x00DC);
}

TEST_CASE("Several packets in one feed come out in arrival order")
{
    PacketCodec codec;
    std::vector<WorldPacket> out;

    std::vector<uint8> wire = ClientFrame(0x0001, { 0xAA });
    const std::vector<uint8> second = ClientFrame(0x0002, {});
    const std::vector<uint8> third  = ClientFrame(0x0003, { 0xBB, 0xCC });
    wire.insert(wire.end(), second.begin(), second.end());
    wire.insert(wire.end(), third.begin(), third.end());

    CHECK(codec.Feed(wire.data(), wire.size(), out) == DecodeStatus::Ok);
    REQUIRE(out.size() == 3);
    CHECK(out[0].GetOpcode() == 0x0001);
    CHECK(out[1].GetOpcode() == 0x0002);
    CHECK(out[2].GetOpcode() == 0x0003);
    CHECK(out[1].size() == 0);
}

TEST_CASE("A size below the opcode width is malformed")
{
    // `size` counts the four opcode bytes, so 3 is impossible -- and computing
    // the payload length from it would underflow into a 4 GB allocation.
    PacketCodec codec;
    std::vector<WorldPacket> out;

    const std::vector<uint8> wire = RawHeader(3, 0x0001);

    CHECK(codec.Feed(wire.data(), wire.size(), out) == DecodeStatus::Malformed);
    CHECK(out.empty());
}

TEST_CASE("A size past the client cap is malformed")
{
    PacketCodec codec;
    std::vector<WorldPacket> out;

    const std::vector<uint8> wire = RawHeader(proto::MAX_CLIENT_PACKET_SIZE + 1, 0x0001);

    CHECK(codec.Feed(wire.data(), wire.size(), out) == DecodeStatus::Malformed);
    CHECK(out.empty());
}

TEST_CASE("An opcode with a non-zero high half is malformed")
{
    // The client's opcode field is 32 bits but 1.12 opcodes live in the low 16.
    // Anything in the top half is a peer that is not the client we support, and
    // truncating it to uint16 would silently dispatch the wrong handler.
    PacketCodec codec;
    std::vector<WorldPacket> out;

    const std::vector<uint8> wire = RawHeader(4, 0x00010001);

    CHECK(codec.Feed(wire.data(), wire.size(), out) == DecodeStatus::Malformed);
    CHECK(out.empty());
}

TEST_CASE("The header decryptor runs once per header, over all six bytes")
{
    // Running it per fragment would consume the cipher's chain state out of step
    // with the peer and desynchronise every later header.
    int calls = 0;
    size_t seenLen = 0;

    PacketCodec codec([&calls, &seenLen](uint8*, size_t len)
    {
        ++calls;
        seenLen = len;
    });

    std::vector<WorldPacket> out;
    const std::vector<uint8> wire = ClientFrame(0x0001, { 0x42 });

    for (size_t i = 0; i < wire.size(); ++i)
    {
        codec.Feed(&wire[i], 1, out);
    }

    REQUIRE(out.size() == 1);
    CHECK(calls == 1);
    CHECK(seenLen == proto::CLIENT_HEADER_SIZE);
}

TEST_CASE("Encode writes a four-byte header: size big-endian, opcode little-endian")
{
    WorldPacket packet(0x01FD, 4);
    const uint8 payload[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    packet.append(payload, sizeof(payload));

    const std::vector<uint8> wire = PacketCodec::Encode(packet, PacketCodec::HeaderEncryptor());

    REQUIRE(wire.size() == 4 + sizeof(payload));
    CHECK(wire[0] == 0x00);  // size = payload + 2 opcode bytes = 6
    CHECK(wire[1] == 0x06);
    CHECK(wire[2] == 0xFD);  // opcode little-endian
    CHECK(wire[3] == 0x01);
    CHECK(wire[4] == 0xDE);
    CHECK(wire[7] == 0xEF);
}

TEST_CASE("A large packet still gets a four-byte header, never the 0x80 form")
{
    // This is the 1.12-versus-WotLK difference and it is a one-way door: the
    // three-byte size marked by 0x80 in the first byte arrives in WotLK, and a
    // 1.12 client reads four header bytes unconditionally. Handing it five
    // desynchronises the stream permanently -- it is not a packet it can skip.
    WorldPacket packet(0x01FD, 0);
    const std::vector<uint8> payload(40000, 0x5A);
    packet.append(payload.data(), payload.size());

    const std::vector<uint8> wire = PacketCodec::Encode(packet, PacketCodec::HeaderEncryptor());

    REQUIRE(wire.size() == 4 + payload.size());

    // 40002 == 0x9C42. The top bit of 0x9C is set, which is exactly the bit a
    // WotLK codec would have read as "three-byte size follows".
    CHECK(wire[0] == 0x9C);
    CHECK(wire[1] == 0x42);
    CHECK(wire[2] == 0xFD);
    CHECK(wire[3] == 0x01);
}

TEST_CASE("The header encryptor is handed the four bytes it must transform")
{
    size_t seenLen = 0;
    int calls = 0;

    WorldPacket packet(0x0001, 0);
    const std::vector<uint8> wire = PacketCodec::Encode(packet,
        [&calls, &seenLen](uint8*, size_t len)
        {
            ++calls;
            seenLen = len;
        });

    CHECK(calls == 1);
    CHECK(seenLen == 4);
    CHECK(wire.size() == 4);
}
