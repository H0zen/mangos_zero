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
#include "ObjectGuid.h"
#include "Movement/Spline/MoveSplineInitArgs.h"

namespace Motion
{
    using Movement::PointsArray;
    using Movement::Vector3;

    struct Facing
    {
        enum class Mode : uint8
        {
            None,
            Angle,
            Target,
            Spot
        };

        Mode       mode = Mode::None;
        float      angle = 0.0f;
        ObjectGuid target = 0;
        Vector3    spot;

        static Facing ToAngle(float o)
        {
            Facing f;
            f.mode = Mode::Angle;
            f.angle = o;
            return f;
        }

        static Facing ToTarget(ObjectGuid guid)
        {
            Facing f;
            f.mode = Mode::Target;
            f.target = guid;
            return f;
        }

        static Facing ToSpot(Vector3 const& p)
        {
            Facing f;
            f.mode = Mode::Spot;
            f.spot = p;
            return f;
        }
    };

    enum MoveFlags : uint32
    {
        MOVE_NONE         = 0x00,
        MOVE_WALK         = 0x01,
        MOVE_FLY          = 0x02,
        MOVE_STRAIGHT     = 0x04,
        MOVE_FORCE_DEST   = 0x08,

        MOVE_REQUIRE_PATH = 0x10,

        MOVE_REQUIRE_ROUTE = 0x20
    };

    struct MoveIntent
    {
        enum class Act : uint8
        {
            Hold,
            Move,
            Done
        };

        Act     act = Act::Hold;
        Vector3 goal;
        Facing  facing;
        uint32  flags = MOVE_NONE;

        float pathLengthLimit = 0.0f;

        PointsArray const* path = nullptr;

        bool Has(MoveFlags f) const { return (flags & f) != 0; }

        static MoveIntent Hold(Facing f = {})
        {
            MoveIntent i;
            i.act = Act::Hold;
            i.facing = f;
            return i;
        }

        static MoveIntent Move(Vector3 const& to, uint32 moveFlags = MOVE_NONE, Facing f = {})
        {
            MoveIntent i;
            i.act = Act::Move;
            i.goal = to;
            i.flags = moveFlags;
            i.facing = f;
            return i;
        }

        static MoveIntent Done()
        {
            MoveIntent i;
            i.act = Act::Done;
            return i;
        }

        MoveIntent& Along(PointsArray const& points)
        {
            path = &points;
            return *this;
        }

        MoveIntent& WithinLength(float yards)
        {
            pathLengthLimit = yards;
            return *this;
        }
    };

    struct MoveStatus
    {
        bool    traveling = false;
        bool    arrived = false;
        bool    blocked = false;
        int32   pathIndex = 0;
        Vector3 legGoal;
    };
}
