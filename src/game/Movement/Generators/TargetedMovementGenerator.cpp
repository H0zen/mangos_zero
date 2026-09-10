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

#include "TargetedMovementGenerator.h"
#include "Creature.h"
#include "MotionFrame.h"
#include "World.h"

namespace
{

    constexpr float CHASE_RANGE = 0.5f;
    constexpr float CHASE_RECHASE_RANGE = 0.75f;

    constexpr float FOLLOW_RECALCULATE_FACTOR = 1.0f;

    constexpr float FOLLOW_DIST_GAP_FOR_DIST_FACTOR = 3.0f;
    constexpr float FOLLOW_DIST_RECALCULATE_FACTOR = 1.0f;
}

void TargetedMovementGenerator::ResetTracking()
{
    m_haveDest = false;
    m_targetReached = false;
    m_recheckTime.Reset(0);
    ResetLeg();
}

Motion::Vector3 TargetedMovementGenerator::ComputeDestination(Unit& owner) const
{
    Motion::IMotionFrame const& frame = Motion::FrameFor(owner);
    Unit const& target = *i_target.getTarget();

    const bool chaseHeadOn =
        GetMovementGeneratorType() == CHASE_MOTION_TYPE && m_angle == 0.0f;

    const float absAngle = chaseHeadOn
        ? Motion::AngleBetween(frame.ObjectPosition(owner, target), frame.MoverPosition(owner))
        : frame.ObjectOrientation(owner, target) + m_angle;

    return frame.NearPoint(owner, target, owner.Where().Extent(),
                           TargetDistance(owner, false), absAngle);
}

bool TargetedMovementGenerator::RequiresNewPosition(Unit& owner,
                                                    Motion::Vector3 const& spot) const
{
    const float allowed = TargetDistance(owner, true);

    Motion::IMotionFrame const& frame = Motion::FrameFor(owner);
    const Motion::Vector3 target = frame.ObjectPosition(owner, *i_target.getTarget());

    const float dx = spot.x - target.x;
    const float dy = spot.y - target.y;
    float distSq = dx * dx + dy * dy;

    if (IsCreature(&owner) && static_cast<Creature&>(owner).CanFly())
    {
        const float dz = spot.z - target.z;
        distSq += dz * dz;
    }

    const float maxdist = allowed + i_target->Where().Extent();
    return !(distSq < maxdist * maxdist);
}

Motion::MoveIntent TargetedMovementGenerator::Intent(Unit& owner,
                                                     Motion::MoveStatus const& status,
                                                     uint32 diff)
{

    if (!i_target.isValid() || !i_target->IsInWorld())
    {
        return Motion::MoveIntent::Done();
    }

    if (!owner.IsAlive())
    {
        return Motion::MoveIntent::Hold();
    }

    const bool blockedByState =
        owner.hasUnitState(UNIT_STAT_NOT_MOVE) ||
        (GetMovementGeneratorType() == CHASE_MOTION_TYPE &&
         owner.hasUnitState(UNIT_STAT_NO_COMBAT_MOVEMENT)) ||
        LostTarget(owner);

    if (blockedByState)
    {
        ClearMoveState(owner);
        return Motion::MoveIntent::Hold();
    }

    if (owner.IsNonMeleeSpellCasted(false, false, true))
    {
        if (!owner.IsStopped())
        {
            owner.StopMoving();
        }
        return Motion::MoveIntent::Hold();
    }

    bool needDest = !m_haveDest;
    m_recheckTime.Update(diff);
    if (m_recheckTime.Passed())
    {
        m_recheckTime.Reset(RecheckIntervalMs());
        needDest = RequiresNewPosition(owner, status.legGoal);
    }

    if (needDest)
    {
        m_dest = ComputeDestination(owner);
        m_haveDest = true;
        m_targetReached = false;
        AddMoveState(owner);
    }

    if (!status.traveling && !m_targetReached)
    {
        m_targetReached = true;
        ReachTarget(owner);
    }

    const Motion::Facing facing = (m_angle == 0.0f)
        ? Motion::Facing::ToTarget(i_target->GetObjectGuid())
        : Motion::Facing{};

    if (!needDest && !status.traveling)
    {
        return Motion::MoveIntent::Hold(facing);
    }

    uint32 flags = Motion::MOVE_REQUIRE_PATH;

    if (EnableWalking(owner))
    {
        flags |= Motion::MOVE_WALK;
    }

    if (IsCreature(&owner) && static_cast<Creature&>(owner).IsPet() &&
        owner.hasUnitState(UNIT_STAT_FOLLOW))
    {
        flags |= Motion::MOVE_FORCE_DEST;
    }

    return Motion::MoveIntent::Move(m_dest, flags, facing);
}

