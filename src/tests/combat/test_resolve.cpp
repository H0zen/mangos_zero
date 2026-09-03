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

// Resolution, end to end, with nothing running.
//
// The cases that matter most are the ones about what resolve does NOT do: it
// never spends a shield, it never delivers a split. Both come back as a plan for
// apply() to carry out, which is what stops a split victim dying before the blow
// that split the damage has landed.

#include "doctest.h"

#include "Combat/Resolve.h"

using combat::Absorber;
using combat::Attempt;
using combat::Combatant;
using combat::Defences;
using combat::Landing;
using combat::Outcome;
using combat::Resolve;
using combat::Rolls;
using combat::Source;
using combat::Splitter;

namespace
{
    Combatant Attacker()
    {
        Combatant c;
        c.level = 60;
        c.isPlayer = true;
        c.classId = CLASS_WARRIOR;
        c.weaponSkill = 300;
        c.maxSkillForLevel = 300;
        return c;
    }

    Combatant Victim(int32 health = 1000)
    {
        Combatant c;
        c.level = 60;
        c.defenceSkill = 300;
        c.maxDefenceForLevel = 300;
        c.health = health;
        return c;
    }

    // Two different guids: damage a unit does to itself is not split onto
    // anyone, so a fixture that left both empty would be testing that rule by
    // accident rather than the one it names.
    Attempt Swing(int32 base)
    {
        Attempt a;
        a.attacker = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(1));
        a.victim = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(2));
        a.source = Source::MeleeMain;
        a.school = SPELL_SCHOOL_MASK_NORMAL;
        a.base = base;
        return a;
    }

    Attempt Bolt(int32 base)
    {
        Attempt a;
        a.attacker = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(1));
        a.victim = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(2));
        a.source = Source::Spell;
        a.spellId = 133;
        a.school = SPELL_SCHOOL_MASK_FIRE;
        a.base = base;
        return a;
    }

    ObjectGuid SomeGuid(uint32 counter)
    {
        return ObjectGuid(HIGHGUID_PLAYER, counter);
    }
}

TEST_CASE("A clean swing with nothing in the way deals its base")
{
    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), Defences(), false, Rolls());

    CHECK(out.landing == Landing::Hit);
    CHECK(out.dealt == 100);
    CHECK(out.absorbed == 0);
    CHECK(out.resisted == 0);
    CHECK(out.blocked == 0);
    CHECK(out.beforeMitigation == 100);
    CHECK_FALSE(out.victimDies);
}

TEST_CASE("An immune victim ends it before anything is rolled")
{
    Defences d;
    d.immune = true;

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.landing == Landing::Immune);
    CHECK(out.dealt == 0);
    CHECK(out.absorbs.empty());
    CHECK(out.splits.empty());
}

TEST_CASE("A miss deals nothing and costs no shield")
{
    Combatant a = Attacker();
    a.missChance = 10000;   // certain

    Defences d;
    Absorber shield;
    shield.caster = SomeGuid(1);
    shield.spellId = 17;
    shield.remaining = 500;
    d.absorbers.push_back(shield);

    const Outcome out = Resolve(Swing(100), a, Victim(), d, false, Rolls());

    CHECK(out.landing == Landing::Miss);
    CHECK(out.dealt == 0);
    CHECK(out.absorbed == 0);
    CHECK(out.absorbs.empty());
}

TEST_CASE("A critical swing lands twice its base")
{
    Combatant a = Attacker();
    a.critChance = 10000;

    const Outcome out = Resolve(Swing(100), a, Victim(), Defences(), false, Rolls());

    CHECK(out.landing == Landing::Crit);
    CHECK(out.dealt == 200);
}

TEST_CASE("A crushing blow lands half again")
{
    Combatant a = Attacker();
    a.isPlayer = false;
    a.canCrush = true;
    a.level = 63;
    a.maxSkillForLevel = 315;

    const Outcome out = Resolve(Swing(100), a, Victim(), Defences(), false, Rolls());

    CHECK(out.landing == Landing::Crush);
    CHECK(out.dealt == 150);
}

TEST_CASE("A block takes the shield's value off the blow")
{
    Combatant v = Victim();
    v.blockChance = 10000;

    Defences d;
    d.blockValue = 30;

    const Outcome out = Resolve(Swing(100), Attacker(), v, d, false, Rolls());

    CHECK(out.landing == Landing::Block);
    CHECK(out.blocked == 30);
    CHECK(out.dealt == 70);
}

