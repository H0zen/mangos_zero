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

namespace stats
{

    struct RankRates
    {
        float health = 1.0f;
        float damage = 1.0f;
        float spellDamage = 1.0f;
    };

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

    struct Vitals
    {
        uint32 health = 1;
        uint32 mana = 0;
    };

    inline Vitals VitalsFromTable(uint32 baseHealth, uint32 baseMana,
                                  float healthMultiplier, float powerMultiplier)
    {
        Vitals made;
        made.health = uint32(baseHealth * healthMultiplier);
        made.mana = uint32(baseMana * powerMultiplier);

        return made;
    }

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

    inline uint32 ScaledHealth(uint32 health, float rate)
    {
        const uint32 scaled = uint32(health * rate);

        return scaled < 1 ? 1 : scaled;
    }

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

    inline float FallShare(float yards, float yardsForgiven)
    {
        const float share = 0.018f * (yards - yardsForgiven) - 0.2426f;

        return share > 0.0f ? share : 0.0f;
    }

    inline uint32 FallDamage(float yards, float yardsForgiven, uint32 fullHealth, float rate)
    {
        return uint32(FallShare(yards, yardsForgiven) * float(fullHealth) * rate);
    }

    inline uint32 CreatureShieldBlock(uint32 level, float strength)
    {
        return level / 2 + uint32(strength / 20.0f);
    }

    inline float Simple(Modifiers const& mods) { return mods.Folded(); }

    struct Swing
    {
        float least = 0.0f;
        float most = 0.0f;
    };

    inline Swing CreatureSwing(Modifiers const& mods, float weaponLeast, float weaponMost,
                               float attackPowerGained, float perSecond, float damageMultiplier)
    {

        float const fromPower = attackPowerGained * perSecond / 14.0f;
        float const base = mods.baseValue + fromPower;

        float const share = mods.TotalPct();

        Swing swing;
        swing.least = ((base + weaponLeast) * damageMultiplier * mods.basePct + mods.totalValue) * share;
        swing.most = ((base + weaponMost) * damageMultiplier * mods.basePct + mods.totalValue) * share;

        return swing;
    }

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
