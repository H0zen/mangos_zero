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

/**
 * @brief How long a thing has left, and nothing else about it.
 *
 * The clock a conjured thing keeps: an area effect, a called-up minion, anything granted a
 * length of time when it was made. It runs only while something spends it, so a thing whose
 * owner stops ticking stops ageing -- which is what the game means by a duration.
 *
 * A span of nought is no clock at all: the thing stays until something other than time ends
 * it. That is the state a fresh one is in, so a holder that never grants anything is
 * unbounded, and Spend on it always says there is time left.
 *
 * The clock is the whole of what this holds. What happens when it runs out belongs to the
 * thing that owns it, which is the only one that knows what going away means. And a clock
 * the client can see -- an item's duration, which travels in the update fields -- stays in
 * the field mirror, because two copies of one number is one copy too many.
 */
class Lifespan
{
    public:

        /// A length, running from now. Zero clears the clock.
        void Grant(uint32 ms)
        {
            m_granted = ms;
            m_left = ms;
        }

        /// Back to the full length it was granted.
        void Restore() { m_left = m_granted; }

        /// No clock: it stays until something else ends it.
        void Clear()
        {
            m_granted = 0;
            m_left = 0;
        }

        /// Was a length ever granted? A span of nought is no span at all.
        bool Bounded() const { return m_granted > 0; }

        uint32 Left() const { return m_left; }
        uint32 Granted() const { return m_granted; }

        /// Is the clock out? Always false while unbounded.
        bool Spent() const { return Bounded() && m_left == 0; }

        /**
         * @brief Spend elapsed time.
         *
         * @return true once the clock is out, and on every reading after that, so a caller
         *         that misses the exact tick still learns it is over.
         */
        bool Spend(uint32 elapsed)
        {
            if (!Bounded())
            {
                return false;
            }

            m_left = elapsed >= m_left ? 0 : m_left - elapsed;

            return m_left == 0;
        }

        /**
         * @brief Bring the end closer, or push it further out.
         *
         * A positive amount is time taken off. Pushing out never goes past the full length
         * that was granted: a thing cannot outlive the term it was made with.
         */
        void Shorten(int32 ms)
        {
            if (!Bounded())
            {
                return;
            }

            if (ms >= 0)
            {
                m_left = uint32(ms) >= m_left ? 0 : m_left - uint32(ms);
                return;
            }

            const uint32 added = uint32(-ms);
            m_left = m_left + added > m_granted ? m_granted : m_left + added;
        }

    private:

        uint32 m_granted = 0;
        uint32 m_left = 0;
};
