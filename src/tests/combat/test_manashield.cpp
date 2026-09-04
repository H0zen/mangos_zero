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

// Shields that charge mana for their work.
//
// A shield with a multiplier stops damage only while the victim can pay for it,
// and the payment is planned here rather than taken -- like everything else the
// Outcome carries back for apply() to carry out.

#include "doctest.h"

#include "Combat/Resolve.h"
#include "SharedDefines.h"

using combat::Absorber;
using combat::Blow;
using combat::Combatant;
using combat::Defences;
using combat::Result;
using combat::Outcome;
using combat::Resolve;
using combat::Rolls;
using combat::Delivery;

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
        c.guid = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(1));
        return c;
    }

    Combatant Victim(int32 health = 1000)
    {
        Combatant c;
        c.level = 60;
        c.defenceSkill = 300;
        c.maxDefenceForLevel = 300;
        c.health = health;
        c.guid = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(2));
        return c;
    }

    Blow Swing(int32 base)
    {
        Blow a;
        a.attacker = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(1));
        a.victim = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(2));
        a.delivery = Delivery::MeleeMain;
        a.school = combat::School::Physical;
        a.amount = base;
        return a;
    }

    Absorber ManaShield(int32 remaining, float multiplier)
    {
        Absorber shield;
        shield.caster = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(2));
        shield.spellId = 1463;
        shield.remaining = remaining;
        shield.manaMultiplier = multiplier;
        return shield;
    }
}

TEST_CASE("A mana shield spends mana for what it stops")
{
    Defences d;
    d.mana = 1000;
    d.absorbers.push_back(ManaShield(100, 2.f));

    const Outcome out = Resolve(Swing(50), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.absorbed == 50);
    CHECK(out.dealt == 0);
    CHECK(out.manaSpent == 100);   // fifty points at two mana each

    REQUIRE(out.absorbs.size() == 1);
    CHECK(out.absorbs[0].amount == 50);
    CHECK(out.absorbs[0].manaSpent == 100);
    CHECK_FALSE(out.absorbs[0].exhausted);
}

TEST_CASE("A mana shield stops only what the mana can pay for")
{
    Defences d;
    d.mana = 40;                                  // enough for twenty points
    d.absorbers.push_back(ManaShield(500, 2.f));

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.absorbed == 20);
    CHECK(out.manaSpent == 40);
    CHECK(out.dealt == 80);
}

TEST_CASE("A mana shield with no mana behind it stops nothing")
{
    Defences d;
    d.mana = 0;
    d.absorbers.push_back(ManaShield(500, 2.f));

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.absorbed == 0);
    CHECK(out.manaSpent == 0);
    CHECK(out.dealt == 100);
    CHECK(out.absorbs.empty());
}

TEST_CASE("Running out of mana does not break the shield")
{
    // The distinction the flag has to keep: a shield the blow used up comes off,
    // a shield the mage merely cannot afford stays on for when mana returns.
    Defences d;
    d.mana = 40;
    d.absorbers.push_back(ManaShield(500, 2.f));

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    REQUIRE(out.absorbs.size() == 1);
    CHECK_FALSE(out.absorbs[0].exhausted);
}

TEST_CASE("A mana shield used up does come off")
{
    Defences d;
    d.mana = 10000;
    d.absorbers.push_back(ManaShield(30, 2.f));

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    REQUIRE(out.absorbs.size() == 1);
    CHECK(out.absorbs[0].amount == 30);
    CHECK(out.absorbs[0].exhausted);
    CHECK(out.dealt == 70);
}

TEST_CASE("Two mana shields draw from the same pool")
{
    // The budget carries down the list: the second shield works with what the
    // first left, not with the full bar.
    Defences d;
    d.mana = 60;
    d.absorbers.push_back(ManaShield(1000, 2.f));
    d.absorbers.push_back(ManaShield(1000, 2.f));

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.manaSpent == 60);
    CHECK(out.absorbed == 30);
    CHECK(out.dealt == 70);
}

TEST_CASE("A free shield is spent before a mana shield is charged for")
{
    // The order the list is built in, and it matters: a mage should not pay mana
    // for damage a free ward would have taken.
    Defences d;
    d.mana = 1000;

    Absorber ward;
    ward.caster = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(2));
    ward.spellId = 17;
    ward.remaining = 60;
    d.absorbers.push_back(ward);

    d.absorbers.push_back(ManaShield(1000, 2.f));

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    CHECK(out.absorbed == 100);
    CHECK(out.dealt == 0);

    REQUIRE(out.absorbs.size() == 2);
    CHECK(out.absorbs[0].spellId == 17);
    CHECK(out.absorbs[0].amount == 60);
    CHECK(out.absorbs[0].manaSpent == 0);

    CHECK(out.absorbs[1].amount == 40);
    CHECK(out.absorbs[1].manaSpent == 80);
}

TEST_CASE("Damage you do to yourself is not split onto anyone")
{
    Defences d;
    combat::Splitter splitter;
    splitter.target = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(9));
    splitter.fraction = 0.5f;
    d.splitters.push_back(splitter);

    Blow self = Swing(100);
    self.victim = self.attacker;

    Combatant me = Attacker();
    Combatant meAsVictim = Victim();
    meAsVictim.guid = me.guid;

    const Outcome out = Resolve(self, me, meAsVictim, d, false, Rolls());

    CHECK(out.splits.empty());
    CHECK(out.dealt == 100);
}

TEST_CASE("Damage from somebody else still splits")
{
    Defences d;
    combat::Splitter splitter;
    splitter.target = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(9));
    splitter.fraction = 0.5f;
    d.splitters.push_back(splitter);

    const Outcome out = Resolve(Swing(100), Attacker(), Victim(), d, false, Rolls());

    REQUIRE(out.splits.size() == 1);
    CHECK(out.splits[0].amount == 50);
    CHECK(out.dealt == 50);
}
