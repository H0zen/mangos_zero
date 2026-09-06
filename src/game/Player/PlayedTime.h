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

#include <ctime>

/**
 * How long a character has been played, and since when.
 *
 * Two totals are kept, both in seconds: everything since he was made, and
 * everything since he reached the level he is on. The second is set back to
 * nothing each time he levels, which is why they are two numbers and not one
 * with a subtraction.
 *
 * Both are advanced from a single mark, moved forward every tick. The mark is
 * wall-clock time rather than the tick's own count, so time the server spends
 * stalled still counts as time he was played -- he was logged in for it.
 *
 * The hour he logged in is kept apart from the mark, because it never moves: it
 * is what the played-time answer and the session's own accounting are measured
 * from.
 */
class PlayedTime
{
    public:

        /// Starts both clocks at the given hour, for a character coming into the world.
        void StartAt(time_t when)
        {
            m_loggedInAt = when;
            m_mark = when;
        }

        /// Sets both totals to nothing, for a character who has just been made.
        void Fresh(time_t when)
        {
            m_mark = when;
            m_total = 0;
            m_atThisLevel = 0;
        }

        time_t LoggedInAt() const { return m_loggedInAt; }
        time_t Mark() const { return m_mark; }

        uint32 Total() const { return m_total; }
        uint32 AtThisLevel() const { return m_atThisLevel; }

        void Total(uint32 seconds) { m_total = seconds; }
        void AtThisLevel(uint32 seconds) { m_atThisLevel = seconds; }

        /// Carries both totals up to the given hour and moves the mark there.
        void Advance(time_t now)
        {
            if (now <= m_mark)
            {
                return;
            }

            uint32 const elapsed = uint32(now - m_mark);
            m_total += elapsed;
            m_atThisLevel += elapsed;
            m_mark = now;
        }

        /// How long since the mark, without moving it.
        uint32 Since(time_t now) const
        {
            return now > m_mark ? uint32(now - m_mark) : 0;
        }

        /// He has reached a new level, so the second total starts again.
        void NewLevel() { m_atThisLevel = 0; }

    private:

        time_t m_loggedInAt = 0;
        time_t m_mark = 0;

        uint32 m_total = 0;
        uint32 m_atThisLevel = 0;
};
