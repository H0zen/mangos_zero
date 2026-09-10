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

class Unit;

namespace xp
{

    inline uint32 GreyLevel(uint32 level)
    {
        if (level <= 5)
        {
            return 0;
        }

        if (level <= 39)
        {
            return level - 5 - level / 10;
        }

        if (level == 60)
        {
            return 51;
        }

        return level - 1 - level / 5;
    }

    inline uint32 ZeroDifference(uint32 level)
    {
        if (level < 8)  { return 5; }
        if (level < 10) { return 6; }
        if (level < 12) { return 7; }
        if (level < 16) { return 8; }
        if (level < 20) { return 9; }
        if (level < 30) { return 11; }
        if (level < 40) { return 12; }
        if (level < 45) { return 13; }
        if (level < 50) { return 14; }
        if (level < 55) { return 15; }
        if (level < 60) { return 16; }

        return 17;
    }

    enum class Colour
    {
        Red,
        Orange,
        Yellow,
        Green,
        Grey
    };

    inline Colour ColourOf(uint32 killerLevel, uint32 victimLevel)
    {
        if (victimLevel >= killerLevel + 5)
        {
            return Colour::Red;
        }

        if (victimLevel >= killerLevel + 3)
        {
            return Colour::Orange;
        }

        if (victimLevel >= killerLevel - 2)
        {
            return Colour::Yellow;
        }

        if (victimLevel > GreyLevel(killerLevel))
        {
            return Colour::Green;
        }

        return Colour::Grey;
    }

    inline uint32 BaseFromKill(uint32 killerLevel, uint32 victimLevel)
    {
        const uint32 base = 45;

        if (victimLevel >= killerLevel)
        {
            uint32 gap = victimLevel - killerLevel;
            if (gap > 4)
            {
                gap = 4;
            }

            return ((killerLevel * 5 + base) * (20 + gap) / 10 + 1) / 2;
        }

        if (victimLevel > GreyLevel(killerLevel))
        {
            const uint32 zero = ZeroDifference(killerLevel);

            return (killerLevel * 5 + base) * (zero + victimLevel - killerLevel) / zero;
        }

        return 0;
    }

    struct Quarry
    {
        uint32 level = 1;

        bool elite = false;

        bool worthNothing = false;
    };

    inline uint32 FromKill(uint32 killerLevel, Quarry const& quarry, float rate)
    {
        if (quarry.worthNothing)
        {
            return 0;
        }

        uint32 gained = BaseFromKill(killerLevel, quarry.level);
        if (gained == 0)
        {
            return 0;
        }

        if (quarry.elite)
        {
            gained *= 2;
        }

        return uint32(gained * rate);
    }

    inline float GroupShare(uint32 count, bool isRaid)
    {
        if (isRaid)
        {

            return 1.0f;
        }

        switch (count)
        {
            case 0:
            case 1:
            case 2:  return 1.0f;
            case 3:  return 1.166f;
            case 4:  return 1.3f;
            case 5:
            default: return 1.4f;
        }
    }

    Quarry QuarryOf(Unit const& victim);
}
