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

#include <cmath>
#include "Utilities/Errors.h"
#include <algorithm>
#include "Utilities/MathDefines.h"
#include "ObjectPosSelector.h"
#include "Occupant.h"

#define OCCUPY_POS_ANGLE_ATAN_FACTOR                      1.8f

ObjectPosSelector::ObjectPosSelector(float x, float y, float dist, float searchedForSize, Occupant const* searchPosFor)
    : m_centerX(x), m_centerY(y), m_searcherDist(dist), m_searchPosFor(searchPosFor)
{

    if (searchedForSize == 0.0f)
    {
        searchedForSize = DEFAULT_WORLD_OBJECT_SIZE;
    }

    if (m_searcherDist == 0.0f)
    {
        m_searcherDist = DEFAULT_WORLD_OBJECT_SIZE;
    }

    m_searchedForReqHAngle = atan(OCCUPY_POS_ANGLE_ATAN_FACTOR * searchedForSize / m_searcherDist);

    m_nextUsedAreaItr[USED_POS_PLUS]  = m_UsedAreaLists[USED_POS_PLUS].begin();
    m_nextUsedAreaItr[USED_POS_MINUS] = m_UsedAreaLists[USED_POS_MINUS].begin();
    m_stepAngle[USED_POS_PLUS]  = 0.0f;
    m_stepAngle[USED_POS_MINUS] = 0.0f;
}

void ObjectPosSelector::AddUsedArea(Occupant const* obj, float angle, float dist)
{
    MANGOS_ASSERT(obj);

    if (dist == 0.0f)
    {
        return;
    }

    float sr_angle = atan(OCCUPY_POS_ANGLE_ATAN_FACTOR * obj->Where().Extent() / dist);

    if (angle >= 0)
    {
        m_UsedAreaLists[USED_POS_PLUS].insert(UsedArea(angle, OccupiedArea(sr_angle, obj)));
    }
    else
    {
        m_UsedAreaLists[USED_POS_MINUS].insert(UsedArea(-angle, OccupiedArea(sr_angle, obj)));
    }
}

bool ObjectPosSelector::CheckAngle(UsedArea const& usedArea, UsedAreaSide side, float angle) const
{
    float used_angle = usedArea.first * SignOf(side);
    float used_offset = usedArea.second.angleOffset;

    return fabs(used_angle - angle) > used_offset || (m_searchPosFor && usedArea.second.occupyingObj == m_searchPosFor);
}

bool ObjectPosSelector::CheckOriginalAngle() const
{

    return (m_UsedAreaLists[USED_POS_PLUS].empty()  || CheckAngle(*m_UsedAreaLists[USED_POS_PLUS].begin(), USED_POS_PLUS, 0.0f)) &&
        (m_UsedAreaLists[USED_POS_MINUS].empty() || CheckAngle(*m_UsedAreaLists[USED_POS_MINUS].begin(), USED_POS_MINUS, 0.0f));
}

void ObjectPosSelector::InitializeAngle()
{
    InitializeAngle(USED_POS_PLUS);
    InitializeAngle(USED_POS_MINUS);
}

void ObjectPosSelector::InitializeAngle(UsedAreaSide side)
{
    m_nextUsedAreaItr[side] = m_UsedAreaLists[side].begin();

    if (!m_UsedAreaLists[~side].empty())
    {
        UsedArea const& otherArea = *m_UsedAreaLists[~side].begin();
        m_stepAngle[side] = std::max(m_searchedForReqHAngle + otherArea.second.angleOffset - otherArea.first, 0.0f);
    }
    else
    {
        m_stepAngle[side] = 0.0f;
    }

    m_stepAngle[side] -= m_searchedForReqHAngle;
}

bool ObjectPosSelector::NextAngle(float& angle)
{

    for (;;)
    {

        if (m_stepAngle[USED_POS_PLUS] < M_PI_F && m_stepAngle[USED_POS_PLUS] <= m_stepAngle[USED_POS_MINUS])
        {
            if (NextSideAngle(USED_POS_PLUS, angle))
            {
                return true;
            }
        }

        else if (m_stepAngle[USED_POS_MINUS] < M_PI_F)
        {
            if (NextSideAngle(USED_POS_MINUS, angle))
            {
                return true;
            }
        }

        else
        {
            break;
        }
    }

    return false;
}

bool ObjectPosSelector::NextSideAngle(UsedAreaSide side, float& angle)
{

    m_stepAngle[side] += (m_searchedForReqHAngle + 0.01);

    if (m_stepAngle[side] > M_PI_F)
    {
        return false;
    }

    if (m_nextUsedAreaItr[side] == m_UsedAreaLists[side].end())
    {
        angle = m_stepAngle[side] * SignOf(side);
        return true;
    }

    if ((m_searchPosFor && m_nextUsedAreaItr[side]->second.occupyingObj == m_searchPosFor) ||

        (m_stepAngle[side] + m_searchedForReqHAngle < m_nextUsedAreaItr[side]->first - m_nextUsedAreaItr[side]->second.angleOffset))
    {
        angle = m_stepAngle[side] * SignOf(side);
        return true;
    }

    m_stepAngle[side] = m_nextUsedAreaItr[side]->first + m_nextUsedAreaItr[side]->second.angleOffset;

    ++m_nextUsedAreaItr[side];

    return false;
}

bool ObjectPosSelector::NextUsedAngle(float& angle)
{
    if (m_nextUsedAreaItr[USED_POS_PLUS] == m_UsedAreaLists[USED_POS_PLUS].end() &&
        m_nextUsedAreaItr[USED_POS_MINUS] == m_UsedAreaLists[USED_POS_MINUS].end())
    {
        return false;
    }

    if (m_nextUsedAreaItr[USED_POS_PLUS] != m_UsedAreaLists[USED_POS_PLUS].end() &&
        (m_nextUsedAreaItr[USED_POS_MINUS] == m_UsedAreaLists[USED_POS_MINUS].end() ||
        m_nextUsedAreaItr[USED_POS_PLUS]->first <= m_nextUsedAreaItr[USED_POS_MINUS]->first))
    {
        angle = m_nextUsedAreaItr[USED_POS_PLUS]->first * SignOf(USED_POS_PLUS);
        ++m_nextUsedAreaItr[USED_POS_PLUS];
    }
    else
    {
        angle = m_nextUsedAreaItr[USED_POS_MINUS]->first * SignOf(USED_POS_MINUS);
        ++m_nextUsedAreaItr[USED_POS_MINUS];
    }

    return true;
}
