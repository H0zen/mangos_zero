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

// The one place a live Unit is turned into the values the core reads.
//
// Everything below the core is written against Combatant and Defences, which are
// numbers. This is the seam where the game's objects become those numbers, and
// keeping it in one file is what stops the core growing a dependency on Unit by
// accident.
//
// The reads are pairwise on purpose: a miss chance, a weapon skill and a defence
// skill are all statements about an attacker AGAINST a victim, not properties
// either one carries alone.

#include "Combat/Combatant.h"
#include "Combat/School.h"
#include "Combat/Defences.h"
#include "SharedDefines.h"

class Unit;

namespace combat
{
    /// The attacker's side, measured against the victim it is swinging at.
    Combatant ReadAttacker(const Unit& attacker, const Unit& victim,
                           WeaponAttackType attackType);

    /**
     * @brief The attacker's side when the attacker is not a unit.
     *
     * A gameobject casts -- a trap, a fire it starts -- but it has no auras, no
     * weapon and no class, and it cannot be swung with. All the core reads of
     * such a caster is its level, which scales the victim's armour and
     * resistance, and its guid for the log.
     *
     * The victim is always a unit: a gameobject takes no damage and does not
     * die, so it is never on the receiving side of a resolution.
     */
    Combatant ReadObjectCaster(ObjectGuid caster, uint32 level);

    /// The victim's side, measured against the attacker.
    Combatant ReadVictim(const Unit& victim, const Unit& attacker);

    /// What the victim has in the way of a blow of this school.
    Defences ReadDefences(const Unit& victim, const Unit& attacker,
                          School school);
}
