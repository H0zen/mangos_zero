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

// The spatial boundary: distance across frames is infinite, and every question
// built on distance fails CLOSED.
//
// A ship is itself a map. Deck coordinates are the real coordinates on that map,
// and the server's idea of where the hull is in the world is a waypoint estimate
// it has no business composing with. So a man on the deck and a man on the shore
// are not near each other, not in each other's arc, and never will be by any
// arithmetic this class offers -- while two men on the SAME deck compare in the
// vessel's frame exactly as two men on a continent do.
//
// The subtle one is IsInBack: it reads `!HasInArc(...)`, and HasInArc is false
// across frames, so the negation alone would say "yes, behind you" about someone
// on another map. It survives only because the conjunction checks distance first.
// That ordering is load-bearing and is pinned below.

#include "doctest.h"

#include "Geometry/Placement.h"

#include <cmath>

using Geometry::Frame;
using Geometry::Placement;
using Geometry::Vector3;

namespace
{
    const uint32_t MAP_EASTERN_KINGDOMS = 0;
    const uint32_t MAP_KALIMDOR = 1;
    const uint32_t NO_INSTANCE = 0;

    const uint64_t VESSEL_A = 0x1FC0000000000001ull;
    const uint64_t VESSEL_B = 0x1FC0000000000002ull;

    Placement At(const Frame& frame, float x, float y, float z, float facing = 0.f)
    {
        Placement p(0.f);
        p.EnterFrame(frame, Vector3(x, y, z), facing);
        return p;
    }
}

TEST_CASE("Two placements in the same frame measure normally")
{
    const Frame world = Frame::World(MAP_EASTERN_KINGDOMS, NO_INSTANCE);

    const Placement a = At(world, 0.f, 0.f, 0.f);
    const Placement b = At(world, 3.f, 4.f, 0.f);

    CHECK(a.ShareFrame(b));
    CHECK(a.DistanceTo(b) == doctest::Approx(5.f));
    CHECK(a.WithinDist(b, 6.f));
    CHECK_FALSE(a.WithinDist(b, 4.f));
}

TEST_CASE("Across two maps every measure fails closed")
{
    // Same coordinates in both, so nothing but the frame can be telling them
    // apart. A composition bug would report zero distance here.
    const Placement here  = At(Frame::World(MAP_EASTERN_KINGDOMS, NO_INSTANCE), 10.f, 10.f, 10.f);
    const Placement there = At(Frame::World(MAP_KALIMDOR, NO_INSTANCE), 10.f, 10.f, 10.f);

    CHECK_FALSE(here.ShareFrame(there));

    CHECK(std::isinf(here.DistanceTo(there)));
    CHECK(here.DistanceTo(there) == Placement::Unreachable());
    CHECK(std::isinf(here.HeightGapTo(there)));

    CHECK_FALSE(here.WithinDist(there, 1000000.f));
    CHECK_FALSE(here.WithinRange(there, 0.f, 1000000.f));
    CHECK_FALSE(here.HasInArc(there, Placement::TwoPi()));
    CHECK_FALSE(here.IsInFront(there, 1000000.f, Placement::TwoPi()));

    // The one that only fails closed because distance is checked first.
    CHECK_FALSE(here.IsInBack(there, 1000000.f, Placement::Pi()));
}

TEST_CASE("The same map in two instances is two frames")
{
    // Instance isolation is the same mechanism, not a separate check bolted on.
    const Placement one = At(Frame::World(MAP_EASTERN_KINGDOMS, 1), 0.f, 0.f, 0.f);
    const Placement two = At(Frame::World(MAP_EASTERN_KINGDOMS, 2), 0.f, 0.f, 0.f);

    CHECK_FALSE(one.ShareFrame(two));
    CHECK(std::isinf(one.DistanceTo(two)));
    CHECK_FALSE(one.WithinDist(two, 1.f));
}

TEST_CASE("A deck is a frame of its own, and the shore is not in it")
{
    // The invariant in one case: aboard, the world does not exist.
    const Frame deck = Frame::Deck(VESSEL_A);
    const Frame shore = Frame::World(MAP_EASTERN_KINGDOMS, NO_INSTANCE);

    const Placement aboard = At(deck, 2.f, 0.f, 0.f);
    const Placement ashore = At(shore, 2.f, 0.f, 0.f);

    CHECK(deck.IsDeck());
    CHECK_FALSE(shore.IsDeck());

    CHECK_FALSE(aboard.ShareFrame(ashore));
    CHECK(std::isinf(aboard.DistanceTo(ashore)));
    CHECK_FALSE(aboard.WithinDist(ashore, 1000000.f));
    CHECK_FALSE(aboard.HasInArc(ashore, Placement::TwoPi()));
    CHECK_FALSE(aboard.IsInBack(ashore, 1000000.f, Placement::Pi()));
}

