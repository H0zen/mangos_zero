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

// How often something happened, per second, since somebody last asked.
//
// Counting is cheap and happens on hot paths; dividing happens once every few
// seconds where nobody is watching the clock. Reading the rate consumes it, so
// two readers would each see part of the truth -- there is one reporter, and
// that is the point rather than an oversight.

#include "Platform/Define.h"

#include <atomic>

namespace metrics
{
    class Rate
    {
        public:

            Rate() = default;

            Rate(const Rate&) = delete;
            Rate& operator=(const Rate&) = delete;

            /// Counted from whichever map thread the event happened on, so the
            /// increment is atomic. Relaxed is enough: nothing is ordered
            /// against it, and a count that lands in the next window instead of
            /// this one changes a rate by one event.
            void Add(uint32 count = 1)
            {
                m_count.fetch_add(count, std::memory_order_relaxed);
            }

            /// Total since construction, for a counter that should only ever
            /// climb -- a leak shows as a number that never settles.
            uint64 Total() const { return m_total; }

            /**
             * @brief Events per second over the elapsed window, and reset.
             *
             * An elapsed time of zero reports zero rather than dividing: a
             * sampler called twice in the same millisecond is a caller's bug,
             * not a reason to produce an infinity that poisons a log line.
             */
            float Sample(uint32 elapsedMs)
            {
                const uint32 counted = m_count.exchange(0, std::memory_order_relaxed);
                m_total += counted;

                if (elapsedMs == 0)
                {
                    return 0.f;
                }
                return static_cast<float>(counted) * 1000.f / static_cast<float>(elapsedMs);
            }

            /// What has accumulated since the last sample, without consuming it.
            uint32 Pending() const { return m_count.load(std::memory_order_relaxed); }

        private:

            std::atomic<uint32> m_count{0};

            // Only the one reporter touches this, inside Sample.
            uint64 m_total = 0;
    };
}
