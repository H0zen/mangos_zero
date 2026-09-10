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

#include "Creature.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "LivingWorldAnchorPolicy.h"
#include "Database/DatabaseEnv.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "ObjectGuid.h"
#include "SQLStorages.h"
#include "SpellMgr.h"
#include "GossipDef.h"
#include "Player.h"
#include "GameEventMgr.h"
#include "PoolManager.h"
#include "Log.h"
#include "LootMgr.h"
#include "CreatureAI.h"
#include "CreatureAISelector.h"
#include "InstanceData.h"
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGroundMgr.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Spell.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "CreatureLinkingMgr.h"
#include "DisableMgr.h"
#include "MovementGenerator.h"
#include "Policies/Singleton.h"

void Creature::SetWalk(bool enable, bool asDefault)
{
    if (asDefault)
    {
        if (enable)
        {
            clearUnitState(UNIT_STAT_RUNNING);
        }
        else
        {
            addUnitState(UNIT_STAT_RUNNING);
        }
    }

    if (enable == m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE))
    {
        return;
    }

    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_WALK_MODE);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_WALK_MODE);
    }

    WorldPacket data(enable ? SMSG_SPLINE_MOVE_SET_WALK_MODE : SMSG_SPLINE_MOVE_SET_RUN_MODE, 9);
    data << GetPackGUID();
    Deliver(Audience::Around(*this).AndSubject(), &data);
}

void Creature::SetLevitate(bool enable)
{
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_LEVITATING);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_LEVITATING);
    }
}

void Creature::SetSwim(bool enable)
{
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_SWIMMING);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_SWIMMING);
    }

    WorldPacket data(enable ? SMSG_SPLINE_MOVE_START_SWIM : SMSG_SPLINE_MOVE_STOP_SWIM);
    data << GetPackGUID();
    Deliver(Audience::Around(*this).AndSubject(), &data);
}

void Creature::SetCanFly(bool )
{

}

void Creature::SetFeatherFall(bool enable)
{
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_SAFE_FALL);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_SAFE_FALL);
    }

    WorldPacket data(enable ? SMSG_SPLINE_MOVE_FEATHER_FALL : SMSG_SPLINE_MOVE_NORMAL_FALL);
    data << GetPackGUID();
    Deliver(Audience::Around(*this).AndSubject(), &data);
}

void Creature::SetHover(bool enable)
{
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_HOVER);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_HOVER);
    }

    WorldPacket data(enable ? SMSG_SPLINE_MOVE_SET_HOVER : SMSG_SPLINE_MOVE_UNSET_HOVER, 9);
    data << GetPackGUID();
    Deliver(Audience::Around(*this), &data);
}

void Creature::SetRoot(bool enable)
{
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_ROOT);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_ROOT);
    }

    WorldPacket data(enable ? SMSG_SPLINE_MOVE_ROOT : SMSG_SPLINE_MOVE_UNROOT, 9);
    data << GetPackGUID();
    Deliver(Audience::Around(*this).AndSubject(), &data);
}

void Creature::SetWaterWalk(bool enable)
{
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_WATERWALKING);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_WATERWALKING);
    }

    WorldPacket data(enable ? SMSG_SPLINE_MOVE_WATER_WALK : SMSG_SPLINE_MOVE_LAND_WALK, 9);
    data << GetPackGUID();
    Deliver(Audience::Around(*this).AndSubject(), &data);
}
