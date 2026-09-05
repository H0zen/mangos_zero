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
    /// Every span is rounded to the nearest millisecond before it is added on.
    uint32 Millis(float seconds)
    {
        return seconds > 0.0f ? uint32(std::lround(double(seconds) * 1000.0)) : 0u;
    }

    /**
     * How long one leg takes: a Catmull-Rom spline through its nodes, crossed
     * span by span.
     *
     * @param toSpeed seconds of accelerating before the cruising speed is reached
     * @param runUp   ground covered while doing it
     */
    uint32 SailTime(std::vector<Geometry::Vector3> const& nodes, float speed, float accel,
                    float toSpeed, float runUp)
    {
        if (nodes.size() < 2)
        {
            return 0;
        }

        Movement::SplineBase spline;
        spline.init_spline(nodes.data(), Movement::SplineBase::index_type(nodes.size()),
                           Movement::SplineBase::ModeCatmullrom);

        uint32 total = 0;
        Movement::SplineBase::index_type const first = spline.first();

        for (Movement::SplineBase::index_type i = first; i < spline.last(); ++i)
        {
            float const ds = spline.SegLength(i);
            float seconds;

            if (i == first)
            {
                // Out of a standstill: reach the cruising speed if the span is long
                // enough for it, and otherwise spend the whole span still gaining.
                seconds = runUp <= ds ? toSpeed + (ds - runUp) / speed
                                      : std::sqrt(2.0f * ds / accel);
            }
            else
            {
                // Every span after it is entered and left at the same speed, so the
                // run-up is paid at both ends -- or, on a short span, the vessel is
                // still gaining at the halfway mark and brakes from there.
                seconds = runUp <= ds * 0.5f ? 2.0f * toSpeed + (ds - 2.0f * runUp) / speed
                                             : 2.0f * std::sqrt(ds / accel);
            }

            total += Millis(seconds);
        }

        return total;
    }
}

VesselRoute::VesselRoute(std::vector<TaxiPathNodeEntry const*> const& nodes, float speed, float accel)
{
    if (nodes.empty() || speed <= 0.0f || accel <= 0.0f)
    {
        return;
    }

    float const toSpeed = speed / accel;
    float const runUp = 0.5f * speed * toSpeed;

    std::vector<Geometry::Vector3> leg;
    uint32 lastMap = nodes.front()->ContinentID;
    bool jumped = false;

    for (TaxiPathNodeEntry const* node : nodes)
    {
        if (node->ContinentID != lastMap || jumped)
        {
            m_period += SailTime(leg, speed, accel, toSpeed, runUp);
            if (leg.size() >= 2)
            {
                ++m_legs;
            }
            leg.clear();
            lastMap = node->ContinentID;
        }

        if (node->Flags & TAXI_NODE_STOP)
        {
            m_waiting += node->Delay * IN_MILLISECONDS;
        }

        leg.push_back(Geometry::Vector3(node->LocX, node->LocY, node->LocZ));
        jumped = (node->Flags & TAXI_NODE_TELEPORT) != 0;
    }

    m_period += SailTime(leg, speed, accel, toSpeed, runUp);
    if (leg.size() >= 2)
    {
        ++m_legs;
    }

    m_period += m_waiting;
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