TEST_CASE("A block never takes more than the blow carried")
{
    Combatant v = Victim();
    v.blockChance = 10000;

    Defences d;
    d.blockValue = 500;

    const Outcome out = Resolve(Swing(100), Attacker(), v, d, false, Rolls());

    CHECK(out.blocked == 100);
    CHECK(out.dealt >= 0);
}

TEST_CASE("Armour takes a share of a physical blow, never all of it")
{
    Defences d;
    d.armour = 4000;

    const Outcome out = Resolve(Swing(1000), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.landing == Landing::Hit);
    CHECK(out.dealt < 1000);
    CHECK(out.dealt > 0);
}

TEST_CASE("Armour is capped, so a mountain of it still lets damage through")
{
    Defences d;
    d.armour = 1000000;

    const Outcome out = Resolve(Swing(1000), Attacker(), Victim(), d, false, Rolls());

    // The cap is three quarters, so at least a quarter survives.
    CHECK(out.dealt >= 250);
}

TEST_CASE("Armour does not touch a magical blow")
{
    Defences d;
    d.armour = 100000;

    const Outcome out = Resolve(Bolt(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.landing == Landing::Hit);
    CHECK(out.dealt == 100);
}

TEST_CASE("Resistance takes a share of a magical blow")
{
    Defences d;
    d.resistance = 200;

    Rolls rolls;
    rolls.resist = 9999;   // the top band

    const Outcome out = Resolve(Bolt(1000), Attacker(), Victim(), d, false, rolls);

    CHECK(out.resisted > 0);
    CHECK(out.dealt == 1000 - out.resisted);
}

TEST_CASE("A blow resisted to nothing is reported as resisted, not as a hit for zero")
{
    Defences d;
    d.resistance = 100000;

    Rolls rolls;
    rolls.resist = 9999;

    const Outcome out = Resolve(Bolt(4), Attacker(), Victim(), d, false, rolls);

    CHECK(out.landing == Landing::Resist);
    CHECK(out.dealt == 0);
}

TEST_CASE("A shield is planned against, not spent")
{
    Defences d;
    Absorber shield;
    shield.caster = SomeGuid(7);
    shield.spellId = 17;
    shield.remaining = 30;
    d.absorbers.push_back(shield);

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.dealt == 70);
    CHECK(out.absorbed == 30);

    REQUIRE(out.absorbs.size() == 1);
    CHECK(out.absorbs[0].caster == SomeGuid(7));
    CHECK(out.absorbs[0].spellId == 17);
    CHECK(out.absorbs[0].amount == 30);
    CHECK(out.absorbs[0].exhausted);

    // The shield resolve was handed is untouched: spending it is apply()'s job.
    CHECK(d.absorbers[0].remaining == 30);
}

TEST_CASE("A shield bigger than the blow survives it")
{
    Defences d;
    Absorber shield;
    shield.caster = SomeGuid(7);
    shield.remaining = 500;
    d.absorbers.push_back(shield);

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.dealt == 0);
    CHECK(out.absorbed == 100);
    REQUIRE(out.absorbs.size() == 1);
    CHECK(out.absorbs[0].amount == 100);
    CHECK_FALSE(out.absorbs[0].exhausted);
}

TEST_CASE("Shields are spent in the order they are held, and only as far as needed")
{
    Defences d;

    Absorber first;
    first.caster = SomeGuid(1);
    first.remaining = 40;
    d.absorbers.push_back(first);

    Absorber second;
    second.caster = SomeGuid(2);
    second.remaining = 40;
    d.absorbers.push_back(second);

    Absorber third;
    third.caster = SomeGuid(3);
    third.remaining = 40;
    d.absorbers.push_back(third);

    const Outcome out = Resolve(Swing(70), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.absorbed == 70);
    CHECK(out.dealt == 0);

    REQUIRE(out.absorbs.size() == 2);
    CHECK(out.absorbs[0].caster == SomeGuid(1));
    CHECK(out.absorbs[0].amount == 40);
    CHECK(out.absorbs[0].exhausted);
    CHECK(out.absorbs[1].caster == SomeGuid(2));
    CHECK(out.absorbs[1].amount == 30);
    CHECK_FALSE(out.absorbs[1].exhausted);
}

TEST_CASE("A shield of the wrong school is passed over")
{
    Defences d;
    Absorber shield;
    shield.caster = SomeGuid(1);
    shield.remaining = 500;
    shield.schoolMask = SPELL_SCHOOL_MASK_FROST;
    d.absorbers.push_back(shield);

    const Outcome out = Resolve(Bolt(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.absorbed == 0);
    CHECK(out.dealt == 100);
    CHECK(out.absorbs.empty());
}

TEST_CASE("A split is planned, not dealt")
{
    // The defect this whole design exists to remove: the recipient must not be
    // damaged during the mitigation of the blow that split it.
    Defences d;
    Splitter splitter;
    splitter.target = SomeGuid(42);
    splitter.spellId = 6229;
    splitter.fraction = 0.3f;
    d.splitters.push_back(splitter);

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.dealt == 70);

    REQUIRE(out.splits.size() == 1);
    CHECK(out.splits[0].target == SomeGuid(42));
    CHECK(out.splits[0].spellId == 6229);
    CHECK(out.splits[0].amount == 30);
}

