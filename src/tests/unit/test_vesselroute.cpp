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
// them is that line and the water between two nodes is exactly the gap between
// them. That makes the profile checkable by hand: at the speeds every classic
// vessel carries, 30 units a second and one unit a second squared, she needs 30
// seconds and 450 units of water to reach cruising speed.
//
// The thing these pin down is what a stretch is. It is berth to berth, not node
// to node: a vessel does not stop at every waypoint on her way across the sea.

#include "doctest.h"

#include "VesselRoute.h"

#include <cmath>

namespace
{
    float const SPEED = 30.0f;
    float const ACCEL = 1.0f;

    float const TO_SPEED = SPEED / ACCEL;                   // 30 s
    float const RUN_UP = 0.5f * SPEED * TO_SPEED;           // 450 units

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

    /// Four nodes a kilometre apart on one map: three kilometres of open water.
    struct StraightRun
    {
        TaxiPathNodeEntry a = Node(0, 0, 0.0f);
        TaxiPathNodeEntry b = Node(1, 0, 1000.0f);
        TaxiPathNodeEntry c = Node(2, 0, 2000.0f);
        TaxiPathNodeEntry d = Node(3, 0, 3000.0f);

        std::vector<TaxiPathNodeEntry const*> nodes{&a, &b, &c, &d};
    };

    /// Pulling away from a berth, or coming into one.
    uint32 Once(float ds)
    {
        float const t = RUN_UP >= ds ? std::sqrt(2.0f * ds / ACCEL)
                                     : (ds - RUN_UP) / SPEED + TO_SPEED;
        return uint32(t * 1000.0f + 0.5f);
    }

    /// Berth to berth, so both ends are paid for.
    uint32 Twice(float ds)
    {
        float const t = RUN_UP >= ds * 0.5f ? 2.0f * std::sqrt(ds / ACCEL)
                                            : (ds - 2.0f * RUN_UP) / SPEED + 2.0f * TO_SPEED;
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

TEST_CASE("vessel route: with nowhere to berth she never slows at all")
{
    StraightRun run;
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    CHECK(route.Legs() == 1);
    CHECK(route.Waiting() == 0);

    // Three kilometres at a flat thirty, and not one node's worth of braking.
    CHECK(route.Period() == 100000);
}

TEST_CASE("vessel route: a berth is what a stretch of water runs between")
{
    StraightRun run;
    run.c = Node(2, 0, 2000.0f, TAXI_NODE_STOP, 45);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    CHECK(route.Waiting() == 45u * 1000u);

    // Out of the start to the berth, then out of the berth to the end.
    CHECK(route.Period() == Once(2000.0f) + Once(1000.0f) + 45u * 1000u);

    // Berthing costs more than the flat cruise it replaces, twice over.
    StraightRun plain;
    CHECK(route.Period() > VesselRoute(plain.nodes, SPEED, ACCEL).Period() + 45u * 1000u);
}

TEST_CASE("vessel route: the water between two berths is paid for at both ends")
{
    StraightRun run;
    run.b = Node(1, 0, 1000.0f, TAXI_NODE_STOP, 30);
    run.c = Node(2, 0, 2000.0f, TAXI_NODE_STOP, 30);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    CHECK(route.Waiting() == 60u * 1000u);
    CHECK(route.Period() == Once(1000.0f) + Twice(1000.0f) + Once(1000.0f) + 60u * 1000u);
}

TEST_CASE("vessel route: a berth she is already at is no stretch of water")
{
    StraightRun run;
    run.a = Node(0, 0, 0.0f, TAXI_NODE_STOP, 60);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    // The first node of a leg is where she already lies, so it is not a stretch
    // she sails and the wait is not hers to serve again.
    CHECK(route.Waiting() == 0);
    CHECK(route.Period() == 100000);
}

TEST_CASE("vessel route: a teleport breaks the lap in two without changing map")
{
    StraightRun run;
    run.b = Node(1, 0, 1000.0f, TAXI_NODE_TELEPORT);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    // A kilometre up to the jump and a kilometre after it; the water between the
    // two is never sailed, so a third of the route costs nothing.
    CHECK(route.Legs() == 2);
    CHECK(route.Period() == 2u * uint32(1000.0f / SPEED * 1000.0f));
}

TEST_CASE("vessel route: a change of map breaks it the same way")
{
    StraightRun run;
    run.c = Node(2, 1, 2000.0f);
    run.d = Node(3, 1, 3000.0f);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    CHECK(route.Legs() == 2);
    CHECK(route.Period() == 2u * uint32(1000.0f / SPEED * 1000.0f));
}
