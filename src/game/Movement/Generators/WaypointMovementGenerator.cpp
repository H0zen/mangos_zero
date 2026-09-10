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

#include "Utterance.h"
#include "Utilities/Errors.h"
#include "Utilities/MathDefines.h"
#include "WaypointMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "MotionFrame.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WaypointManager.h"
#include "WaypointSmoothing.h"
#include "Movement/Spline/MoveSpline.h"
#include "Movement/Spline/MoveSplineInit.h"

#include <memory>
#include <cmath>
#include <sstream>

namespace
{

    constexpr int32 SKIP_DEAD_NODE_DELAY = 50;

    constexpr float PLAYER_FLIGHT_SPEED = 32.0f;

    bool AppendLeg(Motion::IPathQuery& query, Motion::Vector3 const& start,
                   WaypointNode const& endNode, Movement::PointsArray& points)
    {
        const Motion::Vector3 goal(endNode.x, endNode.y, endNode.z);

        if (!query.Calculate(start, goal, false, 0.0f) || !query.Routed())
        {
            return false;
        }

        Movement::PointsArray const& leg = query.Points();
        if (leg.size() < 2)
        {
            return false;
        }

        if (!points.empty() &&
            (leg.front() - points.back()).length() >= WAYPOINT_SMOOTHING_MIN_SEGMENT_LENGTH)
        {
            return false;
        }

        if (points.empty())
        {
            points.push_back(leg.front());
        }

        for (auto itr = leg.begin() + 1; itr != leg.end(); ++itr)
        {
            if ((*itr - points.back()).length() >= WAYPOINT_SMOOTHING_MIN_SEGMENT_LENGTH)
            {
                points.push_back(*itr);
            }
        }

        return true;
    }
}

void WaypointMovementGenerator::LoadPath(Creature& creature, int32 pathId,
                                         WaypointPathOrigin wpOrigin, uint32 overwriteEntry)
{
    DETAIL_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "LoadPath: loading waypoint path for %s", creature.GetGuidStr().c_str());

    ClearSegment();

    if (!overwriteEntry)
    {
        overwriteEntry = creature.GetEntry();
    }

    if (wpOrigin == PATH_NO_PATH && pathId == 0)
    {
        m_path = sWaypointMgr.GetDefaultPath(overwriteEntry, creature.GetGUIDLow(), &m_pathOrigin);
    }
    else
    {
        m_pathOrigin = (wpOrigin == PATH_NO_PATH) ? PATH_FROM_ENTRY : wpOrigin;
        m_path = sWaypointMgr.GetPathFromOrigin(overwriteEntry, creature.GetGUIDLow(), pathId, m_pathOrigin);
    }

    m_pathId = pathId;

    if (!m_path)
    {
        if (m_pathOrigin == PATH_FROM_EXTERNAL)
        {
            sLog.outErrorScriptLib("WaypointMovementGenerator::LoadPath: %s doesn't have waypoint path %i", creature.GetGuidStr().c_str(), pathId);
        }
        else
        {
            sLog.outErrorDb("WaypointMovementGenerator::LoadPath: %s doesn't have waypoint path %i", creature.GetGuidStr().c_str(), pathId);
        }
        return;
    }

    if (m_path->empty())
    {
        return;
    }

    m_currentNode = m_path->begin()->first;
    m_lastReachedWaypoint = 0;
}

void WaypointMovementGenerator::InitializeWaypointPath(Unit& owner, int32 pathId,
                                                       WaypointPathOrigin wpSource,
                                                       uint32 initialDelay, uint32 overwriteEntry)
{
    LoadPath(static_cast<Creature&>(owner), pathId, wpSource, overwriteEntry);
    m_nextMoveTime.Reset(initialDelay);

    m_haveLeg = false;
    ResetLeg();
}

void WaypointMovementGenerator::Initialize(Unit& owner)
{
    owner.addUnitState(UNIT_STAT_ROAMING);
    owner.clearUnitState(UNIT_STAT_WAYPOINT_PAUSED);
    m_haveLeg = false;
    ResetLeg();
}

void WaypointMovementGenerator::Reset(Unit& owner)
{
    owner.addUnitState(UNIT_STAT_ROAMING);

    m_haveLeg = false;
    ResetLeg();
}

void WaypointMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();
    Finalize(owner);
    ResetLeg();
}

