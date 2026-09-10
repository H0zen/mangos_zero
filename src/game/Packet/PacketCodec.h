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

#include <utility>
#include "Platform/Define.h"
#include "WorldPacket.h"

#include <cstddef>
#include <functional>
#include <vector>

namespace proto
{

    static const size_t CLIENT_HEADER_SIZE = 6;

    static const uint32 MAX_CLIENT_PACKET_SIZE = 10240;

    enum class DecodeStatus
    {
        Ok,
        Malformed
    };

    class PacketCodec
    {
        public:

            typedef std::function<void(uint8* header, size_t len)> HeaderDecryptor;

            typedef std::function<void(uint8* header, size_t len)> HeaderEncryptor;

            explicit PacketCodec(HeaderDecryptor decryptor = HeaderDecryptor());

            DecodeStatus Feed(const uint8* data, size_t len,
                              std::vector<WorldPacket>& out);

            static std::vector<uint8> Encode(const WorldPacket& packet,
                                             const HeaderEncryptor& encryptor);

            void SetHeaderDecryptor(HeaderDecryptor decryptor)
            {
                m_decryptor = std::move(decryptor);
            }

        private:

            HeaderDecryptor m_decryptor;

            uint8  m_header[CLIENT_HEADER_SIZE];
            size_t m_headerFill;

            bool   m_haveHeader;
            uint16 m_opcode;
            uint32 m_payloadNeeded;

            std::vector<uint8> m_payload;
    };
}
