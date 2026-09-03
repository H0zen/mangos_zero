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

#include "Combat/Roll.h"

namespace combat
{
    namespace
    {
        /// Skill above the victim's cap shifts every avoidance band down by
        /// 0.04% per point.
        int32 SkillBonus(const Combatant& attacker, const Combatant& victim)
        {
            return 4 * (attacker.weaponSkill - victim.maxDefenceForLevel);
        }

        /// Defence beyond a level's cap counts for nothing.
        int32 CappedDefence(const Combatant& victim)
        {
            return victim.defenceSkill > victim.maxDefenceForLevel
                       ? victim.maxDefenceForLevel
                       : victim.defenceSkill;
        }
    }

    Landing RollMelee(const Combatant& attacker, const Combatant& victim,
                      bool fromBehind, bool isSpellSwing, uint32 roll)
    {
        if (victim.isEvading)
        {
            return Landing::Evade;
        }

        const int32 skillBonus = SkillBonus(attacker, victim);
        int32 sum = 0;
        const int32 r = int32(roll);

        // Miss
        if (attacker.missChance > 0 && r < (sum += attacker.missChance))
        {
            return Landing::Miss;
        }

        // A player who is not on his feet is hit critically by anything that can
        // crit at all, before any avoidance is considered.
        if (victim.isPlayer && victim.isSitting && attacker.critChance > 0)
        {
            return Landing::Crit;
        }

        // Only players lose their dodge to an attacker behind them.
        if (!victim.isPlayer || !fromBehind)
        {
            int32 dodge = victim.dodgeChance;
            if (dodge > 0 && (dodge -= skillBonus) > 0 && r < (sum += dodge))
            {
                return Landing::Dodge;
            }
        }

        // Nothing is parried or blocked from behind, by anyone.
        if (!fromBehind)
        {
            if (victim.canParry && victim.parryChance > 0)
            {
                const int32 parry = victim.parryChance - skillBonus;
                if (parry > 0 && r < (sum += parry))
                {
                    return Landing::Parry;
                }
            }
        }

        // A player or pet swinging up at a higher-level creature glances, up to
        // forty percent of the time. Abilities do not glance, and neither does a
        // ranged shot -- the caller marks both.
        if (!isSpellSwing && (attacker.isPlayer || attacker.isPet) &&
            !victim.isPlayer && !victim.isPet && attacker.level < victim.level)
        {
            const int32 skill = attacker.weaponSkill > attacker.maxSkillForLevel
                                    ? attacker.maxSkillForLevel
                                    : attacker.weaponSkill;

            int32 glancing = (10 + 2 * (victim.defenceSkill - skill)) * 100;
            if (glancing > 4000)
            {
                glancing = 4000;
            }
            if (glancing > 0 && r < (sum += glancing))
            {
                return Landing::Glance;
            }
        }

        if (!fromBehind && victim.canBlock)
        {
            int32 block = victim.blockChance;
            if (block > 0 && (block -= skillBonus) > 0 && r < (sum += block))
            {
                return Landing::Block;
            }
        }

        if (attacker.critChance > 0 && r < (sum += attacker.critChance))
        {
            return Landing::Crit;
        }

        // A creature three levels up, or fifteen weapon skill above the victim's
        // defence, can crush. Auto-attacks only.
        if (attacker.canCrush && !attacker.isPlayer && !attacker.isPet && !isSpellSwing)
        {
            const int32 lacking = attacker.maxSkillForLevel - CappedDefence(victim);
            if (lacking >= 15)
            {
                // Two percent per lacking point, starting at fifteen.
                const int32 crush = lacking * 200 - 1500;
                if (r < (sum += crush))
                {
                    return Landing::Crush;
                }
            }
        }

        return Landing::Hit;
    }

    Landing RollSpell(const Combatant& attacker, const Combatant& victim,
                      int32 missChance, int32 critChance, bool canCrit, uint32 roll)
    {
        (void)attacker;

        if (victim.isEvading)
        {
            return Landing::Evade;
        }

        int32 sum = 0;
        const int32 r = int32(roll);

        if (missChance > 0 && r < (sum += missChance))
        {
            return Landing::Miss;
        }

        if (canCrit && critChance > 0 && r < (sum += critChance))
        {
            return Landing::Crit;
        }

        return Landing::Hit;
    }
}
