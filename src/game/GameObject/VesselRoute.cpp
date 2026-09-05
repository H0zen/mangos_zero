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

#include <cmath>

namespace
{
    /// Chords per span. The client's own figure, and a length is only ever as
    /// right as the number of chords it was summed from.
    uint32 const CHORDS_PER_SPAN = 20;

    /**
     * A point on one span of a Catmull-Rom spline.
     *
     * The span is drawn over four nodes and runs between the middle two, so a
     * leg of n nodes carries n-3 spans and its outermost node at each end only
     * lends the curve its tangent.
     */
    Geometry::Vector3 OnSpan(Geometry::Vector3 const* p, float t)
    {
        float const w0 = ((-0.5f * t + 1.0f) * t - 0.5f) * t;
        float const w1 = ((1.5f * t - 2.5f) * t) * t + 1.0f;
        float const w2 = ((-1.5f * t + 2.0f) * t + 0.5f) * t;
        float const w3 = ((0.5f * t - 0.5f) * t) * t;

        return p[0] * w0 + p[1] * w1 + p[2] * w2 + p[3] * w3;
    }

    /// The span's length, as the client measures it: the sum of its chords.
    float SpanLength(Geometry::Vector3 const* p)
    {
        Geometry::Vector3 last = p[1];
        float total = 0.0f;

        for (uint32 i = 1; i <= CHORDS_PER_SPAN; ++i)
        {
            Geometry::Vector3 const next = OnSpan(p, float(i) / float(CHORDS_PER_SPAN));
            total += (next - last).magnitude();
            last = next;
        }

        return total;
    }

    /// What the vessel is doing over one run of water, and for how long.
    struct Profile
    {
        float speed = 0.0f;
        float accel = 0.0f;
        float toSpeed = 0.0f;                               // seconds spent getting up to speed
        float runUp = 0.0f;                                 // water covered while doing it

        /// Pulling away from a berth, or coming into one: one change of speed.
        float Once(float ds) const
        {
            return runUp >= ds ? std::sqrt(2.0f * ds / accel) : (ds - runUp) / speed + toSpeed;
        }

        /// Berth to berth: she leaves one and brakes into the next, so it is paid twice.
        float Twice(float ds) const
        {
            return runUp >= ds * 0.5f ? 2.0f * std::sqrt(ds / accel)
                                      : (ds - 2.0f * runUp) / speed + 2.0f * toSpeed;
        }

        /// How far into a run of that length and shape she is after that long.
        float Travelled(float ds, uint32 ramps, float seconds) const
        {
            if (seconds <= 0.0f)
            {
                return 0.0f;
            }

            if (ramps == 0)
            {
                return std::min(ds, speed * seconds);
            }

            if (ramps == 1)
            {
                // She pulls away, and cruises once she is up to speed -- unless the run
                // is too short for her ever to get there.
                if (runUp >= ds)
                {
                    return std::min(ds, 0.5f * accel * seconds * seconds);
                }

                return seconds <= toSpeed ? 0.5f * accel * seconds * seconds
                                          : std::min(ds, runUp + speed * (seconds - toSpeed));
            }

            if (runUp >= ds * 0.5f)
            {
                // Too short to reach cruising speed: she climbs to the halfway mark and
                // falls away from it.
                float const apex = std::sqrt(ds / accel);
                if (seconds <= apex)
                {
                    return 0.5f * accel * seconds * seconds;
                }

                float const left = std::max(0.0f, 2.0f * apex - seconds);
                return std::min(ds, ds - 0.5f * accel * left * left);
            }

            float const cruise = (ds - 2.0f * runUp) / speed;
            if (seconds <= toSpeed)
            {
                return 0.5f * accel * seconds * seconds;
            }
            if (seconds <= toSpeed + cruise)
            {
                return runUp + speed * (seconds - toSpeed);
            }

            float const left = std::max(0.0f, toSpeed + cruise + toSpeed - seconds);
            return std::min(ds, ds - 0.5f * accel * left * left);
        }
    };

    /// A leg while it is being built: the nodes, and where she berths on them.
    struct Building
    {
        uint32 map = 0;
        std::vector<Geometry::Vector3> nodes;
        /// Index of the node she berths at, and how long she stays, in milliseconds.
        std::vector<std::pair<uint32, uint32>> berths;
    };

    /// Every span's length, and the distance reached at every node.
    void Measure(VesselLeg& leg)
    {
        leg.reached.assign(leg.nodes.size() > 1 ? leg.nodes.size() - 1 : 0, 0.0f);

        for (size_t k = 2; k < leg.reached.size(); ++k)
        {
            leg.reached[k] = leg.reached[k - 1] + SpanLength(&leg.nodes[k - 2]);
        }
    }

    /// Rounded the way the client rounds every run before adding it on.
    uint32 Millis(float seconds)
    {
        return seconds > 0.0f ? uint32(std::lround(double(seconds) * 1000.0)) : 0u;
    }

