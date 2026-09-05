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

// The numbers a creature fights with.
//
// These were arithmetic inside methods that also read a unit and wrote its
// fields, so nothing could be asked of them without a world to ask it in. They
// are asked here instead.

#include "doctest.h"

#include "Stats/CreatureNumbers.h"

TEST_CASE("modifiers: two are added and two are multiplied, base first")
{
    Modifiers mods;
    mods.baseValue = 100.0f;
    mods.basePct = 1.5f;                                    // 150
    mods.totalValue = 40.0f;                                // 190
    mods.totalPct = 2.0f;                                   // 380

    CHECK(mods.Folded() == doctest::Approx(380.0f));
    CHECK(mods.Base() == doctest::Approx(150.0f));
}

TEST_CASE("modifiers: a total percentage of nothing takes the whole number")
{
    Modifiers mods;
    mods.baseValue = 5000.0f;
    mods.basePct = 3.0f;
    mods.totalValue = 9000.0f;
    mods.totalPct = 0.0f;

    // However much is piled on underneath.
    CHECK(mods.Folded() == doctest::Approx(0.0f));
    CHECK(mods.TotalPct() == doctest::Approx(0.0f));

    // And a negative one is read the same way, not as a sign flip.
    mods.totalPct = -2.0f;
    CHECK(mods.Folded() == doctest::Approx(0.0f));
    CHECK(mods.TotalPct() == doctest::Approx(0.0f));
}

TEST_CASE("modifiers: the share is the percentage read as more or less than the whole")
{
    Modifiers mods;

    mods.totalPct = 1.0f;
    CHECK(mods.TotalShare() == doctest::Approx(0.0f));      // exactly the whole

    mods.totalPct = 1.25f;
    CHECK(mods.TotalShare() == doctest::Approx(0.25f));     // a quarter more

    mods.totalPct = 0.6f;
    CHECK(mods.TotalShare() == doctest::Approx(-0.4f));     // two fifths less

    // Nothing at all reads as the whole taken away.
    mods.totalPct = 0.0f;
    CHECK(mods.TotalShare() == doctest::Approx(-1.0f));
}

TEST_CASE("creature attack power: a base, a flat addition and a share")
{
    Modifiers mods;
    mods.baseValue = 120.0f;
    mods.basePct = 0.5f;
    mods.totalValue = 30.0f;
    mods.totalPct = 1.1f;

    stats::AttackPower const power = stats::CreatureAttackPower(mods);

    CHECK(power.base == 60);                                // the base, folded
    CHECK(power.added == 30);                               // added whole, not scaled
    CHECK(power.share == doctest::Approx(0.1f));
}

TEST_CASE("creature swing: what the weapon does, with nothing on top")
{
    Modifiers mods;                                         // every one neutral

    // No attack power above the template's, so the weapon speaks for itself.
    stats::Swing const swing = stats::CreatureSwing(mods, 10.0f, 20.0f, 0.0f, 2.0f, 1.0f);

    CHECK(swing.least == doctest::Approx(10.0f));
    CHECK(swing.most == doctest::Approx(20.0f));
}

TEST_CASE("creature swing: only the attack power above the template's counts")
{
    Modifiers mods;

    // Fourteen points of attack power over a two second swing is two damage:
    // a point is worth a fourteenth of a second's worth.
    stats::Swing const gained = stats::CreatureSwing(mods, 10.0f, 20.0f, 14.0f, 2.0f, 1.0f);
    CHECK(gained.least == doctest::Approx(12.0f));
    CHECK(gained.most == doctest::Approx(22.0f));

    // And losing it below the template's takes damage away again.
    stats::Swing const lost = stats::CreatureSwing(mods, 10.0f, 20.0f, -14.0f, 2.0f, 1.0f);
    CHECK(lost.least == doctest::Approx(8.0f));
    CHECK(lost.most == doctest::Approx(18.0f));
}

TEST_CASE("creature swing: a slower weapon turns the same power into more damage")
{
    Modifiers mods;

    stats::Swing const quick = stats::CreatureSwing(mods, 0.0f, 0.0f, 140.0f, 1.0f, 1.0f);
    stats::Swing const slow = stats::CreatureSwing(mods, 0.0f, 0.0f, 140.0f, 3.5f, 1.0f);

    CHECK(quick.most == doctest::Approx(10.0f));
    CHECK(slow.most == doctest::Approx(35.0f));
}

TEST_CASE("creature swing: the template's multiplier scales the weapon, not what is added on")
{
    Modifiers mods;
    mods.totalValue = 100.0f;                               // added after the multiplier

    stats::Swing const swing = stats::CreatureSwing(mods, 10.0f, 10.0f, 0.0f, 2.0f, 3.0f);

    // Thirty from the weapon tripled, then a hundred that is not.
    CHECK(swing.least == doctest::Approx(130.0f));
}

TEST_CASE("creature swing: a total percentage of nothing leaves no damage at all")
{
    Modifiers mods;
    mods.baseValue = 500.0f;
    mods.totalValue = 500.0f;
    mods.totalPct = 0.0f;

    stats::Swing const swing = stats::CreatureSwing(mods, 100.0f, 200.0f, 1000.0f, 2.0f, 5.0f);

    CHECK(swing.least == doctest::Approx(0.0f));
    CHECK(swing.most == doctest::Approx(0.0f));
}

TEST_CASE("creature numbers: armour, health and power are the fold and no more")
{
    Modifiers mods;
    mods.baseValue = 200.0f;
    mods.basePct = 1.5f;
    mods.totalValue = 100.0f;
    mods.totalPct = 1.2f;

    CHECK(stats::Simple(mods) == doctest::Approx(480.0f));
}
