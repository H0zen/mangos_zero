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

#include "VesselRoute.h"
#include "DBCStores.h"
#include "Common/TimeConstants.h"
#include "movement/spline.h"

#include <cmath>

namespace
{
    /// Every stretch is rounded to the nearest millisecond before it is added on.
    uint32 Millis(float seconds)
    {
        return seconds > 0.0f ? uint32(std::lround(double(seconds) * 1000.0)) : 0u;
    }

    /// What the vessel is doing over one stretch of water, and for how long.
    struct Profile
    {
        float speed;
        float accel;
        float toSpeed;                                      // seconds spent getting up to speed
        float runUp;                                        // water covered while doing it

        /// Leaving a berth, or coming into one: one acceleration is paid.
        float Once(float ds) const
        {
            return runUp >= ds ? std::sqrt(2.0f * ds / accel) : (ds - runUp) / speed + toSpeed;
        }

        /// Berth to berth: she leaves one and comes into the next, so it is paid twice.
        float Twice(float ds) const
        {
            return runUp >= ds * 0.5f ? 2.0f * std::sqrt(ds / accel)
                                      : (ds - 2.0f * runUp) / speed + 2.0f * toSpeed;
        }
    };

    /// One stretch of water the vessel sails without jumping, and where she berths on it.
    struct Leg
    {
        std::vector<Geometry::Vector3> points;
        /// Index of the node she berths at, and how long she stays, in milliseconds.
        std::vector<std::pair<uint32, uint32>> berths;
    };

    /**
     * How long one leg takes.
     *
     * The leg is a Catmull-Rom spline through its nodes, but the vessel does not
     * slow at every node: she runs berth to berth, so the stretch that a speed
     * profile is applied to is the water between one berth and the next. A leg
     * with no berth at all is crossed at a flat cruise, no acceleration at all.
     */
    uint32 SailTime(Leg const& leg, Profile const& how)
    {
        if (leg.points.size() < 2)
        {
            return 0;
        }

        Movement::SplineBase spline;
        spline.init_spline(leg.points.data(), Movement::SplineBase::index_type(leg.points.size()),
                           Movement::SplineBase::ModeCatmullrom);

        // How far along the leg each node stands.
        std::vector<float> reached(leg.points.size(), 0.0f);
        for (size_t i = 1; i < leg.points.size(); ++i)
        {
            reached[i] = reached[i - 1] + spline.SegLength(Movement::SplineBase::index_type(i));
        }

        uint32 total = 0;
        float behind = 0.0f;
        size_t sailed = 0;

        for (auto const& berth : leg.berths)
        {
            // A berth at the far end is the end of the leg, and the run in to it is
            // what is left over below.
            if (berth.first >= leg.points.size() - 1)
            {
                break;
            }

            float const reach = reached[berth.first];
            total += Millis(sailed == 0 ? how.Once(reach - behind) : how.Twice(reach - behind));
            behind = reach;
            ++sailed;
        }

        float const left = reached.back() - behind;
        total += Millis(sailed != 0 ? how.Once(left) : left / how.speed);

        return total;
    }
}

VesselRoute::VesselRoute(std::vector<TaxiPathNodeEntry const*> const& nodes, float speed, float accel)
{
    if (nodes.empty() || speed <= 0.0f || accel <= 0.0f)
    {
        return;
    }

    Profile const how{speed, accel, speed / accel, 0.5f * speed * (speed / accel)};

    std::vector<Leg> legs(1);
    uint32 lastMap = nodes.front()->ContinentID;
    bool jumped = false;

    for (TaxiPathNodeEntry const* node : nodes)
    {
        if (node->ContinentID != lastMap || jumped)
        {
            legs.emplace_back();
            lastMap = node->ContinentID;
        }

        Leg& leg = legs.back();

        // A berth on the first node of a leg is the one she has just left, so there is
        // no water behind it to have sailed and the client does not record it.
        if ((node->Flags & TAXI_NODE_STOP) && !leg.points.empty())
        {
            leg.berths.emplace_back(uint32(leg.points.size()), node->Delay * IN_MILLISECONDS);
        }

        leg.points.push_back(Geometry::Vector3(node->LocX, node->LocY, node->LocZ));
        jumped = (node->Flags & TAXI_NODE_TELEPORT) != 0;
    }

    // The client holds one berth list at a time and still has the last leg's when it
    // totals the lap, so it charges that leg's berth times once for every leg. On every
    // classic route the legs berth alike and it comes to the true sum.
    uint32 perLeg = 0;
    for (auto const& berth : legs.back().berths)
    {
        perLeg += berth.second;
    }

    m_legs = uint32(legs.size());
    m_waiting = perLeg * m_legs;
    m_period = m_waiting;

    for (Leg const& leg : legs)
    {
        m_period += SailTime(leg, how);
    }
}

VesselRoute VesselRoute::Along(uint32 pathId, float speed, float accel)
{
    std::vector<TaxiPathNodeEntry const*> nodes;

    if (pathId < sTaxiPathNodesByPath.size())
    {
        TaxiPathNodeList const& path = sTaxiPathNodesByPath[pathId];
        nodes.reserve(path.size());
        for (size_t i = 0; i < path.size(); ++i)
        {
            nodes.push_back(&static_cast<TaxiPathNodeEntry const&>(path[i]));
        }
    }

    return VesselRoute(nodes, speed, accel);
}
