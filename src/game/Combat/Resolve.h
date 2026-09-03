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

// One question, answered without touching anything.
//
// Resolve() reads an Attempt, both sides and what the victim has in the way, and
// returns the whole Outcome: how it landed, what reached health, and what it
// WOULD cost the shields and split auras that softened it. It writes nothing.
// Taking the shields down and delivering the splits is Apply()'s job, working from
// the plan in the Outcome.
//
// That division is not tidiness. The old pipeline computed absorption by
// consuming it, and delivered split damage by recursing into a fresh hit in the
// middle of the parent's mitigation -- so a split victim could die, proc and
// generate threat before the blow that split the damage had landed at all.

#include "Combat/Attempt.h"
#include "Combat/Combatant.h"
#include "Combat/Defences.h"

namespace combat
{
    /**
     * @brief Every random draw one resolution needs, supplied by the caller.
     *
     * Named rather than positional so that a later draw can be added without
     * silently reordering the ones already there. All are in [0, ROLL_RANGE)
     * except `glanceBand`, which is a fraction in [0, 1).
     */
    struct Rolls
    {
        uint32 hit = 0;         ///< picks the band in the hit table
        uint32 resist = 0;      ///< how much of a magical blow is resisted
        float glanceBand = 0.f; ///< where in the glancing range this one falls
    };

    /**
     * @brief Resolve one attempt. Pure.
     *
     * `fromBehind` arrives from the caller because facing is geometry, and for
     * anyone aboard a vessel it has to be measured in that deck's frame.
     */
    Outcome Resolve(const Attempt& attempt,
                    const Combatant& attacker,
                    const Combatant& victim,
                    const Defences& defences,
                    bool fromBehind,
                    const Rolls& rolls);
}
