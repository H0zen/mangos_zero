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
 * The four ways anything stacks onto one of a unit's numbers.
 *
 * Every number a unit fights with -- its armour, its health, the power it has to
 * spend, the damage its weapon does -- is kept as these four and folded into one
 * on demand. Two are added and two are multiplied, and the order they fold in is
 * fixed: what is base is settled first, and what is total is applied to the whole.
 *
 * A total percentage of nothing means the number is nothing at all, whatever the
 * rest say. That is how an effect that takes a unit's armour to zero works, and
 * why it cannot be argued with by piling on flat armour underneath.
 */
struct Modifiers
{
    float baseValue = 0.0f;
    float basePct = 1.0f;
    float totalValue = 0.0f;
    float totalPct = 1.0f;

    /// The one number the four come to.
    float Folded() const
    {
        if (totalPct <= 0.0f)
        {
            return 0.0f;
        }

        return ((baseValue * basePct) + totalValue) * totalPct;
    }

    /// What is settled before anything total is applied.
    float Base() const { return baseValue * basePct; }

    /// A total percentage that is not positive is read as nothing at all, and
    /// every number that folds one reads it this way.
    float TotalPct() const { return totalPct <= 0.0f ? 0.0f : totalPct; }

    /// The same percentage as a share above or below the whole, which is how the
    /// attack power fields want it. Nothing at all reads as the whole taken away.
    float TotalShare() const { return TotalPct() - 1.0f; }
};
