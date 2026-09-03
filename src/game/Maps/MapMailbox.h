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

#include "Platform/Define.h"
#include "ObjectGuid.h"

#include <memory>
#include <utility>
#include <vector>

class WorldPacket;
class WorldSession;

/**
 * @brief The packets one map owes its own tick.
 *
 * Filled in the serial phase, where World::UpdateSessions routes each map-bound
 * opcode here. Emptied in the parallel phase by the owning map alone. The phases
 * do not overlap, which is why there is no mutex.
 *
 * A posted packet is not a promise that it runs: the serial phase continues
 * after the post and may teleport the player, log him out or destroy him, so
 * each entry carries the guid it was posted for and the drain re-checks it.
 */
class MapMailbox
{
    public:

        struct Entry
        {
            /// Valid for the tick: sessions die only in the serial phase,
            /// which has finished before any map drains.
            WorldSession* session = nullptr;

            /// Who the packet was posted for. Re-checked at the drain.
            ObjectGuid player;

            std::unique_ptr<WorldPacket> packet;
        };

        MapMailbox() = default;

        MapMailbox(const MapMailbox&) = delete;
        MapMailbox& operator=(const MapMailbox&) = delete;

        /// Serial phase only.
        void Post(WorldSession* session, ObjectGuid player,
                  std::unique_ptr<WorldPacket> packet)
        {
            // Posting mid-drain would read and write this queue from the
            // parallel phase, which is what the phase boundary stands in for.
            MANGOS_ASSERT(!m_draining);
            m_pending.push_back(Entry{ session, player, std::move(packet) });
        }

        /// Parallel phase only. Hands over the tick's packets and leaves the
        /// mailbox empty, so a later post queues for the next tick.
        std::vector<Entry> Take()
        {
            m_draining = true;
            std::vector<Entry> taken;
            taken.swap(m_pending);
            m_draining = false;
            return taken;
        }

        bool Empty() const { return m_pending.empty(); }
        size_t Depth() const { return m_pending.size(); }

    private:

        std::vector<Entry> m_pending;
        bool m_draining = false;
};
