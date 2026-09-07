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

TEST_CASE("creature shield block: half its level and a twentieth of its strength")
{
    CHECK(stats::CreatureShieldBlock(60, 200.0f) == 40u);   // 30 + 10
    CHECK(stats::CreatureShieldBlock(1, 0.0f) == 0u);
}

TEST_CASE("creature shield block: both halves are counted down, not rounded")
{
    // level 5 gives two, not two and a half; strength 39 gives one, not two
    CHECK(stats::CreatureShieldBlock(5, 39.0f) == 3u);
}

// The level a creature comes out at, and what it is made of once it has one.
//
// The rank rates are handed in rather than read from a configuration, so what a
// rare elite is worth can be asked with no world to ask it in.

TEST_CASE("level: a forced level wins over the band")
{
    CHECK(stats::CreatureLevel(10, 20, 17, 12) == 17);
    CHECK(stats::CreatureLevel(10, 10, 60, 10) == 60);
}

TEST_CASE("level: a band of one level needs no roll")
{
    CHECK(stats::CreatureLevel(35, 35, 0, 999) == 35);
}

TEST_CASE("level: a band takes the roll it was given")
{
    CHECK(stats::CreatureLevel(10, 20, 0, 14) == 14);
}

TEST_CASE("vitals: the table is scaled by the template's own multipliers")
{
    const stats::Vitals made = stats::VitalsFromTable(1000, 500, 1.5f, 2.0f);

    CHECK(made.health == 1500);
    CHECK(made.mana == 1000);
}

TEST_CASE("vitals: the band is read at the level the creature came out at")
{
    // A band from level 10 to 20, health 100 to 200: level 15 sits halfway.
    const stats::Vitals middle = stats::VitalsFromBand(200, 100, 60, 20, 15, 10, 20);
    CHECK(middle.health == 150);
    CHECK(middle.mana == 40);

    // The ends are the ends, whichever way round the row states them.
    const stats::Vitals bottom = stats::VitalsFromBand(100, 200, 20, 60, 10, 10, 20);
    CHECK(bottom.health == 100);
    CHECK(bottom.mana == 20);

    const stats::Vitals top = stats::VitalsFromBand(100, 200, 20, 60, 20, 10, 20);
    CHECK(top.health == 200);
    CHECK(top.mana == 60);
}

TEST_CASE("vitals: a band of one level sits at its start")
{
    const stats::Vitals made = stats::VitalsFromBand(300, 100, 90, 10, 40, 40, 40);

    CHECK(made.health == 100);
    CHECK(made.mana == 10);
}

TEST_CASE("health: a rate that would leave nothing still leaves one")
{
    CHECK(stats::ScaledHealth(1000, 1.0f) == 1000);
    CHECK(stats::ScaledHealth(1000, 2.5f) == 2500);

    // A server that turns creatures right down does not get things alive with no health.
    CHECK(stats::ScaledHealth(1000, 0.0f) == 1);
    CHECK(stats::ScaledHealth(1, 0.4f) == 1);
}

TEST_CASE("rank rates: a rank nobody set multiplies nothing")
{
    stats::RankRates rates;

    CHECK(rates.health == doctest::Approx(1.0f));
    CHECK(rates.damage == doctest::Approx(1.0f));
    CHECK(rates.spellDamage == doctest::Approx(1.0f));
}

// How far off a creature notices somebody.

TEST_CASE("notice: twenty yards against an equal")
{
    CHECK(stats::NoticeRange(30, 30, 0.0f, 60, 1.0f) == doctest::Approx(20.0f));
}

TEST_CASE("notice: a yard for every level of difference, both ways")
{
    // A viewer five levels below is noticed five yards further off.
    CHECK(stats::NoticeRange(30, 25, 0.0f, 60, 1.0f) == doctest::Approx(25.0f));

    // Five levels above, five yards closer.
    CHECK(stats::NoticeRange(30, 35, 0.0f, 60, 1.0f) == doctest::Approx(15.0f));
}

TEST_CASE("notice: the gap stops counting at twenty-five levels")
{
    // A level 30 and a level 5 are noticed at the same distance by a level 60.
    CHECK(stats::NoticeRange(60, 30, 0.0f, 60, 1.0f)
          == doctest::Approx(stats::NoticeRange(60, 5, 0.0f, 60, 1.0f)));
}

TEST_CASE("notice: never closer than five yards")
{
    // Twenty levels above would give nothing at all.
    CHECK(stats::NoticeRange(10, 40, 0.0f, 60, 1.0f) == doctest::Approx(5.0f));
}

TEST_CASE("notice: detection only counts five levels under the cap")
{
    // A level 55 against a cap of 60: the auras are read.
    CHECK(stats::NoticeRange(55, 55, 10.0f, 60, 1.0f) == doctest::Approx(30.0f));

    // A level 56 is too close to the cap; the same auras add nothing.
    CHECK(stats::NoticeRange(56, 56, 10.0f, 60, 1.0f) == doctest::Approx(20.0f));
}

TEST_CASE("notice: the rate multiplies the whole answer, and nothing means nobody")
{
    CHECK(stats::NoticeRange(30, 30, 0.0f, 60, 2.0f) == doctest::Approx(40.0f));
    CHECK(stats::NoticeRange(30, 30, 0.0f, 60, 0.5f) == doctest::Approx(10.0f));

    // A server that turns noticing off is not a server with a five yard floor.
    CHECK(stats::NoticeRange(30, 30, 100.0f, 60, 0.0f) == doctest::Approx(0.0f));
}

// What a fall costs.

TEST_CASE("fall: a short drop is free")
{
    // The share reaches nothing at 0.2426 / 0.018, which is 13.48 yards.
    CHECK(stats::FallShare(10.0f, 0.0f) == doctest::Approx(0.0f));
    CHECK(stats::FallShare(13.0f, 0.0f) == doctest::Approx(0.0f));
    CHECK(stats::FallShare(13.4f, 0.0f) == doctest::Approx(0.0f));
    CHECK(stats::FallShare(14.0f, 0.0f) > 0.0f);
}

TEST_CASE("fall: the share rises with the drop")
{
    const float shorter = stats::FallShare(20.0f, 0.0f);
    const float longer = stats::FallShare(40.0f, 0.0f);

    CHECK(longer > shorter);

    // 0.018 * 40 - 0.2426
    CHECK(longer == doctest::Approx(0.4774f));
}

TEST_CASE("fall: what is forgiven is taken off the drop first")
{
    // Twenty yards forgiven turns a forty yard fall into a twenty yard one.
    CHECK(stats::FallShare(40.0f, 20.0f) == doctest::Approx(stats::FallShare(20.0f, 0.0f)));

    // Enough forgiveness makes any fall free.
    CHECK(stats::FallShare(40.0f, 40.0f) == doctest::Approx(0.0f));
}

TEST_CASE("fall: the damage is a share of everything he has, at the server's rate")
{
    // A forty yard fall takes about 48 percent of a thousand.
    CHECK(stats::FallDamage(40.0f, 0.0f, 1000, 1.0f) == 477);
    CHECK(stats::FallDamage(40.0f, 0.0f, 1000, 2.0f) == 954);
    CHECK(stats::FallDamage(40.0f, 0.0f, 1000, 0.0f) == 0);

    // And a free fall costs nothing however much health he has.
    CHECK(stats::FallDamage(10.0f, 0.0f, 100000, 100.0f) == 0);
}
