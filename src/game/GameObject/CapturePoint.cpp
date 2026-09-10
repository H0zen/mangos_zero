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

#include "CapturePoint.h"
#include "GameObject.h"

namespace
{

    float NeutralBand(uint32 neutralPercent) { return neutralPercent * 0.5f; }

    int WholePercent(float slider) { return static_cast<int>(slider); }
}

void CapturePoint::SliderAt(float value, uint32 neutralPercent)
{
    m_slider = value;

    float const band = NeutralBand(neutralPercent);

    if (WholePercent(m_slider) == CAPTURE_SLIDER_ALLIANCE)
    {
        m_state = CaptureState::AllianceHolds;
    }
    else if (WholePercent(m_slider) == CAPTURE_SLIDER_HORDE)
    {
        m_state = CaptureState::HordeHolds;
    }
    else if (m_slider > CAPTURE_SLIDER_MIDDLE + band)
    {
        m_state = CaptureState::AllianceGaining;
    }
    else if (m_slider < CAPTURE_SLIDER_MIDDLE - band)
    {
        m_state = CaptureState::HordeGaining;
    }
    else
    {
        m_state = CaptureState::Neutral;
    }
}

void CapturePoint::SliderTowards(Team side, float delta)
{
    if (side == ALLIANCE)
    {
        m_slider += delta;
        if (m_slider > CAPTURE_SLIDER_ALLIANCE)
        {
            m_slider = CAPTURE_SLIDER_ALLIANCE;
        }
    }
    else
    {
        m_slider -= delta;
        if (m_slider < CAPTURE_SLIDER_HORDE)
        {
            m_slider = CAPTURE_SLIDER_HORDE;
        }
    }
}

bool CapturePoint::IsTickDue(uint32 elapsed)
{
    m_tick += elapsed;
    if (m_tick < CAPTURE_TICK)
    {
        return false;
    }

    m_tick -= CAPTURE_TICK;
    return true;
}

bool CapturePoint::Arrived(ObjectGuid who)
{
    return m_standing.insert(who).second;
}

CaptureShift CapturePoint::Shift(Team pushing, GameObjectInfo const& info)
{
    float const band = NeutralBand(info.capturePoint.neutralPercent);

    CaptureShift shift;

    if (m_state != CaptureState::AllianceHolds && WholePercent(m_slider) == CAPTURE_SLIDER_ALLIANCE)
    {
        shift.eventId = info.capturePoint.winEventID1;
        m_state = CaptureState::AllianceHolds;
    }
    else if (m_state != CaptureState::HordeHolds && WholePercent(m_slider) == CAPTURE_SLIDER_HORDE)
    {
        shift.eventId = info.capturePoint.winEventID2;
        m_state = CaptureState::HordeHolds;
    }

    else if (m_state != CaptureState::AllianceGaining && m_slider > CAPTURE_SLIDER_MIDDLE + band && pushing == ALLIANCE)
    {
        shift.eventId = info.capturePoint.progressEventID1;
        shift.objectiveTaken = m_state == CaptureState::Neutral;
        m_state = CaptureState::AllianceGaining;
    }
    else if (m_state != CaptureState::HordeGaining && m_slider < CAPTURE_SLIDER_MIDDLE - band && pushing == HORDE)
    {
        shift.eventId = info.capturePoint.progressEventID2;
        shift.objectiveTaken = m_state == CaptureState::Neutral;
        m_state = CaptureState::HordeGaining;
    }

    else if (m_state != CaptureState::Neutral && m_slider >= CAPTURE_SLIDER_MIDDLE - band && m_slider <= CAPTURE_SLIDER_MIDDLE + band)
    {
        shift.eventId = pushing == ALLIANCE ? info.capturePoint.neutralEventID1 : info.capturePoint.neutralEventID2;
        m_state = CaptureState::Neutral;
    }

    else if ((m_state == CaptureState::HordeHolds || m_state == CaptureState::HordeGaining) && pushing == ALLIANCE)
    {
        shift.eventId = info.capturePoint.contestedEventID1;
        m_state = CaptureState::HordeContested;
    }
    else if ((m_state == CaptureState::AllianceHolds || m_state == CaptureState::AllianceGaining) && pushing == HORDE)
    {
        shift.eventId = info.capturePoint.contestedEventID2;
        m_state = CaptureState::AllianceContested;
    }

    return shift;
}
