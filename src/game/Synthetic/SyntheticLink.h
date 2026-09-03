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

// A client that is not there, counting what would have been sent to it.
//
// The point of the exercise is a number the server cannot otherwise produce:
// how many bytes a second one player costs when five hundred of them are
// standing in the same square. That number is only true if everything above it
// is the real thing -- the real session, the real visibility sweep, the real
// update packets. So this replaces the socket and nothing else.
//
// Bytes are counted as they would go on the wire: the payload plus the four
// header bytes a 1.12 server header takes. Counting the payload alone would
// understate a burst of small packets, which is exactly the shape movement
// relay has.

#include "IClientLink.h"

#include <atomic>
#include <string>

namespace synthetic
{
    class SyntheticLink : public proto::IClientLink
    {
        public:

            /// The header a 1.12 server packet carries: size big-endian, then
            /// opcode. Fixed at four -- see PacketCodec::Encode.
            static const uint32 SERVER_HEADER_BYTES = 4;

            explicit SyntheticLink(std::string address) : m_address(std::move(address)) {}

            void SendPacket(const WorldPacket& packet) override
            {
                if (m_closed.load(std::memory_order_relaxed))
                {
                    return;
                }
                m_bytes.fetch_add(uint64(packet.size()) + SERVER_HEADER_BYTES,
                                  std::memory_order_relaxed);
                m_packets.fetch_add(1, std::memory_order_relaxed);
            }

            void Close() override { m_closed.store(true, std::memory_order_relaxed); }

            const std::string& GetRemoteAddress() const override { return m_address; }

            bool IsClosed() const override { return m_closed.load(std::memory_order_relaxed); }

            uint64 Bytes() const { return m_bytes.load(std::memory_order_relaxed); }
            uint64 Packets() const { return m_packets.load(std::memory_order_relaxed); }

            /// Take the counts and start again, for one reporting window.
            uint64 TakeBytes() { return m_bytes.exchange(0, std::memory_order_relaxed); }
            uint64 TakePackets() { return m_packets.exchange(0, std::memory_order_relaxed); }

        private:

            // Written from whichever map thread owns the player being sent to.
            std::atomic<uint64> m_bytes{0};
            std::atomic<uint64> m_packets{0};
            std::atomic<bool> m_closed{false};

            std::string m_address;
    };
}
