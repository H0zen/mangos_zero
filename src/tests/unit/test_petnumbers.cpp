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

// The numbers a pet fights with.
//
// A pet derives from its stats the way a player does, but only from what it has
// GAINED over what it was created with -- the created amount is already in the
// base, and adding it twice is the mistake these are here to catch.

#include "doctest.h"

#include "Stats/PetNumbers.h"

TEST_CASE("pet armour: two points for every point of agility")
{
    Modifiers mods;
    mods.baseValue = 100.0f;

    CHECK(stats::PetArmour(mods, 0.0f) == doctest::Approx(100.0f));
    CHECK(stats::PetArmour(mods, 50.0f) == doctest::Approx(200.0f));
}

TEST_CASE("pet armour: agility lands after the base percentage, not before")
{
    Modifiers mods;
    mods.baseValue = 100.0f;
    mods.basePct = 2.0f;

    // The base doubles; the agility does not.
    CHECK(stats::PetArmour(mods, 50.0f) == doctest::Approx(300.0f));
}

TEST_CASE("pet health: ten for every point of stamina gained since it was made")
{
    Modifiers mods;

    // Nothing gained, so nothing but what it was created with.
    CHECK(stats::PetMaxHealth(mods, 500.0f, 0.0f) == doctest::Approx(500.0f));

    CHECK(stats::PetMaxHealth(mods, 500.0f, 12.0f) == doctest::Approx(620.0f));

    // Losing stamina takes health away again.
    CHECK(stats::PetMaxHealth(mods, 500.0f, -12.0f) == doctest::Approx(380.0f));
}

TEST_CASE("pet health: what it was created with scales, what it gained does not")
{
    Modifiers mods;
    mods.basePct = 2.0f;

    // A thousand from the doubled base, and the gained stamina on top untouched.
    CHECK(stats::PetMaxHealth(mods, 500.0f, 10.0f) == doctest::Approx(1100.0f));
}

TEST_CASE("pet power: fifteen mana for every point of intellect gained")
{
    Modifiers mods;

    CHECK(stats::PetMaxPower(mods, 300.0f, 0.0f) == doctest::Approx(300.0f));
    CHECK(stats::PetMaxPower(mods, 300.0f, 20.0f) == doctest::Approx(600.0f));

    // Anything that is not mana grows from no stat at all, which the caller says
    // by passing nothing gained.
    CHECK(stats::PetMaxPower(mods, 100.0f, 0.0f) == doctest::Approx(100.0f));
}

TEST_CASE("pet attack power: twice strength, less the twenty it starts with")
{
    CHECK(stats::PetAttackPowerFromStrength(10.0f, false) == doctest::Approx(0.0f));
    CHECK(stats::PetAttackPowerFromStrength(100.0f, false) == doctest::Approx(180.0f));

    // An imp gets it one for one, and is the only pet that does.
    CHECK(stats::PetAttackPowerFromStrength(10.0f, true) == doctest::Approx(0.0f));
    CHECK(stats::PetAttackPowerFromStrength(100.0f, true) == doctest::Approx(90.0f));
}

TEST_CASE("pet swing: the whole attack power counts, not a part of it")
{
    Modifiers mods;

    // Fourteen points over a two second swing is two damage, and unlike a
    // creature nothing is taken off for what a template already had.
    stats::Swing const swing = stats::PetSwing(mods, 10.0f, 20.0f, 14.0f, 2.0f);

    CHECK(swing.least == doctest::Approx(12.0f));
    CHECK(swing.most == doctest::Approx(22.0f));
}

TEST_CASE("pet swing: a slower swing turns the same power into more damage")
{
    Modifiers mods;

    stats::Swing const quick = stats::PetSwing(mods, 0.0f, 0.0f, 140.0f, 1.0f);
    stats::Swing const slow = stats::PetSwing(mods, 0.0f, 0.0f, 140.0f, 2.5f);

    CHECK(quick.most == doctest::Approx(10.0f));
    CHECK(slow.most == doctest::Approx(25.0f));
}

TEST_CASE("pet swing: a total percentage of nothing leaves no damage at all")
{
    Modifiers mods;
    mods.baseValue = 400.0f;
    mods.totalValue = 400.0f;
    mods.totalPct = 0.0f;

    stats::Swing const swing = stats::PetSwing(mods, 50.0f, 90.0f, 700.0f, 2.0f);

    CHECK(swing.least == doctest::Approx(0.0f));
    CHECK(swing.most == doctest::Approx(0.0f));
}

TEST_CASE("pet mood: a happy one hits a quarter harder and a sad one a quarter softer")
{
    CHECK(stats::HappinessScale(stats::PET_HAPPY) == doctest::Approx(1.25f));
    CHECK(stats::HappinessScale(stats::PET_CONTENT) == doctest::Approx(1.0f));
    CHECK(stats::HappinessScale(stats::PET_UNHAPPY) == doctest::Approx(0.75f));

    // A mood nobody defined leaves the damage as it was, rather than zeroing it.
    CHECK(stats::HappinessScale(99) == doctest::Approx(1.0f));
}
