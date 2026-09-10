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

#include <vector>
#include "MoveSplineFlag.h"
#include "Geometry/Vector3.h"

class Unit;

namespace Movement
{

    typedef std::vector<Vector3> PointsArray;

    union FacingInfo
    {

        struct
        {
            float x, y, z;
        } f;
        uint64  target;
        float   angle;

        FacingInfo(float o) : angle(o) {}

        FacingInfo(uint64 t) : target(t) {}

        FacingInfo() : target(0) {}
    };

    struct MoveSplineInitArgs
    {

        MoveSplineInitArgs(size_t path_capacity = 16) : path_Idx_offset(0),
            velocity(0.f), splineId(0), facing(), flags()
        {
            path.reserve(path_capacity);
        }

        PointsArray path;
        FacingInfo facing;
        MoveSplineFlag flags;
        int32 path_Idx_offset;
        float velocity;
        uint32 splineId;

        bool Validate(Unit* unit) const;
        private:

            bool _checkPathBounds() const;
    };
}
