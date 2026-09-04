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

// What the geometry does when handed a value that is not a number.
//
// The wire itself is guarded: VerifyMovementInfo runs every client position
// through IsValidMapCoord, which rejects a non-finite x, y, z or o. So none of
// this is reachable from a packet. It is reachable from arithmetic -- a spline
// that divides by a zero span, a summon offset off a degenerate direction -- and
// the behaviour on that path decides whether a bad number stops or spreads.
//
// The rule these pin: comparisons against NaN are false, so every predicate
// fails CLOSED on its own. That is the property to keep, and the reason a NaN
// must be allowed to stay a NaN rather than being turned into a plausible number
// somewhere in the middle.

#include "doctest.h"

#include "Geometry/GeometryMath.h"
#include "Geometry/Placement.h"

#include <cmath>
#include <limits>

using Geometry::Placement;
using Geometry::Vector3;

namespace
{
    const float NOT_A_NUMBER = std::numeric_limits<float>::quiet_NaN();
    const float INFINITE = std::numeric_limits<float>::infinity();

}

TEST_CASE("wrap leaves a non-finite value alone instead of converting it to int")
{
    // The conversion in iFloor() is undefined for anything outside int's range.
    // Guarding here keeps every caller of NormalizeOrientation off that path.
    CHECK(std::isnan(Geometry::wrap(NOT_A_NUMBER, 0.f, Placement::TwoPi())));
    CHECK(std::isinf(Geometry::wrap(INFINITE, 0.f, Placement::TwoPi())));
    CHECK(std::isinf(Geometry::wrap(-INFINITE, 0.f, Placement::TwoPi())));
}

TEST_CASE("wrap still wraps finite values exactly as before")
{
    // The guard must not have moved any real answer.
    const float twoPi = Placement::TwoPi();

    CHECK(Geometry::wrap(0.f, 0.f, twoPi) == doctest::Approx(0.f));
    CHECK(Geometry::wrap(1.f, 0.f, twoPi) == doctest::Approx(1.f));
    CHECK(Geometry::wrap(twoPi + 1.f, 0.f, twoPi) == doctest::Approx(1.f));
    CHECK(Geometry::wrap(-1.f, 0.f, twoPi) == doctest::Approx(twoPi - 1.f));
    CHECK(Geometry::wrap(3.f * twoPi + 2.f, 0.f, twoPi) == doctest::Approx(2.f));
}

TEST_CASE("A NaN orientation stays NaN rather than becoming a plausible facing")
{
    // Before the guard this came back 0.0f at -O2 -- due east, and indistinguishable
    // from a real answer. A NaN that survives makes every arc test below fail closed.
    Placement p(0.f);
    p.EnterFrame(0, 0, Vector3(0.f, 0.f, 0.f), NOT_A_NUMBER);

    CHECK(std::isnan(p.Facing()));
    CHECK_FALSE(p.IsFinite());
}

TEST_CASE("A placement with a NaN facing is in nobody's arc")
{
    Placement viewer(0.f);
    viewer.EnterFrame(0, 0, Vector3(0.f, 0.f, 0.f), NOT_A_NUMBER);

    Placement target(0.f);
    target.EnterFrame(0, 0, Vector3(10.f, 0.f, 0.f), 0.f);

    // Distance still works -- only the facing is corrupt.
    CHECK(viewer.DistanceTo(target) == doctest::Approx(10.f));

    // But every question that consults the facing refuses.
    CHECK_FALSE(viewer.HasInArc(target, Placement::TwoPi()));
    CHECK_FALSE(viewer.IsInFront(target, 20.f, Placement::Pi()));
}

TEST_CASE("A NaN position fails every reach test closed")
{
    // The predicates all reduce to a comparison against NaN, which is false.
    Placement here(0.f);
    here.EnterFrame(0, 0, Vector3(0.f, 0.f, 0.f), 0.f);

    Placement nowhere(0.f);
    nowhere.EnterFrame(0, 0, Vector3(NOT_A_NUMBER, NOT_A_NUMBER, NOT_A_NUMBER), 0.f);

    CHECK_FALSE(nowhere.IsFinite());

    CHECK_FALSE(here.WithinDist(nowhere, 1000000.f));
    CHECK_FALSE(nowhere.WithinDist(here, 1000000.f));
    CHECK_FALSE(here.WithinRange(nowhere, 0.f, 1000000.f));
    CHECK_FALSE(here.HasInArc(nowhere, Placement::TwoPi()));
    CHECK_FALSE(here.IsInFront(nowhere, 1000000.f, Placement::Pi()));
    CHECK_FALSE(here.IsInBack(nowhere, 1000000.f, Placement::Pi()));
}

