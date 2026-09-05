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

#pragma once

#include "ObjectGuid.h"
#include "SharedDefines.h"

struct GameObjectInfo;

/// The two ends of the bar and the mark between them.
enum CapturePointSliderValue
{
    CAPTURE_SLIDER_ALLIANCE         = 100,                  // full alliance
    CAPTURE_SLIDER_HORDE            = 0,                    // full horde
    CAPTURE_SLIDER_MIDDLE           = 50                    // middle
};

/// How long the world waits between two pushes of the bar.
uint32 const CAPTURE_TICK = 5000;

/// What the tower shows, which is what the bar's position amounts to.
enum class CaptureState
{
    Neutral,                                                // the bar is in the middle band and the tower is nobody's
    AllianceGaining,
    HordeGaining,
    AllianceContested,                                      // alliance holds it, horde is pushing back
    HordeContested,
    AllianceHolds,
    HordeHolds
};

/// What a push of the bar amounts to.
struct CaptureShift
{
    uint32 eventId = 0;                                     // 0 when the push changes nothing worth announcing
    bool objectiveTaken = false;                            // a side has just taken the tower off neutral
};

/**
 * A tower that two sides take from each other by standing next to it.
 *
 * A bar runs from one side's end to the other. Every five seconds whichever
 * side has more players inside the ring drags it a little their way, faster the
 * more of them there are, and the tower announces whatever the bar's new
 * position amounts to: gaining, contested, neutral, or held outright.
 *
 * It keeps the ring's occupants because the bar is drawn on their screens and
 * nobody else's: they are told the value as they walk in, told each whole
 * percent it moves while they stand there, and told to drop it as they leave.
 */
class CapturePoint
{
    public:
        float Slider() const { return m_slider; }
        CaptureState State() const { return m_state; }

        /// The bar is put at this value and shows whatever that position means.
        void SliderAt(float value, uint32 neutralPercent);

        /// Drags the bar that far toward the given side, stopping at its end.
        void SliderTowards(Team side, float delta);

        /// @return true once every CAPTURE_TICK of world time.
        bool IsTickDue(uint32 elapsed);

        /// @return true when this one was not standing inside the ring before.
        bool Arrived(ObjectGuid const& who);
        void Left(ObjectGuid const& who) { m_standing.erase(who); }
        GuidSet const& Standing() const { return m_standing; }
        bool IsDeserted() const { return m_standing.empty(); }
        void Desert() { m_standing.clear(); }

        /// Reads the bar's new position and moves the tower to the state it calls for.
        CaptureShift Shift(Team pushing, GameObjectInfo const& info);

    private:
        uint32       m_tick = 0;
        float        m_slider = 0.0f;
        CaptureState m_state = CaptureState::Neutral;
        GuidSet      m_standing;
};
