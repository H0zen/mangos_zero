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

// The hit tables, exercised as functions.
//
// The roll is a parameter, so each band is a case rather than a statistic: put
// the roll one unit inside a band and one unit past it, and the boundary is
// pinned. No server, no random number generator, no Unit.

#include "doctest.h"

#include "Combat/Roll.h"

using combat::Combatant;
using combat::Result;
using combat::RollMelee;
using combat::RollSpell;
using combat::Strike;

namespace
{
    /// The bands the table can produce, as one name each.
    ///
    /// A Strike keeps the ending and the amplifiers apart, because a critical
    /// blow and a blocked one both landed. The table still picks exactly one
    /// band per roll, so for a boundary case the two forms carry the same
    /// information and this collapses them back into the one the case is about.
    enum class Band
    {
        Hit, Crit, Glance, Crush, Block, Miss, Dodge, Parry, Evade
    };

    Band BandOf(const Strike& strike)
    {
        switch (strike.result)
        {
            case Result::Missed:  return Band::Miss;
            case Result::Dodged:  return Band::Dodge;
            case Result::Parried: return Band::Parry;
            case Result::Evaded:  return Band::Evade;
            default:              break;
        }

        if (strike.crit)     { return Band::Crit; }
        if (strike.glancing) { return Band::Glance; }
        if (strike.crushing) { return Band::Crush; }
        if (strike.blocked)  { return Band::Block; }
        return Band::Hit;
    }

    /// A level-60 player with a capped weapon skill and no chance of anything
    /// but a plain hit. Cases turn on one number at a time from here.
    Combatant Attacker()
    {
        Combatant c;
        c.level = 60;
        c.isPlayer = true;
        c.weaponSkill = 300;
        c.maxSkillForLevel = 300;
        return c;
    }

    /// A level-60 creature that avoids nothing.
    Combatant Victim()
    {
        Combatant c;
        c.level = 60;
        c.defenceSkill = 300;
        c.maxDefenceForLevel = 300;
        return c;
    }

    Band Swing(const Combatant& a, const Combatant& v, uint32 roll,
               bool fromBehind = false, bool isSpellSwing = false)
    {
        return BandOf(RollMelee(a, v, fromBehind, isSpellSwing, roll));
    }
}

TEST_CASE("With no avoidance and no crit, every roll is a plain hit")
{
    const Combatant a = Attacker();
    const Combatant v = Victim();

    CHECK(Swing(a, v, 0) == Band::Hit);
    CHECK(Swing(a, v, 5000) == Band::Hit);
    CHECK(Swing(a, v, combat::ROLL_RANGE - 1) == Band::Hit);
}

TEST_CASE("An evading victim takes nothing, whatever the roll")
{
    const Combatant a = Attacker();
    Combatant v = Victim();
    v.isEvading = true;
    v.dodgeChance = 5000;

    CHECK(Swing(a, v, 0) == Band::Evade);
    CHECK(Swing(a, v, 9999) == Band::Evade);
}

TEST_CASE("Miss owns the bottom of the range")
{
    Combatant a = Attacker();
    a.missChance = 500;   // five percent
    const Combatant v = Victim();

    CHECK(Swing(a, v, 0) == Band::Miss);
    CHECK(Swing(a, v, 499) == Band::Miss);
    CHECK(Swing(a, v, 500) == Band::Hit);
}

TEST_CASE("The bands stack in order: miss, then dodge, then parry")
{
    Combatant a = Attacker();
    a.missChance = 500;

    Combatant v = Victim();
    v.dodgeChance = 500;
    v.parryChance = 500;

    CHECK(Swing(a, v, 499) == Band::Miss);
    CHECK(Swing(a, v, 500) == Band::Dodge);
    CHECK(Swing(a, v, 999) == Band::Dodge);
    CHECK(Swing(a, v, 1000) == Band::Parry);
    CHECK(Swing(a, v, 1499) == Band::Parry);
    CHECK(Swing(a, v, 1500) == Band::Hit);
}

TEST_CASE("Weapon skill above the victim's cap shrinks the avoidance bands")
{
    // Four hundredths of a percent per point. Ten points of skill takes 40 off
    // a dodge chance stated in the same units.
    Combatant a = Attacker();
    a.weaponSkill = 310;

    Combatant v = Victim();
    v.dodgeChance = 500;

    CHECK(Swing(a, v, 459) == Band::Dodge);
    CHECK(Swing(a, v, 460) == Band::Hit);
}

TEST_CASE("Skill can close an avoidance band entirely")
{
    Combatant a = Attacker();
    a.weaponSkill = 400;   // a hundred points over

    Combatant v = Victim();
    v.dodgeChance = 300;   // less than the 400 the bonus removes

    CHECK(Swing(a, v, 0) == Band::Hit);
}

