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

#include <algorithm>

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
    /**
     * @brief What a creature's rank multiplies its numbers by.
     *
     * A rank is a statement about how hard the thing is meant to be, and a server says how
     * much harder by three numbers. They are read from the configuration and handed in;
     * nothing here knows where they came from.
     */
    struct RankRates
    {
        float health = 1.0f;
        float damage = 1.0f;
        float spellDamage = 1.0f;
    };

    /**
     * @brief The level a creature spawns at.
     *
     * A template names a band. A spawn either forces a level or takes one from the band,
     * and `roll` is that draw -- passed in, so the answer can be checked without a die.
     */
    inline uint32 CreatureLevel(uint32 minLevel, uint32 maxLevel, uint32 forced, uint32 roll)
    {
        if (forced != 0)
        {
            return forced;
        }

        if (minLevel == maxLevel)
        {
            return minLevel;
        }

        return roll;
    }

    /// What a creature is made of before its rank is applied.
    struct Vitals
    {
        uint32 health = 1;
        uint32 mana = 0;
    };

    /// From the class-and-level table, which is scaled by the template's own multipliers.
    inline Vitals VitalsFromTable(uint32 baseHealth, uint32 baseMana,
                                  float healthMultiplier, float powerMultiplier)
    {
        Vitals made;
        made.health = uint32(baseHealth * healthMultiplier);
        made.mana = uint32(baseMana * powerMultiplier);

        return made;
    }

    /**
     * @brief From the template's own band, read at the level the creature came out at.
     *
     * The band is given by its ends whichever way round the row states them, and the level
     * decides how far along it this creature sits. A band of one level sits at its start.
     */
    inline Vitals VitalsFromBand(uint32 healthAtOneEnd, uint32 healthAtOther,
                                 uint32 manaAtOneEnd, uint32 manaAtOther,
                                 uint32 level, uint32 minLevel, uint32 maxLevel)
    {
        const float along = maxLevel == minLevel
                          ? 0.0f
                          : float(level - minLevel) / float(maxLevel - minLevel);

        const uint32 leastHealth = std::min(healthAtOneEnd, healthAtOther);
        const uint32 mostHealth = std::max(healthAtOneEnd, healthAtOther);
        const uint32 leastMana = std::min(manaAtOneEnd, manaAtOther);
        const uint32 mostMana = std::max(manaAtOneEnd, manaAtOther);

        Vitals made;
        made.health = leastHealth + uint32(along * (mostHealth - leastHealth));
        made.mana = leastMana + uint32(along * (mostMana - leastMana));

        return made;
    }

    /// Nothing alive has less than one point of health, whatever the rate says.
    inline uint32 ScaledHealth(uint32 health, float rate)
    {
        const uint32 scaled = uint32(health * rate);

        return scaled < 1 ? 1 : scaled;
    }

    /**
     * @brief How far off a creature notices somebody.
     *
     * Twenty yards against an equal, a yard more for every level the viewer is below it, a
     * yard less for every level above -- and no more than twenty-five levels' worth either
     * way, so a level 30 and a level 5 are noticed at the same distance by a level 60.
     * Never closer than five yards, which is where a fight happens anyway.
     *
     * Detection yards are what the two sides' auras add between them, and they only count
     * while the creature is low enough for the client to bother: five levels under the cap.
     * The rate is what this server multiplies the whole answer by, and a rate of nothing
     * means a creature notices nobody.
     */
    inline float NoticeRange(uint32 creatureLevel, uint32 viewerLevel, float detectionYards,
                             uint32 maxPlayerLevel, float rate)
    {
        if (rate == 0.0f)
        {
            return 0.0f;
        }

        int32 gap = int32(viewerLevel) - int32(creatureLevel);
        if (gap < -25)
        {
            gap = -25;
        }

        float yards = 20.0f - float(gap);

        if (creatureLevel + 5 <= maxPlayerLevel)
        {
            yards += detectionYards;
        }

        if (yards < 5.0f)
        {
            yards = 5.0f;
        }

        return yards * rate;
    }

    /// What a creature stops with a shield it does not carry. It has no shield
    /// and no shield value in its row, so the game answers from its size.
    inline uint32 CreatureShieldBlock(uint32 level, float strength)
    {
        return level / 2 + uint32(strength / 20.0f);
    }

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
