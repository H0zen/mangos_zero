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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

// The four running numbers auras keep on each of a unit's stat groups.

#include "doctest.h"

#include "Stats/Tallies.h"

TEST_CASE("tallies: a fresh board is flat and whole")
{
    Tallies board;

    CHECK(board.Value(UNIT_MOD_ARMOR, BASE_VALUE) == doctest::Approx(0.0f));
    CHECK(board.Value(UNIT_MOD_ARMOR, BASE_PCT) == doctest::Approx(1.0f));
    CHECK(board.Value(UNIT_MOD_ARMOR, TOTAL_VALUE) == doctest::Approx(0.0f));
    CHECK(board.Value(UNIT_MOD_ARMOR, TOTAL_PCT) == doctest::Approx(1.0f));
    CHECK(board.Folded(UNIT_MOD_ARMOR) == doctest::Approx(0.0f));
    CHECK_FALSE(board.Ready());
}

TEST_CASE("tallies: a flat share is added and taken back")
{
    Tallies board;

    board.Put(UNIT_MOD_ARMOR, TOTAL_VALUE, 250.0f, true);
    CHECK(board.Value(UNIT_MOD_ARMOR, TOTAL_VALUE) == doctest::Approx(250.0f));

    board.Put(UNIT_MOD_ARMOR, TOTAL_VALUE, 250.0f, false);
    CHECK(board.Value(UNIT_MOD_ARMOR, TOTAL_VALUE) == doctest::Approx(0.0f));
}

TEST_CASE("tallies: a percentage share multiplies in and divides back out")
{
    Tallies board;

    board.Put(UNIT_MOD_ARMOR, TOTAL_PCT, 20.0f, true);
    CHECK(board.Value(UNIT_MOD_ARMOR, TOTAL_PCT) == doctest::Approx(1.2f));

    board.Put(UNIT_MOD_ARMOR, TOTAL_PCT, 20.0f, false);
    CHECK(board.Value(UNIT_MOD_ARMOR, TOTAL_PCT) == doctest::Approx(1.0f));
}

TEST_CASE("tallies: two percentage shares stack on each other, not side by side")
{
    Tallies board;

    board.Put(UNIT_MOD_ARMOR, TOTAL_PCT, 50.0f, true);
    board.Put(UNIT_MOD_ARMOR, TOTAL_PCT, 50.0f, true);

    // 1.5 * 1.5, not 1 + 0.5 + 0.5
    CHECK(board.Value(UNIT_MOD_ARMOR, TOTAL_PCT) == doctest::Approx(2.25f));
}

TEST_CASE("tallies: a share of minus a hundred is read as minus two hundred")
{
    Tallies board;

    // taking the whole away would leave nought and never divide back out
    board.Put(UNIT_MOD_ARMOR, TOTAL_PCT, -100.0f, true);

    CHECK(board.Value(UNIT_MOD_ARMOR, TOTAL_PCT) == doctest::Approx(0.0f));
    board.Put(UNIT_MOD_ARMOR, TOTAL_PCT, -100.0f, false);
    CHECK(board.Value(UNIT_MOD_ARMOR, TOTAL_PCT) == doctest::Approx(1.0f));
}

TEST_CASE("tallies: a total percentage that is not positive reads as nothing")
{
    Tallies board;
    board.Value(UNIT_MOD_ARMOR, TOTAL_PCT, -0.5f);

    CHECK(board.Value(UNIT_MOD_ARMOR, TOTAL_PCT) == doctest::Approx(0.0f));
    CHECK(board.Folded(UNIT_MOD_ARMOR) == doctest::Approx(0.0f));

    // the raw four still carry what was written
    CHECK(board.Of(UNIT_MOD_ARMOR).totalPct == doctest::Approx(-0.5f));
}

TEST_CASE("tallies: the four fold in the order the chain runs")
{
    Tallies board;
    board.Value(UNIT_MOD_ARMOR, BASE_VALUE, 100.0f);
    board.Value(UNIT_MOD_ARMOR, BASE_PCT, 2.0f);
    board.Value(UNIT_MOD_ARMOR, TOTAL_VALUE, 50.0f);
    board.Value(UNIT_MOD_ARMOR, TOTAL_PCT, 1.1f);

    // ((100 * 2) + 50) * 1.1
    CHECK(board.Folded(UNIT_MOD_ARMOR) == doctest::Approx(275.0f));
}

TEST_CASE("tallies: what the unit was made with sits under the base value")
{
    Tallies board;
    board.Made(STAT_AGILITY, 60.0f);
    board.Value(UNIT_MOD_STAT_AGILITY, BASE_VALUE, 40.0f);
    board.Value(UNIT_MOD_STAT_AGILITY, BASE_PCT, 1.5f);
    board.Value(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, 10.0f);

    // ((40 + 60) * 1.5) + 10
    CHECK(board.FoldedOver(STAT_AGILITY) == doctest::Approx(160.0f));
    CHECK(board.Made(STAT_AGILITY) == doctest::Approx(60.0f));
}

TEST_CASE("tallies: what it was made with is left alone when nothing is put on")
{
    Tallies board;
    board.Made(STAT_STAMINA, 75.0f);

    CHECK(board.FoldedOver(STAT_STAMINA) == doctest::Approx(75.0f));
}

TEST_CASE("tallies: a group nothing was put on folds to what it was made with")
{
    Tallies board;

    CHECK(board.FoldedOver(STAT_SPIRIT) == doctest::Approx(0.0f));
    CHECK(board.Folded(UNIT_MOD_ATTACK_POWER) == doctest::Approx(0.0f));
}

TEST_CASE("tallies: the groups are kept apart")
{
    Tallies board;
    board.Put(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, 10.0f, true);

    CHECK(board.Value(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE) == doctest::Approx(10.0f));
    CHECK(board.Value(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE) == doctest::Approx(0.0f));
}

TEST_CASE("tallies: the sheet is told nothing until it is built")
{
    Tallies board;

    CHECK_FALSE(board.Ready());
    board.Ready(true);
    CHECK(board.Ready());
}

TEST_CASE("tallies: a group names the stat, school and power it is kept for")
{
    CHECK(stats::StatOf(UNIT_MOD_STAT_INTELLECT) == STAT_INTELLECT);
    CHECK(stats::StatOf(UNIT_MOD_ARMOR) == STAT_STRENGTH);

    CHECK(stats::SchoolOf(UNIT_MOD_RESISTANCE_FROST) == SPELL_SCHOOL_FROST);
    CHECK(stats::SchoolOf(UNIT_MOD_ARMOR) == SPELL_SCHOOL_NORMAL);

    CHECK(stats::PowerOf(UNIT_MOD_RAGE) == POWER_RAGE);
    CHECK(stats::PowerOf(UNIT_MOD_ARMOR) == POWER_MANA);
}

TEST_CASE("tallies: a group past the end reads as nothing rather than reading past the board")
{
    Tallies board;

    CHECK(board.Value(UNIT_MOD_END, BASE_VALUE) == doctest::Approx(0.0f));
    CHECK(board.Folded(UNIT_MOD_END) == doctest::Approx(0.0f));
    CHECK(board.Of(UNIT_MOD_END).Folded() == doctest::Approx(0.0f));
}
