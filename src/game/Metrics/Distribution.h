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

                size_t rank = static_cast<size_t>(fraction * static_cast<float>(m_count) + 0.9999f);
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
