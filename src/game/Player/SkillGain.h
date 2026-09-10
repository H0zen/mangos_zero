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

namespace skill
{

    struct Chances
    {
        uint32 orange = 0;
        uint32 yellow = 0;
        uint32 green = 0;
        uint32 grey = 0;
    };

    inline int32 ChanceAt(uint32 skillValue, uint32 greyLevel, uint32 greenLevel,
                          uint32 yellowLevel, Chances const& paid)
    {
        if (skillValue >= greyLevel)
        {
            return int32(paid.grey * 10);
        }

        if (skillValue >= greenLevel)
        {
            return int32(paid.green * 10);
        }

        if (skillValue >= yellowLevel)
        {
            return int32(paid.yellow * 10);
        }

        return int32(paid.orange * 10);
    }

    inline int32 Thinned(int32 chance, uint32 skillValue, uint32 steps)
    {
        if (steps == 0)
        {
            return chance;
        }

        return chance >> (skillValue / steps);
    }

    inline int32 FishingChance(uint32 skillValue)
    {
        return (skillValue < 75 ? 100 : 2500 / int32(skillValue - 50)) * 10;
    }
}
