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

// The loop a lift runs.
//
// The shape exercised here is the one a Deeprun tram car actually has in
// TransportAnimation.dbc: out along its own X axis, a dwell at the far end with
// the doors playing a different sequence, back again, and a dwell at the start.
// The last keyframe closes the loop, so its time is the period.

#include "doctest.h"

#include "LiftPath.h"

namespace
{
    TransportAnimationEntry Frame(uint32 when, float x, float z, uint32 sequence)
    {
        TransportAnimationEntry frame{};
        frame.TransportID = 176080;
        frame.TimeIndex = when;
        frame.PosX = x;
        frame.PosZ = z;
        frame.SequenceID = sequence;
        return frame;
    }

    /// Out to 100 over ten seconds, standing there for five, back by the thirtieth.
    struct Tram
    {
        TransportAnimationEntry start = Frame(0, 0.0f, 0.0f, 162);
        TransportAnimationEntry there = Frame(10000, 100.0f, -20.0f, 164);
        TransportAnimationEntry leave = Frame(15000, 100.0f, -20.0f, 162);
        TransportAnimationEntry home = Frame(30000, 0.0f, 0.0f, 164);

        TransportAnimation frames{&start, &there, &leave, &home};
    };
}

TEST_CASE("lift path: nothing to run means nothing moves")
{
    LiftPath const nowhere;

    CHECK(nowhere.IsEmpty());
    CHECK(nowhere.Period() == 0);
    CHECK(nowhere.OffsetAt(1234) == Geometry::Vector3());
    CHECK(nowhere.SequenceAt(1234) == 0);
}

TEST_CASE("lift path: the last keyframe closes the loop, so its time is the period")
{
    Tram tram;
    LiftPath const path(&tram.frames);

    CHECK_FALSE(path.IsEmpty());
    CHECK(path.Period() == 30000);
}

TEST_CASE("lift path: it stands exactly on its keyframes and slides between them")
{
    Tram tram;
    LiftPath const path(&tram.frames);

    CHECK(path.OffsetAt(0).x == doctest::Approx(0.0f));
    CHECK(path.OffsetAt(10000).x == doctest::Approx(100.0f));

    // Half way out is half way there, on every axis at once.
    Geometry::Vector3 const half = path.OffsetAt(5000);
    CHECK(half.x == doctest::Approx(50.0f));
    CHECK(half.z == doctest::Approx(-10.0f));

    // And a third of the way back from the far end.
    CHECK(path.OffsetAt(20000).x == doctest::Approx(100.0f - 100.0f / 3.0f));
}

TEST_CASE("lift path: a dwell is a pair of keyframes that do not move")
{
    Tram tram;
    LiftPath const path(&tram.frames);

    CHECK(path.OffsetAt(11000).x == doctest::Approx(100.0f));
    CHECK(path.OffsetAt(14999).x == doctest::Approx(100.0f));
}

TEST_CASE("lift path: the loop repeats, and asking past it asks inside it")
{
    Tram tram;
    LiftPath const path(&tram.frames);

    CHECK(path.OffsetAt(35000).x == doctest::Approx(path.OffsetAt(5000).x));
    CHECK(path.OffsetAt(30000).x == doctest::Approx(path.OffsetAt(0).x));

    // Three loops on and it is still in the same place.
    CHECK(path.OffsetAt(3 * 30000 + 5000).x == doctest::Approx(50.0f));
}

TEST_CASE("lift path: the animation in force is the last one that started")
{
    Tram tram;
    LiftPath const path(&tram.frames);

    CHECK(path.SequenceAt(0) == 162);
    CHECK(path.SequenceAt(9999) == 162);
    CHECK(path.SequenceAt(10000) == 164);                   // the doors, at the far end
    CHECK(path.SequenceAt(14999) == 164);
    CHECK(path.SequenceAt(15000) == 162);                   // moving again
    CHECK(path.SequenceAt(29999) == 162);
}

TEST_CASE("lift path: one keyframe is a platform that never leaves it")
{
    TransportAnimationEntry only = Frame(0, 7.0f, 3.0f, 5);
    TransportAnimation frames{&only};
    LiftPath const path(&frames);

    CHECK(path.Period() == 0);
    CHECK(path.OffsetAt(0).x == doctest::Approx(7.0f));
    CHECK(path.OffsetAt(999999).x == doctest::Approx(7.0f));
    CHECK(path.SequenceAt(999999) == 5);
}
