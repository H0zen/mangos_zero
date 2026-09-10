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

enum CapturePointSliderValue
{
    CAPTURE_SLIDER_ALLIANCE         = 100,
    CAPTURE_SLIDER_HORDE            = 0,
    CAPTURE_SLIDER_MIDDLE           = 50
};

uint32 const CAPTURE_TICK = 5000;

enum class CaptureState
{
    Neutral,
    AllianceGaining,
    HordeGaining,
    AllianceContested,
    HordeContested,
    AllianceHolds,
    HordeHolds
};

struct CaptureShift
{
    uint32 eventId = 0;
    bool objectiveTaken = false;
};

class CapturePoint
{
    public:
        float Slider() const { return m_slider; }
        CaptureState State() const { return m_state; }

        void SliderAt(float value, uint32 neutralPercent);

        void SliderTowards(Team side, float delta);

        bool IsTickDue(uint32 elapsed);

        bool Arrived(ObjectGuid who);
        void Left(ObjectGuid who) { m_standing.erase(who); }
        GuidSet const& Standing() const { return m_standing; }
        bool IsDeserted() const { return m_standing.empty(); }
        void Desert() { m_standing.clear(); }

        CaptureShift Shift(Team pushing, GameObjectInfo const& info);

    private:
        uint32       m_tick = 0;
        float        m_slider = 0.0f;
        CaptureState m_state = CaptureState::Neutral;
        GuidSet      m_standing;
};