TEST_CASE("Gap reports zero for a NaN separation, and DistanceTo inherits it")
{
    // KNOWN DEFECT, pinned here so it is visible rather than surprising.
    //
    // Gap() computes `separation - reach` and returns it only when > 0. With a
    // NaN separation that comparison is false, so the answer is 0.0f -- which
    // reads as "touching". DistanceTo() and HeightGapTo() are built on Gap, so
    // they report contact for a position that is not a position, while
    // WithinDist() on the same pair correctly says false.
    //
    // A caller written as `if (a.DistanceTo(b) < range)` therefore disagrees with
    // one written as `if (a.WithinDist(b, range))`. Changing Gap() reaches every
    // distance in the server, so it is reported rather than altered here.
    CHECK(Placement::Gap(NOT_A_NUMBER, 5.f) == doctest::Approx(0.f));

    Placement here(0.f);
    here.EnterFrame(0, 0, Vector3(0.f, 0.f, 0.f), 0.f);

    Placement nowhere(0.f);
    nowhere.EnterFrame(0, 0, Vector3(NOT_A_NUMBER, 0.f, 0.f), 0.f);

    CHECK(here.DistanceTo(nowhere) == doctest::Approx(0.f));  // says "touching"
    CHECK_FALSE(here.WithinDist(nowhere, 1.f));               // says "not in reach"
}

TEST_CASE("Gap passes an infinite separation through")
{
    // Unlike NaN, an infinite separation survives -- which is what lets
    // DistanceTo() return Unreachable() across frames and have it mean something.
    CHECK(std::isinf(Placement::Gap(INFINITE, 5.f)));
    CHECK(Placement::Gap(INFINITE, 5.f) == Placement::Unreachable());
}

TEST_CASE("A target standing exactly on you is reported dead ahead")
{
    // atan2(0, 0) is 0 by definition, so a co-located target has bearing zero and
    // sits in any arc. The consequence worth knowing: IsInBack is never true at
    // zero separation, so a behind-only effect cannot fire on something perfectly
    // on top of its caster.
    Placement viewer(0.f);
    viewer.EnterFrame(0, 0, Vector3(0.f, 0.f, 0.f), 0.f);

    Placement onTop(0.f);
    onTop.EnterFrame(0, 0, Vector3(0.f, 0.f, 0.f), 0.f);

    CHECK(viewer.BearingTo(onTop) == doctest::Approx(0.f));
    CHECK(viewer.HasInArc(onTop, Placement::Pi()));
    CHECK_FALSE(viewer.IsInBack(onTop, 5.f, Placement::Pi()));
}

TEST_CASE("BearingTo across frames answers zero, unlike its fail-closed siblings")
{
    // KNOWN INCONSISTENCY, pinned. DistanceTo and HeightGapTo return infinity
    // across a frame boundary; BearingTo returns 0.0f, which is a real bearing
    // meaning "dead ahead". Nothing is wrong today because HasInArc checks
    // ShareFrame before it asks, but a new caller reaching for BearingTo directly
    // gets a plausible answer about an object on another map.
    Placement here(0.f);
    here.EnterFrame(0, 0, Vector3(0.f, 0.f, 0.f), 0.f);

    Placement elsewhere(0.f);
    elsewhere.EnterFrame(1, 0, Vector3(500.f, 500.f, 0.f), 0.f);

    CHECK(here.BearingTo(elsewhere) == doctest::Approx(0.f));
    CHECK(std::isinf(here.DistanceTo(elsewhere)));

    // The guard that currently saves it.
    CHECK_FALSE(here.HasInArc(elsewhere, Placement::TwoPi()));
}
