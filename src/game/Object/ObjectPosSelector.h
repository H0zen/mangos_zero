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

#include "Platform/Define.h"
#include <map>

#include<map>

class Occupant;

enum UsedAreaSide { USED_POS_PLUS, USED_POS_MINUS };

inline UsedAreaSide operator ~(UsedAreaSide side)
{
    return side == USED_POS_PLUS ? USED_POS_MINUS : USED_POS_PLUS;
}

inline float SignOf(UsedAreaSide side)
{
    return side == USED_POS_PLUS ? 1.0f : -1.0f;
}

struct ObjectPosSelector
{
    struct OccupiedArea
    {
        OccupiedArea(float _angleOffset, Occupant const* obj) : angleOffset(_angleOffset), occupyingObj(obj) {}
        float angleOffset;
        Occupant const* occupyingObj;
    };

    typedef std::multimap<float, OccupiedArea> UsedAreaList;
    typedef UsedAreaList::value_type UsedArea;

    ObjectPosSelector(float x, float y, float dist, float searchedForSize, Occupant const* searchPosFor);

    void AddUsedArea(Occupant const* obj, float angle, float dist);

    bool CheckOriginalAngle() const;

    void InitializeAngle();

    bool NextAngle(float& angle);
    bool NextUsedAngle(float& angle);

    bool CheckAngle(UsedArea const& usedArea, UsedAreaSide side, float angle) const;
    void InitializeAngle(UsedAreaSide side);
    bool NextSideAngle(UsedAreaSide side, float& angle);

    float m_centerX;
    float m_centerY;
    float m_searcherDist;
    float m_searchedForReqHAngle;

    UsedAreaList m_UsedAreaLists[2];

    UsedAreaList::const_iterator m_nextUsedAreaItr[2];

    float m_stepAngle[2];

    Occupant const* m_searchPosFor;
};
