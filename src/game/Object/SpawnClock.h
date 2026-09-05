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
 * When a thing placed in the world next comes or goes.
 *
 * Two kinds of thing keep this clock and they read it in opposite directions. A
 * PERMANENT spawn belongs to the world: it is taken away, and the moment on the
 * clock is when it comes back, so a clock reading zero means it is standing
 * there now. A FLEETING one was summoned or belongs to someone: it is here from
 * the start and the moment is when it goes for good, so a clock reading zero
 * means it is already gone.
 *
 * That is why the database keeps the two in one signed column,
 * `gameobject`.`spawntimesecs`: the number is how long it waits and the sign is
 * which of the two it is.
 *
 * A thing with no delay at all is on no clock: it stands until something other
 * than time moves it.
 */
class SpawnClock
{
    public:
        /// It belongs to the world and comes back that many seconds after it is taken.
        void ComesBackAfter(uint32 seconds);

        /// It stands for that many seconds and is then gone for good.
        void GoesAwayAfter(uint32 seconds);

        /// It is on no clock and stands until something else moves it.
        void Never();

        /// It comes or goes that many seconds from now, and that becomes its wait.
        void In(uint32 seconds);

        /// The moment it comes or goes; zero when nothing is pending either way.
        time_t Moment() const { return m_moment; }
        void ChangesAt(time_t when) { m_moment = when; }

        /// How long it waits, in seconds; zero when it is on no clock.
        uint32 Delay() const { return m_delay; }

        bool IsPermanent() const { return m_permanent; }
        void Permanent(bool yes) { m_permanent = yes; }

        /// Is it standing in the world at all?
        bool IsUp() const;

        /// The moment it is next up: now, when it already is.
        time_t NextUp(time_t now) const { return m_moment > now ? m_moment : now; }

        /// The two of them as the one signed column the database keeps them in.
        int32 AsSpawnTimeSecs() const;
        void FromSpawnTimeSecs(int32 seconds);

    private:
        time_t m_moment = 0;
        uint32 m_delay = 0;
        bool   m_permanent = true;
};