    /// The runs of water the berths cut the leg into, timed.
    void Time(VesselLeg& leg, std::vector<std::pair<uint32, uint32>> const& berths, Profile const& how)
    {
        if (leg.reached.size() < 3)                         // fewer than four nodes is no span
        {
            return;
        }

        uint32 at = leg.startsAt;
        float behind = 0.0f;
        size_t sailed = 0;

        for (auto const& berth : berths)
        {
            if (berth.first >= leg.nodes.size() - 1)
            {
                break;
            }

            VesselLeg::Run run;
            run.startsAt = at;
            run.from = behind;
            run.to = leg.reached[berth.first];
            run.ramps = sailed == 0 ? 1u : 2u;
            run.sails = Millis(sailed == 0 ? how.Once(run.to - run.from) : how.Twice(run.to - run.from));
            run.waits = berth.second;

            at += run.sails + run.waits;
            behind = run.to;
            ++sailed;

            leg.runs.push_back(run);
        }

        VesselLeg::Run tail;
        tail.startsAt = at;
        tail.from = behind;
        tail.to = leg.reached.back();
        tail.ramps = sailed != 0 ? 1u : 0u;
        tail.sails = Millis(sailed != 0 ? how.Once(tail.to - tail.from)
                                        : (tail.to - tail.from) / how.speed);
        leg.runs.push_back(tail);
    }
}

Geometry::Vector3 VesselLeg::From() const
{
    if (nodes.size() > 1)
    {
        return nodes[1];
    }

    return nodes.empty() ? Geometry::Vector3() : nodes.front();
}

VesselRoute::VesselRoute(std::vector<TaxiPathNodeEntry const*> const& nodes, float speed, float accel)
{
    if (nodes.empty() || speed <= 0.0f || accel <= 0.0f)
    {
        return;
    }

    m_speed = speed;
    m_accel = accel;

    Profile const how{speed, accel, speed / accel, 0.5f * speed * (speed / accel)};

    std::vector<Building> building(1);
    building.back().map = nodes.front()->ContinentID;
    uint32 lastMap = nodes.front()->ContinentID;
    bool jumped = false;

    for (TaxiPathNodeEntry const* node : nodes)
    {
        if (node->ContinentID != lastMap || jumped)
        {
            building.emplace_back();
            building.back().map = node->ContinentID;
            lastMap = node->ContinentID;
        }

        Building& leg = building.back();

        // A berth on the first node of a leg is the one she has just left, so there is
        // no water behind it to have sailed and the client does not record it.
        if ((node->Flags & TAXI_NODE_STOP) && !leg.nodes.empty())
        {
            leg.berths.emplace_back(uint32(leg.nodes.size()), node->Delay * IN_MILLISECONDS);
        }

        leg.nodes.push_back(Geometry::Vector3(node->LocX, node->LocY, node->LocZ));
        jumped = (node->Flags & TAXI_NODE_TELEPORT) != 0;
    }

    // The client holds one berth list at a time and still has the last leg's when it
    // totals the lap, so it charges that leg's berth times once for every leg. On every
    // classic route the legs berth alike and it comes to the true sum.
    uint32 perLeg = 0;
    for (auto const& berth : building.back().berths)
    {
        perLeg += berth.second;
    }

    m_legs.reserve(building.size());

    for (Building const& built : building)
    {
        VesselLeg leg;
        leg.mapId = built.map;
        leg.nodes = built.nodes;
        leg.startsAt = m_period;

        Measure(leg);
        Time(leg, built.berths, how);

        // Only the sailing counts here: the time she lies berthed is charged once for the
        // whole leg, from the one list the client still holds when it totals the lap.
        for (VesselLeg::Run const& run : leg.runs)
        {
            m_period += run.sails;
        }

        m_period += perLeg;
        leg.endsAt = m_period;

        m_legs.push_back(leg);
    }

    m_waiting = perLeg * uint32(building.size());
}

std::set<uint32> VesselRoute::Maps() const
{
    std::set<uint32> maps;
    for (VesselLeg const& leg : m_legs)
    {
        maps.insert(leg.mapId);
    }

    return maps;
}

VesselLeg const* VesselRoute::LegAt(uint32 phaseMs) const
{
    if (m_period == 0)
    {
        return nullptr;
    }

    phaseMs %= m_period;

    for (VesselLeg const& leg : m_legs)
    {
        if (phaseMs >= leg.startsAt && phaseMs < leg.endsAt)
        {
            return &leg;
        }
    }

    return nullptr;
}

VesselPose VesselRoute::PoseAt(uint32 phaseMs) const
{
    VesselPose pose;

    VesselLeg const* leg = LegAt(phaseMs);
    if (!leg || leg->reached.size() < 3)
    {
        return pose;
    }

    Profile const how{m_speed, m_accel, m_speed / m_accel, 0.5f * m_speed * (m_speed / m_accel)};

    uint32 const into = (phaseMs % m_period) - leg->startsAt;
    float travelled = leg->reached.back();

    for (VesselLeg::Run const& run : leg->runs)
    {
        if (into < run.startsAt)
        {
            break;
        }

        uint32 const on = into - run.startsAt;
        if (on < run.sails)
        {
            travelled = run.from + how.Travelled(run.to - run.from, run.ramps, float(on) / 1000.0f);
            break;
        }
        if (on < run.sails + run.waits)
        {
            travelled = run.to;                             // lying at the berth
            break;
        }

        travelled = run.to;
    }

    // Which span of the leg that distance falls on, and how far along it.
    size_t node = 1;
    while (node + 1 < leg->reached.size() && leg->reached[node + 1] <= travelled)
    {
        ++node;
    }

    float const span = leg->reached[node + 1 < leg->reached.size() ? node + 1 : node] - leg->reached[node];
    float const along = span > 0.0f ? (travelled - leg->reached[node]) / span : 0.0f;

    pose.known = true;
    pose.mapId = leg->mapId;
    pose.at = OnSpan(&leg->nodes[node - 1], std::min(1.0f, std::max(0.0f, along)));

    return pose;
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
