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

#include "ConfusedMovementGenerator.h"
#include "MotionFrame.h"
#include "Unit.h"
#include "Util.h"

namespace
{

    constexpr float STAGGER_RADIUS = 10.0f;

    constexpr uint32 STAGGER_INTERVAL_MIN = 800;
    constexpr uint32 STAGGER_INTERVAL_MAX = 1500;

    constexpr uint32 RETRY_DELAY = 50;
}

void ConfusedMovementGenerator::Initialize(Unit& owner)
{
    owner.addUnitState(UNIT_STAT_CONFUSED);

    m_anchor = Motion::FrameFor(owner).MoverPosition(owner);

    m_staggerTime.Reset(0);
    m_haveLurch = false;
    ResetLeg();

    if (!owner.IsAlive() || owner.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        return;
    }

    owner.StopMoving();
    owner.addUnitState(UNIT_STAT_CONFUSED_MOVE);
}

void ConfusedMovementGenerator::Reset(Unit& owner)
{
    m_staggerTime.Reset(0);
    m_haveLurch = false;
    ResetLeg();

    if (!owner.IsAlive() || owner.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        return;
    }

    owner.StopMoving();
    owner.addUnitState(UNIT_STAT_CONFUSED | UNIT_STAT_CONFUSED_MOVE);
}

void ConfusedMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();

    owner.clearUnitState(UNIT_STAT_CONFUSED_MOVE);
    m_haveLurch = false;
    ResetLeg();
}

void ConfusedMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_CONFUSED | UNIT_STAT_CONFUSED_MOVE);

    if (owner.MovesItself())
    {
        owner.StopMoving(true);
    }
}

Motion::MoveIntent ConfusedMovementGenerator::Intent(Unit& owner,
                                                     Motion::MoveStatus const& status,
                                                     uint32 diff)
{

    if (owner.hasUnitState(UNIT_STAT_CAN_NOT_REACT & ~UNIT_STAT_CONFUSED))
    {
        return Motion::MoveIntent::Hold();
    }

    owner.addUnitState(UNIT_STAT_CONFUSED_MOVE);

    if (status.blocked)
    {
        m_haveLurch = false;
        m_staggerTime.Reset(RETRY_DELAY);
    }

    m_staggerTime.Update(diff);
    if (!m_staggerTime.Passed())
    {
        return (status.traveling && m_haveLurch)
            ? Motion::MoveIntent::Move(m_lurch, Motion::MOVE_WALK)
            : Motion::MoveIntent::Hold();
    }

    const auto lurch = Motion::FrameFor(owner).RandomPoint(owner, m_anchor, STAGGER_RADIUS);
    if (!lurch)
    {
        m_staggerTime.Reset(RETRY_DELAY);
        return Motion::MoveIntent::Hold();
    }

    m_lurch = *lurch;
    m_haveLurch = true;
    m_staggerTime.Reset(urand(STAGGER_INTERVAL_MIN, STAGGER_INTERVAL_MAX));

    return Motion::MoveIntent::Move(m_lurch, Motion::MOVE_WALK);
}
