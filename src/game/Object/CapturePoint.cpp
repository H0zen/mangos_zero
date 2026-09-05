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
    /// The band around the middle in which the tower belongs to nobody.
    float NeutralBand(uint32 neutralPercent) { return neutralPercent * 0.5f; }

    /// Whole percents are what the bar is read in; the fraction only accumulates.
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

bool CapturePoint::Arrived(ObjectGuid const& who)
{
    return m_standing.insert(who).second;
}

CaptureShift CapturePoint::Shift(Team pushing, GameObjectInfo const& info)
{
    float const band = NeutralBand(info.capturePoint.neutralPercent);

    CaptureShift shift;

    // One side has dragged the bar all the way to their end.
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

    // The bar has left the neutral band on one side, taking the tower with it.
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

    // The bar has been dragged back into the band, and the tower is nobody's again.
    else if (m_state != CaptureState::Neutral && m_slider >= CAPTURE_SLIDER_MIDDLE - band && m_slider <= CAPTURE_SLIDER_MIDDLE + band)
    {
        shift.eventId = pushing == ALLIANCE ? info.capturePoint.neutralEventID1 : info.capturePoint.neutralEventID2;
        m_state = CaptureState::Neutral;
    }

    // The other side still holds it, but the bar is moving away from them.
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
