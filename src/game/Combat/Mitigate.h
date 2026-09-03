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

// Shields and splitting, decided on their own.
//
// Resolve() runs this as one step of a whole resolution. It is exposed
// separately because the entry points that already know how their blow landed --
// a spell that rolled its own hit, a periodic tick that cannot miss -- need the
// same decision without a second hit table being run over them.
//
// Deciding is all it does. Carrying the plan out is Apply()'s work.

#include "Combat/Attempt.h"
#include "Combat/Defences.h"

namespace combat
{
    /**
     * @brief Decide what the shields take and what is shared away.
     *
     * `damage` comes in already through armour or resistance and goes out as
     * what reaches health. `selfInflicted` suppresses splitting: damage a unit
     * does to itself has nobody to share it with.
     *
     * The Outcome's absorb, split and mana fields are filled; nothing else is
     * touched, and nothing in the world is written.
     */
    void Mitigate(int32& damage, SpellSchoolMask school, const Defences& defences,
                  bool selfInflicted, Outcome& out);
}
