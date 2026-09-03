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

#include "Spells/Ids.h"

#include <cstdint>

namespace spells
{
    class AuraBook;
    class SpellBook;

    /// Where something is, in whichever frame it belongs to.
    struct Where
    {
        uint32_t map = 0;
        uint64_t vessel = 0;    ///< nonzero: coordinates are deck-local to it
        float x = 0.f, y = 0.f, z = 0.f, o = 0.f;

        /// Two things can be measured against each other only in one frame.
        bool ShareFrame(const Where& other) const
        {
            return map == other.map && vessel == other.vessel;
        }
    };

    /// The kinds of thing that can start a cast.
    enum class Actor : uint8_t
    {
        Unit,       ///< players and creatures: has power, auras, can die
        Object,     ///< a gameobject trap or trigger
    };

    /**
     * A caster, assembled rather than inherited.
     *
     * There is no base class and no override. A caster is a snapshot of the
     * facts a cast needs, plus references to the books the entity happens to
     * own. What an entity cannot do shows up as a null reference, not as a
     * subclass that throws or silently does nothing.
     *
     * That matters most for gameobjects. A trap casts, and the client is told
     * about it in the same packets, but a gameobject carries no auras, takes no
     * damage and never dies. Modelling it as a unit that is "mostly like a unit"
     * is what made gameobject casting wrong everywhere it was attempted. Here
     * `auras` and `book` are simply null for one, so every path that wants them
     * has to say what it does without them.
     */
    struct Caster
    {
        Actor kind = Actor::Unit;
        uint64_t guid = 0;

        /// The unit that owns the consequences: a trap's summoner, a pet's
        /// master, or the caster itself. Threat, credit and blame land here.
        uint64_t owner = 0;

        uint8_t level = 1;
        Where where;

        /// Null for a gameobject, which has none.
        AuraBook* auras = nullptr;

        /// Null for anything that does not learn spells or keep cooldowns.
        SpellBook* book = nullptr;

        /// Damage and healing contributed by gear and talents, by school.
        int32_t spellPower[7] = {0, 0, 0, 0, 0, 0, 0};
        int32_t healPower = 0;

        int32_t currentPower = 0;
        int32_t maxPower = 0;
        uint8_t powerType = 0;

        bool alive = true;
        bool sitting = false;
        bool inCombat = false;

        bool IsUnit() const { return kind == Actor::Unit; }

        /// Whether this caster can be affected by auras at all.
        bool Bearable() const { return auras != nullptr; }

        /// Whether cooldowns and known-spell checks apply.
        bool Disciplined() const { return book != nullptr; }
    };

    /// A thing a spell can land on.
    struct Target
    {
        uint64_t guid = 0;
        Where where;
        AuraBook* auras = nullptr;  ///< null when it cannot bear auras
        uint8_t level = 1;
        bool alive = true;
        bool attackable = false;
        bool friendly = false;

        bool Valid() const { return guid != 0; }
    };
}
