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

#include "IdleMovementGenerator.h"
#include "CreatureAI.h"
#include "Creature.h"

IdleMovementGenerator si_idleMovement;

void IdleMovementGenerator::Reset(Unit& )
{
}

void DistractMovementGenerator::Initialize(Unit& owner)
{
    owner.addUnitState(UNIT_STAT_DISTRACTED);
}

void DistractMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_DISTRACTED);
}

void DistractMovementGenerator::Reset(Unit& owner)
{
    Initialize(owner);
}

void DistractMovementGenerator::Interrupt(Unit& )
{
}

bool DistractMovementGenerator::Update(Unit& , uint32 diff)
{
    if (diff > m_timer)
    {
        return false;
    }

    m_timer -= diff;
    return true;
}

void AssistanceDistractMovementGenerator::Finalize(Unit& unit)
{
    unit.clearUnitState(UNIT_STAT_DISTRACTED);
    if (Unit* victim = unit.getVictim())
    {
        if (unit.IsAlive())
        {
            unit.AttackStop(true);
            ((Creature*)&unit)->AI()->AttackStart(victim);
        }
    }
}
