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

// The two hit tables. They are the only place a swing and a spell disagree, and
// they meet again immediately: both answer in the same Landing.
//
// The roll is a PARAMETER. A hit table that draws its own random number cannot
// be checked -- every boundary in it becomes a statistical argument instead of a
// case. Passed in, each band is one assertion. The convention is already in the
// tree: Placement::RandomPointAround takes its roll the same way and for the
// same reason.
//
// 1.12 rolls one number against cumulative bands in a fixed order: miss, dodge,
// parry, glancing, block, crit, crushing. The order is the rule, not an
// implementation detail -- a dodge and a crit cannot both happen, and which one
// wins is decided by which band the single roll fell in.

#include "Combat/Attempt.h"
#include "Combat/Combatant.h"

namespace combat
{
    /// The roll's range: hundredths of a percent, so 10000 is certainty.
    const uint32 ROLL_RANGE = 10000;

    /**
     * @brief What a weapon swing does, given a roll in [0, ROLL_RANGE).
     *
     * `fromBehind` is handed in rather than computed. Facing is geometry, and on
     * a vessel it has to be measured in the deck's frame; combat has no business
     * knowing that, and a version that reached for world positions would be
     * wrong for everyone aboard a ship.
     *
     * `isSpellSwing` marks an ability that uses the melee table rather than an
     * auto-attack: those cannot glance and cannot be crushed.
     */
    Landing RollMelee(const Combatant& attacker, const Combatant& victim,
                      bool fromBehind, bool isSpellSwing, uint32 roll);

    /**
     * @brief What a spell does, given a roll in [0, ROLL_RANGE).
     *
     * Spells weigh a chance to land and a chance to be resisted, and nothing
     * else: there is no dodging or parrying a bolt in 1.12.
     */
    Landing RollSpell(const Combatant& attacker, const Combatant& victim,
                      int32 missChance, int32 critChance, bool canCrit, uint32 roll);
}
