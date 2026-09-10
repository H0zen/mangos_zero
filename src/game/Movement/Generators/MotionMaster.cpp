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

#include "Utilities/Errors.h"
#include "MotionMaster.h"
#include "ConfusedMovementGenerator.h"
#include "FleeingMovementGenerator.h"
#include "HomeMovementGenerator.h"
#include "IdleMovementGenerator.h"
#include "PointMovementGenerator.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "RandomMovementGenerator.h"
#include "Movement/Spline/MoveSpline.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "Map.h"
#include "CreatureAISelector.h"
#include "Creature.h"
#include "CreatureLinkingMgr.h"
#include "Pet.h"
#include "DBCStores.h"
#include "Log.h"

#include <cassert>
#include <cmath>
#include <sstream>

inline static bool isStatic(MovementGenerator* mv)
{
    return (mv == &si_idleMovement);
}

void MotionMaster::Initialize()
{

    m_owner->StopMoving();

    Clear(false, true);

    if (IsCreature(m_owner) && !m_owner->hasUnitState(UNIT_STAT_CONTROLLED))
    {
        MovementGenerator* movement = FactorySelector::selectMovementGenerator((Creature*)m_owner);
        push(movement == nullptr ? &si_idleMovement : movement);
        top()->Initialize(*m_owner);
        if (top()->GetMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            static_cast<WaypointMovementGenerator*>(top())->InitializeWaypointPath(*m_owner, 0, PATH_NO_PATH, 0, 0);
        }
    }
    else
    {
        push(&si_idleMovement);
    }
}

MotionMaster::~MotionMaster()
{

    while (!empty())
    {
        MovementGenerator* m = top();
        pop();
        if (!isStatic(m))
        {
            delete m;
        }
    }
}

void MotionMaster::UpdateMotion(uint32 diff)
{
    if (m_owner->hasUnitState(UNIT_STAT_CAN_NOT_MOVE))
    {
        return;
    }

    MANGOS_ASSERT(!empty());
    m_cleanFlag |= MMCF_UPDATE;

    if (!top()->Update(*m_owner, diff))
    {
        m_cleanFlag &= ~MMCF_UPDATE;
        MovementExpired();
    }
    else
    {
        m_cleanFlag &= ~MMCF_UPDATE;
    }

    if (m_expList)
    {
        for (size_t i = 0; i < m_expList->size(); ++i)
        {
            MovementGenerator* mg = (*m_expList)[i];
            if (!isStatic(mg))
            {
                delete mg;
            }
        }

        delete m_expList;
        m_expList = nullptr;

        if (empty())
        {
            Initialize();
        }

        if (m_cleanFlag & MMCF_RESET)
        {
            top()->Reset(*m_owner);
            m_cleanFlag &= ~MMCF_RESET;
        }
    }
}

void MotionMaster::DirectClean(bool reset, bool all)
{
    while (all ? !empty() : size() > 1)
    {
        MovementGenerator* curr = top();
        pop();
        curr->Finalize(*m_owner);

        if (!isStatic(curr))
        {
            delete curr;
        }
    }

    if (!all && reset)
    {
        MANGOS_ASSERT(!empty());
        top()->Reset(*m_owner);
    }
}

void MotionMaster::DelayedClean(bool reset, bool all)
{
    if (reset)
    {
        m_cleanFlag |= MMCF_RESET;
    }
    else
    {
        m_cleanFlag &= ~MMCF_RESET;
    }

    if (empty() || (!all && size() == 1))
    {
        return;
    }

    if (!m_expList)
    {
        m_expList = new ExpireList();
    }

    while (all ? !empty() : size() > 1)
    {
        MovementGenerator* curr = top();
        pop();
        curr->Finalize(*m_owner);

        if (!isStatic(curr))
        {
            m_expList->push_back(curr);
        }
    }
}

