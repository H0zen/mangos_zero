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

class SpawnClock
{
    public:

        void ComesBackAfter(uint32 seconds);

        void GoesAwayAfter(uint32 seconds);

        void Never();

        void In(uint32 seconds);

        time_t Moment() const { return m_moment; }
        void ChangesAt(time_t when) { m_moment = when; }

        uint32 Delay() const { return m_delay; }

        bool IsPermanent() const { return m_permanent; }
        void Permanent(bool yes) { m_permanent = yes; }

        bool IsUp() const;

        time_t NextUp(time_t now) const { return m_moment > now ? m_moment : now; }

        int32 AsSpawnTimeSecs() const;
        void FromSpawnTimeSecs(int32 seconds);

    private:
        time_t m_moment = 0;
        uint32 m_delay = 0;
        bool   m_permanent = true;
};
