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

// The watch kept from a unit's death to its return.

#include "doctest.h"

#include "Creature/Vigil.h"

namespace
{
    time_t const NOON = 1000000;
}

TEST_CASE("vigil: a unit that has not died is due back now")
{
    Vigil watch;

    CHECK(watch.BackAt(NOON) == NOON);
}

TEST_CASE("vigil: while its body lies there, it is due back when the body goes and the wait after")
{
    Vigil watch;
    watch.CorpseGoesAt(NOON + 60);
    watch.RespawnDelay(300);

    CHECK(watch.BackAt(NOON) == NOON + 60 + 300);
}

TEST_CASE("vigil: once the body is gone, the hour it stands again is the answer")
{
    Vigil watch;
    watch.CorpseGoesAt(NOON - 10);
    watch.RespawnsAt(NOON + 500);
    watch.RespawnDelay(300);

    CHECK(watch.BackAt(NOON) == NOON + 500);
}

TEST_CASE("vigil: the hour it stands again wins over a body still lying there")
{
    Vigil watch;
    watch.CorpseGoesAt(NOON + 60);
    watch.RespawnsAt(NOON + 10);
    watch.RespawnDelay(300);

    CHECK(watch.BackAt(NOON) == NOON + 10);
}

TEST_CASE("vigil: an hour that has passed is not in the future")
{
    Vigil watch;
    watch.CorpseGoesAt(NOON - 100);
    watch.RespawnsAt(NOON - 50);

    CHECK(watch.BackAt(NOON) == NOON);
}

TEST_CASE("vigil: the grace runs down a tick at a time and then stops")
{
    Vigil watch;
    watch.AggroDelay(5000);

    CHECK(watch.StillDazed(2000));
    CHECK(watch.AggroDelay() == 3000);

    CHECK(watch.StillDazed(2999));
    CHECK(watch.AggroDelay() == 1);

    CHECK_FALSE(watch.StillDazed(1));
    CHECK(watch.AggroDelay() == 0);
}

TEST_CASE("vigil: a tick longer than the grace ends it without going under")
{
    Vigil watch;
    watch.AggroDelay(100);

    CHECK_FALSE(watch.StillDazed(100000));
    CHECK(watch.AggroDelay() == 0);
}

TEST_CASE("vigil: a unit with no grace left is never dazed")
{
    Vigil watch;

    CHECK_FALSE(watch.StillDazed(0));
    CHECK(watch.AggroDelay() == 0);
}

TEST_CASE("vigil: a fresh watch carries the world's defaults")
{
    Vigil watch;

    CHECK(watch.RespawnDelay() == 25);
    CHECK(watch.CorpseDelay() == 60);
    CHECK_FALSE(watch.DeadByDefault());
    CHECK(watch.KilledAt() == 0);
}