TEST_CASE("Two men on the same deck measure against each other normally")
{
    // The other half of the rule, and the half a too-eager fail-closed breaks:
    // within one vessel, deck coordinates ARE the coordinates, so melee reach and
    // spell range work exactly as they do on a continent.
    const Frame deck = Frame::Deck(VESSEL_A);

    const Placement bosun = At(deck, 0.f, 0.f, 0.f);
    const Placement cook  = At(deck, 3.f, 4.f, 0.f);

    CHECK(bosun.ShareFrame(cook));
    CHECK(bosun.DistanceTo(cook) == doctest::Approx(5.f));
    CHECK(bosun.WithinDist(cook, 5.5f));
}

TEST_CASE("Two different vessels are two different frames")
{
    const Placement onA = At(Frame::Deck(VESSEL_A), 0.f, 0.f, 0.f);
    const Placement onB = At(Frame::Deck(VESSEL_B), 0.f, 0.f, 0.f);

    CHECK_FALSE(onA.ShareFrame(onB));
    CHECK(std::isinf(onA.DistanceTo(onB)));
    CHECK_FALSE(onA.WithinDist(onB, 1000000.f));
}

TEST_CASE("An unplaced placement shares a frame with nothing, including itself")
{
    // Nowhere is not a place. Two objects that have both left the world must not
    // come out adjacent because their frames happen to compare equal.
    const Placement nowhere;
    const Placement alsoNowhere;
    const Placement somewhere = At(Frame::World(MAP_EASTERN_KINGDOMS, NO_INSTANCE), 0.f, 0.f, 0.f);

    CHECK_FALSE(nowhere.IsPlaced());
    CHECK_FALSE(nowhere.ShareFrame(alsoNowhere));
    CHECK_FALSE(nowhere.ShareFrame(nowhere));
    CHECK_FALSE(nowhere.ShareFrame(somewhere));
    CHECK_FALSE(somewhere.ShareFrame(nowhere));

    CHECK(std::isinf(nowhere.DistanceTo(alsoNowhere)));
    CHECK_FALSE(nowhere.WithinDist(alsoNowhere, 1000000.f));
}

TEST_CASE("Leaving a frame makes a placement unreachable again")
{
    const Frame world = Frame::World(MAP_EASTERN_KINGDOMS, NO_INSTANCE);

    Placement a = At(world, 0.f, 0.f, 0.f);
    const Placement b = At(world, 1.f, 0.f, 0.f);
    REQUIRE(a.WithinDist(b, 2.f));

    a.LeaveFrame();

    CHECK_FALSE(a.IsPlaced());
    CHECK_FALSE(a.ShareFrame(b));
    CHECK(std::isinf(a.DistanceTo(b)));
}

TEST_CASE("Rebasing onto a deck moves an object out of the world's reach")
{
    // Boarding, in one call: the coordinates are read from elsewhere, and the
    // frame changes. Nothing about the shore's distances survives it.
    const Frame world = Frame::World(MAP_EASTERN_KINGDOMS, NO_INSTANCE);

    Placement sailor = At(world, 0.f, 0.f, 0.f);
    const Placement dockhand = At(world, 1.f, 0.f, 0.f);
    REQUIRE(sailor.WithinDist(dockhand, 2.f));

    sailor.Rebase(Frame::Deck(VESSEL_A));

    CHECK(sailor.CurrentFrame().IsDeck());
    CHECK_FALSE(sailor.ShareFrame(dockhand));
    CHECK(std::isinf(sailor.DistanceTo(dockhand)));
}

TEST_CASE("A candidate in another frame never wins a nearest-of comparison")
{
    // IsNearer picks between two candidates. Out-of-frame must lose whichever
    // side it is on, or a target-selection sweep starts preferring things on
    // other maps because their coordinates happen to be small.
    const Frame world = Frame::World(MAP_EASTERN_KINGDOMS, NO_INSTANCE);

    const Placement viewer = At(world, 0.f, 0.f, 0.f);
    const Placement near_  = At(world, 1.f, 0.f, 0.f);
    const Placement far_   = At(world, 50.f, 0.f, 0.f);
    const Placement other  = At(Frame::World(MAP_KALIMDOR, NO_INSTANCE), 0.f, 0.f, 0.f);

    CHECK(viewer.IsNearer(near_, far_));
    CHECK_FALSE(viewer.IsNearer(far_, near_));

    // `other` sits at the viewer's exact coordinates in another map. It must not
    // beat a real candidate 50 yards away.
    CHECK_FALSE(viewer.IsNearer(other, far_));
    CHECK(viewer.IsNearer(far_, other));
}