void WaypointMovementGenerator::Finalize(Unit& owner)
{
    ClearSegment();
    m_haveLeg = false;

    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    static_cast<Creature&>(owner).SetWalk(!owner.hasUnitState(UNIT_STAT_RUNNING_STATE), false);
}

bool WaypointMovementGenerator::Stopped(Unit const& owner) const
{
    return !m_nextMoveTime.Passed() || owner.hasUnitState(UNIT_STAT_WAYPOINT_PAUSED);
}

bool WaypointMovementGenerator::CanMove(Unit const& owner, uint32 diff)
{
    m_nextMoveTime.Update(diff);

    if (m_nextMoveTime.Passed() && owner.hasUnitState(UNIT_STAT_WAYPOINT_PAUSED))
    {
        m_nextMoveTime.Reset(1);
    }

    return m_nextMoveTime.Passed() && !owner.hasUnitState(UNIT_STAT_WAYPOINT_PAUSED);
}

void WaypointMovementGenerator::OnArrived(Creature& creature)
{
    if (!m_path || m_path->empty())
    {
        return;
    }

    m_lastReachedWaypoint = m_currentNode;

    if (m_isArrivalDone)
    {
        return;
    }

    creature.clearUnitState(UNIT_STAT_ROAMING_MOVE);
    m_isArrivalDone = true;

    WaypointPath::const_iterator currPoint = m_path->find(m_currentNode);
    MANGOS_ASSERT(currPoint != m_path->end());
    WaypointNode const& node = currPoint->second;

    if (node.script_id)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature movement start script %u at point %u for %s.", node.script_id, m_currentNode, creature.GetGuidStr().c_str());
        creature.GetMap()->Scripts().Start(DBS_ON_CREATURE_MOVEMENT, node.script_id, &creature, &creature);
    }

    if (WaypointBehavior* behavior = node.behavior)
    {
        if (behavior->emote != 0)
        {
            creature.HandleEmote(behavior->emote);
        }

        if (behavior->spell != 0)
        {
            creature.CastSpell(&creature, behavior->spell, false);
        }

        if (behavior->model1 != 0)
        {
            creature.SetDisplayId(behavior->model1);
        }

        if (behavior->textid[0])
        {
            int32 textId = behavior->textid[0];

            if (behavior->textid[1])
            {
                int count = 2;
                for (; count < MAX_WAYPOINT_TEXT; ++count)
                {
                    if (!behavior->textid[count])
                    {
                        break;
                    }
                }

                textId = behavior->textid[urand(0, count - 1)];
            }

            if (MangosStringLocale const* textData = sObjectMgr.GetMangosStringLocale(textId))
            {
                Utter(creature, textData, nullptr);
            }
            else
            {
                sLog.outErrorDb("%s reached waypoint %u, attempted to do text %i, but required text-data could not be found", creature.GetGuidStr().c_str(), m_currentNode, textId);
            }
        }
    }

    if (creature.AI())
    {
        uint32 type = WAYPOINT_MOTION_TYPE;
        if (m_pathOrigin == PATH_FROM_EXTERNAL && m_pathId > 0)
        {
            type = EXTERNAL_WAYPOINT_MOVE + m_pathId;
        }
        creature.AI()->MovementInform(type, m_currentNode);
    }

    Stop(node.delay);
}

void WaypointMovementGenerator::ProcessSegmentProgress(Creature& creature, int32 pathIndex)
{
    while (m_segmentArrivals < m_segment.size() &&
           HasReachedWaypointEndpoint(pathIndex, m_segment[m_segmentArrivals].pathPointIndex))
    {
        m_currentNode = m_segment[m_segmentArrivals].pointId;
        m_isArrivalDone = false;
        OnArrived(creature);
        ++m_segmentArrivals;

        if (!creature.movespline->Finalized() && !Stopped(creature))
        {
            creature.addUnitState(UNIT_STAT_ROAMING_MOVE);
        }
    }
}

