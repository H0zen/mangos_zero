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

#include "MotionDriver.h"
#include "ObjectLookup.h"
#include "Unit.h"
#include "Movement/Spline/MoveSpline.h"
#include "Movement/Spline/MoveSplineInit.h"

#include <cmath>

namespace
{

    constexpr float MIN_RELAY_DISTANCE = 0.5f;

    constexpr float FACING_EPSILON = 0.01f;
}

void MotionDriver::ResetLeg()
{
    m_legGoal = Motion::Vector3();
    m_haveLeg = false;
    m_blocked = false;
    m_speedChanged = false;
    m_wasTraveling = false;
}

Motion::IPathQuery* MotionDriver::Query(Unit const& owner)
{
    Motion::IMotionFrame const& frame = Motion::FrameFor(owner);

    if (!m_query || m_queryFrame != frame.Kind())
    {
        m_query = frame.CreatePathQuery(owner);
        m_queryFrame = frame.Kind();
    }

    return m_query.get();
}

Motion::MoveStatus MotionDriver::BeginTick(Unit& owner)
{
    const bool traveling = !owner.movespline->Finalized();

    Motion::MoveStatus status;
    status.traveling = traveling;
    status.arrived = m_wasTraveling && !traveling;
    status.blocked = m_blocked;
    status.pathIndex = owner.movespline->Initialized() ? owner.movespline->currentPathIdx() : 0;

    if (m_haveLeg)
    {
        status.legGoal = m_legGoal;
    }

    m_blocked = false;
    m_wasTraveling = traveling;

    return status;
}

bool MotionDriver::Apply(Unit& owner, Motion::MoveIntent const& intent)
{
    switch (intent.act)
    {
        case Motion::MoveIntent::Act::Done:
            return false;

        case Motion::MoveIntent::Act::Move:
            ReconcileMove(owner, intent);
            return true;

        case Motion::MoveIntent::Act::Hold:
            ReconcileHold(owner, intent);
            return true;
    }

    return true;
}

bool MotionDriver::ReconcileMove(Unit& owner, Motion::MoveIntent const& intent)
{

    bool relay = !m_haveLeg || owner.movespline->Finalized();

    if (!relay && m_speedChanged && !intent.path)
    {
        relay = true;
    }

    if (!relay)
    {
        const Motion::Vector3 drift = intent.goal - m_legGoal;
        relay = drift.squaredLength() > MIN_RELAY_DISTANCE * MIN_RELAY_DISTANCE;
    }

    return relay ? LayLeg(owner, intent) : false;
}

bool MotionDriver::LayLeg(Unit& owner, Motion::MoveIntent const& intent)
{
    Movement::MoveSplineInit init(owner);

    if (intent.path && intent.path->size() >= 2)
    {

        init.MovebyPath(*intent.path);
    }
    else if (intent.Has(Motion::MOVE_STRAIGHT))
    {

        init.MoveTo(intent.goal.x, intent.goal.y, intent.goal.z, false);
    }
    else
    {

        Motion::IPathQuery* query = Query(owner);
        const Motion::Vector3 start = Motion::FrameFor(owner).MoverPosition(owner);

        const bool routed = query && query->Calculate(start, intent.goal,
                                                      intent.Has(Motion::MOVE_FORCE_DEST),
                                                      intent.pathLengthLimit);

        if (!routed || (intent.Has(Motion::MOVE_REQUIRE_PATH) && query->Failed()) ||
            (intent.Has(Motion::MOVE_REQUIRE_ROUTE) && !query->Routed()))
        {
            m_blocked = true;
            return false;
        }

        init.MovebyPath(query->Points());
    }

    switch (intent.facing.mode)
    {
        case Motion::Facing::Mode::Angle:
            init.SetFacing(intent.facing.angle);
            break;

        case Motion::Facing::Mode::Spot:
            init.SetFacing(intent.facing.spot);
            break;

        case Motion::Facing::Mode::Target:
            if (Unit* target = ObjectLookup::GetUnit(owner, intent.facing.target))
            {
                init.SetFacing(target);
            }
            break;

        case Motion::Facing::Mode::None:
            break;
    }

    init.SetWalk(intent.Has(Motion::MOVE_WALK));

    if (intent.Has(Motion::MOVE_FLY))
    {
        init.SetFly();
    }

    init.Launch();

    m_legGoal = intent.goal;
    m_haveLeg = true;
    m_blocked = false;
    m_speedChanged = false;
    m_wasTraveling = true;

    return true;
}

void MotionDriver::ReconcileHold(Unit& owner, Motion::MoveIntent const& intent)
{

    if (!owner.movespline->Finalized())
    {
        return;
    }

    switch (intent.facing.mode)
    {
        case Motion::Facing::Mode::Target:
        {
            Unit* target = ObjectLookup::GetUnit(owner, intent.facing.target);
            if (target && !owner.Where().HasInArc(target->Where(), FACING_EPSILON))
            {
                owner.SetInFront(target);
            }
            break;
        }
        case Motion::Facing::Mode::Angle:
        {
            if (std::fabs(owner.Where().Facing() - intent.facing.angle) > FACING_EPSILON)
            {
                owner.SetFacingTo(intent.facing.angle);
            }
            break;
        }
        case Motion::Facing::Mode::Spot:
        {
            const float angle = owner.Where().BearingTo(Geometry::Vector2(intent.facing.spot.x, intent.facing.spot.y));
            if (std::fabs(owner.Where().Facing() - angle) > FACING_EPSILON)
            {
                owner.SetFacingTo(angle);
            }
            break;
        }
        case Motion::Facing::Mode::None:
            break;
    }
}
