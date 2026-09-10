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

namespace standing
{

    struct Rates
    {

        float lowLevelKill = 1.0f;
        float lowLevelQuest = 1.0f;

        float overall = 1.0f;
    };

    struct FactionRate
    {
        bool stated = false;
        float rate = 0.0f;
    };

    inline int32 Gained(int32 base, float percent, bool beneathHim, float lowLevel,
                        FactionRate const& faction, float overall)
    {
        if (beneathHim && lowLevel != 1.0f)
        {
            percent *= lowLevel;
        }

        if (percent <= 0.0f)
        {
            return 0;
        }

        if (faction.stated)
        {

            if (faction.rate <= 0.0f)
            {
                return 0;
            }

            percent *= faction.rate;
        }

        return int32(overall * base * percent / 100.0f);
    }
}
