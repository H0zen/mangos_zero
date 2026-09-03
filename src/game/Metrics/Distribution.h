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

// The last N of something, and what its tail looks like.
//
// A mean hides exactly what matters here. A map that ticks in 4 ms and once a
// minute takes 300 averages beautifully and stutters visibly, so what gets
// reported is the tail: p50 to say what normal is, p99 to say what the worst
// regular case is, and the maximum to say whether anything went truly wrong.
//
// The window is fixed and overwrites its oldest sample, so a long-running
// server reports what is happening now rather than what happened all evening,
// and the whole thing allocates nothing.

#include "Platform/Define.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace metrics
{
    template <size_t N>
    class Distribution
    {
        public:

            static_assert(N > 0, "a distribution needs room for at least one sample");

            void Add(uint32 sample)
            {
                m_samples[m_next] = sample;
                m_next = (m_next + 1) % N;
                if (m_count < N)
                {
                    ++m_count;
                }
            }

            size_t Count() const { return m_count; }
            bool Empty() const { return m_count == 0; }

            /**
             * @brief The value at `fraction` of the way up the sorted window.
             *
             * Nearest-rank: the smallest sample at or above the given share of
             * the window. Reports zero when nothing has been recorded, which a
             * caller reads as "no data" rather than "instant".
             */
            uint32 Percentile(float fraction) const
            {
                if (m_count == 0)
                {
                    return 0;
                }

                std::array<uint32, N> sorted = m_samples;
                const auto end = sorted.begin() + m_count;
                std::sort(sorted.begin(), end);

                if (fraction <= 0.f)
                {
                    return sorted[0];
                }
                if (fraction >= 1.f)
                {
                    return sorted[m_count - 1];
                }

                size_t rank = size_t(fraction * float(m_count) + 0.9999f);
                if (rank == 0)
                {
                    rank = 1;
                }
                if (rank > m_count)
                {
                    rank = m_count;
                }
                return sorted[rank - 1];
            }

            uint32 Max() const
            {
                if (m_count == 0)
                {
                    return 0;
                }
                return *std::max_element(m_samples.begin(), m_samples.begin() + m_count);
            }

            void Reset()
            {
                m_count = 0;
                m_next = 0;
            }

        private:

            std::array<uint32, N> m_samples{};
            size_t m_count = 0;
            size_t m_next = 0;
    };
}
