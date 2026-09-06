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

// Where a unit belongs, how far it may stray, and what it leashes to.

#include "doctest.h"

#include "Creature/Station.h"

TEST_CASE("station: a fresh station stands nowhere, idle, five yards wide")
{
    Station post;

    CHECK(post.Radius() == doctest::Approx(5.0f));
    CHECK(post.Wander() == IDLE_MOTION_TYPE);
    CHECK_FALSE(post.Where().IsPlaced());
}

TEST_CASE("station: where it belongs is a whole pose, facing and all")
{
    Station post;
    post.Where(Geometry::Placement::Somewhere(571, Geometry::Vector3(10.0f, 20.0f, 30.0f), 1.5f));

    CHECK(post.Where().MapId() == 571);
    CHECK(post.Where().X() == doctest::Approx(10.0f));
    CHECK(post.Where().Y() == doctest::Approx(20.0f));
    CHECK(post.Where().Z() == doctest::Approx(30.0f));
    CHECK(post.Where().Facing() == doctest::Approx(1.5f));
}

TEST_CASE("station: putting it somewhere keeps the frame it was put in")
{
    Station post;
    Geometry::Placement const onADeck =
        Geometry::Placement::Somewhere(369, Geometry::Vector3(0.0f, 0.0f, 0.0f), 0.0f);

    post.PlaceInFrameOf(onADeck, Geometry::Vector3(3.0f, 4.0f, 5.0f), 2.0f);

    CHECK(post.Where().MapId() == 369);
    CHECK(post.Where().X() == doctest::Approx(3.0f));
    CHECK(post.Where().Facing() == doctest::Approx(2.0f));
}

TEST_CASE("station: the leash point is not where it belongs")
{
    Station post;
    post.Where(Geometry::Placement::Somewhere(0, Geometry::Vector3(0.0f, 0.0f, 0.0f), 0.0f));
    post.Anchor(Geometry::Vector3(100.0f, 200.0f, 300.0f));

    CHECK(post.Anchor().x == doctest::Approx(100.0f));
    CHECK(post.Where().X() == doctest::Approx(0.0f));
}

TEST_CASE("station: a radius of nothing is how a row says stand still")
{
    Station post;
    post.Radius(0.0f);
    post.Wander(RANDOM_MOTION_TYPE);

    CHECK(post.Radius() == doctest::Approx(0.0f));
    CHECK(post.Wander() == RANDOM_MOTION_TYPE);
}