void ChaseMovementGenerator::Initialize(Unit& owner)
{
    if (IsCreature(&owner))
    {
        static_cast<Creature&>(owner).SetWalk(false, false);
    }

    owner.addUnitState(UNIT_STAT_CHASE);
    ResetTracking();
}

void ChaseMovementGenerator::Reset(Unit& owner)
{
    Initialize(owner);
}

void ChaseMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();
    owner.clearUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
    ResetTracking();
}

void ChaseMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
}

bool ChaseMovementGenerator::LostTarget(Unit& owner) const
{
    return owner.getVictim() != GetTarget();
}

void ChaseMovementGenerator::ReachTarget(Unit& owner)
{
    if (InMeleeReach(owner, *(GetTarget())))
    {
        owner.Attack(GetTarget(), true);
    }
}

float ChaseMovementGenerator::TargetDistance(Unit& owner, bool forRangeCheck) const
{
    if (!forRangeCheck)
    {
        return m_offset + CHASE_RANGE * CombatReachBetween(*i_target.getTarget(), owner);
    }

    return CHASE_RECHASE_RANGE * CombatReachBetween(*i_target.getTarget(), owner) -
           i_target->Where().Extent();
}

void FollowMovementGenerator::Initialize(Unit& owner)
{
    owner.addUnitState(UNIT_STAT_FOLLOW);
    SyncSpeedWithMaster(owner);
    ResetTracking();
}

void FollowMovementGenerator::Reset(Unit& owner)
{
    Initialize(owner);
}

void FollowMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();
    owner.clearUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);
    SyncSpeedWithMaster(owner);
    ResetTracking();
}

void FollowMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);
    SyncSpeedWithMaster(owner);
}

bool FollowMovementGenerator::EnableWalking(Unit& owner) const
{

    return IsCreature(&owner) && i_target.isValid() && i_target->IsWalking();
}

void FollowMovementGenerator::SyncSpeedWithMaster(Unit& owner) const
{
    if (!IsCreature(&owner))
    {
        return;
    }

    Creature& creature = static_cast<Creature&>(owner);

    if (!creature.IsPet() || !i_target.isValid() ||
        i_target->GetObjectGuid() != creature.GetOwnerGuid())
    {
        return;
    }

    creature.Pacing().Reckon(MOVE_RUN, true);
    creature.Pacing().Reckon(MOVE_WALK, true);
    creature.Pacing().Reckon(MOVE_SWIM, true);
}

float FollowMovementGenerator::TargetDistance(Unit& owner, bool forRangeCheck) const
{
    if (!forRangeCheck)
    {
        return m_offset + owner.Where().Extent() +
               i_target->Where().Extent();
    }

    float allowed = sWorld.getConfig(CONFIG_FLOAT_RATE_TARGET_POS_RECALCULATION_RANGE) -
                    i_target->Where().Extent();

    allowed += FOLLOW_RECALCULATE_FACTOR *
               (owner.Where().Extent() + i_target->Where().Extent());

    if (m_offset > FOLLOW_DIST_GAP_FOR_DIST_FACTOR)
    {
        allowed += FOLLOW_DIST_RECALCULATE_FACTOR * m_offset;
    }

    return allowed;
}
