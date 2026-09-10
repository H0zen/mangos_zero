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

#include "Utilities/MathDefines.h"
#include "FleeingMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "MotionFrame.h"
#include "ObjectLookup.h"
#include "Util.h"
#include <cmath>

namespace
{

    constexpr float MIN_QUIET_DISTANCE = 28.0f;
    constexpr float MAX_QUIET_DISTANCE = 43.0f;

    constexpr float FLEE_PATH_LENGTH_LIMIT = 30.0f;

    constexpr uint32 REST_AFTER_BOLT_MIN = 800;
    constexpr uint32 REST_AFTER_BOLT_MAX = 1500;

    constexpr uint32 RETRY_DELAY = 50;
}

std::optional<Motion::Vector3> FleeingMovementGenerator::PickFleePoint(Unit& owner) const
{
    Motion::IMotionFrame const& frame = Motion::FrameFor(owner);

    float distFromCaster = 0.0f;
    float angleToCaster = frand(0, 2 * M_PI_F);

    if (Unit const* fright = ObjectLookup::GetUnit(owner, m_frightGuid))
    {

        distFromCaster = fright->Where().DistanceTo(owner.Where());
        if (distFromCaster > 0.2f)
        {
            angleToCaster = Motion::AngleBetween(frame.ObjectPosition(owner, *fright),
                                                 frame.MoverPosition(owner));
        }
    }

    float dist, angle;
    if (distFromCaster < MIN_QUIET_DISTANCE)
    {

        dist = frand(0.4f, 1.3f) * (MIN_QUIET_DISTANCE - distFromCaster);
        angle = angleToCaster + frand(-M_PI_F / 8, M_PI_F / 8);
    }
    else if (distFromCaster > MAX_QUIET_DISTANCE)
    {

        dist = frand(0.4f, 1.0f) * (MAX_QUIET_DISTANCE - MIN_QUIET_DISTANCE);
        angle = -angleToCaster + frand(-M_PI_F / 4, M_PI_F / 4);
    }
    else
    {

        dist = frand(0.6f, 1.2f) * (MAX_QUIET_DISTANCE - MIN_QUIET_DISTANCE);
        angle = frand(0, 2 * M_PI_F);
    }

    const Motion::Vector3 from = frame.MoverPosition(owner);

    const Motion::Vector3 guess(from.x + dist * cos(angle),
                                from.y + dist * sin(angle),
                                from.z + 0.5f);

    return frame.GroundPoint(owner, from, guess);
}

void FleeingMovementGenerator::Initialize(Unit& owner)
{
    owner.addUnitState(UNIT_STAT_FLEEING | UNIT_STAT_FLEEING_MOVE);
    owner.StopMoving();

    if (IsCreature(&owner))
    {
        static_cast<Creature&>(owner).SetWalk(false, false);
        owner.SetTargetGuid(0);
    }

    m_restTime.Reset(0);
    m_haveFleePoint = false;
    ResetLeg();
}

void FleeingMovementGenerator::Reset(Unit& owner)
{
    Initialize(owner);
}

void FleeingMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();

    owner.clearUnitState(UNIT_STAT_FLEEING_MOVE);
    m_haveFleePoint = false;
    ResetLeg();
}

void FleeingMovementGenerator::Finalize(Unit& owner)
{
    if (IsCreature(&owner))
    {
        static_cast<Creature&>(owner).SetWalk(!owner.hasUnitState(UNIT_STAT_RUNNING_STATE), false);
    }
    else
    {
        owner.StopMoving();
    }

    owner.clearUnitState(UNIT_STAT_FLEEING | UNIT_STAT_FLEEING_MOVE);
}

Motion::MoveIntent FleeingMovementGenerator::Intent(Unit& owner,
                                                    Motion::MoveStatus const& status,
                                                    uint32 diff)
{
    if (!owner.IsAlive())
    {
        return Motion::MoveIntent::Done();
    }

    if (owner.hasUnitState((UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_NOT_MOVE) & ~UNIT_STAT_FLEEING))
    {
        owner.clearUnitState(UNIT_STAT_FLEEING_MOVE);
        return Motion::MoveIntent::Hold();
    }

    if (status.blocked)
    {
        m_haveFleePoint = false;
        m_restTime.Reset(RETRY_DELAY);
    }

    if (status.traveling && m_haveFleePoint)
    {
        return Motion::MoveIntent::Move(m_fleePoint, Motion::MOVE_REQUIRE_PATH);
    }

    m_restTime.Update(diff);
    if (!m_restTime.Passed())
    {
        return Motion::MoveIntent::Hold();
    }

    const auto point = PickFleePoint(owner);
    if (!point)
    {
        m_restTime.Reset(RETRY_DELAY);
        return Motion::MoveIntent::Hold();
    }

    owner.addUnitState(UNIT_STAT_FLEEING_MOVE);
    m_fleePoint = *point;
    m_haveFleePoint = true;
    m_restTime.Reset(urand(REST_AFTER_BOLT_MIN, REST_AFTER_BOLT_MAX));

    return Motion::MoveIntent::Move(m_fleePoint, Motion::MOVE_REQUIRE_PATH)
        .WithinLength(FLEE_PATH_LENGTH_LIMIT);
}

Motion::MoveIntent TimedFleeingMovementGenerator::Intent(Unit& owner,
                                                         Motion::MoveStatus const& status,
                                                         uint32 diff)
{
    m_totalFleeTime.Update(diff);
    if (m_totalFleeTime.Passed())
    {
        return Motion::MoveIntent::Done();
    }

    return FleeingMovementGenerator::Intent(owner, status, diff);
}

void TimedFleeingMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_FLEEING | UNIT_STAT_FLEEING_MOVE);

    if (Unit* victim = owner.getVictim())
    {
        if (owner.IsAlive())
        {
            owner.AttackStop(true);
            static_cast<Creature&>(owner).AI()->AttackStart(victim);
        }
    }
}
