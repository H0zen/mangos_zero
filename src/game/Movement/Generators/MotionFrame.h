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
#include "Utilities/MathDefines.h"
#include "Movement/Spline/MoveSplineInitArgs.h"

#include <cmath>
#include <memory>
#include <optional>

class Unit;
class Occupant;

namespace Motion
{
    using Movement::PointsArray;
    using Movement::Vector3;

    enum class FrameKind : uint8
    {
        World,
        Transport
    };

    inline float AngleBetween(Vector3 const& from, Vector3 const& to)
    {
        const float a = std::atan2(to.y - from.y, to.x - from.x);
        return (a >= 0.0f) ? a : (2 * M_PI_F + a);
    }

    class IPathQuery
    {
        public:
            virtual ~IPathQuery() = default;

            virtual bool Calculate(Vector3 const& start, Vector3 const& goal,
                                   bool forceDestination, float lengthLimit) = 0;

            virtual PointsArray const& Points() const = 0;

            virtual bool Failed() const = 0;

            virtual bool Routed() const = 0;

            virtual bool Reachable() const = 0;
    };

    class IMotionFrame
    {
        public:
            virtual ~IMotionFrame() = default;

            virtual FrameKind Kind() const = 0;

            virtual std::unique_ptr<IPathQuery> CreatePathQuery(Unit const& mover) const = 0;

            virtual Vector3 MoverPosition(Unit const& mover) const = 0;

            virtual Vector3 FromWorld(Unit const& mover, Vector3 const& world) const = 0;

            virtual Vector3 ObjectPosition(Unit const& mover, Occupant const& obj) const = 0;

            virtual float ObjectOrientation(Unit const& mover, Occupant const& obj) const = 0;

            virtual Vector3 NearPoint(Unit const& mover, Occupant const& target,
                                      float searcherBounding, float distance2d,
                                      float absAngle) const = 0;

            virtual std::optional<Vector3> RandomPoint(Unit& mover, Vector3 const& centre,
                                                       float radius) const = 0;

            virtual std::optional<Vector3> GroundPoint(Unit& mover, Vector3 const& from,
                                                       Vector3 const& guess) const = 0;
    };

    IMotionFrame const& FrameFor(Unit const& mover);
}
