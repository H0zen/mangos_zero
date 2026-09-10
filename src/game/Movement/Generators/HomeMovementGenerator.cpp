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

#include "HomeMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "MotionFrame.h"

namespace
{

    constexpr float HOME_ARRIVAL_TOLERANCE = 2.0f;

    constexpr float HOME_PROGRESS_EPSILON = 0.5f;

    constexpr uint32 HOME_STALL_BUDGET = 10000;
}

void HomeMovementGenerator::Initialize(Unit& owner)
{
    m_arrived = false;
    m_haveHome = false;
    m_pathIndex = 0;
    m_homeIntentIssued = false;
    m_expectHomeLeg = false;
    m_onHomeLeg = false;
    m_stalled = 0;
    RefreshSpeedRates(owner);
    ResetLeg();

    if (owner.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        return;
    }

    float x, y, z, o;
    MotionMaster* motion = owner.GetMotionMaster();

    if (motion->empty() || !motion->top()->GetResetPosition(owner, x, y, z, o))
    {
        const Creature& home = static_cast<Creature&>(owner);
        x = home.Spawn().X();
        y = home.Spawn().Y();
        z = home.Spawn().Z();
        o = home.Spawn().Facing();
    }

    m_home = Motion::Vector3(x, y, z);
    m_facing = o;
    m_haveHome = true;

    owner.clearUnitState(UNIT_STAT_ALL_DYN_STATES);

    owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
}

void HomeMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    m_pathIndex = 0;
    m_homeIntentIssued = false;
    m_expectHomeLeg = false;
    m_onHomeLeg = false;
    RefreshSpeedRates(owner);
    ResetLeg();
}

void HomeMovementGenerator::Reset(Unit& owner)
{

    if (m_haveHome)
    {
        owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    }

    m_pathIndex = 0;
    m_homeIntentIssued = false;
    m_expectHomeLeg = false;
    m_onHomeLeg = false;
    RefreshSpeedRates(owner);
    ResetLeg();
}

Motion::MoveIntent HomeMovementGenerator::Intent(Unit& owner,
                                                 Motion::MoveStatus const& status,
                                                 uint32 diff)
{

    const bool speedChanged = RefreshSpeedRates(owner);

    const bool spent = !Resumable(owner, status, diff);

    if (status.arrived && m_haveHome && !AtHome(owner))
    {
        if (spent)
        {
            m_arrived = true;
            return Motion::MoveIntent::Done();
        }

        owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

        m_expectHomeLeg = true;
        m_onHomeLeg = false;
        m_homeIntentIssued = true;

        return Motion::MoveIntent::Move(m_home, Motion::MOVE_NONE,
                                        Motion::Facing::ToAngle(m_facing));
    }

    if (!m_haveHome || status.arrived || status.blocked || spent)
    {

        if (!status.arrived)
        {
            owner.InterruptMoving();
        }

        m_arrived = true;
        return Motion::MoveIntent::Done();
    }

    owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    if (!m_homeIntentIssued || !status.traveling || speedChanged)
    {
        m_expectHomeLeg = true;
        m_onHomeLeg = false;
        m_homeIntentIssued = true;
    }

    return Motion::MoveIntent::Move(m_home, Motion::MOVE_NONE,
                                    Motion::Facing::ToAngle(m_facing));
}

bool HomeMovementGenerator::AtHome(Unit const& owner) const
{
    const Motion::Vector3 gap = Motion::FrameFor(owner).MoverPosition(owner) - m_home;

    return gap.squaredLength() <= HOME_ARRIVAL_TOLERANCE * HOME_ARRIVAL_TOLERANCE;
}

bool HomeMovementGenerator::Resumable(Unit const& owner,
                                      Motion::MoveStatus const& status,
                                      uint32 diff)
{
    if (!m_haveHome)
    {
        return false;
    }

    const Motion::Vector3 position = Motion::FrameFor(owner).MoverPosition(owner);

    if (m_expectHomeLeg)
    {
        m_expectHomeLeg = false;
        m_onHomeLeg = status.traveling || status.arrived;
        m_pathIndex = 0;
        m_progressPosition = position;
    }

    bool progressed = false;

    if (m_onHomeLeg)
    {

        if (status.pathIndex < m_pathIndex)
        {
            m_onHomeLeg = false;
        }
        else
        {
            const Motion::Vector3 travelled = position - m_progressPosition;
            progressed = (status.traveling && status.pathIndex > m_pathIndex) ||
                travelled.squaredLength() >= HOME_PROGRESS_EPSILON * HOME_PROGRESS_EPSILON;
            m_pathIndex = status.pathIndex;

            if (progressed)
            {
                m_progressPosition = position;
            }
        }
    }

    if (progressed)
    {
        m_stalled = 0;
    }
    else
    {

        m_stalled = diff >= HOME_STALL_BUDGET - m_stalled
            ? HOME_STALL_BUDGET
            : m_stalled + diff;
    }

    return m_stalled < HOME_STALL_BUDGET;
}

bool HomeMovementGenerator::RefreshSpeedRates(Unit const& owner)
{
    static_assert(MAX_MOVE_TYPE == 6,
                  "HomeMovementGenerator must track every speed that can re-lay its route");

    bool changed = false;

    for (uint32 i = 0; i < m_speedRates.size(); ++i)
    {
        const float rate = owner.Pacing().RateOf(UnitMoveType(i));
        changed = changed || m_speedRates[i] != rate;
        m_speedRates[i] = rate;
    }

    return changed;
}

void HomeMovementGenerator::Finalize(Unit& owner)
{

    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    if (!m_arrived)
    {
        return;
    }

    Creature& creature = static_cast<Creature&>(owner);

    if (creature.GetTemporaryFactionFlags() & TEMPFACTION_RESTORE_REACH_HOME)
    {
        creature.ClearTemporaryFaction();
    }

    if (m_haveHome)
    {
        creature.SetFacingTo(m_facing);
    }

    creature.SetWalk(!creature.hasUnitState(UNIT_STAT_RUNNING_STATE) && !creature.IsLevitating(), false);
    creature.LoadCreatureAddon(true);
    creature.AI()->JustReachedHome();
}
