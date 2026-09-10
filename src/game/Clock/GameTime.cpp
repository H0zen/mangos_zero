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

#include "GameTime.h"
#include "Timer.h"

namespace GameTime
{

    time_t const StartTime = time(nullptr);

    time_t GameTime = 0;

    uint32 GameMSTime = 0;

    std::chrono::system_clock::time_point GameTimeSystemPoint = std::chrono::system_clock::time_point::min();

    std::chrono::steady_clock::time_point GameTimeSteadyPoint = std::chrono::steady_clock::time_point::min();

    time_t GetStartTime()
    {
        return StartTime;
    }

    time_t GetGameTime()
    {
        return GameTime;
    }

    uint32 GetGameTimeMS()
    {
        return GameMSTime;
    }

    uint64 GetAbsoluteTimeMS()
    {
        return uint64(std::chrono::duration_cast<std::chrono::milliseconds>(
                          GameTimeSystemPoint.time_since_epoch()).count());
    }

    std::chrono::system_clock::time_point GetGameTimeSystemPoint()
    {
        return GameTimeSystemPoint;
    }

    std::chrono::steady_clock::time_point GetGameTimeSteadyPoint()
    {
        return GameTimeSteadyPoint;
    }

    uint32 GetUptime()
    {
        return uint32(GameTime - StartTime);
    }

    void UpdateGameTimers()
    {
        GameTime = time(nullptr);
        GameMSTime = getMSTime();
        GameTimeSystemPoint = std::chrono::system_clock::now();
        GameTimeSteadyPoint = std::chrono::steady_clock::now();
    }
}