void WaypointMovementGenerator::BuildSmoothPath(Creature& creature,
                                                WaypointPath::const_iterator startPoint)
{
    ClearSegment();
    m_legPoints.clear();

    if (m_pathOrigin == PATH_FROM_EXTERNAL)
    {
        return;
    }

    Motion::IMotionFrame const& frame = Motion::FrameFor(creature);
    const std::unique_ptr<Motion::IPathQuery> query = frame.CreatePathQuery(creature);

    Motion::Vector3 start = frame.MoverPosition(creature);

    WaypointSmoothingBounds bounds;

    WaypointPath::const_iterator currPoint = startPoint;
    for (size_t segment = 0; segment < WAYPOINT_SMOOTHING_MAX_LOOKAHEAD; ++segment)
    {
        WaypointNode const& node = currPoint->second;
        const size_t committed = m_legPoints.size();

        if (!AppendLeg(*query, start, node, m_legPoints))
        {

            if (committed == 0)
            {
                ClearSegment();
                m_legPoints.clear();
                return;
            }
            break;
        }

        WaypointSmoothingBounds trial = bounds;
        for (size_t i = committed; i < m_legPoints.size(); ++i)
        {
            AddWaypointSmoothingPoint(trial, m_legPoints[i].x, m_legPoints[i].y, m_legPoints[i].z);
        }

        if (committed != 0 && !IsWaypointSmoothingWithinBudget(trial))
        {
            m_legPoints.resize(committed);
            break;
        }

        bounds = trial;
        m_segment.push_back({currPoint->first, m_legPoints.size() - 1});

        WaypointSmoothingNode smoothing;
        smoothing.hasDelay = node.delay != 0;
        smoothing.hasScript = node.script_id != 0;
        smoothing.hasBehavior = node.behavior != nullptr && !node.behavior->isEmpty();

        if (!IsWaypointSmoothingSafe(smoothing))
        {
            break;
        }

        WaypointPath::const_iterator nextPoint = currPoint;
        if (++nextPoint == m_path->end())
        {
            nextPoint = m_path->begin();
        }

        if (nextPoint == startPoint)
        {
            break;
        }

        start = Motion::Vector3(node.x, node.y, node.z);
        currPoint = nextPoint;
    }

    if (m_segment.size() <= 1 || m_legPoints.size() < 2)
    {
        ClearSegment();
        m_legPoints.clear();
    }
}

Motion::MoveIntent WaypointMovementGenerator::WalkPreparedLeg() const
{

    uint32 flags = Motion::MOVE_REQUIRE_PATH;
    if (m_legWalk)
    {
        flags |= Motion::MOVE_WALK;
    }

    Motion::MoveIntent intent = Motion::MoveIntent::Move(m_legEnd, flags, m_legFacing);

    if (!m_legPoints.empty())
    {
        intent.Along(m_legPoints);
    }

    return intent;
}

Motion::MoveIntent WaypointMovementGenerator::PrepareMove(Creature& creature)
{
    m_haveLeg = false;
    m_legPoints.clear();

    if (!m_path || m_path->empty() || Stopped(creature))
    {
        return Motion::MoveIntent::Hold();
    }

    if (!creature.IsAlive() || creature.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        return Motion::MoveIntent::Hold();
    }

    WaypointPath::const_iterator currPoint = m_path->find(m_currentNode);
    MANGOS_ASSERT(currPoint != m_path->end());

    if (WaypointBehavior* behavior = currPoint->second.behavior)
    {
        if (behavior->model2 != 0)
        {
            creature.SetDisplayId(behavior->model2);
        }
        creature.SetUInt32Value(UNIT_NPC_EMOTESTATE, 0);
    }

    if (m_isArrivalDone)
    {
        bool reachedLast = false;
        if (++currPoint == m_path->end())
        {
            reachedLast = true;
            currPoint = m_path->begin();
        }

        if (creature.AI() && m_pathOrigin == PATH_FROM_EXTERNAL && m_pathId > 0)
        {
            creature.AI()->MovementInform(
                (reachedLast ? EXTERNAL_WAYPOINT_FINISHED_LAST : EXTERNAL_WAYPOINT_MOVE_START) + m_pathId,
                currPoint->first);

            if (creature.IsDead() || !creature.IsInWorld())
            {
                return Motion::MoveIntent::Hold();
            }
        }

        m_currentNode = currPoint->first;
    }

    m_isArrivalDone = false;
    creature.addUnitState(UNIT_STAT_ROAMING_MOVE);

    BuildSmoothPath(creature, currPoint);

    WaypointNode const* finalNode = &currPoint->second;
    if (!m_legPoints.empty())
    {
        WaypointPath::const_iterator finalPoint = m_path->find(m_segment.back().pointId);
        MANGOS_ASSERT(finalPoint != m_path->end());
        finalNode = &finalPoint->second;
    }

    m_legEnd = Motion::Vector3(finalNode->x, finalNode->y, finalNode->z);

    m_legFacing = (finalNode->orientation != 100 && finalNode->delay != 0)
        ? Motion::Facing::ToAngle(finalNode->orientation)
        : Motion::Facing{};

    m_haveLeg = true;

    m_legWalk = !creature.hasUnitState(UNIT_STAT_RUNNING_STATE) && !creature.IsLevitating();
    creature.SetWalk(m_legWalk, false);

    return WalkPreparedLeg();
}