TEST_CASE("A flat split moves a fixed amount")
{
    Defences d;
    Splitter splitter;
    splitter.target = SomeGuid(42);
    splitter.flat = 25;
    d.splitters.push_back(splitter);

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.dealt == 75);
    REQUIRE(out.splits.size() == 1);
    CHECK(out.splits[0].amount == 25);
}

TEST_CASE("A split never moves more than is left")
{
    Defences d;
    Splitter splitter;
    splitter.target = SomeGuid(42);
    splitter.flat = 500;
    d.splitters.push_back(splitter);

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.dealt == 0);
    REQUIRE(out.splits.size() == 1);
    CHECK(out.splits[0].amount == 100);
}

TEST_CASE("Shields are spent before anything is split")
{
    Defences d;

    Absorber shield;
    shield.caster = SomeGuid(1);
    shield.remaining = 50;
    d.absorbers.push_back(shield);

    Splitter splitter;
    splitter.target = SomeGuid(42);
    splitter.fraction = 0.5f;
    d.splitters.push_back(splitter);

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    // 100 - 50 absorbed = 50 left, half of which is split.
    CHECK(out.absorbed == 50);
    REQUIRE(out.splits.size() == 1);
    CHECK(out.splits[0].amount == 25);
    CHECK(out.dealt == 25);
}

TEST_CASE("A killing blow says so, with the overshoot")
{
    const Outcome out = Resolve(Swing(150), Attacker(), Victim(100), Defences(), false, Rolls());

    CHECK(out.dealt == 150);
    CHECK(out.victimDies);
    CHECK(out.overkill == 50);
}

TEST_CASE("A blow that exactly empties the bar kills, with no overshoot")
{
    const Outcome out = Resolve(Swing(100), Attacker(), Victim(100), Defences(), false, Rolls());

    CHECK(out.victimDies);
    CHECK(out.overkill == 0);
}

TEST_CASE("A blow a shield swallows entirely does not kill")
{
    Defences d;
    Absorber shield;
    shield.caster = SomeGuid(1);
    shield.remaining = 1000;
    d.absorbers.push_back(shield);

    const Outcome out = Resolve(Swing(150), Attacker(), Victim(100), d, false, Rolls());

    CHECK(out.dealt == 0);
    CHECK_FALSE(out.victimDies);
    CHECK(out.overkill == 0);
}

TEST_CASE("A periodic tick neither misses nor crits")
{
    Attempt tick;
    tick.source = combat::Source::Periodic;
    tick.school = SPELL_SCHOOL_MASK_SHADOW;
    tick.base = 40;

    Combatant a = Attacker();
    a.missChance = 10000;   // would be certain for a swing
    a.critChance = 10000;

    const Outcome out = Resolve(tick, a, Victim(), Defences(), false, Rolls());

    CHECK(out.landing == Landing::Hit);
    CHECK(out.dealt == 40);
}

TEST_CASE("An evading victim refuses a periodic tick too")
{
    Attempt tick;
    tick.source = combat::Source::Periodic;
    tick.base = 40;

    Combatant v = Victim();
    v.isEvading = true;

    const Outcome out = Resolve(tick, Attacker(), v, Defences(), false, Rolls());

    CHECK(out.landing == Landing::Evade);
    CHECK(out.dealt == 0);
}

TEST_CASE("Resolving twice with the same inputs gives the same answer")
{
    Combatant a = Attacker();
    a.missChance = 500;
    a.critChance = 500;

    Defences d;
    d.armour = 2000;
    Absorber shield;
    shield.caster = SomeGuid(1);
    shield.remaining = 20;
    d.absorbers.push_back(shield);

    Rolls rolls;
    rolls.hit = 3333;
    rolls.resist = 777;
    rolls.glanceBand = 0.5f;

    const Outcome first = Resolve(Swing(100), a, Victim(), d, false, rolls);
    const Outcome second = Resolve(Swing(100), a, Victim(), d, false, rolls);

    CHECK(first.landing == second.landing);
    CHECK(first.dealt == second.dealt);
    CHECK(first.absorbed == second.absorbed);
    CHECK(first.absorbs.size() == second.absorbs.size());
}