TEST_CASE("A player behind you takes your dodge away, a creature's does not")
{
    Combatant a = Attacker();

    Combatant player = Victim();
    player.isPlayer = true;
    player.dodgeChance = 5000;

    CHECK(Swing(a, player, 0, /*fromBehind*/ false) == Band::Dodge);
    CHECK(Swing(a, player, 0, /*fromBehind*/ true) == Band::Hit);

    Combatant creature = Victim();
    creature.dodgeChance = 5000;

    CHECK(Swing(a, creature, 0, /*fromBehind*/ true) == Band::Dodge);
}

TEST_CASE("Nobody parries or blocks an attacker behind them")
{
    const Combatant a = Attacker();

    Combatant v = Victim();
    v.parryChance = 5000;
    v.blockChance = 5000;

    CHECK(Swing(a, v, 0, /*fromBehind*/ false) == Band::Parry);
    CHECK(Swing(a, v, 0, /*fromBehind*/ true) == Band::Hit);
}

TEST_CASE("A creature forbidden to parry or block does neither")
{
    const Combatant a = Attacker();

    Combatant v = Victim();
    v.parryChance = 5000;
    v.blockChance = 5000;
    v.canParry = false;
    v.canBlock = false;

    CHECK(Swing(a, v, 0) == Band::Hit);
}

TEST_CASE("A seated player is hit critically before anything can avoid it")
{
    const Combatant a = [] { Combatant c = Attacker(); c.critChance = 1; return c; }();

    Combatant v = Victim();
    v.isPlayer = true;
    v.isSitting = true;
    v.dodgeChance = 5000;
    v.parryChance = 5000;

    CHECK(Swing(a, v, 0) == Band::Crit);
    CHECK(Swing(a, v, 9999) == Band::Crit);
}

TEST_CASE("A miss still beats a seated victim's automatic crit")
{
    // Miss is rolled first, so sitting down does not make a swing unmissable.
    Combatant a = Attacker();
    a.missChance = 500;
    a.critChance = 1;

    Combatant v = Victim();
    v.isPlayer = true;
    v.isSitting = true;

    CHECK(Swing(a, v, 0) == Band::Miss);
    CHECK(Swing(a, v, 500) == Band::Crit);
}

TEST_CASE("A seated victim is not auto-crit by an attacker who cannot crit")
{
    const Combatant a = Attacker();   // critChance 0

    Combatant v = Victim();
    v.isPlayer = true;
    v.isSitting = true;

    CHECK(Swing(a, v, 0) == Band::Hit);
}

TEST_CASE("A player swinging up at a higher-level creature glances")
{
    Combatant a = Attacker();
    a.level = 60;

    Combatant v = Victim();
    v.level = 63;
    v.defenceSkill = 315;
    v.maxDefenceForLevel = 315;

    // (10 + 2 * (315 - 300)) * 100 = 4000, the cap.
    CHECK(Swing(a, v, 3999) == Band::Glance);
    CHECK(Swing(a, v, 4000) == Band::Hit);
}

TEST_CASE("Glancing is capped at forty percent however wide the skill gap")
{
    Combatant a = Attacker();
    a.level = 60;

    Combatant v = Victim();
    v.level = 70;
    v.defenceSkill = 500;
    v.maxDefenceForLevel = 500;

    CHECK(Swing(a, v, 3999) == Band::Glance);
    CHECK(Swing(a, v, 4000) == Band::Hit);
}

TEST_CASE("An ability does not glance, and neither does a swing at a player")
{
    Combatant a = Attacker();
    a.level = 60;

    Combatant creature = Victim();
    creature.level = 63;
    creature.defenceSkill = 315;
    creature.maxDefenceForLevel = 315;

    CHECK(Swing(a, creature, 0, false, /*isSpellSwing*/ true) == Band::Hit);

    Combatant player = creature;
    player.isPlayer = true;
    CHECK(Swing(a, player, 0) == Band::Hit);
}

TEST_CASE("A creature does not glance at all")
{
    Combatant a = Attacker();
    a.isPlayer = false;
    a.level = 60;

    Combatant v = Victim();
    v.level = 63;
    v.defenceSkill = 315;
    v.maxDefenceForLevel = 315;

    CHECK(Swing(a, v, 0) == Band::Hit);
}

TEST_CASE("Block sits between glancing and crit")
{
    const Combatant a = [] { Combatant c = Attacker(); c.critChance = 500; return c; }();

    Combatant v = Victim();
    v.blockChance = 500;

    CHECK(Swing(a, v, 499) == Band::Block);
    CHECK(Swing(a, v, 500) == Band::Crit);
    CHECK(Swing(a, v, 999) == Band::Crit);
    CHECK(Swing(a, v, 1000) == Band::Hit);
}

TEST_CASE("A creature far above its victim crushes")
{
    Combatant a = Attacker();
    a.isPlayer = false;
    a.canCrush = true;
    a.level = 63;
    a.maxSkillForLevel = 315;

    Combatant v = Victim();
    v.defenceSkill = 300;
    v.maxDefenceForLevel = 300;

    // Fifteen points lacking: 15 * 200 - 1500 = 1500.
    CHECK(Swing(a, v, 1499) == Band::Crush);
    CHECK(Swing(a, v, 1500) == Band::Hit);
}