Motion::MoveIntent WaypointMovementGenerator::Intent(Unit& owner,
                                                     Motion::MoveStatus const& status,
                                                     uint32 diff)
{

    if (owner.hasUnitState(UNIT_STAT_NOT_MOVE) || !m_path || m_path->empty())
    {
        owner.clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return Motion::MoveIntent::Hold();
    }

    Creature& creature = static_cast<Creature&>(owner);

    if (status.blocked)
    {
        ClearSegment();
        m_isArrivalDone = true;
        m_haveLeg = false;
        Stop(SKIP_DEAD_NODE_DELAY);
    }

    if (Stopped(creature))
    {
        return CanMove(creature, diff) ? PrepareMove(creature) : Motion::MoveIntent::Hold();
    }

    if (!m_haveLeg && !status.traveling)
    {
        return PrepareMove(creature);
    }

    switch (GetWaypointSegmentUpdateState(!status.traveling, creature.IsStopped()))
    {
        case WaypointSegmentUpdateState::Stopped:
            ClearSegment();
            m_haveLeg = false;
            Stop(STOP_TIME_FOR_PLAYER);
            return Motion::MoveIntent::Hold();

        case WaypointSegmentUpdateState::Finalized:
            ProcessSegmentProgress(creature, status.pathIndex);
            if (!m_isArrivalDone)
            {
                OnArrived(creature);
            }
            ClearSegment();

            if (Stopped(creature))
            {
                m_haveLeg = false;
                return Motion::MoveIntent::Hold();
            }

            return PrepareMove(creature);

        case WaypointSegmentUpdateState::Moving:
            ProcessSegmentProgress(creature, status.pathIndex);
            break;
    }

    return m_haveLeg ? WalkPreparedLeg() : Motion::MoveIntent::Hold();
}

bool WaypointMovementGenerator::GetResetPosition(Unit& owner, float& x, float& y, float& z,
                                                 float& o) const
{
    if (!m_path || m_path->empty())
    {
        return false;
    }

    Creature& creature = static_cast<Creature&>(owner);

    float combatX, combatY, combatZ;
    combatX = creature.CombatAnchor().x;
    combatY = creature.CombatAnchor().y;
    combatZ = creature.CombatAnchor().z;

    if (combatX != 0.0f || combatY != 0.0f || combatZ != 0.0f)
    {
        x = combatX;
        y = combatY;
        z = combatZ;
        o = creature.Where().Facing();

        WaypointPath::const_iterator nextPoint = m_path->find(m_currentNode);
        if (nextPoint != m_path->end())
        {
            const float dx = nextPoint->second.x - x;
            const float dy = nextPoint->second.y - y;
            if (dx != 0.0f || dy != 0.0f)
            {
                o = atan2(dy, dx);
                o = (o >= 0) ? o : 2 * M_PI_F + o;
            }
        }

        return true;
    }

    WaypointPath::const_iterator lastPoint = m_path->find(m_lastReachedWaypoint);

    if (!m_lastReachedWaypoint && lastPoint == m_path->end())
    {
        return false;
    }

    MANGOS_ASSERT(lastPoint != m_path->end());
    WaypointNode const& curWP = lastPoint->second;

    x = curWP.x;
    y = curWP.y;
    z = curWP.z;

    if (curWP.orientation != 100)
    {
        o = curWP.orientation;
        return true;
    }

    WaypointNode const& prevWP = (lastPoint != m_path->begin())
        ? std::prev(lastPoint)->second
        : m_path->rbegin()->second;

    o = atan2(y - prevWP.y, x - prevWP.x);
    o = (o >= 0) ? o : 2 * M_PI_F + o;

    return true;
}

