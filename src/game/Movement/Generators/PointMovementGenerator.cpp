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

#include "PointMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Map.h"
#include "MotionFrame.h"
#include "TemporarySummon.h"
#include "World.h"
#include "Movement/Spline/MoveSpline.h"

void PointMovementGenerator::Initialize(Unit& owner)
{
    if (owner.hasUnitState(UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_NOT_MOVE))
    {
        return;
    }

    owner.StopMoving();
    owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    ResetLeg();
}

void PointMovementGenerator::Reset(Unit& owner)
{
    Initialize(owner);
}

void PointMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    ResetLeg();
}

void PointMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    if (owner.movespline->Finalized())
    {
        MovementInform(owner);
    }
}

void RoutedPointMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    if (m_arrived && owner.movespline->Finalized())
    {
        MovementInform(owner);
    }
}

Motion::MoveIntent RoutedPointMovementGenerator::Intent(Unit& owner, Motion::MoveStatus const& status,
                                                        uint32 diff)
{

    if (status.arrived &&
        owner.Where().DistanceTo(Geometry::Vector3(m_dest.x, m_dest.y, m_dest.z)) < 10.0f)
    {
        m_arrived = true;
    }

    return PointMovementGenerator::Intent(owner, status, diff);
}

void PointMovementGenerator::MovementInform(Unit& owner) const
{
    if (!IsCreature(&owner))
    {
        return;
    }

    Creature& creature = static_cast<Creature&>(owner);

    if (creature.AI())
    {
        creature.AI()->MovementInform(POINT_MOTION_TYPE, m_id);
    }

    if (!creature.IsTemporarySummon())
    {
        return;
    }

    const ObjectGuid summonerGuid = static_cast<TemporarySummon&>(creature).GetSummonerGuid();
    if (!(GuidHigh(summonerGuid) == HIGHGUID_UNIT))
    {
        return;
    }

    if (Creature* summoner = creature.GetMap()->GetCreature(summonerGuid))
    {
        if (summoner->AI())
        {
            summoner->AI()->SummonedMovementInform(&creature, POINT_MOTION_TYPE, m_id);
        }
    }
}

Motion::MoveIntent PointMovementGenerator::Intent(Unit& owner,
                                                  Motion::MoveStatus const& status,
                                                  uint32 )
{
    if (owner.hasUnitState(UNIT_STAT_CAN_NOT_MOVE))
    {
        owner.clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return Motion::MoveIntent::Hold();
    }

    if (status.arrived || status.blocked)
    {
        return Motion::MoveIntent::Done();
    }

    owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    const Motion::Vector3 goal = Motion::FrameFor(owner).FromWorld(owner, m_dest);

    return Motion::MoveIntent::Move(goal, LegFlags());
}

void AssistanceMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    Creature& creature = static_cast<Creature&>(owner);
    creature.SetNoCallAssistance(false);
    creature.CallAssistance();

    if (creature.IsAlive())
    {
        creature.GetMotionMaster()->MoveSeekAssistanceDistract(
            sWorld.getConfig(CONFIG_UINT32_CREATURE_FAMILY_ASSISTANCE_DELAY));
    }
}

Motion::MoveIntent EffectMovementGenerator::Intent(Unit& ,
                                                   Motion::MoveStatus const& status,
                                                   uint32 )
{

    return status.traveling ? Motion::MoveIntent::Hold() : Motion::MoveIntent::Done();
}

void EffectMovementGenerator::Finalize(Unit& owner)
{
    if (!IsCreature(&owner))
    {
        return;
    }

    Creature& creature = static_cast<Creature&>(owner);

    if (creature.AI() && owner.movespline->Finalized())
    {
        creature.AI()->MovementInform(EFFECT_MOTION_TYPE, m_id);
    }

    if (!owner.IsAlive() ||
        owner.hasUnitState(UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING | UNIT_STAT_NO_COMBAT_MOVEMENT))
    {
        return;
    }

    if (Unit* victim = owner.getVictim())
    {
        owner.GetMotionMaster()->MoveChase(victim);
    }
    else
    {
        owner.GetMotionMaster()->Initialize();
    }
}
