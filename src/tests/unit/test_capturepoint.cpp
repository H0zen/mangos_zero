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

// The bar on a contested tower.
//
// Everything the tower announces follows from where the bar stands and which
// side pushed it there, so that is what these exercise: the position the state
// is read off, the ends it cannot pass, and the fact that a push which moves
// the bar without moving it out of its band announces nothing at all.

#include "doctest.h"

#include "CapturePoint.h"
#include "GameObject.h"

namespace
{
    /// Twenty percent neutral, so the tower belongs to nobody between 40 and 60.
    GameObjectInfo Tower()
    {
        GameObjectInfo info{};
        info.type = GAMEOBJECT_TYPE_CAPTURE_POINT;
        info.capturePoint.neutralPercent = 20;
        info.capturePoint.winEventID1 = 41;
        info.capturePoint.winEventID2 = 42;
        info.capturePoint.contestedEventID1 = 71;
        info.capturePoint.contestedEventID2 = 72;
        info.capturePoint.progressEventID1 = 11;
        info.capturePoint.progressEventID2 = 12;
        info.capturePoint.neutralEventID1 = 61;
        info.capturePoint.neutralEventID2 = 62;
        return info;
    }
}

TEST_CASE("capture point: the state is read off the bar's position")
{
    CapturePoint point;

    point.SliderAt(CAPTURE_SLIDER_MIDDLE, 20);
    CHECK(point.State() == CaptureState::Neutral);

    // The edges of the neutral band belong to it; only past them does a side gain.
    point.SliderAt(60.0f, 20);
    CHECK(point.State() == CaptureState::Neutral);
    point.SliderAt(40.0f, 20);
    CHECK(point.State() == CaptureState::Neutral);

    point.SliderAt(60.5f, 20);
    CHECK(point.State() == CaptureState::AllianceGaining);
    point.SliderAt(39.5f, 20);
    CHECK(point.State() == CaptureState::HordeGaining);

    point.SliderAt(CAPTURE_SLIDER_ALLIANCE, 20);
    CHECK(point.State() == CaptureState::AllianceHolds);
    point.SliderAt(CAPTURE_SLIDER_HORDE, 20);
    CHECK(point.State() == CaptureState::HordeHolds);

    // A hair short of the end is not the end: the bar is read in whole percents.
    point.SliderAt(99.9f, 20);
    CHECK(point.State() == CaptureState::AllianceGaining);
}

TEST_CASE("capture point: the bar stops at both ends")
{
    CapturePoint point;

    point.SliderAt(95.0f, 20);
    point.SliderTowards(ALLIANCE, 10.0f);
    CHECK(point.Slider() == doctest::Approx(CAPTURE_SLIDER_ALLIANCE));

    point.SliderAt(5.0f, 20);
    point.SliderTowards(HORDE, 10.0f);
    CHECK(point.Slider() == doctest::Approx(CAPTURE_SLIDER_HORDE));

    // Anywhere else it simply moves, and the fraction is kept.
    point.SliderAt(CAPTURE_SLIDER_MIDDLE, 20);
    point.SliderTowards(HORDE, 2.5f);
    CHECK(point.Slider() == doctest::Approx(47.5f));
}

TEST_CASE("capture point: the tick fires on the five second mark and keeps the rest")
{
    CapturePoint point;

    CHECK_FALSE(point.IsTickDue(3000));
    CHECK(point.IsTickDue(3000));                           // 6000: one tick, 1000 left over

    CHECK_FALSE(point.IsTickDue(3000));                     // 4000
    CHECK(point.IsTickDue(1000));                           // exactly on the mark
}

TEST_CASE("capture point: it knows who walked in and who walked out")
{
    CapturePoint point;

    ObjectGuid const one(HIGHGUID_PLAYER, uint32(1));
    ObjectGuid const two(HIGHGUID_PLAYER, uint32(2));

    CHECK(point.IsDeserted());

    CHECK(point.Arrived(one));
    CHECK_FALSE(point.Arrived(one));                        // still the same one standing there
    CHECK(point.Arrived(two));
    CHECK(point.Standing().size() == 2);

    point.Left(one);
    CHECK_FALSE(point.IsDeserted());
    point.Desert();
    CHECK(point.IsDeserted());
}

TEST_CASE("capture point: a push announces only what it changes")
{
    GameObjectInfo const info = Tower();
    CapturePoint point;

    point.SliderAt(CAPTURE_SLIDER_MIDDLE, info.capturePoint.neutralPercent);

    // Out of the neutral band: alliance gains it, which completes the objective.
    point.SliderTowards(ALLIANCE, 15.0f);
    CaptureShift shift = point.Shift(ALLIANCE, info);
    CHECK(shift.eventId == info.capturePoint.progressEventID1);
    CHECK(shift.objectiveTaken);
    CHECK(point.State() == CaptureState::AllianceGaining);

    // Pushing further in the same direction is not news until an end is reached.
    point.SliderTowards(ALLIANCE, 5.0f);
    shift = point.Shift(ALLIANCE, info);
    CHECK(shift.eventId == 0);
    CHECK_FALSE(shift.objectiveTaken);
    CHECK(point.State() == CaptureState::AllianceGaining);

    // All the way to the end: the tower is held outright.
    point.SliderTowards(ALLIANCE, 40.0f);
    shift = point.Shift(ALLIANCE, info);
    CHECK(shift.eventId == info.capturePoint.winEventID1);
    CHECK_FALSE(shift.objectiveTaken);
    CHECK(point.State() == CaptureState::AllianceHolds);

    // Horde pushes back against a held tower: contested, and nothing is taken.
    point.SliderTowards(HORDE, 5.0f);
    shift = point.Shift(HORDE, info);
    CHECK(shift.eventId == info.capturePoint.contestedEventID2);
    CHECK_FALSE(shift.objectiveTaken);
    CHECK(point.State() == CaptureState::AllianceContested);

    // Back inside the band: nobody's again, and taking it later counts anew.
    point.SliderTowards(HORDE, 40.0f);
    shift = point.Shift(HORDE, info);
    CHECK(shift.eventId == info.capturePoint.neutralEventID2);
    CHECK(point.State() == CaptureState::Neutral);

    // The far edge of the band is still the band, so it takes a push past it.
    point.SliderTowards(HORDE, 15.0f);
    CHECK(point.Slider() == doctest::Approx(40.0f));
    CHECK(point.Shift(HORDE, info).eventId == 0);
    CHECK(point.State() == CaptureState::Neutral);

    point.SliderTowards(HORDE, 5.0f);
    shift = point.Shift(HORDE, info);
    CHECK(shift.eventId == info.capturePoint.progressEventID2);
    CHECK(shift.objectiveTaken);
    CHECK(point.State() == CaptureState::HordeGaining);
}
