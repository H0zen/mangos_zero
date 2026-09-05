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

// How often a game object has been used, and by whom.
//
// The two counts are not the same count, and a summoning ritual turns on the
// difference: it completes when enough different players have joined it, so one
// player clicking five times has used it five times and is still one caster.

#include "doctest.h"

#include "UserTally.h"

TEST_CASE("user tally: uses and users are counted apart")
{
    UserTally tally;

    ObjectGuid const one(HIGHGUID_PLAYER, uint32(1));
    ObjectGuid const two(HIGHGUID_PLAYER, uint32(2));

    CHECK(tally.Uses() == 0);
    CHECK(tally.Distinct() == 0);
    CHECK(tally.First().IsEmpty());

    tally.UsedBy(one);
    tally.UsedBy(one);
    CHECK(tally.Uses() == 2);
    CHECK(tally.Distinct() == 1);

    tally.UsedBy(two);
    CHECK(tally.Uses() == 3);
    CHECK(tally.Distinct() == 2);
    CHECK(tally.Everyone().size() == 2);
}

TEST_CASE("user tally: the first one through is kept, and only the first")
{
    UserTally tally;

    ObjectGuid const opener(HIGHGUID_PLAYER, uint32(7));
    ObjectGuid const latecomer(HIGHGUID_PLAYER, uint32(9));

    tally.UsedBy(opener);
    tally.UsedBy(latecomer);
    tally.UsedBy(latecomer);

    CHECK(tally.First() == opener);
}

TEST_CASE("user tally: a use by nobody in particular still counts")
{
    UserTally tally;

    // A vein, a fishing hole and a trap have charges but no names to keep.
    tally.Used();
    tally.Used();

    CHECK(tally.Uses() == 2);
    CHECK(tally.Distinct() == 0);
    CHECK(tally.First().IsEmpty());
}

TEST_CASE("user tally: forgetting leaves it as good as untouched")
{
    UserTally tally;

    tally.UsedBy(ObjectGuid(HIGHGUID_PLAYER, uint32(3)));
    tally.Used();
    tally.Forget();

    CHECK(tally.Uses() == 0);
    CHECK(tally.Distinct() == 0);
    CHECK(tally.First().IsEmpty());

    // And it takes a new first user afterwards.
    ObjectGuid const next(HIGHGUID_PLAYER, uint32(4));
    tally.UsedBy(next);
    CHECK(tally.First() == next);
}
