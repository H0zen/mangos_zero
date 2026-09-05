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

#include "Modifiers.h"

/**
 * The numbers a creature fights with, worked out and nothing else.
 *
 * Nothing here reads a unit or writes a field. What a creature's armour comes to
 * given its modifiers is a question with an answer, and the answer does not
 * depend on there being a creature to ask -- which is what lets it be checked.
 *
 * A creature is the simple case, and worth reading first: most of its numbers are
 * its modifiers folded, with no stat to derive them from. Only the swing is
 * really computed, and only because attack power feeds into it.
 */
namespace stats
{
    /// Armour, health, a school's resistance, a pool of power: the fold, no more.
    inline float Simple(Modifiers const& mods) { return mods.Folded(); }

    /// What a swing does, at its least and its most.
    struct Swing
    {
        float least = 0.0f;
        float most = 0.0f;
    };

    /**
     * @brief The damage one of a creature's weapons does.
     *
     * @param mods The weapon's own four modifiers.
     * @param weaponLeast The weapon's own low roll.
     * @param weaponMost The weapon's own high roll.
     * @param attackPowerGained How much attack power it has above what its
     *        template was written with. Only the difference counts: the template's
     *        damage already includes the attack power the template gave it.
     * @param perSecond How much of a second one swing is worth, which is how
     *        attack power becomes damage.
     * @param damageMultiplier The template's own multiplier over the whole thing.
     */
    inline Swing CreatureSwing(Modifiers const& mods, float weaponLeast, float weaponMost,
                               float attackPowerGained, float perSecond, float damageMultiplier)
    {
        // Fourteen is what a point of attack power is worth over a second.
        float const fromPower = attackPowerGained * perSecond / 14.0f;
        float const base = mods.baseValue + fromPower;

        float const share = mods.TotalPct();

        Swing swing;
        swing.least = ((base + weaponLeast) * damageMultiplier * mods.basePct + mods.totalValue) * share;
        swing.most = ((base + weaponMost) * damageMultiplier * mods.basePct + mods.totalValue) * share;

        return swing;
    }

    /// What the attack power fields are set to: a base, a flat addition, and a share.
    struct AttackPower
    {
        int32 base = 0;
        int32 added = 0;
        float share = 0.0f;
    };

    inline AttackPower CreatureAttackPower(Modifiers const& mods)
    {
        AttackPower power;
        power.base = static_cast<int32>(mods.Base());
        power.added = static_cast<int32>(mods.totalValue);
        power.share = mods.TotalShare();

        return power;
    }
}
