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
// The routes here run in a straight line, so the Catmull-Rom spline through them
// is that line and the water between two nodes is exactly the gap between them.
// That makes the profile checkable by hand: at the speeds every classic vessel
// carries, 30 units a second and one unit a second squared, she needs 30 seconds
// and 450 units of water to reach cruising speed.
//
// Two things these pin down. A stretch is berth to berth, not node to node: a
// vessel does not stop at every waypoint on her way across the sea. And she does
// not sail the whole node list -- the outermost node at each end only lends the
// curve its tangent.

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

    /// Six nodes a kilometre apart on one map, of which she sails four: three
    /// kilometres of open water between node 1 and node 4.
    struct StraightRun
    {
        TaxiPathNodeEntry n0 = Node(0, 0, 0.0f);
        TaxiPathNodeEntry n1 = Node(1, 0, 1000.0f);
        TaxiPathNodeEntry n2 = Node(2, 0, 2000.0f);
        TaxiPathNodeEntry n3 = Node(3, 0, 3000.0f);
        TaxiPathNodeEntry n4 = Node(4, 0, 4000.0f);
        TaxiPathNodeEntry n5 = Node(5, 0, 5000.0f);

        std::vector<TaxiPathNodeEntry const*> nodes{&n0, &n1, &n2, &n3, &n4, &n5};
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

    // Three nodes are two tangents and a point, which is no water at all.
    StraightRun run;
    std::vector<TaxiPathNodeEntry const*> const three{&run.n0, &run.n1, &run.n2};
    CHECK(VesselRoute(three, SPEED, ACCEL).Period() == 0);

    // A vessel that cannot move never finishes a lap either.
    CHECK(VesselRoute(run.nodes, 0.0f, ACCEL).Period() == 0);
    CHECK(VesselRoute(run.nodes, SPEED, 0.0f).Period() == 0);
}

TEST_CASE("vessel route: the outermost node at each end only steers")
{
    StraightRun run;
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    CHECK(route.Legs().size() == 1);
    CHECK(route.Waiting() == 0);

    // Five kilometres of nodes, three kilometres of water, flat cruise throughout.
    CHECK(route.Period() == 100000);

    // A seventh node adds a kilometre of water, not just a kilometre of curve.
    TaxiPathNodeEntry n6 = Node(6, 0, 6000.0f);
    std::vector<TaxiPathNodeEntry const*> longer(run.nodes);
    longer.push_back(&n6);
    CHECK(VesselRoute(longer, SPEED, ACCEL).Period() == 133333);
}

TEST_CASE("vessel route: a berth is what a stretch of water runs between")
{
    StraightRun run;
    run.n3 = Node(3, 0, 3000.0f, TAXI_NODE_STOP, 45);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    CHECK(route.Waiting() == 45u * 1000u);

    // Two kilometres from node 1 to the berth at node 3, then one to node 4.
    CHECK(route.Period() == Once(2000.0f) + Once(1000.0f) + 45u * 1000u);
}

TEST_CASE("vessel route: the water between two berths is paid for at both ends")
{
    StraightRun run;
    run.n2 = Node(2, 0, 2000.0f, TAXI_NODE_STOP, 30);
    run.n3 = Node(3, 0, 3000.0f, TAXI_NODE_STOP, 30);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    CHECK(route.Waiting() == 60u * 1000u);
    CHECK(route.Period() == Once(1000.0f) + Twice(1000.0f) + Once(1000.0f) + 60u * 1000u);
}

TEST_CASE("vessel route: a berth she is already at is no stretch of water")
{
    StraightRun run;
    run.n0 = Node(0, 0, 0.0f, TAXI_NODE_STOP, 60);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    // The first node of a leg is where she already lies, so it is not a stretch
    // she sails and the wait is not hers to serve again.
    CHECK(route.Waiting() == 0);
    CHECK(route.Period() == 100000);
}

TEST_CASE("vessel route: a teleport breaks the lap in two without changing map")
{
    StraightRun run;
    run.n1 = Node(1, 0, 1000.0f, TAXI_NODE_TELEPORT);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    // Two nodes before the jump are not enough to sail on; the four after it
    // carry one kilometre of water.
    CHECK(route.Legs().size() == 2);
    CHECK(route.Period() == uint32(1000.0f / SPEED * 1000.0f));
}

TEST_CASE("vessel route: a change of map breaks it the same way")
{
    StraightRun run;
    run.n2 = Node(2, 1, 2000.0f);
    run.n3 = Node(3, 1, 3000.0f);
    run.n4 = Node(4, 1, 4000.0f);
    run.n5 = Node(5, 1, 5000.0f);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    CHECK(route.Legs().size() == 2);
    CHECK(route.Period() == uint32(1000.0f / SPEED * 1000.0f));
}

TEST_CASE("vessel route: the legs tile the lap end to end")
{
    StraightRun run;
    run.n3 = Node(3, 0, 3000.0f, TAXI_NODE_TELEPORT);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    REQUIRE(route.Legs().size() == 2);

    uint32 next = 0;
    for (VesselLeg const& leg : route.Legs())
    {
        CHECK(leg.startsAt == next);
        CHECK(leg.endsAt >= leg.startsAt);
        next = leg.endsAt;
    }

    CHECK(next == route.Period());
}

