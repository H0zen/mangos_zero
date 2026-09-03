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

// Hits that other hits caused, waiting their turn.
//
// A shield that splits damage onto a guardian, and a proc that answers a blow
// with a bolt, are the same shape: a blow lands, and because it landed there is
// another. Dealing that second one from inside the first -- which is what a
// direct call does -- means it resolves against state the first has not finished
// writing. The guardian can die, proc and pull threat before the blow that split
// the damage onto him has been applied at all.
//
// So they queue. Apply finishes, the queue drains, and each child resolves
// against a world that has settled.
//
// Depth is carried per entry rather than counted globally: a chain is a tree, and
// what matters is how far THIS branch has come, not how many hits happened.

#include "Combat/Attempt.h"

#include <deque>

namespace combat
{
    /// One hit waiting to be dealt, and how far down a chain it already is.
    struct PendingHit
    {
        Attempt attempt;
        uint8 depth = 0;
    };

    class HitQueue
    {
        public:

            /// A proc that casts, whose cast damages, whose damage procs, is
            /// ordinary at two or three. Past this it is two effects feeding
            /// each other and the chain is cut.
            static constexpr uint8 MAX_DEPTH = 8;

            /**
             * @brief Queue a hit caused by one at `parentDepth`.
             *
             * Refuses, and says so, once the branch is too deep. The caller logs
             * it: a dropped chain is worth knowing about, and silently swallowing
             * one turns a runaway aura pair into a mystery.
             */
            bool Push(const Attempt& attempt, uint8 parentDepth)
            {
                if (parentDepth >= MAX_DEPTH)
                {
                    ++m_dropped;
                    return false;
                }

                PendingHit pending;
                pending.attempt = attempt;
                pending.depth = static_cast<uint8>(parentDepth + 1);
                m_pending.push_back(pending);
                return true;
            }

            /// A hit begun by intent rather than by another hit.
            bool PushRoot(const Attempt& attempt)
            {
                PendingHit pending;
                pending.attempt = attempt;
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

            /// How many hits were refused for depth since the counter was last
            /// read. Worth a metric: a number that climbs is a chain that loops.
            uint32 Dropped() const { return m_dropped; }
            void ClearDropped() { m_dropped = 0; }

            void Clear()
            {
                m_pending.clear();
                m_dropped = 0;
            }

        private:

            // First in, first out: children are dealt in the order the blows that
            // caused them landed, so a log reads in the order things happened.
            std::deque<PendingHit> m_pending;
            uint32 m_dropped = 0;
    };
}