void MotionMaster::DirectExpire(bool reset)
{
    if (empty() || size() == 1)
    {
        return;
    }

    MovementGenerator* curr = top();
    pop();

    while (!empty() && (top()->GetMovementGeneratorType() == CHASE_MOTION_TYPE || top()->GetMovementGeneratorType() == FOLLOW_MOTION_TYPE))
    {
        MovementGenerator* temp = top();
        pop();
        temp->Finalize(*m_owner);
        delete temp;
    }

    MovementGenerator* nowTop = empty() ? nullptr : top();

    curr->Finalize(*m_owner);

    if (!isStatic(curr))
    {
        delete curr;
    }

    if (empty())
    {
        Initialize();
    }

    if (reset && top() == nowTop)
    {
        top()->Reset(*m_owner);
    }
}

void MotionMaster::DelayedExpire(bool reset)
{
    if (reset)
    {
        m_cleanFlag |= MMCF_RESET;
    }
    else
    {
        m_cleanFlag &= ~MMCF_RESET;
    }

    if (empty() || size() == 1)
    {
        return;
    }

    MovementGenerator* curr = top();
    pop();

    if (!m_expList)
    {
        m_expList = new ExpireList();
    }

    while (!empty() && (top()->GetMovementGeneratorType() == CHASE_MOTION_TYPE || top()->GetMovementGeneratorType() == FOLLOW_MOTION_TYPE))
    {
        MovementGenerator* temp = top();
        pop();
        temp ->Finalize(*m_owner);
        m_expList->push_back(temp);
    }

    curr->Finalize(*m_owner);

    if (!isStatic(curr))
    {
        m_expList->push_back(curr);
    }
}

void MotionMaster::MoveIdle()
{
    if (empty() || !isStatic(top()))
    {
        push(&si_idleMovement);
    }
}

void MotionMaster::MoveRandomAroundPoint(float x, float y, float z, float radius, float )
{
    if (m_owner->MovesItself())
    {
        sLog.outError("%s attempt to move random.", m_owner->GetGuidStr().c_str());
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s move random.", m_owner->GetGuidStr().c_str());
        Mutate(new RandomMovementGenerator(x, y, z, radius));
    }
}

void MotionMaster::MoveTargetedHome()
{
    if (m_owner->hasUnitState(UNIT_STAT_LOST_CONTROL))
    {
        return;
    }

    Clear(false);

    if (IsCreature(m_owner) && !((Creature*)m_owner)->GetCharmerOrOwnerGuid())
    {

        if (static_cast<Creature*>(m_owner)->Links().RefollowMaster())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s refollowed linked master", m_owner->GetGuidStr().c_str());
        }
        else
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s targeted home", m_owner->GetGuidStr().c_str());
            Mutate(new HomeMovementGenerator());
        }
    }
    else if (IsCreature(m_owner) && ((Creature*)m_owner)->GetCharmerOrOwnerGuid())
    {
        if (Unit* target = ((Creature*)m_owner)->GetCharmerOrOwner())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s follow to %s", m_owner->GetGuidStr().c_str(), target->GetGuidStr().c_str());
            Mutate(new FollowMovementGenerator(*target, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE));
        }
        else
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s attempt but fail to follow owner", m_owner->GetGuidStr().c_str());
        }
    }
    else
    {
        sLog.outError("%s attempt targeted home", m_owner->GetGuidStr().c_str());
    }
}

void MotionMaster::MoveConfused()
{
    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s move confused", m_owner->GetGuidStr().c_str());

    Mutate(new ConfusedMovementGenerator());
}

void MotionMaster::MoveChase(Unit* target, float dist, float angle)
{

    if (!target)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s chase to %s", m_owner->GetGuidStr().c_str(), target->GetGuidStr().c_str());

    Mutate(new ChaseMovementGenerator(*target, dist, angle));
}

void MotionMaster::MoveFollow(Unit* target, float dist, float angle)
{
    if (m_owner->hasUnitState(UNIT_STAT_LOST_CONTROL))
    {
        return;
    }

    Clear();

    if (!target)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s follow to %s", m_owner->GetGuidStr().c_str(), target->GetGuidStr().c_str());

    Mutate(new FollowMovementGenerator(*target, dist, angle));
}

