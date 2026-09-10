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

#include "MoveSplineInitArgs.h"
#include "PathFinder.h"

class Unit;

namespace Movement
{

    class MoveSplineInit
    {
        public:

            explicit MoveSplineInit(Unit& m);

            int32 Launch();

            void Stop();

            void SetFacing(float angle);

            void SetFacing(Vector3 const& point);

            void SetFacing(const Unit* target);

            void MovebyPath(const PointsArray& path, int32 pointId = 0);

            void MoveTo(const Vector3& destination, bool generatePath = false, bool forceDestination = false, float maxPathRange = 0.0f);

            void MoveTo(float x, float y, float z, bool generatePath = false, bool forceDestination = false, float maxPathRange = 0.0f);

            void SetFirstPointId(int32 pointId) { args.path_Idx_offset = pointId; }

            void SetFly();

            void SetWalk(bool enable);

            void SetCyclic();

            void SetFall();

            void SetVelocity(float velocity);

            PointsArray& Path()
            {
                return args.path;
            }
        protected:

            MoveSplineInitArgs args;
            Unit&  unit;
    };

    inline void MoveSplineInit::SetFly()
    {
        args.flags.flying = true;
    }

    inline void MoveSplineInit::SetWalk(bool enable) { args.flags.runmode = !enable;}

    inline void MoveSplineInit::SetCyclic()
    {
        args.flags.cyclic = true;
    }

    inline void MoveSplineInit::SetFall()
    {
        args.flags.falling = true;
    }

    inline void MoveSplineInit::SetVelocity(float vel) { args.velocity = vel;}

    inline void MoveSplineInit::MovebyPath(const PointsArray& controls, int32 path_offset)
    {
        args.path_Idx_offset = path_offset;
        args.path.assign(controls.begin(), controls.end());
    }

    inline void MoveSplineInit::MoveTo(float x, float y, float z, bool generatePath, bool forceDestination, float maxPathRange)
    {
        Vector3 v(x, y, z);
        MoveTo(v, generatePath, forceDestination, maxPathRange);
    }

    inline void MoveSplineInit::MoveTo(const Vector3& dest, bool generatePath, bool forceDestination, float maxPathRange)
    {
        if (generatePath)
        {
            PathFinder path(&unit);
            if (maxPathRange > 0.0f)
            {
                path.setPathLengthLimit(maxPathRange);
            }
            path.calculate(dest.x, dest.y, dest.z, forceDestination);
            if (!(path.getPathType() & PATHFIND_NOPATH))
            {
                MovebyPath(path.getPath());
                return;
            }
        }
        args.path_Idx_offset = 0;
        args.path.resize(2);
        args.path[1] = dest;
    }

    inline void MoveSplineInit::SetFacing(Vector3 const& spot)
    {
        args.facing.f.x = spot.x;
        args.facing.f.y = spot.y;
        args.facing.f.z = spot.z;
        args.flags.EnableFacingPoint();
    }
}
