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

// The currency of combat: one question in, one answer out.
//
// A swing and a spell hit differ only in how they are ROLLED. Everything under
// that -- armour, resistance, absorption, splitting, immunity -- is one
// computation over one pair of units, so it is written once and both entries
// pass through it.
//
// An Attempt is what an initiator knows before anything is looked up: who, at
// whom, from what, for how much before mitigation. An Outcome is the whole
// answer, including what it would cost the auras that softened it. Neither
// carries a pointer into the world, so both survive being handed across a tick.

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <vector>

namespace combat
{
    /// Which table rolls the hit. The only thing melee and spells disagree on.
    enum class Source : uint8
    {
        MeleeMain,
        MeleeOff,
        Ranged,
        Spell,
        Periodic,       ///< a damage-over-time tick
        Environment,    ///< falling, drowning, lava
    };

    inline bool IsWeaponSwing(Source source)
    {
        return source == Source::MeleeMain || source == Source::MeleeOff ||
               source == Source::Ranged;
    }

    /// How the hit landed. One enum for both tables: the values a swing can
    /// produce and a spell cannot simply never appear on a spell's Outcome.
    enum class Landing : uint8
    {
        Hit,
        Crit,
        Miss,
        Dodge,
        Parry,
        Block,          ///< partial: some damage still lands
        Glance,
        Crush,
        Resist,         ///< fully resisted
        Immune,
        Evade,          ///< the victim is not accepting hits at all
    };

    inline bool Landed(Landing landing)
    {
        return landing == Landing::Hit || landing == Landing::Crit ||
               landing == Landing::Block || landing == Landing::Glance ||
               landing == Landing::Crush;
    }

    /**
     * @brief What an initiator asks: this attacker, this victim, this much.
     *
     * `base` is pre-mitigation and already carries whatever the initiator knows
     * -- weapon damage and attack power for a swing, the effect's computed
     * amount and its spell modifiers for a cast. Mitigation is not the
     * initiator's business and is applied exactly once, in resolve().
     */
    struct Attempt
    {
        ObjectGuid attacker;
        ObjectGuid victim;

        Source source = Source::MeleeMain;

        uint32 spellId = 0;         ///< 0 for a swing
        uint8 effectIndex = 0;      ///< only meaningful for a spell

        SpellSchoolMask school = SPELL_SCHOOL_MASK_NORMAL;
        int32 base = 0;             ///< before mitigation

        bool canCrit = true;
        bool triggered = false;     ///< fired by a proc rather than by intent
    };

    /**
     * @brief An absorbing aura, and how much of this hit it would take.
     *
     * resolve() decides the split of a hit across the shields covering it;
     * apply() is what actually spends them. Keeping the decision and the
     * spending apart is what lets resolve() stay a function of its arguments.
     */
    struct AbsorbShare
    {
        ObjectGuid caster;      ///< who cast the shield
        uint32 spellId = 0;
        int32 amount = 0;       ///< taken off this hit
        bool exhausted = false; ///< the shield is spent and comes off
    };

    /**
     * @brief Damage this hit moves onto somebody else.
     *
     * A split aura sends part of the hit to another unit. It is recorded here
     * and dealt AFTER the hit that caused it, rather than recursively during
     * the mitigation of that hit -- otherwise the recipient can die, proc and
     * generate threat before the blow that split the damage has landed.
     */
    struct SplitShare
    {
        ObjectGuid target;
        uint32 spellId = 0;
        int32 amount = 0;
    };

    /**
     * @brief The whole answer.
     *
     * Fields a swing produces and a spell does not -- `blocked`, `Glance` --
     * are simply zero and absent there. That is the shape of the wire too: the
     * blocks in an attacker-state update that only a swing fills.
     */
    struct Outcome
    {
        Landing landing = Landing::Hit;

        int32 dealt = 0;        ///< what reaches the victim's health
        int32 absorbed = 0;
        int32 resisted = 0;
        int32 blocked = 0;
        int32 overkill = 0;     ///< dealt beyond the victim's remaining health

        bool victimDies = false;

        std::vector<AbsorbShare> absorbs;
        std::vector<SplitShare> splits;

        /// Before mitigation, for the log and for threat.
        int32 beforeMitigation = 0;
    };
}