void MotionMaster::MovePoint(uint32 id, float x, float y, float z, bool generatePath)
{
    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s targeted point (Id: %u X: %f Y: %f Z: %f)", m_owner->GetGuidStr().c_str(), id, x, y, z);

    Mutate(new PointMovementGenerator(id, x, y, z, generatePath));
}

void MotionMaster::MovePointRouted(uint32 id, float x, float y, float z)
{
    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s targeted point, routed only (Id: %u X: %f Y: %f Z: %f)", m_owner->GetGuidStr().c_str(), id, x, y, z);

    Mutate(new RoutedPointMovementGenerator(id, x, y, z));
}

void MotionMaster::MoveSeekAssistance(float x, float y, float z)
{
    if (m_owner->MovesItself())
    {
        sLog.outError("%s attempt to seek assistance", m_owner->GetGuidStr().c_str());
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s seek assistance (X: %f Y: %f Z: %f)",
                         m_owner->GetGuidStr().c_str(), x, y, z);
        Mutate(new AssistanceMovementGenerator(x, y, z));
    }
}

void MotionMaster::MoveSeekAssistanceDistract(uint32 time)
{
    if (m_owner->MovesItself())
    {
        sLog.outError("%s attempt to call distract after assistance", m_owner->GetGuidStr().c_str());
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s is distracted after assistance call (Time: %u)",
                         m_owner->GetGuidStr().c_str(), time);
        Mutate(new AssistanceDistractMovementGenerator(time));
    }
}

void MotionMaster::MoveFleeing(Unit* enemy, uint32 time)
{
    if (!enemy)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s flee from %s", m_owner->GetGuidStr().c_str(), enemy->GetGuidStr().c_str());

    if (time &&IsCreature(m_owner))
    {
        Mutate(new TimedFleeingMovementGenerator(enemy->GetObjectGuid(), time));
    }
    else
    {
        Mutate(new FleeingMovementGenerator(enemy->GetObjectGuid()));
    }
}

void MotionMaster::MoveWaypoint(int32 id , uint32 source , uint32 initialDelay , uint32 overwriteEntry )
{
    if (IsCreature(m_owner))
    {
        if (GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            sLog.outError("%s attempt to MoveWaypoint() but is already using waypoint", m_owner->GetGuidStr().c_str());
            return;
        }

        Creature* creature = (Creature*)m_owner;

        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s start MoveWaypoint()", m_owner->GetGuidStr().c_str());
        WaypointMovementGenerator* newWPMMgen = new WaypointMovementGenerator(*creature);
        Mutate(newWPMMgen);
        newWPMMgen->InitializeWaypointPath(*creature, id, (WaypointPathOrigin)source, initialDelay, overwriteEntry);
    }
    else
    {
        sLog.outError("Non-creature %s attempt to MoveWaypoint()", m_owner->GetGuidStr().c_str());
    }
}

void MotionMaster::MoveTaxiFlight(uint32 path, uint32 pathnode)
{
    if (m_owner->MovesItself())
    {
        if (path < sTaxiPathNodesByPath.size())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s taxi to (Path %u node %u)", m_owner->GetGuidStr().c_str(), path, pathnode);
            FlightPathMovementGenerator* mgen = new FlightPathMovementGenerator(sTaxiPathNodesByPath[path], pathnode);
            Mutate(mgen);
        }
        else
        {
            sLog.outError("%s attempt taxi to (nonexistent Path %u node %u)",
                          m_owner->GetGuidStr().c_str(), path, pathnode);
        }
    }
    else
    {
        sLog.outError("%s attempt taxi to (Path %u node %u)",
                      m_owner->GetGuidStr().c_str(), path, pathnode);
    }
}

void MotionMaster::MoveDistract(uint32 timer)
{
    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s distracted (timer: %u)", m_owner->GetGuidStr().c_str(), timer);
    DistractMovementGenerator* mgen = new DistractMovementGenerator(timer);
    Mutate(mgen);
}

