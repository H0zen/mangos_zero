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

#include <atomic>

namespace metrics
{
    class Rate
    {
        public:

            Rate() = default;

            Rate(const Rate&) = delete;
            Rate& operator=(const Rate&) = delete;

            void Add(uint32 count = 1)
            {
                m_count.fetch_add(count, std::memory_order_relaxed);
            }

            uint64 Total() const { return m_total; }

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

            uint32 Pending() const { return m_count.load(std::memory_order_relaxed); }

        private:

            std::atomic<uint32> m_count{0};

            uint64 m_total = 0;
    };
}
