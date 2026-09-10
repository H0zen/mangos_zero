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
#include "packet_builder.h"
#include "MoveSpline.h"
#include "WorldPacket.h"

namespace Movement
{

    inline void operator << (ByteBuffer& b, const Vector3& v)
    {
        b << v.x << v.y << v.z;
    }

    inline void operator >> (ByteBuffer& b, Vector3& v)
    {
        b >> v.x >> v.y >> v.z;
    }

    void PacketBuilder::WriteCommonMonsterMovePart(const MoveSpline& move_spline, WorldPacket& data)
    {
        MoveSplineFlag splineflags = move_spline.splineflags;

        data << move_spline.spline.getPoint(move_spline.spline.first());
        data << move_spline.GetId();

        switch (splineflags & MoveSplineFlag::Mask_Final_Facing)
        {
            default:
                data << uint8(MonsterMoveNormal);
                break;
            case MoveSplineFlag::Final_Target:
                data << uint8(MonsterMoveFacingTarget);
                data << move_spline.facing.target;
                break;
            case MoveSplineFlag::Final_Angle:
                data << uint8(MonsterMoveFacingAngle);
                data << move_spline.facing.angle;
                break;
            case MoveSplineFlag::Final_Point:
                data << uint8(MonsterMoveFacingSpot);
                data << move_spline.facing.f.x << move_spline.facing.f.y << move_spline.facing.f.z;
                break;
        }

        splineflags.enter_cycle = move_spline.isCyclic();

        data << uint32((splineflags & ~MoveSplineFlag::Mask_No_Monster_Move) | MoveSplineFlag::Runmode);
        data << move_spline.Duration();
    }

    void WriteLinearPath(const Spline<int32>& spline, ByteBuffer& data)
    {
        Movement::SplineBase::ControlArray const& pathPoint = spline.getPoints();

        uint32 pathSize = spline.last() - spline.first() - 1;
        MANGOS_ASSERT(pathSize >= 0);

        Vector3 destination = pathPoint[spline.last()];
        data << pathSize;
        data << destination;

        for (uint32 i = spline.first(); i < spline.first() + pathSize; i++)
        {
            Vector3 offset = destination - pathPoint[i];
            data.appendPackXYZ(offset.x, offset.y, offset.z);
        }
    }

    void WriteCatmullRomPath(const Spline<int32>& spline, ByteBuffer& data)
    {
        uint32 count = spline.getPointCount() - 3;
        data << count;
        data.append<Vector3>(&spline.getPoint(2), count);
    }

    void WriteCatmullRomCyclicPath(const Spline<int32>& spline, ByteBuffer& data)
    {
        uint32 count = spline.getPointCount() - 3;
        data << uint32(count + 1);
        data << spline.getPoint(1);
        data.append<Vector3>(&spline.getPoint(1), count);
    }

    void PacketBuilder::WriteMonsterMove(const MoveSpline& move_spline, WorldPacket& data)
    {
        WriteCommonMonsterMovePart(move_spline, data);

        const Spline<int32>& spline = move_spline.spline;
        MoveSplineFlag splineflags = move_spline.splineflags;
        if (splineflags & MoveSplineFlag::Mask_CatmullRom)
        {
            if (splineflags.cyclic)
            {
                WriteCatmullRomCyclicPath(spline, data);
            }
            else
            {
                WriteCatmullRomPath(spline, data);
            }
        }
        else
        {
            WriteLinearPath(spline, data);
        }
    }

    void PacketBuilder::WriteCreate(const MoveSpline& move_spline, ByteBuffer& data)
    {
        MoveSplineFlag splineFlags = move_spline.splineflags;

        data << splineFlags.raw();

        if (splineFlags.final_point)
        {
            data << move_spline.facing.f.x << move_spline.facing.f.y << move_spline.facing.f.z;
        }
        else if (splineFlags.final_target)
        {
            data << move_spline.facing.target;
        }
        else if (splineFlags.final_angle)
        {
            data << move_spline.facing.angle;
        }

        data << move_spline.timePassed();
        data << move_spline.Duration();
        data << move_spline.GetId();

        uint32 nodes = move_spline.getPath().size();
        data << nodes;
        data.append<Vector3>(&move_spline.getPath()[0], nodes);
        data << (move_spline.isCyclic() ? Vector3::zero() : move_spline.FinalDestination());
    }
}
