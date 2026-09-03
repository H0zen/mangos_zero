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

#include "WorldPacket.h"

#include <deque>
#include <memory>
#include <mutex>

/**
 * @brief One session's inbox: what the network has read and the tick has not run.
 *
 * The one real thread boundary on the receive path. A network thread enqueues; the
 * serial phase of the tick drains, and is the only consumer -- map-bound packets
 * are routed out of here into the owning map's mailbox rather than being fetched
 * from here by a map thread. So one lock covers the queue and the closed flag
 * together, and a packet crosses one lock, not two.
 *
 * Closing is one-way, and it frees what is still queued: a session whose socket
 * died must not leave packets behind for a drain that will never come.
 */
class SessionMailbox
{
    public:

        SessionMailbox() = default;
        ~SessionMailbox();

        SessionMailbox(const SessionMailbox&) = delete;
        SessionMailbox& operator=(const SessionMailbox&) = delete;

        /// Takes the packet unless the mailbox is closed, in which case the
        /// packet is dropped and the call reports false.
        bool Enqueue(std::unique_ptr<WorldPacket> packet);

        /// The next packet, or nothing when the mailbox is empty or closed.
        std::unique_ptr<WorldPacket> Next();

        void Close();
        bool IsClosed() const;

    private:

        mutable std::mutex m_lock;
        bool m_closed = false;
        std::deque<std::unique_ptr<WorldPacket>> m_packets;
};
