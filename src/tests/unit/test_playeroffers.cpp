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

// What has been put to a character and not yet answered.

#include "doctest.h"

#include "Offers/PlayerOffers.h"

TEST_CASE("raising: an offer with nobody behind it does not stand")
{
    ResurrectOffer offer;

    CHECK_FALSE(offer.Stands());
    CHECK_FALSE(offer.MovesHim());
    CHECK(offer.StandsFrom(ObjectGuid()));
}

TEST_CASE("raising: an offer stands only for the one who made it")
{
    ObjectGuid const healer(HIGHGUID_PLAYER, uint32(7));
    ObjectGuid const someoneElse(HIGHGUID_PLAYER, uint32(8));

    ResurrectOffer offer;
    offer.from = healer;

    CHECK(offer.Stands());
    CHECK(offer.StandsFrom(healer));
    CHECK_FALSE(offer.StandsFrom(someoneElse));
}

TEST_CASE("raising: only an offer from a player moves him before he is raised")
{
    ResurrectOffer byPlayer;
    byPlayer.from = ObjectGuid(HIGHGUID_PLAYER, uint32(7));
    CHECK(byPlayer.MovesHim());

    // A spell raising him where he lies names no player, so nothing is moved.
    ResurrectOffer bySpell;
    bySpell.from = ObjectGuid(HIGHGUID_UNIT, uint32(300), uint32(9));
    CHECK(bySpell.Stands());
    CHECK_FALSE(bySpell.MovesHim());
}

TEST_CASE("raising: withdrawing leaves nothing behind")
{
    ResurrectOffer offer;
    offer.from = ObjectGuid(HIGHGUID_PLAYER, uint32(7));
    offer.at = Geometry::Placement::Somewhere(1, Geometry::Vector3(10.0f, 20.0f, 30.0f));
    offer.health = 500;
    offer.mana = 300;

    offer.Withdraw();

    CHECK_FALSE(offer.Stands());
    CHECK(offer.health == 0);
    CHECK(offer.mana == 0);
}

TEST_CASE("summons: one that was never made has already run out")
{
    SummonOffer offer;

    CHECK_FALSE(offer.Stands(1));
}

TEST_CASE("summons: one just made stands, and keeps the place it names")
{
    SummonOffer offer;
    offer.Offer(1, 10.0f, 20.0f, 30.0f, 1000);

    CHECK(offer.Stands(1000));
    CHECK(offer.at.MapId() == 1);
    CHECK(offer.at.X() == doctest::Approx(10.0f));
    CHECK(offer.at.Y() == doctest::Approx(20.0f));
    CHECK(offer.at.Z() == doctest::Approx(30.0f));
}

TEST_CASE("summons: it stands up to and including the moment it runs out")
{
    SummonOffer offer;
    offer.Offer(1, 0.0f, 0.0f, 0.0f, 1000);

    time_t const deadline = 1000 + MAX_PLAYER_SUMMON_DELAY;

    CHECK(offer.Stands(deadline - 1));
    CHECK(offer.Stands(deadline));
    CHECK_FALSE(offer.Stands(deadline + 1));
}

TEST_CASE("summons: refusing one is the same as its deadline having passed")
{
    SummonOffer offer;
    offer.Offer(1, 0.0f, 0.0f, 0.0f, 1000);
    CHECK(offer.Stands(1000));

    offer.Withdraw();

    CHECK_FALSE(offer.Stands(1000));
}
