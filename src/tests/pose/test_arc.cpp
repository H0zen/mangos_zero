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

// Arcs: facing tests, and what they do at the edges of the circle.
//
// HasInArc(other, arc) is |relative bearing| <= arc/2, with the arc first pushed
// through NormalizeOrientation into [0, 2*pi). That normalisation is where the
// surprises live, and they are pinned here rather than left to be discovered by
// a behind-only effect that never fires.

#include "doctest.h"

#include "Geometry/Placement.h"

#include <cmath>
#include <limits>

using Geometry::Frame;
using Geometry::Placement;
using Geometry::Vector3;

namespace
{
    const float NOT_A_NUMBER = std::numeric_limits<float>::quiet_NaN();
    const float INFINITE = std::numeric_limits<float>::infinity();

    Frame World() { return Frame::World(0, 0); }

    /// A viewer at the origin facing +X (orientation 0).
    Placement Viewer()
    {
        Placement p(0.f);
        p.EnterFrame(World(), Vector3(0.f, 0.f, 0.f), 0.f);
        return p;
    }

    /// A target ten yards away at `angle` measured from +X.
    Placement TargetAt(float angle)
    {
        Placement p(0.f);
        p.EnterFrame(World(),
                     Vector3(10.f * std::cos(angle), 10.f * std::sin(angle), 0.f), 0.f);
        return p;
    }
}

TEST_CASE("Relative bearing lands in (-pi, pi]")
{
    const Placement viewer = Viewer();

    CHECK(viewer.RelativeBearingTo(TargetAt(0.f)) == doctest::Approx(0.f));
    CHECK(viewer.RelativeBearingTo(TargetAt(Placement::Pi() / 2.f))
          == doctest::Approx(Placement::Pi() / 2.f));

    // Dead astern is the knife edge, and its SIGN is not determinate.
    //
    // SignedOrientation folds with `a > pi ? a - 2pi : a`, so whether a bearing of
    // half a turn lands a fraction of an ULP above or below pi decides between
    // +pi and -pi. Both name the same direction, and every arc test here compares
    // the bearing against +/-half, so the ambiguity cancels. A caller that reads
    // the SIGN as "to port or to starboard" gets an arbitrary answer at exactly
    // this bearing -- which is why what is asserted is the magnitude.
    CHECK(std::fabs(viewer.RelativeBearingTo(TargetAt(Placement::Pi())))
          == doctest::Approx(Placement::Pi()));

    // A target to starboard is a negative bearing.
    CHECK(viewer.RelativeBearingTo(TargetAt(-Placement::Pi() / 2.f))
          == doctest::Approx(-Placement::Pi() / 2.f));
}

TEST_CASE("A half-circle arc is the front half")
{
    const Placement viewer = Viewer();
    const float pi = Placement::Pi();

    CHECK(viewer.HasInArc(TargetAt(0.f), pi));              // dead ahead
    CHECK(viewer.HasInArc(TargetAt(pi / 2.f - 0.01f), pi)); // just inside port beam
    CHECK(viewer.HasInArc(TargetAt(-pi / 2.f + 0.01f), pi));

    CHECK_FALSE(viewer.HasInArc(TargetAt(pi / 2.f + 0.01f), pi));
    CHECK_FALSE(viewer.HasInArc(TargetAt(pi), pi));         // dead astern
}

TEST_CASE("Facing rotates the arc with the viewer")
{
    Placement viewer(0.f);
    viewer.EnterFrame(World(), Vector3(0.f, 0.f, 0.f), Placement::Pi());  // facing -X

    const float pi = Placement::Pi();

    CHECK(viewer.HasInArc(TargetAt(pi), pi));        // now dead ahead
    CHECK_FALSE(viewer.HasInArc(TargetAt(0.f), pi)); // now astern
}

TEST_CASE("A full-circle arc collapses to dead ahead only")
{
    // KNOWN DEFECT, pinned.
    //
    // NormalizeOrientation wraps into the HALF-OPEN interval [0, 2*pi), so 2*pi
    // itself comes back as 0. Half of that is 0, and the test becomes
    // `bearing >= 0 && bearing <= 0` -- true for a target exactly dead ahead and
    // false for every other. So HasInArc(target, TwoPi()), which reads as "is he
    // anywhere around me", actually asks "is he precisely in front of me".
    //
    // The same trap catches any arc at or above 2*pi: 3*pi wraps to pi and asks
    // for a 90-degree cone. Callers must pass an arc strictly inside (0, 2*pi).
    const Placement viewer = Viewer();
    const float twoPi = Placement::TwoPi();

    CHECK(Placement::NormalizeOrientation(twoPi) == doctest::Approx(0.f));

    CHECK(viewer.HasInArc(TargetAt(0.f), twoPi));              // dead ahead: yes
    CHECK_FALSE(viewer.HasInArc(TargetAt(0.5f), twoPi));       // anywhere else: no
    CHECK_FALSE(viewer.HasInArc(TargetAt(Placement::Pi()), twoPi));

    // An arc just under the full circle behaves as one would expect a full one to.
    const float almostAll = twoPi - 0.001f;
    CHECK(viewer.HasInArc(TargetAt(0.5f), almostAll));
    CHECK(viewer.HasInArc(TargetAt(3.f), almostAll));
}