void WaypointMovementGenerator::GetPathInformation(std::ostringstream& oss) const
{
    oss << "WaypointMovement: Last Reached WP: " << m_lastReachedWaypoint << " ";
    oss << "(Loaded path " << m_pathId << " from " << WaypointManager::GetOriginString(m_pathOrigin) << ")\n";
}

void WaypointMovementGenerator::AddToWaypointPauseTime(int32 waitTimeDiff)
{
    m_nextMoveTime.Update(waitTimeDiff);
    if (m_nextMoveTime.Passed())
    {
        m_nextMoveTime.Reset(0);
    }
}

bool WaypointMovementGenerator::SetNextWaypoint(uint32 pointId)
{
    if (!m_path || m_path->empty() || m_path->find(pointId) == m_path->end())
    {
        return false;
    }

    m_nextMoveTime.Reset(1);
    m_isArrivalDone = false;
    ClearSegment();
    m_haveLeg = false;
    ResetLeg();

    m_currentNode = pointId;
    return true;
}

uint32 FlightPathMovementGenerator::GetPathAtMapEnd() const
{
    if (m_currentNode >= m_path->size())
    {
        return m_path->size();
    }

    const uint32 curMapId = (*m_path)[m_currentNode].ContinentID;

    for (uint32 i = m_currentNode; i < m_path->size(); ++i)
    {
        if ((*m_path)[i].ContinentID != curMapId)
        {
            return i;
        }
    }

    return m_path->size();
}

void FlightPathMovementGenerator::Initialize(Unit& owner)
{
    Reset(owner);
}

void FlightPathMovementGenerator::Reset(Unit& owner)
{
    Player& player = static_cast<Player&>(owner);

    player.GetHostileRefManager().setOnlineOfflineState(false);
    player.addUnitState(UNIT_STAT_TAXI_FLIGHT);
    player.SetUnitFlag(UNIT_FLAG_CLIENT_CONTROL_LOST | UNIT_FLAG_TAXI_FLIGHT);

    Movement::MoveSplineInit init(player);

    const uint32 end = GetPathAtMapEnd();
    for (uint32 i = m_currentNode; i != end; ++i)
    {
        init.Path().push_back(Movement::Vector3((*m_path)[i].LocX, (*m_path)[i].LocY, (*m_path)[i].LocZ));
    }

    init.SetFirstPointId(m_currentNode);
    init.SetFly();
    init.SetVelocity(PLAYER_FLIGHT_SPEED);
    init.Launch();
}

void FlightPathMovementGenerator::Interrupt(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_TAXI_FLIGHT);
}

void FlightPathMovementGenerator::Finalize(Unit& owner)
{
    Player& player = static_cast<Player&>(owner);

    player.clearUnitState(UNIT_STAT_TAXI_FLIGHT);

    player.Unmount();
    player.RemoveUnitFlag(UNIT_FLAG_CLIENT_CONTROL_LOST | UNIT_FLAG_TAXI_FLIGHT);

    if (!player.m_taxi.empty())
    {
        return;
    }

    player.GetHostileRefManager().setOnlineOfflineState(true);
    if (player.pvpInfo.inHostileArea)
    {
        player.CastSpell(&player, 2479, true);
    }

    player.StopMoving(true);
}

bool FlightPathMovementGenerator::Update(Unit& owner, uint32 )
{
    const uint32 pointId = uint32(owner.movespline->currentPathIdx());

    if (pointId > m_currentNode)
    {
        bool departure = true;
        while (pointId != m_currentNode)
        {
            m_currentNode += uint32(departure);
            departure = !departure;
        }
    }

    return m_currentNode < (m_path->size() - 1);
}

void FlightPathMovementGenerator::SetCurrentNodeAfterTeleport()
{
    if (m_path->empty())
    {
        return;
    }

    const uint32 map0 = (*m_path)[0].ContinentID;

    for (size_t i = 1; i < m_path->size(); ++i)
    {
        if ((*m_path)[i].ContinentID != map0)
        {
            m_currentNode = i;
            return;
        }
    }
}

bool FlightPathMovementGenerator::GetResetPosition(Unit& , float& x, float& y,
                                                   float& z, float& ) const
{
    TaxiPathNodeEntry const& node = (*m_path)[m_currentNode];
    x = node.LocX;
    y = node.LocY;
    z = node.LocZ;

    return true;
}
