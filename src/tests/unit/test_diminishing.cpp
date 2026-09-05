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

// The control-effect history, exercised on values.
//
// The clock is an argument, so "fifteen seconds later" is a number here rather
// than a wait, and no unit, map or server is involved in any of it. That is the
// point of pulling this out of Unit: the whole component is reachable from a
// test that constructs one object.

#include "doctest.h"

#include "Unit/Auras/Diminishing.h"

using unit::Diminishing;
using unit::Fade;

namespace
{
    constexpr DiminishingGroup GROUP = DIMINISHING_CONTROL_STUN;
    constexpr DiminishingGroup OTHER = DIMINISHING_FEAR;
    constexpr uint32 T0 = 100000;
}

TEST_CASE("A group nobody has touched is at full strength")
{
    Diminishing d;

    CHECK(d.Empty());
    CHECK(d.FadeOf(GROUP, T0) == Fade::Full);
}

TEST_CASE("Each landing fades the group one step, and it stops at immune")
{
    Diminishing d;

    d.RecordHit(GROUP, T0);
    CHECK(d.FadeOf(GROUP, T0) == Fade::Half);

    d.RecordHit(GROUP, T0);
    CHECK(d.FadeOf(GROUP, T0) == Fade::Quarter);

    d.RecordHit(GROUP, T0);
    CHECK(d.FadeOf(GROUP, T0) == Fade::Immune);

    // Further hits cannot make it worse than immune.
    d.RecordHit(GROUP, T0);
    d.RecordHit(GROUP, T0);
    CHECK(d.FadeOf(GROUP, T0) == Fade::Immune);
}

TEST_CASE("Groups fade independently")
{
    Diminishing d;

    d.RecordHit(GROUP, T0);
    d.RecordHit(GROUP, T0);

    CHECK(d.FadeOf(GROUP, T0) == Fade::Quarter);
    CHECK(d.FadeOf(OTHER, T0) == Fade::Full);
}

TEST_CASE("The history is forgotten after the quiet window, not before")
{
    Diminishing d;

    d.RecordHit(GROUP, T0);
    d.Hold(GROUP);
    d.Release(GROUP, T0);

    const uint32 window = Diminishing::RESET_WINDOW_MS;

    CHECK(d.FadeOf(GROUP, T0 + window) == Fade::Half);
    CHECK(d.FadeOf(GROUP, T0 + window + 1) == Fade::Full);
}

TEST_CASE("An aura still held keeps the history alive however long it lasts")
{
    // The window starts when the last one comes off. A stun that has been on
    // the victim for a minute has not been quiet for a second.
    Diminishing d;

    d.RecordHit(GROUP, T0);
    d.Hold(GROUP);

    CHECK(d.FadeOf(GROUP, T0 + 10 * Diminishing::RESET_WINDOW_MS) == Fade::Half);

    d.Release(GROUP, T0 + 10 * Diminishing::RESET_WINDOW_MS);
    CHECK(d.FadeOf(GROUP, T0 + 10 * Diminishing::RESET_WINDOW_MS + 1) == Fade::Half);
}

TEST_CASE("Two auras of a group have to both come off before the clock starts")
{
    Diminishing d;

    d.RecordHit(GROUP, T0);
    d.Hold(GROUP);
    d.Hold(GROUP);
    d.Release(GROUP, T0);

    const uint32 late = T0 + Diminishing::RESET_WINDOW_MS + 1;
    CHECK(d.FadeOf(GROUP, late) == Fade::Half);

    d.Release(GROUP, late);
    CHECK(d.FadeOf(GROUP, late + Diminishing::RESET_WINDOW_MS + 1) == Fade::Full);
}

TEST_CASE("Releasing what was never held changes nothing")
{
    Diminishing d;

    d.RecordHit(GROUP, T0);
    d.Release(GROUP, T0);
    d.Release(GROUP, T0);

    CHECK(d.FadeOf(GROUP, T0) == Fade::Half);
}

TEST_CASE("Clearing forgets every group")
{
    Diminishing d;

    d.RecordHit(GROUP, T0);
    d.RecordHit(OTHER, T0);
    d.Clear();

    CHECK(d.Empty());
    CHECK(d.FadeOf(GROUP, T0) == Fade::Full);
    CHECK(d.FadeOf(OTHER, T0) == Fade::Full);
}

TEST_CASE("Shorten states the penalty and nothing else")
{
    CHECK(Diminishing::Shorten(8000, Fade::Full) == 8000);
    CHECK(Diminishing::Shorten(8000, Fade::Half) == 4000);
    CHECK(Diminishing::Shorten(8000, Fade::Quarter) == 2000);
    CHECK(Diminishing::Shorten(8000, Fade::Immune) == 0);
}

TEST_CASE("A permanent duration is not shortened")
{
    // -1 means it lasts until something takes it off, and a quarter of that is
    // not a smaller duration -- it is a different meaning.
    CHECK(Diminishing::Shorten(-1, Fade::Quarter) == -1);
    CHECK(Diminishing::Shorten(-1, Fade::Immune) == -1);
}

TEST_CASE("The clock wrapping does not resurrect a spent history")
{
    // The game clock is a rolling millisecond counter, so a release recorded
    // just before it wraps must still read as recent just after.
    Diminishing d;

    const uint32 nearMax = 0xFFFFFFFFu - 1000;

    d.RecordHit(GROUP, nearMax);
    d.Hold(GROUP);
    d.Release(GROUP, nearMax);

    CHECK(d.FadeOf(GROUP, 500) == Fade::Half);       // 1500 ms later, wrapped
    CHECK(d.FadeOf(GROUP, 20000) == Fade::Full);     // past the window
}
