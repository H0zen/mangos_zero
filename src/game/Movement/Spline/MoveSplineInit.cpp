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

#include "MoveSplineInit.h"
#include "MoveSpline.h"
#include "packet_builder.h"
#include "Unit.h"
#include "Transports.h"
#include "TransportMap.h"
#include "Map.h"
#include "Player.h"
#include "Creature.h"

namespace
{

    ObjectGuid DeckVesselGuidOf(Unit const& unit)
    {
        if (Map* on = unit.GetMap())
        {
            if (TransportMap* hull = on->AsTransport())
            {
                if (Transport* vessel = hull->Vessel())
                {
                    return vessel->GetObjectGuid();
                }
            }
        }
        return 0;
    }
}

namespace Movement
{

    UnitMoveType SelectSpeedType(uint32 moveFlags)
    {
        if (moveFlags & MOVEFLAG_SWIMMING)
        {
            if (moveFlags & MOVEFLAG_BACKWARD )
            {
                return MOVE_SWIM_BACK;
            }
            else
            {
                return MOVE_SWIM;
            }
        }
        else if (moveFlags & MOVEFLAG_WALK_MODE)
        {

            return MOVE_WALK;
        }
        else if (moveFlags & MOVEFLAG_BACKWARD )
        {
            return MOVE_RUN_BACK;
        }

        return MOVE_RUN;
    }

    int32 MoveSplineInit::Launch()
    {
        MoveSpline& move_spline = *unit.movespline;

        const ObjectGuid vesselGuid = DeckVesselGuidOf(unit);

        Vector3 real_position(unit.Where().X(), unit.Where().Y(), unit.Where().Z());

        if (!move_spline.Finalized())
        {
            real_position = move_spline.ComputePosition();
        }

        if (args.path.empty())
        {

            MoveTo(real_position);
        }

        args.path[0] = real_position;
        uint32 moveFlags = unit.m_movementInfo.GetMovementFlags();
        if (args.flags.runmode)
        {
            moveFlags &= ~MOVEFLAG_WALK_MODE;
        }
        else
        {
            moveFlags |= MOVEFLAG_WALK_MODE;
        }

        moveFlags |= (MOVEFLAG_SPLINE_ENABLED | MOVEFLAG_FORWARD);

        if (args.velocity == 0.f)
        {
            args.velocity = unit.Pacing().At(SelectSpeedType(moveFlags));
        }

        if (!args.Validate(&unit))
        {
            return 0;
        }

        unit.m_movementInfo.SetMovementFlags((MovementFlags)moveFlags);
        move_spline.Initialize(args);

        WorldPacket data(SMSG_MONSTER_MOVE, 64);
        data << unit.GetPackGUID();

        if (!(vesselGuid == 0))
        {
            data.SetOpcode(SMSG_MONSTER_MOVE_TRANSPORT);
            data << PackGuid(vesselGuid);
        }

        PacketBuilder::WriteMonsterMove(move_spline, data);
        Deliver(Audience::Around(unit).AndSubject(), &data);

        return move_spline.Duration();
    }

    void MoveSplineInit::Stop()
    {
        MoveSpline& move_spline = *unit.movespline;

        if (move_spline.Finalized())
        {
            return;
        }

        const ObjectGuid vesselGuid = DeckVesselGuidOf(unit);

        Location real_position(unit.Where().X(), unit.Where().Y(), unit.Where().Z(), unit.Where().Facing());

        if (!move_spline.Finalized() )
        {
            real_position = move_spline.ComputePosition();
        }
        if (args.path.empty())
        {

            MoveTo(real_position);
        }

        args.path[0] = real_position;

        if (unit.IsInWorld())
        {
            unit.MovedTo(real_position.x, real_position.y, real_position.z, real_position.orientation);
        }

        args.flags = MoveSplineFlag::Done;
        unit.m_movementInfo.RemoveMovementFlag(MovementFlags(MOVEFLAG_FORWARD | MOVEFLAG_SPLINE_ENABLED));
        move_spline.Initialize(args);

        WorldPacket data(SMSG_MONSTER_MOVE, 64);
        data << unit.GetPackGUID();

        if (!(vesselGuid == 0))
        {
            data.SetOpcode(SMSG_MONSTER_MOVE_TRANSPORT);
            data << PackGuid(vesselGuid);
        }

        data << real_position.x << real_position.y << real_position.z;
        data << move_spline.GetId();
        data << uint8(MonsterMoveStop);
        Deliver(Audience::Around(unit).AndSubject(), &data);
    }

    MoveSplineInit::MoveSplineInit(Unit& m) : unit(m)
    {

        args.flags.runmode = !unit.m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE);
        args.flags.flying = unit.m_movementInfo.HasMovementFlag((MovementFlags)(MOVEFLAG_CAN_FLY | MOVEFLAG_FLYING | MOVEFLAG_LEVITATING));
    }

    void MoveSplineInit::SetFacing(const Unit* target)
    {
        args.flags.EnableFacingTarget();
        args.facing.target = target->GetObjectGuid();
    }

    void MoveSplineInit::SetFacing(float angle)
    {
        args.facing.angle = Geometry::wrap(angle, 0.f, (float)Geometry::twoPi());
        args.flags.EnableFacingAngle();
    }
}
