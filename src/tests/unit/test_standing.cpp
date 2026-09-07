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

// How much a deed moves a faction's opinion, and how a stretch of rest is paid.

#include "doctest.h"

#include "Rest.h"
#include "Standing.h"

TEST_CASE("standing: the whole of a deed at a rate of one")
{
    standing::FactionRate silent;

    CHECK(standing::Gained(100, 100.0f, false, 1.0f, silent, 1.0f) == 100);
    CHECK(standing::Gained(250, 100.0f, false, 1.0f, silent, 1.0f) == 250);
}

TEST_CASE("standing: the auras' percentage scales it")
{
    standing::FactionRate silent;

    CHECK(standing::Gained(100, 110.0f, false, 1.0f, silent, 1.0f) == 110);
    CHECK(standing::Gained(100, 50.0f, false, 1.0f, silent, 1.0f) == 50);
}

TEST_CASE("standing: a deed beneath him pays the low-level rate")
{
    standing::FactionRate silent;

    CHECK(standing::Gained(100, 100.0f, true, 0.5f, silent, 1.0f) == 50);

    // And only when it was beneath him.
    CHECK(standing::Gained(100, 100.0f, false, 0.5f, silent, 1.0f) == 100);

    // A low-level rate of one changes nothing either way.
    CHECK(standing::Gained(100, 100.0f, true, 1.0f, silent, 1.0f) == 100);
}

TEST_CASE("standing: a percentage of nothing or less pays nothing")
{
    standing::FactionRate silent;

    CHECK(standing::Gained(100, 0.0f, false, 1.0f, silent, 1.0f) == 0);
    CHECK(standing::Gained(100, -10.0f, false, 1.0f, silent, 1.0f) == 0);
}

TEST_CASE("standing: a faction may pay its own way, or refuse outright")
{
    standing::FactionRate doubled;
    doubled.stated = true;
    doubled.rate = 2.0f;

    CHECK(standing::Gained(100, 100.0f, false, 1.0f, doubled, 1.0f) == 200);

    standing::FactionRate refuses;
    refuses.stated = true;
    refuses.rate = 0.0f;

    CHECK(standing::Gained(100, 100.0f, false, 1.0f, refuses, 100.0f) == 0);
}

TEST_CASE("standing: the server's overall rate is applied last")
{
    standing::FactionRate silent;

    CHECK(standing::Gained(100, 100.0f, false, 1.0f, silent, 3.0f) == 300);
}

TEST_CASE("rest: awake, in an inn, and in a field")
{
    rest::Rates paid;
    paid.inGame = 1.0f;
    paid.offlineInn = 4.0f;
    paid.offlineWilderness = 4.0f;

    CHECK(rest::RateFor(false, false, paid) == doctest::Approx(1.0f));
    CHECK(rest::RateFor(false, true, paid) == doctest::Approx(1.0f));

    // Logged out in an inn pays its rate; anywhere else pays a quarter of it.
    CHECK(rest::RateFor(true, true, paid) == doctest::Approx(4.0f));
    CHECK(rest::RateFor(true, false, paid) == doctest::Approx(1.0f));
}