TEST_CASE("Arc is measured only inside a shared frame")
{
    const Frame world = Frame::World(MAP_EASTERN_KINGDOMS, NO_INSTANCE);

    // Facing +X, with the target straight ahead.
    const Placement viewer = At(world, 0.f, 0.f, 0.f, 0.f);
    const Placement ahead  = At(world, 10.f, 0.f, 0.f);
    const Placement behind = At(world, -10.f, 0.f, 0.f);

    CHECK(viewer.HasInArc(ahead, Placement::Pi()));
    CHECK_FALSE(viewer.HasInArc(behind, Placement::Pi()));

    CHECK(viewer.IsInFront(ahead, 20.f, Placement::Pi()));
    CHECK(viewer.IsInBack(behind, 20.f, Placement::Pi()));

    // And out of range, front and back are both false -- not "behind by default".
    CHECK_FALSE(viewer.IsInFront(ahead, 5.f, Placement::Pi()));
    CHECK_FALSE(viewer.IsInBack(behind, 5.f, Placement::Pi()));
}

TEST_CASE("Gap subtracts reach and never goes below zero")
{
    // Contact distance: overlapping bodies are at gap zero, not a negative one
    // that would read as "closer than touching" in a comparison.
    CHECK(Placement::Gap(10.f, 3.f) == doctest::Approx(7.f));
    CHECK(Placement::Gap(2.f, 3.f) == doctest::Approx(0.f));
    CHECK(Placement::Gap(0.f, 0.f) == doctest::Approx(0.f));
}

TEST_CASE("Distance between two placements is body to body, not centre to centre")
{
    // Two large creatures touch sooner than two points do, and DistanceTo already
    // says so: it returns the GAP between the bodies, with both extents taken
    // out. Reading it as a centre separation and subtracting the extents again at
    // the call site is how melee reach ends up short by the size of the target.
    const Frame world = Frame::World(MAP_EASTERN_KINGDOMS, NO_INSTANCE);

    Placement big(5.f);
    big.EnterFrame(world, Vector3(0.f, 0.f, 0.f), 0.f);

    Placement other(5.f);
    other.EnterFrame(world, Vector3(11.f, 0.f, 0.f), 0.f);

    CHECK(big.Extent() == doctest::Approx(5.f));

    // Centres 11 apart, 10 of that eaten by the two extents.
    CHECK(big.DistanceTo(other) == doctest::Approx(1.f));
    CHECK(big.WithinDist(other, 1.5f));
    CHECK_FALSE(big.WithinDist(other, 0.5f));
}

TEST_CASE("Overlapping bodies are at zero distance, never a negative one")
{
    // Gap() floors at zero, so two creatures standing inside each other compare
    // as touching rather than as "nearer than touching" -- which would sort ahead
    // of everything in a nearest-target sweep.
    const Frame world = Frame::World(MAP_EASTERN_KINGDOMS, NO_INSTANCE);

    Placement big(5.f);
    big.EnterFrame(world, Vector3(0.f, 0.f, 0.f), 0.f);

    Placement inside(5.f);
    inside.EnterFrame(world, Vector3(2.f, 0.f, 0.f), 0.f);

    CHECK(big.DistanceTo(inside) == doctest::Approx(0.f));
    CHECK(big.DistanceTo(inside) >= 0.f);
    CHECK(big.WithinDist(inside, 0.f));
}

TEST_CASE("Distance to a bare point takes out only the asker's own extent")
{
    // A point has no body, so only one extent comes off. Passing a creature's
    // position as a Vector3 instead of its Placement therefore measures something
    // different, and the overload that does it is easy to reach by accident.
    const Frame world = Frame::World(MAP_EASTERN_KINGDOMS, NO_INSTANCE);

    Placement big(5.f);
    big.EnterFrame(world, Vector3(0.f, 0.f, 0.f), 0.f);

    CHECK(big.DistanceTo(Vector3(11.f, 0.f, 0.f)) == doctest::Approx(6.f));
}
