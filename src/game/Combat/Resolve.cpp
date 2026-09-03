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

#include "Combat/Resolve.h"
#include "Combat/Roll.h"

#include <algorithm>

namespace combat
{
    namespace
    {
        bool IsPhysical(SpellSchoolMask school)
        {
            return (school & SPELL_SCHOOL_MASK_NORMAL) != 0;
        }

        /// Armour takes a share of a physical blow, capped at three quarters.
        int32 AfterArmour(int32 damage, int32 armour, uint32 attackerLevel)
        {
            if (damage <= 0)
            {
                return 0;
            }
            if (armour < 0)
            {
                armour = 0;
            }

            float share = 0.1f * float(armour) / (8.5f * float(attackerLevel) + 40.f);
            share = share / (1.f + share);
            share = std::max(0.f, std::min(0.75f, share));

            const int32 reduced = int32(float(damage) - float(damage) * share);

            // A blow that connects always leaves a mark.
            return reduced > 1 ? reduced : 1;
        }

        /// The share of a magical blow the victim shrugs off, capped at three
        /// quarters, and drawn in the same four-band pattern the client uses.
        int32 AfterResistance(int32 damage, int32 resistance, uint32 attackerLevel,
                              uint32 roll, int32& resisted)
        {
            resisted = 0;
            if (damage <= 0 || resistance <= 0)
            {
                return damage > 0 ? damage : 0;
            }

            float average = float(resistance) * (0.15f / float(attackerLevel ? attackerLevel : 1));
            average = std::max(0.f, std::min(0.75f, average));

            // Four bands around the average, as the client rolls it.
            const uint32 band = (roll % 10000u) / 2500u;   // 0..3
            const float portion = std::max(0.f, std::min(1.f, average + 0.25f * float(band) - 0.375f));

            resisted = int32(float(damage) * portion);
            if (resisted > damage)
            {
                resisted = damage;
            }
            if (resisted < 0)
            {
                resisted = 0;
            }
            return damage - resisted;
        }

        /// The 1.12 glancing band: how much of the blow survives, from the skill
        /// gap and the attacker's class.
        float GlancingSurvival(const Combatant& attacker, const Combatant& victim,
                               float band)
        {
            float lowBase = 1.3f;
            float highBase = 1.2f;

            switch (attacker.classId)
            {
                case CLASS_SHAMAN:
                case CLASS_PRIEST:
                case CLASS_MAGE:
                case CLASS_WARLOCK:
                case CLASS_DRUID:
                    lowBase -= 0.7f;
                    highBase -= 0.3f;
                    break;
                default:
                    break;
            }

            float lowCap = 0.6f;
            if (attacker.classId == CLASS_WARRIOR || attacker.classId == CLASS_ROGUE)
            {
                lowCap = 0.91f;
            }

            const int32 diff = victim.defenceSkill - attacker.weaponSkill;

            float low = lowBase - 0.05f * float(diff);
            float high = highBase - 0.03f * float(diff);

            low = std::max(0.01f, std::min(low, lowCap));
            high = std::max(0.2f, std::min(0.99f, high));
            if (low > high)
            {
                low = high;
            }

            return low + std::max(0.f, std::min(1.f, band)) * (high - low);
        }

        /// What the landing does to the blow before anything mitigates it.
        int32 AfterLanding(Landing landing, int32 damage, const Combatant& attacker,
                           const Combatant& victim, const Defences& defences,
                           float glanceBand, int32& blocked)
        {
            blocked = 0;

            switch (landing)
            {
                case Landing::Crit:
                    return damage * 2;

                case Landing::Crush:
                    return damage + damage / 2;

                case Landing::Glance:
                    return int32(float(damage) * GlancingSurvival(attacker, victim, glanceBand));

                case Landing::Block:
                    blocked = std::min(defences.blockValue, damage);
                    return damage - blocked;

                default:
                    return damage;
            }
        }

        /// Spend the shields covering this school, in the order they are held.
        void PlanAbsorption(const Defences& defences, SpellSchoolMask school,
                            int32& damage, Outcome& out)
        {
            for (const Absorber& shield : defences.absorbers)
            {
                if (damage <= 0)
                {
                    break;
                }
                if (shield.remaining <= 0 || !shield.Covers(school))
                {
                    continue;
                }

                AbsorbShare share;
                share.caster = shield.caster;
                share.spellId = shield.spellId;

                if (shield.remaining > damage)
                {
                    share.amount = damage;
                    share.exhausted = false;
                }
                else
                {
                    share.amount = shield.remaining;
                    share.exhausted = true;
                }

                damage -= share.amount;
                out.absorbed += share.amount;
                out.absorbs.push_back(share);
            }
        }

        /// Move part of what is left onto whoever shares the victim's pain.
        void PlanSplits(const Defences& defences, int32& damage, Outcome& out)
        {
            for (const Splitter& splitter : defences.splitters)
            {
                if (damage <= 0)
                {
                    break;
                }

                int32 moved = splitter.flat;
                if (splitter.fraction > 0.f)
                {
                    moved += int32(float(damage) * splitter.fraction);
                }
                if (moved <= 0)
                {
                    continue;
                }
                if (moved > damage)
                {
                    moved = damage;
                }

                SplitShare share;
                share.target = splitter.target;
                share.spellId = splitter.spellId;
                share.amount = moved;

                damage -= moved;
                out.splits.push_back(share);
            }
        }
    }

    Outcome Resolve(const Attempt& attempt,
                    const Combatant& attacker,
                    const Combatant& victim,
                    const Defences& defences,
                    bool fromBehind,
                    const Rolls& rolls)
    {
        Outcome out;
        out.beforeMitigation = attempt.base;

        if (defences.immune)
        {
            out.landing = Landing::Immune;
            return out;
        }

        const bool weapon = IsWeaponSwing(attempt.source);

        if (weapon)
        {
            out.landing = RollMelee(attacker, victim, fromBehind,
                                    attempt.spellId != 0, rolls.hit);
        }
        else if (attempt.source == Source::Spell)
        {
            out.landing = RollSpell(attacker, victim, attacker.missChance,
                                    attacker.critChance, attempt.canCrit, rolls.hit);
        }
        else
        {
            // A periodic tick and a fall neither miss nor crit; they simply land.
            out.landing = victim.isEvading ? Landing::Evade : Landing::Hit;
        }

        if (!Landed(out.landing))
        {
            return out;
        }

        int32 damage = AfterLanding(out.landing, attempt.base, attacker, victim,
                                    defences, rolls.glanceBand, out.blocked);

        if (IsPhysical(attempt.school))
        {
            damage = AfterArmour(damage, defences.armour, attacker.level);
        }
        else
        {
            damage = AfterResistance(damage, defences.resistance, attacker.level,
                                     rolls.resist, out.resisted);
            if (damage <= 0)
            {
                out.landing = Landing::Resist;
                return out;
            }
        }

        PlanAbsorption(defences, attempt.school, damage, out);
        PlanSplits(defences, damage, out);

        if (damage < 0)
        {
            damage = 0;
        }

        out.dealt = damage;

        if (victim.health > 0 && damage >= victim.health)
        {
            out.victimDies = true;
            out.overkill = damage - victim.health;
        }

        return out;
    }
}
