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

#include "SpawnClock.h"

void SpawnClock::ComesBackAfter(uint32 seconds)
{
    m_permanent = true;
    m_delay = seconds;
    m_moment = 0;
}

void SpawnClock::GoesAwayAfter(uint32 seconds)
{
    m_permanent = false;
    m_delay = seconds;
    m_moment = 0;
}

void SpawnClock::Never()
{
    m_permanent = true;
    m_delay = 0;
    m_moment = 0;
}

void SpawnClock::In(uint32 seconds)
{
    m_delay = seconds;
    m_moment = seconds > 0 ? time(nullptr) + seconds : 0;
}

bool SpawnClock::IsUp() const
{
    if (m_delay == 0)
    {
        return true;
    }

    // A permanent spawn is up while nothing is pending; a fleeting one is up until
    // its moment is taken off the clock.
    return m_permanent ? m_moment == 0 : m_moment != 0;
}

int32 SpawnClock::AsSpawnTimeSecs() const
{
    return m_permanent ? int32(m_delay) : -int32(m_delay);
}

void SpawnClock::FromSpawnTimeSecs(int32 seconds)
{
    if (seconds >= 0)
    {
        ComesBackAfter(uint32(seconds));
    }
    else
    {
        GoesAwayAfter(uint32(-seconds));
    }
}
