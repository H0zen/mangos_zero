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

// Everything the hit tables read about one side of a fight, and nothing else.
//
// Naming it makes the dependency finite: the roll below needs eleven numbers and
// four flags, not a Unit. That is what lets a hit table be exercised on values,
// and it is also a statement -- anything not here cannot influence whether a
// blow lands, so a change that needs more has to say so by widening this.
//
// It is read fresh for each resolve and thrown away. It is not a snapshot kept
// across a chain of procs, where it would go stale the moment the first effect
// wrote.

#include "Platform/Define.h"
#include "ObjectGuid.h"

namespace combat
{
    /// Chances are in hundredths of a percent, as the client's tables are: 5000
    /// is five percent. Keeping the unit out of floats keeps the roll exact.
    struct Combatant
    {
        ObjectGuid guid;

        uint32 level = 1;
        bool isPlayer = false;
        bool isPet = false;

        /// A creature that has given up and is walking home takes nothing.
        bool isEvading = false;

        /// A player who is not standing is hit critically by anything that can
        /// crit at all.
        bool isSitting = false;

        int32 missChance = 0;
        int32 critChance = 0;
        int32 dodgeChance = 0;
        int32 parryChance = 0;
        int32 blockChance = 0;

        /// Attacker's skill with the weapon in hand, and the cap for its level.
        int32 weaponSkill = 0;
        int32 maxSkillForLevel = 0;

        /// Victim's defence, and the cap for its level. Defence above the cap
        /// does nothing, which the crushing-blow test relies on.
        int32 defenceSkill = 0;
        int32 maxDefenceForLevel = 0;

        /// Creature flags. A player is always allowed all three.
        bool canParry = true;
        bool canBlock = true;
        bool canCrush = false;
    };
}
