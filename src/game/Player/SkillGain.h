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
 * The chance that doing something teaches you a little more of it.
 *
 * A recipe or a node is coloured by how far above your skill it is, and the colour is the
 * whole of the answer: an orange one nearly always teaches, a grey one nearly never. The
 * four chances are what a server chooses; everything here is arithmetic over them.
 *
 * Chances are in tenths of a percent, which is why every one of them is multiplied by ten:
 * a configured 25 means a quarter of the time.
 */
namespace skill
{
    /// What a server pays for each colour, in whole percent.
    struct Chances
    {
        uint32 orange = 0;
        uint32 yellow = 0;
        uint32 green = 0;
        uint32 grey = 0;
    };

    /**
     * @brief The chance of learning from one attempt, in tenths of a percent.
     *
     * The three levels are where the colours change, counted in skill points: at or above
     * `grey` nothing more is learnt to speak of, below `yellow` it is orange.
     */
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

    /**
     * @brief What skinning and mining pay at a high skill.
     *
     * Both fall off as the skill rises, halving every `steps` points: nothing under the
     * first step, half over it, a quarter over the second. A step of nothing turns the
     * falling off off altogether.
     */
    inline int32 Thinned(int32 chance, uint32 skillValue, uint32 steps)
    {
        if (steps == 0)
        {
            return chance;
        }

        return chance >> (skillValue / steps);
    }

    /// The chance of learning from a cast of the rod, which needs no colour: it rises to
    /// seventy-five and falls away after it.
    inline int32 FishingChance(uint32 skillValue)
    {
        return (skillValue < 75 ? 100 : 2500 / int32(skillValue - 50)) * 10;
    }
}