TEST_CASE("vessel route: the leg she is on names the map she sails")
{
    // Five nodes on one map and five on the next: two kilometres of water each.
    TaxiPathNodeEntry n[10];
    std::vector<TaxiPathNodeEntry const*> nodes;
    for (uint32 i = 0; i < 10; ++i)
    {
        n[i] = Node(i, i < 5 ? 0 : 1, 1000.0f * i);
        nodes.push_back(&n[i]);
    }

    VesselRoute const route(nodes, SPEED, ACCEL);

    REQUIRE(route.Legs().size() == 2);
    CHECK(route.Period() == 133334);

    CHECK(route.Legs()[0].mapId == 0);
    CHECK(route.Legs()[0].startsAt == 0);
    CHECK(route.Legs()[0].endsAt == 66667);
    CHECK(route.Legs()[0].From().x == doctest::Approx(1000.0f));

    CHECK(route.Legs()[1].mapId == 1);
    CHECK(route.Legs()[1].startsAt == 66667);
    CHECK(route.Legs()[1].From().x == doctest::Approx(6000.0f));

    // And that is what the moment of the transfer is read off.
    CHECK(route.LegAt(0)->mapId == 0);
    CHECK(route.LegAt(66666)->mapId == 0);
    CHECK(route.LegAt(66667)->mapId == 1);
    CHECK(route.LegAt(133333)->mapId == 1);

    // The lap comes round again.
    CHECK(route.LegAt(133334)->mapId == 0);
    CHECK(route.LegAt(5 * 133334 + 66667)->mapId == 1);
}

TEST_CASE("vessel route: a leg crossed in no time is a leg she is never on")
{
    StraightRun run;
    run.n1 = Node(1, 0, 1000.0f, TAXI_NODE_TELEPORT);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    REQUIRE(route.Legs().size() == 2);
    CHECK(route.Legs()[0].startsAt == route.Legs()[0].endsAt);

    // Every moment of the lap belongs to the leg that has water on it.
    for (uint32 at = 0; at < route.Period(); at += 1000)
    {
        REQUIRE(route.LegAt(at) != nullptr);
        CHECK(route.LegAt(at) == &route.Legs()[1]);
    }
}

TEST_CASE("vessel route: a route with no lap has no leg to be on")
{
    std::vector<TaxiPathNodeEntry const*> const none;
    CHECK(VesselRoute(none, SPEED, ACCEL).LegAt(0) == nullptr);
    CHECK(VesselRoute().LegAt(1234) == nullptr);
}

TEST_CASE("vessel route: the pose is between the nodes, not on the last one passed")
{
    StraightRun run;
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    // Three kilometres at a flat thirty: she is a kilometre along after a third of it.
    CHECK(route.PoseAt(0).at.x == doctest::Approx(1000.0f));
    CHECK(route.PoseAt(50000).at.x == doctest::Approx(2500.0f));
    CHECK(route.PoseAt(99999).at.x == doctest::Approx(4000.0f).epsilon(0.001));

    CHECK(route.PoseAt(50000).known);
    CHECK(route.PoseAt(50000).mapId == 0);

    // And the lap comes round to where it started.
    CHECK(route.PoseAt(100000).at.x == doctest::Approx(route.PoseAt(0).at.x));
}

TEST_CASE("vessel route: she only ever moves forward along the leg")
{
    StraightRun run;
    run.n3 = Node(3, 0, 3000.0f, TAXI_NODE_STOP, 45);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    float behind = 0.0f;
    for (uint32 at = 0; at < route.Period(); at += 250)
    {
        VesselPose const pose = route.PoseAt(at);
        REQUIRE(pose.known);
        CHECK(pose.at.x >= behind - 0.01f);
        CHECK(pose.at.x >= 1000.0f - 0.01f);
        CHECK(pose.at.x <= 4000.0f + 0.01f);
        behind = pose.at.x;
    }
}

TEST_CASE("vessel route: berthed, she does not move at all")
{
    StraightRun run;
    run.n3 = Node(3, 0, 3000.0f, TAXI_NODE_STOP, 45);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    uint32 const arrives = Once(2000.0f);

    // She lies at the berth for the whole of its delay, and the berth is node 3.
    CHECK(route.PoseAt(arrives).at.x == doctest::Approx(3000.0f));
    CHECK(route.PoseAt(arrives + 20000).at.x == doctest::Approx(3000.0f));
    CHECK(route.PoseAt(arrives + 44999).at.x == doctest::Approx(3000.0f));

    // And pulls away from it after that.
    CHECK(route.PoseAt(arrives + 46000).at.x > 3000.0f);
}

TEST_CASE("vessel route: she pulls away from rest, she does not jump to speed")
{
    StraightRun run;
    run.n3 = Node(3, 0, 3000.0f, TAXI_NODE_STOP, 45);
    VesselRoute const route(run.nodes, SPEED, ACCEL);

    // One second out of the berth at one unit a second squared is half a unit,
    // where a flat cruise would already have made thirty.
    CHECK(route.PoseAt(1000).at.x == doctest::Approx(1000.5f).epsilon(0.01));
}

TEST_CASE("vessel route: the maps it touches are the maps its legs are on")
{
    StraightRun plain;
    CHECK(VesselRoute(plain.nodes, SPEED, ACCEL).Maps() == std::set<uint32>{0});

    StraightRun crossing;
    crossing.n2 = Node(2, 1, 2000.0f);
    crossing.n3 = Node(3, 1, 3000.0f);
    crossing.n4 = Node(4, 1, 4000.0f);
    crossing.n5 = Node(5, 1, 5000.0f);
    CHECK(VesselRoute(crossing.nodes, SPEED, ACCEL).Maps() == std::set<uint32>{0, 1});
}
