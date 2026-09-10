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
#include "Geometry/Vector3.h"

#include <set>
#include <vector>

uint32 const TAXI_NODE_TELEPORT = 0x01;

uint32 const TAXI_NODE_STOP     = 0x02;

struct VesselLeg
{

    struct Run
    {
        uint32 startsAt = 0;
        uint32 sails = 0;
        uint32 waits = 0;
        float from = 0.0f;
        float to = 0.0f;

        uint32 ramps = 0;
    };

    uint32 mapId = 0;
    uint32 startsAt = 0;
    uint32 endsAt = 0;

    std::vector<Geometry::Vector3> nodes;

    std::vector<float> reached;
    std::vector<Run> runs;

    Geometry::Vector3 From() const;
};

struct VesselPose
{
    bool known = false;
    uint32 mapId = 0;
    Geometry::Vector3 at;
};

class VesselRoute
{
    public:
        VesselRoute() {}
        VesselRoute(std::vector<TaxiPathNodeEntry const*> const& nodes, float speed, float accel);

        static VesselRoute Along(uint32 pathId, float speed, float accel);

        uint32 Period() const { return m_period; }

        uint32 Waiting() const { return m_waiting; }

        std::vector<VesselLeg> const& Legs() const { return m_legs; }

        std::set<uint32> Maps() const;

        VesselLeg const* LegAt(uint32 phaseMs) const;

        VesselPose PoseAt(uint32 phaseMs) const;

    private:
        uint32 m_period = 0;
        uint32 m_waiting = 0;
        float m_speed = 0.0f;
        float m_accel = 0.0f;
        std::vector<VesselLeg> m_legs;
};
