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

// One blow, and what became of it.
//
// A swing and a spell are the same event. They carry the same attacker and
// victim, the same single school, the same amount, and the same vocabulary of
// endings -- the client holds one list of words for both, and a shield cast by a
// spell stops a sword without asking what swung it. So there is one type here,
// not one per delivery.
//
// What genuinely differs is HOW a blow arrives, and that stays as an attribute
// rather than a type: delivery decides which endings are reachable. A swing can
// be dodged or parried; a bolt of the same school cannot. Glancing and crushing
// belong to weapons alone. That is a difference in the values a field may take,
// never a difference in shape.
//
// Neither Blow nor Outcome holds a pointer into the world, so both survive being
// carried across a tick and handed to a queue.

#include "Combat/School.h"
#include "Platform/Define.h"
#include "ObjectGuid.h"

#include <vector>

namespace combat
{
    /// How a blow reaches its victim.
    enum class Delivery : uint8
    {
        MeleeMain,
        MeleeOff,
        Ranged,
        Spell,
        Periodic,       ///< a tick of something already on the victim
        Environment,    ///< falling, drowning, lava
    };

    inline bool IsWeaponSwing(Delivery delivery)
    {
        return delivery == Delivery::MeleeMain || delivery == Delivery::MeleeOff ||
               delivery == Delivery::Ranged;
    }

    /**
     * @brief How a blow ended.
     *
     * These are the endings the client can name, and it names them the same way
     * for a swing and for a spell. Landed is the only one that puts a number on
     * the screen; every other value puts a word there instead.
     *
     * Blocked, Absorbed and Resisted appear here only when nothing at all got
     * through. A blow that is partly blocked or partly absorbed has Landed and
     * carries the amounts, because that is what the victim sees happen.
     */
    enum class Result : uint8
    {
        Landed,
        Missed,
        Dodged,
        Parried,
        Blocked,        ///< the shield took all of it
        Evaded,         ///< the victim is not accepting blows at all
        Immune,
        Deflected,
        Absorbed,       ///< a shield took all of it
        Resisted,       ///< resistance took all of it
        Reflected,      ///< it goes back the way it came
    };

    inline bool Landed(Result result)
    {
        return result == Result::Landed;
    }

    /**
     * @brief What the roll decided, before anything is subtracted.
     *
     * The ending and the amplifiers are kept apart because they answer different
     * questions. A critical blow and a glancing one both LANDED; they differ in
     * how much of the swing survives. Folding them into the ending is what makes
     * a blocked hit stop procing as a hit, and what makes "did it connect" and
     * "how hard" impossible to ask separately.
     */
    struct Strike
    {
        Result result = Result::Landed;

        bool crit = false;
        bool glancing = false;      ///< weapons only
        bool crushing = false;      ///< weapons only
        bool blocked = false;       ///< partial: the shield's value comes off

        bool Landed() const { return combat::Landed(result); }
    };

    /**
     * @brief What an initiator asks: this attacker, this victim, this much.
     *
     * `amount` is pre-mitigation and already carries whatever the initiator
     * knows -- weapon damage and attack power for a swing, the effect's computed
     * value and its modifiers for a cast. Mitigation is not the initiator's
     * business and happens exactly once, in Resolve.
     */
    struct Blow
    {
        ObjectGuid attacker;
        ObjectGuid victim;

        Delivery delivery = Delivery::MeleeMain;

        /// The spell this blow is. Zero for a plain swing, which is the only
        /// blow with no identity of its own.
        uint32 spellId = 0;
        uint8 effectIndex = 0;

        School school = School::Physical;
        int32 amount = 0;

        bool canCrit = true;
        bool triggered = false;     ///< caused by a proc rather than by intent

        bool SelfInflicted() const { return attacker == victim; }
    };

    /**
     * @brief An absorbing aura, and how much of this blow it would take.
     *
     * Resolve decides the split of a blow across the shields covering it; Apply
     * is what actually takes them down. Keeping the decision and the consequence
     * apart is what lets Resolve stay a function of its arguments.
     */
    struct AbsorbShare
    {
        ObjectGuid caster;      ///< who cast the shield
        uint32 spellId = 0;
        int32 amount = 0;       ///< taken off this blow
        int32 manaSpent = 0;    ///< what holding it up cost the victim
        bool exhausted = false; ///< the shield is spent and comes off
    };

    /**
     * @brief Damage this blow moves onto somebody else.
     *
     * A split aura sends part of the blow to another unit. It is recorded here
     * and dealt AFTER the blow that caused it, rather than recursively during
     * that blow's own mitigation -- otherwise the recipient can die, proc and
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
     * Fields a swing fills and a spell does not -- `blocked`, glancing -- are
     * simply zero and false there. That is the shape of the wire too: the blocks
     * an attacker-state update carries only for a swing.
     */
    struct Outcome
    {
        Strike strike;

        int32 dealt = 0;        ///< what reaches the victim's health
        int32 absorbed = 0;
        int32 resisted = 0;
        int32 blocked = 0;
        int32 overkill = 0;     ///< dealt beyond the victim's remaining health

        bool victimDies = false;

        std::vector<AbsorbShare> absorbs;
        std::vector<SplitShare> splits;

        /// Total drawn from the victim's mana by shields that charge for it.
        int32 manaSpent = 0;

        /// Before mitigation, for the log and for threat.
        int32 beforeMitigation = 0;

        Result Ending() const { return strike.result; }
        bool Landed() const { return strike.Landed(); }
    };
}
