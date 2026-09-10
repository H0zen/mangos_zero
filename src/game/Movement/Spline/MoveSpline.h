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

#include "Utilities/Errors.h"
#include <string>
#include "spline.h"
#include "MoveSplineInitArgs.h"

namespace Movement
{

    enum MonsterMoveType
    {
        MonsterMoveNormal = 0,
        MonsterMoveStop = 1,
        MonsterMoveFacingSpot = 2,
        MonsterMoveFacingTarget = 3,
        MonsterMoveFacingAngle = 4
    };

    struct Location : public Vector3
    {

        Location() : orientation(0) {}

        Location(float x, float y, float z, float o) : Vector3(x, y, z), orientation(o) {}

        Location(const Vector3& v) : Vector3(v), orientation(0) {}

        Location(const Vector3& v, float o) : Vector3(v), orientation(o) {}

        float orientation;
    };

    class MoveSpline
    {
        friend class PacketBuilder;

        public:

            typedef Spline<int32> MySpline;

            enum UpdateResult
            {
                Result_None         = 0x01,
                Result_Arrived      = 0x02,
                Result_NextCycle    = 0x04,
                Result_NextSegment  = 0x08,
            };

        protected:
            MySpline        spline;
            FacingInfo      facing;
            uint32          m_Id;
            MoveSplineFlag  splineflags;
            int32           time_passed;
            int32           point_Idx;
            int32           point_Idx_offset;

            void init_spline(const MoveSplineInitArgs& args);

        protected:

            const MySpline::ControlArray& getPath() const { return spline.getPoints();}

            void computeFallElevation(float& el) const;

            UpdateResult _updateState(int32& ms_time_diff);

            int32 next_timestamp() const { return spline.length(point_Idx + 1);}

            int32 segment_time_elapsed() const { return next_timestamp() - time_passed;}

            int32 timeElapsed() const { return Duration() - time_passed;}

            int32 timePassed() const { return time_passed;}

        public:

            const MySpline& _Spline() const { return spline;}

            int32 _currentSplineIdx() const { return point_Idx;}

            void _Finalize();

            void _Interrupt()
            {
                splineflags.done = true;
            }

        public:

            void Initialize(const MoveSplineInitArgs& args);

            bool Initialized() const { return !spline.empty();}

            explicit MoveSpline();

            template<class UpdateHandler>
                void updateState(int32 difftime, UpdateHandler& handler)
            {
                MANGOS_ASSERT(Initialized());
                do
                {
                    handler(_updateState(difftime));
                }
                while (difftime > 0);
            }

            void updateState(int32 difftime)
            {
                MANGOS_ASSERT(Initialized());
                do
                {
                    _updateState(difftime);
                }
                while (difftime > 0);
            }

            Location ComputePosition() const;

            uint32 GetId() const { return m_Id;}

            bool Finalized() const { return splineflags.done; }

            bool isCyclic() const { return splineflags.cyclic;}

            const Vector3 FinalDestination() const { return Initialized() ? spline.getPoint(spline.last()) : Vector3();}

            const Vector3 CurrentDestination() const { return Initialized() ? spline.getPoint(point_Idx + 1) : Vector3();}

            int32 currentPathIdx() const;

            int32 Duration() const { return spline.length();}

            std::string ToString() const;
    };
}
