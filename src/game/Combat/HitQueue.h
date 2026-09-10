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

#include "Combat/Blow.h"

#include <deque>

namespace combat
{

    struct PendingHit
    {
        Blow blow;
        uint8 depth = 0;
    };

    class HitQueue
    {
        public:

            static constexpr uint8 MAX_DEPTH = 8;

            bool Push(const Blow& blow, uint8 parentDepth)
            {
                if (parentDepth >= MAX_DEPTH)
                {
                    ++m_dropped;
                    return false;
                }

                PendingHit pending;
                pending.blow = blow;
                pending.depth = static_cast<uint8>(parentDepth + 1);
                m_pending.push_back(pending);
                return true;
            }

            bool PushRoot(const Blow& blow)
            {
                PendingHit pending;
                pending.blow = blow;
                pending.depth = 0;
                m_pending.push_back(pending);
                return true;
            }

            bool Pop(PendingHit& out)
            {
                if (m_pending.empty())
                {
                    return false;
                }
                out = m_pending.front();
                m_pending.pop_front();
                return true;
            }

            bool Empty() const { return m_pending.empty(); }
            size_t Size() const { return m_pending.size(); }

            uint32 Dropped() const { return m_dropped; }
            void ClearDropped() { m_dropped = 0; }

            void Clear()
            {
                m_pending.clear();
                m_dropped = 0;
            }

        private:

            std::deque<PendingHit> m_pending;
            uint32 m_dropped = 0;
    };
}
