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

#include "SessionMailbox.h"

#include <utility>

SessionMailbox::~SessionMailbox()
{
    Close();
}

bool SessionMailbox::Enqueue(std::unique_ptr<WorldPacket> packet)
{
    if (!packet)
    {
        return false;
    }

    std::lock_guard<std::mutex> guard(m_lock);
    if (m_closed)
    {
        return false;
    }

    m_packets.push_back(std::move(packet));
    return true;
}

std::unique_ptr<WorldPacket> SessionMailbox::Next()
{
    std::lock_guard<std::mutex> guard(m_lock);
    if (m_closed || m_packets.empty())
    {
        return nullptr;
    }

    std::unique_ptr<WorldPacket> packet = std::move(m_packets.front());
    m_packets.pop_front();
    return packet;
}

void SessionMailbox::Close()
{
    std::deque<std::unique_ptr<WorldPacket>> abandoned;

    {
        std::lock_guard<std::mutex> guard(m_lock);
        if (m_closed)
        {
            return;
        }
        m_closed = true;
        abandoned.swap(m_packets);
    }

    // Freed outside the lock: nothing else can reach them once the mailbox is
    // closed, and a destructor has no business running under it.
}

bool SessionMailbox::IsClosed() const
{
    std::lock_guard<std::mutex> guard(m_lock);
    return m_closed;
}
