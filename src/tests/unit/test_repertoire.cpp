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

// The four spells a unit was made knowing, and when it may cast each again.

#include "doctest.h"

#include "Creature/Repertoire.h"

namespace
{
    time_t const NOON = 1000000;

    uint32 const FIREBALL = 133;
    uint32 const FROSTBOLT = 116;
    uint32 const FIRE_SCHOOL = 5;
}

TEST_CASE("repertoire: a unit made knowing nothing knows nothing")
{
    Repertoire knows;

    CHECK(knows.Slot(0) == 0);
    CHECK_FALSE(knows.Knows(FIREBALL));
    CHECK(knows.NothingDown());
}

TEST_CASE("repertoire: the four slots hold what the row put in them")
{
    Repertoire knows;
    knows.Slot(0, FIREBALL);
    knows.Slot(3, FROSTBOLT);

    CHECK(knows.Slot(0) == FIREBALL);
    CHECK(knows.Slot(3) == FROSTBOLT);
    CHECK(knows.Slot(1) == 0);
    CHECK(knows.Knows(FIREBALL));
    CHECK(knows.Knows(FROSTBOLT));
    CHECK_FALSE(knows.Knows(999));
}

TEST_CASE("repertoire: a slot beyond the four is neither read nor written")
{
    Repertoire knows;
    knows.Slot(CREATURE_MAX_SPELLS, FIREBALL);

    CHECK(knows.Slot(CREATURE_MAX_SPELLS) == 0);
    CHECK_FALSE(knows.Knows(FIREBALL));
}

TEST_CASE("repertoire: knowing nothing is not knowing a spell of nought")
{
    Repertoire knows;

    // every slot holds nought, and nought is not a spell
    CHECK(knows.Knows(0));
}

TEST_CASE("repertoire: a spell is down until the hour it comes back")
{
    Repertoire knows;
    knows.ReadyAt(FIREBALL, NOON + 30);

    CHECK(knows.SpellDown(FIREBALL, NOON));
    CHECK(knows.Left(FIREBALL, NOON) == 30);

    CHECK_FALSE(knows.SpellDown(FIREBALL, NOON + 30));
    CHECK(knows.Left(FIREBALL, NOON + 30) == 0);
}

TEST_CASE("repertoire: a spell nothing was said about is not down")
{
    Repertoire knows;

    CHECK_FALSE(knows.SpellDown(FIREBALL, NOON));
    CHECK(knows.Left(FIREBALL, NOON) == 0);
}

TEST_CASE("repertoire: a category holds for as long as the spell asking says")
{
    Repertoire knows;
    knows.CategoryUsedAt(FIRE_SCHOOL, NOON);

    CHECK(knows.CategoryDown(FIRE_SCHOOL, 10, NOON + 5));
    CHECK_FALSE(knows.CategoryDown(FIRE_SCHOOL, 10, NOON + 10));

    // the same category, asked about by a spell that holds it longer
    CHECK(knows.CategoryDown(FIRE_SCHOOL, 60, NOON + 10));
}

TEST_CASE("repertoire: a category nothing was said about is not held")
{
    Repertoire knows;

    CHECK_FALSE(knows.CategoryDown(FIRE_SCHOOL, 10000, NOON));
}

TEST_CASE("repertoire: forgetting what has come back leaves what is still ahead")
{
    Repertoire knows;
    knows.ReadyAt(FIREBALL, NOON - 10);
    knows.ReadyAt(FROSTBOLT, NOON + 10);

    knows.ForgetExpired(NOON);

    CHECK(knows.StillDown().size() == 1);
    CHECK(knows.StillDown().count(FROSTBOLT) == 1);
    CHECK_FALSE(knows.NothingDown());
}

TEST_CASE("repertoire: a spell that comes back exactly now is forgotten")
{
    Repertoire knows;
    knows.ReadyAt(FIREBALL, NOON);

    knows.ForgetExpired(NOON);

    CHECK(knows.NothingDown());
}

TEST_CASE("repertoire: clearing brings everything back at once")
{
    Repertoire knows;
    knows.Slot(0, FIREBALL);
    knows.ReadyAt(FIREBALL, NOON + 100);
    knows.CategoryUsedAt(FIRE_SCHOOL, NOON);

    knows.Clear();

    CHECK(knows.NothingDown());
    CHECK(knows.CategoriesUsed().empty());

    // what it knows is not a cooldown and is left alone
    CHECK(knows.Slot(0) == FIREBALL);
}
