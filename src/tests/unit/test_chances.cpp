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

// Blocking, parrying, dodging, critting, and getting mana back.
//
// The first four were four copies of the same three lines, which is how a clamp
// goes missing from one of them without anyone noticing. They are one function
// now, and this is where the shape of it is fixed.

#include "doctest.h"

#include "Stats/Chances.h"

TEST_CASE("chance: at the skill its level allows, a guard is what it starts as")
{
    CHECK(stats::Chance(stats::GUARD_FROM_NOTHING, 300, 300, 0.0f) == doctest::Approx(5.0f));
}

TEST_CASE("chance: every point of skill above or below is worth four hundredths")
{
    // Twenty five points of weapon skill over a same-level target: one per cent.
    CHECK(stats::Chance(5.0f, 325, 300, 0.0f) == doctest::Approx(6.0f));

    // And under it, the same the other way.
    CHECK(stats::Chance(5.0f, 275, 300, 0.0f) == doctest::Approx(4.0f));
}

TEST_CASE("chance: what auras add lands whole, after the skill correction")
{
    CHECK(stats::Chance(5.0f, 300, 300, 10.0f) == doctest::Approx(15.0f));

    // And an aura may take away as well as give.
    CHECK(stats::Chance(5.0f, 300, 300, -3.0f) == doctest::Approx(2.0f));
}

TEST_CASE("chance: it never goes below nothing, whatever is taken off it")
{
    // Auras alone.
    CHECK(stats::Chance(5.0f, 300, 300, -50.0f) == doctest::Approx(0.0f));

    // Skill alone: a hundred points under is four per cent, more than the five
    // it started with would cover if anything else were against it.
    CHECK(stats::Chance(5.0f, 0, 300, 0.0f) == doctest::Approx(0.0f));

    // And the two together.
    CHECK(stats::Chance(5.0f, 100, 300, -10.0f) == doctest::Approx(0.0f));
}

TEST_CASE("chance: a dodge starts from agility rather than from a flat five")
{
    // The starting point is the only thing that differs between the guards.
    CHECK(stats::Chance(18.5f, 300, 300, 0.0f) == doctest::Approx(18.5f));
    CHECK(stats::Chance(18.5f, 315, 300, 2.0f) == doctest::Approx(21.1f));
}

TEST_CASE("mana regen: what spirit gives stops while casting, what an aura gives does not")
{
    // Ten a second from spirit, no share of it surviving a cast, and nothing flat.
    stats::ManaRegen const dry = stats::Regeneration(10.0f, 1.0f, 0.0f, 0);
    CHECK(dry.standing == doctest::Approx(10.0f));
    CHECK(dry.casting == doctest::Approx(0.0f));

    // Fifty per five seconds is ten a second, and it keeps coming while casting.
    stats::ManaRegen const flat = stats::Regeneration(0.0f, 1.0f, 50.0f, 0);
    CHECK(flat.standing == doctest::Approx(10.0f));
    CHECK(flat.casting == doctest::Approx(10.0f));
}

TEST_CASE("mana regen: how much of the spirit half survives a cast is a share of it")
{
    stats::ManaRegen const half = stats::Regeneration(10.0f, 1.0f, 0.0f, 50);

    CHECK(half.standing == doctest::Approx(10.0f));
    CHECK(half.casting == doctest::Approx(5.0f));
}

TEST_CASE("mana regen: nothing can buy back more of the spirit half than all of it")
{
    stats::ManaRegen const greedy = stats::Regeneration(10.0f, 1.0f, 0.0f, 300);

    CHECK(greedy.casting == doctest::Approx(10.0f));
    CHECK(greedy.casting == doctest::Approx(greedy.standing));
}

TEST_CASE("mana regen: a percentage aura scales the spirit half alone")
{
    // Doubled spirit, and a flat ten a second that is not doubled.
    stats::ManaRegen const buffed = stats::Regeneration(10.0f, 2.0f, 50.0f, 100);

    CHECK(buffed.standing == doctest::Approx(30.0f));
    CHECK(buffed.casting == doctest::Approx(30.0f));
}
