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

// The numbers a player fights with.
//
// The attack power rules are a table of eleven cases, four of them shapes a
// druid can be in, and a table cannot be checked by reading it. Every row is
// exercised here, with the level and the stats chosen so a wrong coefficient
// cannot come out to the right answer by accident.

#include "doctest.h"

#include "Stats/PlayerNumbers.h"

namespace
{
    /// Level 60, forty of each stat, no shape and no talent.
    stats::Physique Grown(uint8 klass)
    {
        stats::Physique who;
        who.klass = klass;
        who.level = 60.0f;
        who.strength = 40.0f;
        who.agility = 40.0f;
        return who;
    }
}

TEST_CASE("player health: the first twenty stamina are worth one each")
{
    CHECK(stats::HealthFromStamina(0.0f) == doctest::Approx(0.0f));
    CHECK(stats::HealthFromStamina(20.0f) == doctest::Approx(20.0f));

    // And the twenty-first is worth ten.
    CHECK(stats::HealthFromStamina(21.0f) == doctest::Approx(30.0f));
    CHECK(stats::HealthFromStamina(100.0f) == doctest::Approx(820.0f));
}

TEST_CASE("player mana: the same band, at fifteen a point above it")
{
    CHECK(stats::ManaFromIntellect(20.0f) == doctest::Approx(20.0f));
    CHECK(stats::ManaFromIntellect(21.0f) == doctest::Approx(35.0f));
    CHECK(stats::ManaFromIntellect(100.0f) == doctest::Approx(1220.0f));
}

TEST_CASE("player armour: two per agility, and what a stat aura adds lands flat")
{
    Modifiers mods;
    mods.baseValue = 1000.0f;

    CHECK(stats::PlayerArmour(mods, 100.0f, 0.0f) == doctest::Approx(1200.0f));
    CHECK(stats::PlayerArmour(mods, 100.0f, 300.0f) == doctest::Approx(1500.0f));

    // Both land after the base percentage: only the base itself doubles.
    mods.basePct = 2.0f;
    CHECK(stats::PlayerArmour(mods, 100.0f, 300.0f) == doctest::Approx(2500.0f));
}

TEST_CASE("melee attack power: the plate classes get three a level and double strength")
{
    // 60*3 + 40*2 - 20
    CHECK(stats::MeleeAttackPower(Grown(CLASS_WARRIOR)) == doctest::Approx(240.0f));
    CHECK(stats::MeleeAttackPower(Grown(CLASS_PALADIN)) == doctest::Approx(240.0f));
}

TEST_CASE("melee attack power: a shaman gets double strength but only two a level")
{
    // 60*2 + 40*2 - 20
    CHECK(stats::MeleeAttackPower(Grown(CLASS_SHAMAN)) == doctest::Approx(180.0f));
}

TEST_CASE("melee attack power: rogues and hunters are paid for both stats, once each")
{
    // 60*2 + 40 + 40 - 20
    CHECK(stats::MeleeAttackPower(Grown(CLASS_ROGUE)) == doctest::Approx(180.0f));
    CHECK(stats::MeleeAttackPower(Grown(CLASS_HUNTER)) == doctest::Approx(180.0f));

    // The same as a shaman gets, but earned from two stats instead of one
    // doubled -- so a point of agility is worth what a point of strength is.
    CHECK(stats::MeleeAttackPower(Grown(CLASS_ROGUE)) ==
          doctest::Approx(stats::MeleeAttackPower(Grown(CLASS_SHAMAN))));
}

TEST_CASE("melee attack power: the cloth classes get nothing for their level")
{
    // 40 - 10, and the sixty levels count for nothing at all
    CHECK(stats::MeleeAttackPower(Grown(CLASS_MAGE)) == doctest::Approx(30.0f));
    CHECK(stats::MeleeAttackPower(Grown(CLASS_PRIEST)) == doctest::Approx(30.0f));
    CHECK(stats::MeleeAttackPower(Grown(CLASS_WARLOCK)) == doctest::Approx(30.0f));
}

TEST_CASE("melee attack power: a druid out of shape is paid for strength alone")
{
    // 40*2 - 20, with the level counting for nothing
    CHECK(stats::MeleeAttackPower(Grown(CLASS_DRUID)) == doctest::Approx(60.0f));
}

TEST_CASE("melee attack power: a druid in shape is paid per level only by the talent")
{
    stats::Physique bear = Grown(CLASS_DRUID);
    bear.form = FORM_BEAR;

    // Without Predatory Strikes the shape is worth nothing per level.
    CHECK(stats::MeleeAttackPower(bear) == doctest::Approx(60.0f));

    // With it at 150%, sixty levels are worth ninety.
    bear.predatoryStrikes = 1.5f;
    CHECK(stats::MeleeAttackPower(bear) == doctest::Approx(150.0f));

    // A cat is the same, plus agility.
    stats::Physique cat = bear;
    cat.form = FORM_CAT;
    CHECK(stats::MeleeAttackPower(cat) == doctest::Approx(190.0f));

    // A dire bear is a bear.
    stats::Physique dire = bear;
    dire.form = FORM_DIREBEAR;
    CHECK(stats::MeleeAttackPower(dire) == doctest::Approx(150.0f));
}

TEST_CASE("melee attack power: a moonkin gets a level and a half on top of the talent")
{
    stats::Physique moonkin = Grown(CLASS_DRUID);
    moonkin.form = FORM_MOONKIN;

    // Even with no talent: 60*1.5 + 40*2 - 20
    CHECK(stats::MeleeAttackPower(moonkin) == doctest::Approx(150.0f));

    // And the talent adds to the rate rather than replacing it.
    moonkin.predatoryStrikes = 1.5f;
    CHECK(stats::MeleeAttackPower(moonkin) == doctest::Approx(240.0f));
}

TEST_CASE("ranged attack power: a hunter is paid twice over for level and agility")
{
    // 60*2 + 40*2 - 10
    CHECK(stats::RangedAttackPower(Grown(CLASS_HUNTER)) == doctest::Approx(190.0f));
}

TEST_CASE("ranged attack power: rogues and warriors get level and agility once each")
{
    // 60 + 40 - 10
    CHECK(stats::RangedAttackPower(Grown(CLASS_ROGUE)) == doctest::Approx(90.0f));
    CHECK(stats::RangedAttackPower(Grown(CLASS_WARRIOR)) == doctest::Approx(90.0f));
}

TEST_CASE("ranged attack power: everyone else gets agility and no level at all")
{
    CHECK(stats::RangedAttackPower(Grown(CLASS_MAGE)) == doctest::Approx(30.0f));
    CHECK(stats::RangedAttackPower(Grown(CLASS_PALADIN)) == doctest::Approx(30.0f));
    CHECK(stats::RangedAttackPower(Grown(CLASS_SHAMAN)) == doctest::Approx(30.0f));
}

TEST_CASE("ranged attack power: a feral druid has none at all, not merely less")
{
    stats::Physique druid = Grown(CLASS_DRUID);

    // Out of shape it is paid like anyone else.
    CHECK(stats::RangedAttackPower(druid) == doctest::Approx(30.0f));

    // In one, there is nothing in its paws.
    uint32 const feral[] = { FORM_CAT, FORM_BEAR, FORM_DIREBEAR };
    for (uint32 form : feral)
    {
        druid.form = form;
        CHECK(stats::RangedAttackPower(druid) == doctest::Approx(0.0f));
    }

    // A moonkin is not feral, and keeps its agility.
    druid.form = FORM_MOONKIN;
    CHECK(stats::RangedAttackPower(druid) == doctest::Approx(30.0f));
}
