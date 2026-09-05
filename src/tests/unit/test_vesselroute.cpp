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

// How long a vessel's lap takes.
//
// The routes here run in a straight line, so the Catmull-Rom spline through
// them is that line and each span is exactly as long as the gap between two
// nodes. That makes the trapezoidal profile checkable by hand: at the speeds
// every classic vessel carries, 30 units a second and one unit a second
// squared, she needs 30 seconds and 450 units of water to get up to speed.

#include "doctest.h"

#include "VesselRoute.h"

namespace
{
    float const SPEED = 30.0f;
    float const ACCEL = 1.0f;

    /// Seconds of accelerating, and the ground it covers.
    float const TO_SPEED = SPEED / ACCEL;
    float const RUN_UP = 0.5f * SPEED * TO_SPEED;

    TaxiPathNodeEntry Node(uint32 index, uint32 map, float x, uint32 flags = 0, uint32 delay = 0)
    {
        TaxiPathNodeEntry node{};
        node.PathID = 1;
        node.NodeIndex = index;
        node.ContinentID = map;
        node.LocX = x;
        node.Flags = flags;
        node.Delay = delay;
        return node;
    }

    /// Four nodes a kilometre apart on one map, which is three spans of open water.
    struct StraightRun
    {
        TaxiPathNodeEntry a = Node(0, 0, 0.0f);
        TaxiPathNodeEntry b = Node(1, 0, 1000.0f);
        TaxiPathNodeEntry c = Node(2, 0, 2000.0f);
        TaxiPathNodeEntry d = Node(3, 0, 3000.0f);

        std::vector<TaxiPathNodeEntry const*> nodes{&a, &b, &c, &d};
    };

    /// What the client's profile makes of one span, in milliseconds.
    uint32 FromRest(float ds)
    {
        float const t = RUN_UP <= ds ? TO_SPEED + (ds - RUN_UP) / SPEED
                                     : std::sqrt(2.0f * ds / ACCEL);
        return uint32(t * 1000.0f + 0.5f);
    }

    uint32 UpAndDown(float ds)
    {
        float const t = RUN_UP <= ds * 0.5f ? 2.0f * TO_SPEED + (ds - 2.0f * RUN_UP) / SPEED
                                            : 2.0f * std::sqrt(ds / ACCEL);
        return uint32(t * 1000.0f + 0.5f);
    }
}

TEST_CASE("vessel route: a path with nothing to sail takes no time")
{
    std::vector<TaxiPathNodeEntry const*> const none;
    CHECK(VesselRoute(none, SPEED, ACCEL).Period() == 0);

    TaxiPathNodeEntry alone = Node(0, 0, 0.0f);
    std::vector<TaxiPathNodeEntry const*> const one{&alone};
    CHECK(VesselRoute(one, SPEED, ACCEL).Period() == 0);

    // A vessel that cannot move never finishes a lap either.
    StraightRun run;
    CHECK(VesselRoute(run.nodes, 0.0f, ACCEL).Period() == 0);
    CHECK(VesselRoute(run.nodes, SPEED, 0.0f).Period() == 0);
}

TEST_CASE("vessel route: the lap is the profile summed over the spans")
{
    StraightRun run;
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    CHECK(route.Legs() == 1);
    CHECK(route.Waiting() == 0);

    // Out of the standstill once, then up and down twice.
    uint32 const expected = FromRest(1000.0f) + 2 * UpAndDown(1000.0f);
    CHECK(route.Period() == doctest::Approx(double(expected)).epsilon(0.01));

    // She is slower than a straight run at cruising speed, because she spends
    // part of every span getting back to it.
    CHECK(route.Period() > uint32(3000.0f / SPEED * 1000.0f));
}

TEST_CASE("vessel route: a stop adds its own delay and nothing else")
{
    StraightRun plain;
    uint32 const sailing = VesselRoute(plain.nodes, SPEED, ACCEL).Period();

    StraightRun waiting;
    waiting.c = Node(2, 0, 2000.0f, TAXI_NODE_STOP, 45);
    VesselRoute const route(waiting.nodes, SPEED, ACCEL);

    CHECK(route.Waiting() == 45u * 1000u);
    CHECK(route.Period() == sailing + 45u * 1000u);
    CHECK(route.Legs() == 1);                               // stopping is not jumping
}

TEST_CASE("vessel route: a teleport breaks the lap in two without changing map")
{
    StraightRun jumping;
    jumping.b = Node(1, 0, 1000.0f, TAXI_NODE_TELEPORT);
    VesselRoute const route(jumping.nodes, SPEED, ACCEL);

    // One leg up to the jump, one after it: the water between them is never sailed.
    CHECK(route.Legs() == 2);
    CHECK(route.Period() == FromRest(1000.0f) + FromRest(1000.0f));

    StraightRun plain;
    CHECK(route.Period() != VesselRoute(plain.nodes, SPEED, ACCEL).Period());
}

TEST_CASE("vessel route: a change of map breaks it the same way")
{
    StraightRun crossing;
    crossing.c = Node(2, 1, 2000.0f);
    crossing.d = Node(3, 1, 3000.0f);
    VesselRoute const route(crossing.nodes, SPEED, ACCEL);

    CHECK(route.Legs() == 2);
    CHECK(route.Period() == 2 * FromRest(1000.0f));
}
