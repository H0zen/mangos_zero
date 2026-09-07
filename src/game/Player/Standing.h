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
 * How much a deed moves a faction's opinion.
 *
 * Three things multiply the reward and one can cancel it: what the player's auras add, what
 * this server pays for the kind of deed, whether the deed was beneath him, and what the
 * faction's own row says. The order matters, and it is written once here.
 */
namespace standing
{
    /// What this server pays for a deed that was beneath the doer, and overall.
    struct Rates
    {
        /// Applied only when the thing killed or the quest done was grey to him.
        float lowLevelKill = 1.0f;
        float lowLevelQuest = 1.0f;

        /// Applied to everything, always.
        float overall = 1.0f;
    };

    /// What the faction's own row pays for this kind of deed. Nought disables it outright.
    struct FactionRate
    {
        bool stated = false;
        float rate = 0.0f;
    };

    /**
     * @brief The points actually gained.
     *
     * @param base        what the deed is worth before anything is applied.
     * @param percent     the aura-adjusted percentage, a hundred meaning the whole of it.
     * @param beneathHim  the deed was grey: the low-level rate applies.
     * @param lowLevel    the low-level rate for this kind of deed.
     * @param faction     what the faction's row says, if it says anything.
     * @param overall     the server's rate for all reputation.
     */
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
            // A faction that pays nothing for this kind of deed pays nothing at all.
            if (faction.rate <= 0.0f)
            {
                return 0;
            }

            percent *= faction.rate;
        }

        return int32(overall * base * percent / 100.0f);
    }
}
