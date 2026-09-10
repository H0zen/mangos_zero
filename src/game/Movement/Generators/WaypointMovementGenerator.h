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
#include "IntentMovementGenerator.h"
#include "WaypointManager.h"
#include "Movement/Spline/MoveSplineInitArgs.h"

#include <sstream>
#include <vector>

#define STOP_TIME_FOR_PLAYER  (3 * MINUTE * IN_MILLISECONDS)

class WaypointMovementGenerator final : public IntentMovementGenerator
{
    public:
        explicit WaypointMovementGenerator(Creature&) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return WAYPOINT_MOTION_TYPE; }

        bool GetResetPosition(Unit& owner, float& x, float& y, float& z, float& o) const override;

        void InitializeWaypointPath(Unit& owner, int32 pathId, WaypointPathOrigin wpSource,
                                    uint32 initialDelay, uint32 overwriteEntry);

        uint32 getLastReachedWaypoint() const { return m_lastReachedWaypoint; }

        void GetPathInformation(int32& pathId, WaypointPathOrigin& wpOrigin) const
        {
            pathId = m_pathId;
            wpOrigin = m_pathOrigin;
        }

        void GetPathInformation(std::ostringstream& oss) const;

        void AddToWaypointPauseTime(int32 waitTimeDiff);

        bool SetNextWaypoint(uint32 pointId);

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

    private:

        struct SegmentWaypoint
        {
            uint32 pointId;
            size_t pathPointIndex;
        };

        void LoadPath(Creature& creature, int32 pathId, WaypointPathOrigin wpOrigin,
                      uint32 overwriteEntry);

        Motion::MoveIntent PrepareMove(Creature& creature);

        Motion::MoveIntent WalkPreparedLeg() const;

        void OnArrived(Creature& creature);

        void ProcessSegmentProgress(Creature& creature, int32 pathIndex);

        void BuildSmoothPath(Creature& creature, WaypointPath::const_iterator startPoint);

        bool Stopped(Unit const& owner) const;
        bool CanMove(Unit const& owner, uint32 diff);
        void Stop(int32 time) { m_nextMoveTime.Reset(time); }

        void ClearSegment()
        {
            m_segment.clear();
            m_segmentArrivals = 0;
        }

        WaypointPath const* m_path = nullptr;
        uint32 m_currentNode = 0;
        uint32 m_lastReachedWaypoint = 0;
        int32 m_pathId = 0;
        WaypointPathOrigin m_pathOrigin = PATH_NO_PATH;

        TimeTracker m_nextMoveTime{0};
        bool m_isArrivalDone = false;

        std::vector<SegmentWaypoint> m_segment;
        size_t m_segmentArrivals = 0;

        Movement::PointsArray m_legPoints;
        Motion::Vector3 m_legEnd;
        Motion::Facing m_legFacing;
        bool m_legWalk = true;
        bool m_haveLeg = false;
};

class FlightPathMovementGenerator final : public MovementGenerator
{
    public:
        explicit FlightPathMovementGenerator(TaxiPathNodeList const& pathnodes,
                                             uint32 startNode = 0)
            : m_path(&pathnodes), m_currentNode(startNode) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;
        bool Update(Unit& owner, uint32 diff) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return FLIGHT_MOTION_TYPE; }

        bool GetResetPosition(Unit& owner, float& x, float& y, float& z, float& o) const override;

        TaxiPathNodeList const& GetPath() const { return *m_path; }
        uint32 GetCurrentNode() const { return m_currentNode; }

        uint32 GetPathAtMapEnd() const;

        bool HasArrived() const { return m_currentNode >= m_path->size(); }

        void SetCurrentNodeAfterTeleport();
        void SkipCurrentNode() { ++m_currentNode; }

    private:
        TaxiPathNodeList const* m_path;
        uint32 m_currentNode;
};