TEST_CASE("Fourteen points short of the gap is no crush at all")
{
    Combatant a = Attacker();
    a.isPlayer = false;
    a.canCrush = true;
    a.level = 62;
    a.maxSkillForLevel = 314;

    Combatant v = Victim();
    v.defenceSkill = 300;
    v.maxDefenceForLevel = 300;

    CHECK(Swing(a, v, 0) == Band::Hit);
}

TEST_CASE("Defence above a level's cap does not hold off a crush")
{
    // The gap is measured against the capped value, so stacking defence past
    // the cap buys nothing here.
    Combatant a = Attacker();
    a.isPlayer = false;
    a.canCrush = true;
    a.level = 63;
    a.maxSkillForLevel = 315;

    Combatant v = Victim();
    v.defenceSkill = 400;          // well over
    v.maxDefenceForLevel = 300;    // but the cap is what counts

    CHECK(Swing(a, v, 0) == Band::Crush);
}

TEST_CASE("A player never crushes, and an ability never crushes")
{
    Combatant a = Attacker();
    a.canCrush = true;
    a.level = 63;
    a.maxSkillForLevel = 315;

    Combatant v = Victim();
    v.defenceSkill = 300;
    v.maxDefenceForLevel = 300;

    CHECK(Swing(a, v, 0) == Band::Hit);           // isPlayer

    a.isPlayer = false;
    CHECK(Swing(a, v, 0, false, /*isSpellSwing*/ true) == Band::Hit);
}

TEST_CASE("A spell weighs landing and crit, and nothing else")
{
    const Combatant a = Attacker();

    Combatant v = Victim();
    v.dodgeChance = 5000;
    v.parryChance = 5000;
    v.blockChance = 5000;

    // None of the melee avoidance applies to a bolt.
    CHECK(BandOf(RollSpell(a, v, 0, 0, true, 0)) == Band::Hit);
    CHECK(BandOf(RollSpell(a, v, 0, 0, true, 9999)) == Band::Hit);
}

TEST_CASE("A spell's miss band comes before its crit band")
{
    const Combatant a = Attacker();
    const Combatant v = Victim();

    CHECK(BandOf(RollSpell(a, v, 400, 500, true, 399)) == Band::Miss);
    CHECK(BandOf(RollSpell(a, v, 400, 500, true, 400)) == Band::Crit);
    CHECK(BandOf(RollSpell(a, v, 400, 500, true, 899)) == Band::Crit);
    CHECK(BandOf(RollSpell(a, v, 400, 500, true, 900)) == Band::Hit);
}

TEST_CASE("A spell that cannot crit does not, however high the chance")
{
    const Combatant a = Attacker();
    const Combatant v = Victim();

    CHECK(BandOf(RollSpell(a, v, 0, 9000, /*canCrit*/ false, 0)) == Band::Hit);
}

TEST_CASE("A spell against an evading victim is refused too")
{
    const Combatant a = Attacker();
    Combatant v = Victim();
    v.isEvading = true;

    CHECK(BandOf(RollSpell(a, v, 0, 0, true, 0)) == Band::Evade);
}

TEST_CASE("Every band that carries damage comes back as a landing")
{
    // The amplifiers are not endings: a critical, glancing, crushing or blocked
    // blow all connected, and only the avoidance bands did not. Reading that off
    // the ending alone is what the two fields exist for.
    Combatant a = Attacker();
    Combatant v = Victim();

    a.critChance = 10000;
    CHECK(RollMelee(a, v, false, false, 0).Landed());

    a.critChance = 0;
    a.missChance = 10000;
    CHECK_FALSE(RollMelee(a, v, false, false, 0).Landed());

    a.missChance = 0;
    v.dodgeChance = 10000;
    CHECK_FALSE(RollMelee(a, v, false, false, 0).Landed());

    v.dodgeChance = 0;
    v.parryChance = 10000;
    CHECK_FALSE(RollMelee(a, v, false, false, 0).Landed());

    v.parryChance = 0;
    v.blockChance = 10000;
    const Strike blocked = RollMelee(a, v, false, false, 0);
    CHECK(blocked.Landed());
    CHECK(blocked.blocked);

    v.blockChance = 0;
    v.isEvading = true;
    CHECK_FALSE(RollMelee(a, v, false, false, 0).Landed());
}

TEST_CASE("The same inputs always give the same answer")
{
    // The property that makes a hit table reviewable: nothing inside it is
    // drawn, so two calls cannot disagree.
    Combatant a = Attacker();
    a.missChance = 500;
    a.critChance = 500;

    Combatant v = Victim();
    v.dodgeChance = 500;

    for (uint32 roll = 0; roll < combat::ROLL_RANGE; roll += 97)
    {
        CHECK(Swing(a, v, roll) == Swing(a, v, roll));
    }
}
