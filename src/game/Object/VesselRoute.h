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

#include "DBCStructure.h"

#include <vector>

/// A taxi node the vessel arrives at and then jumps from, so the run of nodes
/// before it and the run after it are two separate stretches of open water.
uint32 const TAXI_NODE_TELEPORT = 0x01;
/// A taxi node the vessel waits at, for the node's own delay.
uint32 const TAXI_NODE_STOP     = 0x02;

/**
 * How long a vessel takes to sail one lap of her taxi path.
 *
 * The route is a run of nodes that breaks into legs wherever the vessel jumps:
 * at a change of map, and at a node flagged as a teleport, which happens within
 * one map as well. Each leg is a Catmull-Rom spline through its nodes, of which
 * she sails all but the outermost two: a leg of n nodes has n-3 spans, and the
 * first node and the last only lend the curve its tangents.
 *
 * She does not slow at every node. What a speed profile is applied to is the
 * water between one berth and the next: she pulls away from a berth, runs, and
 * comes into the following one, so a middle stretch pays for accelerating twice
 * and the two end stretches once each. A stretch too short to reach her
 * cruising speed gets a triangular profile that never does. A leg with no berth
 * on it at all is crossed at a flat cruise.
 *
 * The lap is the sum of those crossings plus the time spent berthed, and it is
 * the number the client divides the clock by to decide where to draw the hull.
 * It is computed, not configured: the client computes it too, from the same DBC
 * rows and the same moveSpeed and accelRate we send it, and a lap the two
 * disagree on puts the hull somewhere the server does not believe it is.
 */
class VesselRoute
{
    public:
        VesselRoute(std::vector<TaxiPathNodeEntry const*> const& nodes, float speed, float accel);

        /// The taxi path of that id, sailed at that speed.
        static VesselRoute Along(uint32 pathId, float speed, float accel);

        /// One full lap, in milliseconds; 0 when there is nothing to sail.
        uint32 Period() const { return m_period; }

        /// How much of the lap is spent standing at the stops.
        uint32 Waiting() const { return m_waiting; }

        /// How many stretches of open water the jumps break the lap into.
        uint32 Legs() const { return m_legs; }

    private:
        uint32 m_period = 0;
        uint32 m_waiting = 0;
        uint32 m_legs = 0;
};
