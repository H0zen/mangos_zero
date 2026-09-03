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

// What stands between a blow and a victim's health.
//
// Everything here is read, never written. A shield's remaining strength is an
// input to the decision; drawing on it is Apply()'s business, from the plan the
// Outcome carries back. That separation is the whole reason resolve() can be a
// function of its arguments.

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <vector>

namespace combat
{
    /**
     * @brief A shield, and how much it can still take.
     *
     * Some shields are paid for in mana as they work: every point they stop
     * costs `manaMultiplier` mana, and they stop nothing once the mana is gone.
     * A multiplier of zero is a shield that costs nothing to hold up.
     */
    struct Absorber
    {
        ObjectGuid caster;
        uint32 spellId = 0;
        int32 remaining = 0;
        uint32 schoolMask = SPELL_SCHOOL_MASK_ALL;
        float manaMultiplier = 0.f;

        bool CostsMana() const { return manaMultiplier > 0.f; }

        bool Covers(SpellSchoolMask school) const
        {
            return (schoolMask & school) != 0;
        }
    };

    /**
     * @brief An aura that sends part of the blow to somebody else.
     *
     * A flat share takes a fixed amount off; a fractional one takes a portion.
     * Either way the amount leaves this victim and arrives at another, and the
     * arrival is queued rather than dealt during this hit's mitigation.
     */
    struct Splitter
    {
        ObjectGuid target;
        uint32 spellId = 0;
        int32 flat = 0;
        float fraction = 0.f;
    };

    struct Defences
    {
        int32 armour = 0;

        /// Against the school of the blow being resolved. Already net of any
        /// resistance the attacker ignores.
        int32 resistance = 0;

        /// Subtracted from a blocked blow.
        int32 blockValue = 0;

        /// Nothing of this school touches the victim at all.
        bool immune = false;

        /// What the victim can be drawn on for shields that charge for their work.
        int32 mana = 0;

        std::vector<Absorber> absorbers;
        std::vector<Splitter> splitters;
    };
}