void MotionMaster::MoveFlyOrLand(uint32 id, float x, float y, float z, bool liftOff)
{
    if (!IsCreature(m_owner))
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s targeted point for %s (Id: %u X: %f Y: %f Z: %f)", m_owner->GetGuidStr().c_str(), liftOff ? "liftoff" : "landing", id, x, y, z);
    Mutate(new FlyOrLandMovementGenerator(id, x, y, z, liftOff));
}

void MotionMaster::Mutate(MovementGenerator* m)
{
    if (!empty())
    {
        switch (top()->GetMovementGeneratorType())
        {

            case HOME_MOTION_TYPE:

            case DISTRACT_MOTION_TYPE:
                MovementExpired(false);
            default:
                break;
        }

        if (!empty())
        {
            top()->Interrupt(*m_owner);
        }
    }

    m->Initialize(*m_owner);
    push(m);
}

void MotionMaster::PropagateSpeedChange()
{
    Impl::container_type::iterator it = Impl::c.begin();
    for (; it != end(); ++it)
    {
        (*it)->unitSpeedChanged();
    }
}

bool MotionMaster::SetNextWaypoint(uint32 pointId)
{
    for (Impl::container_type::reverse_iterator rItr = Impl::c.rbegin(); rItr != Impl::c.rend(); ++rItr)
    {
        if ((*rItr)->GetMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            return (static_cast<WaypointMovementGenerator*>(*rItr))->SetNextWaypoint(pointId);
        }
    }
    return false;
}

uint32 MotionMaster::getLastReachedWaypoint() const
{
    for (Impl::container_type::const_reverse_iterator rItr = Impl::c.rbegin(); rItr != Impl::c.rend(); ++rItr)
    {
        if ((*rItr)->GetMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            return (static_cast<WaypointMovementGenerator*>(*rItr))->getLastReachedWaypoint();
        }
    }
    return 0;
}

MovementGeneratorType MotionMaster::GetCurrentMovementGeneratorType() const
{
    if (empty())
    {
        return IDLE_MOTION_TYPE;
    }

    return top()->GetMovementGeneratorType();
}

bool MotionMaster::IsCurrentLegRouted() const
{
    if (empty())
    {
        return false;
    }

    return top()->IsRoutedLeg();
}

void MotionMaster::GetWaypointPathInformation(std::ostringstream& oss) const
{
    for (Impl::container_type::const_reverse_iterator rItr = Impl::c.rbegin(); rItr != Impl::c.rend(); ++rItr)
    {
        if ((*rItr)->GetMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            static_cast<WaypointMovementGenerator*>(*rItr)->GetPathInformation(oss);
            return;
        }
    }
}

bool MotionMaster::GetDestination(float& x, float& y, float& z)
{
    if (m_owner->movespline->Finalized())
    {
        return false;
    }

    const Geometry::Vector3& dest = m_owner->movespline->FinalDestination();
    x = dest.x;
    y = dest.y;
    z = dest.z;
    return true;
}

void MotionMaster::MoveFall()
{

    float tz = m_owner->GetMap()->GetHeight(m_owner->Where().X(), m_owner->Where().Y(), m_owner->Where().Z());
    if (tz <= INVALID_HEIGHT)
    {
        DEBUG_LOG("MotionMaster::MoveFall: unable retrive a proper height at map %u (x: %f, y: %f, z: %f).",
                  m_owner->GetMap()->GetId(), m_owner->Where().X(), m_owner->Where().Y(), m_owner->Where().Z());
        return;
    }

    if (fabs(m_owner->Where().Z() - tz) < 0.1f)
    {
        return;
    }

    Movement::MoveSplineInit init(*m_owner);
    init.MoveTo(m_owner->Where().X(), m_owner->Where().Y(), tz);
    init.SetFall();
    init.Launch();
    Mutate(new EffectMovementGenerator(0));
}