TEST_CASE("An arc of zero is dead ahead only")
{
    const Placement viewer = Viewer();

    CHECK(viewer.HasInArc(TargetAt(0.f), 0.f));
    CHECK_FALSE(viewer.HasInArc(TargetAt(0.01f), 0.f));
}

TEST_CASE("IsInBack is the complement of the front arc, and covers the stern")
{
    const Placement viewer = Viewer();
    const float pi = Placement::Pi();

    // IsInBack(arc) is !HasInArc(2pi - arc), so an arc of pi is the rear half.
    CHECK(viewer.IsInBack(TargetAt(pi), 20.f, pi));            // dead astern
    CHECK(viewer.IsInBack(TargetAt(pi * 0.75f), 20.f, pi));
    CHECK_FALSE(viewer.IsInBack(TargetAt(0.f), 20.f, pi));     // dead ahead
    CHECK_FALSE(viewer.IsInBack(TargetAt(pi / 4.f), 20.f, pi));
}

TEST_CASE("Front and back both refuse a target out of range")
{
    // Distance is checked first in both, so out of reach is neither in front nor
    // behind. For IsInBack that ordering is what stops `!HasInArc` -- which is
    // true for anything it cannot measure -- from answering on its own.
    const Placement viewer = Viewer();
    const float pi = Placement::Pi();

    CHECK_FALSE(viewer.IsInFront(TargetAt(0.f), 5.f, pi));   // target is 10 away
    CHECK_FALSE(viewer.IsInBack(TargetAt(pi), 5.f, pi));
}

TEST_CASE("A NaN arc refuses rather than answering")
{
    // NormalizeOrientation now passes NaN through, so `half` is NaN and both
    // comparisons are false. Before the wrap() guard this path ran a NaN through
    // a float-to-int conversion, which is undefined.
    const Placement viewer = Viewer();

    CHECK(std::isnan(Placement::NormalizeOrientation(NOT_A_NUMBER)));
    CHECK_FALSE(viewer.HasInArc(TargetAt(0.f), NOT_A_NUMBER));
    CHECK_FALSE(viewer.HasInArc(TargetAt(Placement::Pi()), NOT_A_NUMBER));
    CHECK_FALSE(viewer.IsInFront(TargetAt(0.f), 20.f, NOT_A_NUMBER));
}

TEST_CASE("An infinite arc admits everything")
{
    // KNOWN INCONSISTENCY, pinned. An infinite arc survives normalisation, so
    // `half` is infinite and every bearing lies within it. Semantically an
    // unbounded arc containing everything is defensible; what makes it worth
    // stating is that it is the one corrupt input to these functions that fails
    // OPEN, where NaN and every out-of-frame case fail closed. Arcs come from
    // spell data and code constants rather than from the wire, so nothing reaches
    // it today.
    const Placement viewer = Viewer();

    CHECK(viewer.HasInArc(TargetAt(0.f), INFINITE));
    CHECK(viewer.HasInArc(TargetAt(Placement::Pi()), INFINITE));
}

TEST_CASE("A negative arc is folded into a positive one")
{
    // wrap() brings -pi up to +pi, so a caller passing a signed arc by mistake
    // gets a 90-degree cone rather than an empty one. Forgiving, and worth
    // knowing before someone reads a negative arc as "behind".
    const Placement viewer = Viewer();
    const float pi = Placement::Pi();

    CHECK(Placement::NormalizeOrientation(-pi) == doctest::Approx(pi));
    CHECK(viewer.HasInArc(TargetAt(0.f), -pi));
    CHECK_FALSE(viewer.HasInArc(TargetAt(pi), -pi));
}

TEST_CASE("Arc questions still fail closed across frames")
{
    // Stated with a finite arc and a target genuinely dead ahead, so the only
    // thing that can make these false is the frame check itself.
    const float pi = Placement::Pi();

    const Placement here = Viewer();

    Placement elsewhere(0.f);
    elsewhere.EnterFrame(Frame::World(1, 0), Vector3(10.f, 0.f, 0.f), 0.f);

    CHECK_FALSE(here.HasInArc(elsewhere, pi));
    CHECK_FALSE(here.IsInFront(elsewhere, 20.f, pi));
    CHECK_FALSE(here.IsInBack(elsewhere, 20.f, pi));

    // Same again for a deck, which is the case that matters in play.
    Placement aboard(0.f);
    aboard.EnterFrame(Frame::Deck(0x1FC0000000000001ull), Vector3(10.f, 0.f, 0.f), 0.f);

    CHECK_FALSE(here.HasInArc(aboard, pi));
    CHECK_FALSE(here.IsInFront(aboard, 20.f, pi));
    CHECK_FALSE(here.IsInBack(aboard, 20.f, pi));
}

TEST_CASE("SignedOrientation maps the circle onto (-pi, pi]")
{
    const float pi = Placement::Pi();
    const float twoPi = Placement::TwoPi();

    CHECK(Placement::SignedOrientation(0.f) == doctest::Approx(0.f));
    CHECK(Placement::SignedOrientation(pi / 2.f) == doctest::Approx(pi / 2.f));
    CHECK(Placement::SignedOrientation(pi) == doctest::Approx(pi));

    // Just past pi becomes just past -pi.
    CHECK(Placement::SignedOrientation(pi + 0.1f) == doctest::Approx(-pi + 0.1f));
    CHECK(Placement::SignedOrientation(twoPi - 0.1f) == doctest::Approx(-0.1f));
}
