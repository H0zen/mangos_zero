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

#include "Platform/Define.h"

/**
 * The chances a player's swing and a player's guard come to.
 *
 * Blocking, parrying, dodging and critting are worked out the same way and
 * always have been: something to start from, a correction for how the skill
 * that governs it compares with what the level allows, whatever auras add, and
 * never less than nothing.
 *
 * Which skill governs it is the only thing that differs. Guarding is governed by
 * defence; critting by the skill of the weapon in hand.
 */
namespace stats
{
    /// What one point of skill above or below what the level allows is worth.
    float const PER_SKILL_POINT = 0.04f;

    /// A block or a parry starts here for anyone who can do it at all.
    float const GUARD_FROM_NOTHING = 5.0f;

    /**
     * @brief A chance out of a hundred.
     *
     * @param from Where it starts: five for a block or a parry, and what agility
     *        gives for a dodge, or what the modifier group holds for a crit.
     * @param skill The governing skill: defence for a guard, the weapon's for a crit.
     * @param allowedByLevel What that skill could be at this level. Being under it
     *        is what makes a low-skilled weapon miss and a low defence get hit.
     * @param fromAuras What everything else has added, which may be negative.
     * @return Never below nothing. A chance cannot go the other way.
     */
    inline float Chance(float from, int32 skill, int32 allowedByLevel, float fromAuras)
    {
        float const chance = from + (skill - allowedByLevel) * PER_SKILL_POINT + fromAuras;

        return chance < 0.0f ? 0.0f : chance;
    }

    /**
     * @brief What a caster's mana regeneration comes to, in and out of a cast.
     *
     * Two sources, and only one of them stops while casting: what spirit gives
     * is interrupted, and what an aura gives outright is not. How much of the
     * spirit half survives is itself an aura, and it cannot buy back more than
     * all of it.
     */
    struct ManaRegen
    {
        float standing = 0.0f;                              ///< while not casting
        float casting = 0.0f;                               ///< while casting
    };

    inline ManaRegen Regeneration(float fromSpirit, float spiritShare, float flatPerFive,
                                  int32 survivesCasting)
    {
        float const spirit = fromSpirit * spiritShare;
        float const flat = flatPerFive / 5.0f;

        if (survivesCasting > 100)
        {
            survivesCasting = 100;
        }

        ManaRegen regen;
        regen.standing = flat + spirit;
        regen.casting = flat + spirit * survivesCasting / 100.0f;

        return regen;
    }
}
