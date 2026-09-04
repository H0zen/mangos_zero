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

#include "Combat/Blow.h"

#include <cstdint>
#include <vector>

class Unit;

namespace combat
{
    /**
     * The one place a blow changes the world.
     *
     * `resolve` decides what happens and touches nothing. `Apply` carries it
     * out, and is the only function in the engine that moves a health bar,
     * takes a shield down or kills anybody. That is the whole point of the
     * split: when a health total is wrong, there is one function to read.
     *
     * ## Nothing is spent here
     *
     * There is deliberately no `combat::Spend`, and the absence is not an
     * oversight. The line is agency, not the kind of resource and not melee
     * versus spell. An actor that *chooses* to pay in order to act pays in
     * `spells::Spend`; the world taking something because of what happened is
     * this function.
     *
     * Rage is the case that proves it, because it crosses the line in both
     * directions and lands correctly each time. A warrior electing to use
     * Heroic Strike pays rage for it -- that is an ability going through the
     * cast pipeline like any other, and the resource being rage rather than mana
     * changes which column is debited, nothing else. A warrior being struck
     * *gains* rage, and chose none of it; that is step four below.
     *
     * A plain auto-attack, which is not an ability at all, pays nothing and
     * never reaches `spells::Spend` in the first place.
     *
     * Naming both sides "spend" would have put two unrelated ideas under one
     * word in neighbouring namespaces, and the first person to read
     * `combat::Spend` would reasonably have gone looking for a mana cost.
     *
     * ## The order is fixed
     *
     * The steps below happen in this sequence, always, for every blow from any
     * source. The old engine ran them in whatever order each call site had
     * grown into, which is why absorbs, splits and death interacted differently
     * depending on whether the damage came from a swing, a spell or a tick.
     *
     *  1. **Shields give way.** Absorbing auras come down by what they stopped,
     *     the exhausted ones are removed, and any mana their absorption costs is
     *     drawn from the bearer.
     *  2. **Splits are owed.** Damage redirected onto somebody else is recorded
     *     as a debt, not dealt here -- dealing it inline is what let one blow
     *     re-enter the whole pipeline halfway through its own bookkeeping.
     *  3. **Health moves.** Once, by the final figure.
     *  4. **Rage answers the blow.** Dealing damage and taking it both generate
     *     rage, from the figure settled in step three. The old engine did this
     *     from three separate places inside its damage routine, which is why
     *     the amount depended on which path the damage had taken to get there.
     *  5. **Death is settled**, if the figure was enough.
     *  6. **Procs are queued**, never called. What they trigger runs after this
     *     blow is finished and its state is whole.
     *  7. **Owed splits are delivered**, each one a fresh blow through this same
     *     function, under a depth cap.
     */

    /// What a blow left behind, for the log packets and for the proc queue.
    struct Aftermath
    {
        int32_t healthTaken = 0;
        int32_t absorbed = 0;
        int32_t resisted = 0;
        int32_t blocked = 0;
        bool killed = false;

        /// Rage the blow generated, which neither side elected to receive.
        int32_t rageToAttacker = 0;
        int32_t rageToVictim = 0;

        /// Splits recorded in step 2, delivered in step 6.
        std::vector<SplitShare> owed;
    };

    /**
     * Carries out a resolved outcome against its victim.
     *
     * @param attacker who struck; may be the victim itself for self-damage
     * @param victim   who takes it
     * @param outcome  what `resolve` decided, already mitigated
     * @param depth    how many blows deep this chain already is
     */
    Aftermath Apply(Unit& attacker, Unit& victim, const Outcome& outcome, uint8_t depth);

    /// The chain length a split may reach before it is dropped and logged.
    constexpr uint8_t MAX_CHAIN_DEPTH = 8;
}
