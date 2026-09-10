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

#include <utility>
#include <vector>
#include "PacketCodec.h"

#include <algorithm>
#include <cstring>

namespace proto
{
    PacketCodec::PacketCodec(HeaderDecryptor decryptor)
        : m_decryptor(std::move(decryptor)),
          m_headerFill(0),
          m_haveHeader(false),
          m_opcode(0),
          m_payloadNeeded(0)
    {
        std::memset(m_header, 0, sizeof(m_header));
    }

    DecodeStatus PacketCodec::Feed(const uint8* data, size_t len,
                                   std::vector<WorldPacket>& out)
    {
        if (data == nullptr || len == 0)
        {
            return DecodeStatus::Ok;
        }

        size_t offset = 0;

        while (offset < len)
        {

            if (!m_haveHeader)
            {
                const size_t want = CLIENT_HEADER_SIZE - m_headerFill;
                const size_t take = std::min(want, len - offset);

                std::memcpy(m_header + m_headerFill, data + offset, take);
                m_headerFill += take;
                offset       += take;

                if (m_headerFill < CLIENT_HEADER_SIZE)
                {
                    return DecodeStatus::Ok;
                }

                if (m_decryptor)
                {
                    m_decryptor(m_header, CLIENT_HEADER_SIZE);
                }

                const uint32 size = (uint32(m_header[0]) << 8) | uint32(m_header[1]);
                const uint32 cmd  =  uint32(m_header[2])
                                  | (uint32(m_header[3]) << 8)
                                  | (uint32(m_header[4]) << 16)
                                  | (uint32(m_header[5]) << 24);

                if (size < 4 || size > MAX_CLIENT_PACKET_SIZE
                    || cmd > MAX_CLIENT_PACKET_SIZE)
                {
                    return DecodeStatus::Malformed;
                }

                m_opcode        = uint16(cmd);
                m_payloadNeeded = size - 4;
                m_haveHeader    = true;

                m_payload.clear();
                m_payload.reserve(m_payloadNeeded);
            }

            if (m_payloadNeeded > 0)
            {
                const size_t take = std::min(size_t(m_payloadNeeded), len - offset);
                if (take == 0)
                {
                    return DecodeStatus::Ok;
                }

                m_payload.insert(m_payload.end(), data + offset, data + offset + take);
                offset          += take;
                m_payloadNeeded -= uint32(take);

                if (m_payloadNeeded > 0)
                {
                    return DecodeStatus::Ok;
                }
            }

            WorldPacket packet(m_opcode, m_payload.size());
            if (!m_payload.empty())
            {
                packet.append(m_payload.data(), m_payload.size());
            }
            out.push_back(std::move(packet));

            m_haveHeader = false;
            m_headerFill = 0;
            m_payload.clear();
        }

        return DecodeStatus::Ok;
    }

    std::vector<uint8> PacketCodec::Encode(const WorldPacket& packet,
                                           const HeaderEncryptor& encryptor)
    {

        const uint32 size = uint32(packet.size()) + 2;

        const bool large = false;
        (void)large;

        uint8  header[5];
        size_t headerLen = 0;

        if (large)
        {
            header[headerLen++] = uint8(0x80 | ((size >> 16) & 0xFF));
        }
        header[headerLen++] = uint8((size >> 8) & 0xFF);
        header[headerLen++] = uint8(size & 0xFF);

        const uint16 opcode = uint16(packet.GetOpcode());
        header[headerLen++] = uint8(opcode & 0xFF);
        header[headerLen++] = uint8((opcode >> 8) & 0xFF);

        if (encryptor)
        {
            encryptor(header, headerLen);
        }

        std::vector<uint8> wire;
        wire.reserve(headerLen + packet.size());
        wire.insert(wire.end(), header, header + headerLen);

        if (!packet.empty())
        {
            wire.insert(wire.end(), packet.contents(),
                        packet.contents() + packet.size());
        }

        return wire;
    }
}
