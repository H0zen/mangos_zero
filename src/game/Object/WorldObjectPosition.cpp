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

#include "Occupant.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "Creature.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "UpdateData.h"
#include "Util.h"
#include "Transports.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "Movement/Spline/packet_builder.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"
#include "Transports.h"
#include "TransportMap.h"

void Occupant::CleanupsBeforeDelete()
{
    RemoveFromWorld();
}

void Occupant::Update(uint32 update_diff, uint32 )
{
}

void Occupant::_Create(uint32 guidlow, HighGuid guidhigh)
{
    Object::_Create(guidlow, 0, guidhigh);
}

InstanceData* Occupant::GetInstanceData() const
{
    return GetMap()->GetInstanceData();
}

Geometry::Vector3 RandomGroundPointNear(Occupant const& obj, Geometry::Vector3 const& centre,
                                        float distance, float minDist, float const* ori)
{
    if (distance == 0.0f)
    {
        return centre;
    }

    const float angle = ori ? *ori : (rand_norm_f() * Geometry::Placement::TwoPi());

    Geometry::Placement around;
    around.EnterFrameOf(obj.Where(), centre, angle);

    Geometry::Vector3 point = around.RandomPointAround(minDist, distance, angle, rand_norm_f());
    MaNGOS::NormalizeMapCoord(point.x);
    MaNGOS::NormalizeMapCoord(point.y);
    DropToGround(obj, point.x, point.y, point.z);
    return point;
}

void DropToGround(Occupant const& obj, float x, float y, float& z)
{
    if (auto floor = obj.GetMap()->Floor(x, y, z))
    {
        z = *floor + 0.05f;
    }
}

void ClampToAllowedZ(Occupant const& obj, float x, float y, float& z, Map* atMap )
{
    if (!atMap)
    {
        atMap = obj.GetMap();
    }

    const auto floor = atMap->Floor(x, y, z);
    if (!floor)
    {
        return;
    }

    const bool isUnit = IsCreature(&obj) || IsPlayer(&obj);
    if (!isUnit)
    {
        z = *floor;
        return;
    }

    const Unit& unit = static_cast<const Unit&>(obj);
    if (unit.CanFly())
    {
        if (z < *floor)
        {
            z = *floor;
        }
        return;
    }

    float ceiling = *floor;
    if (unit.CanSwim())
    {
        ceiling = atMap->GetTerrain()->GetWaterOrGroundLevel(
                      x, y, z, nullptr, !unit.HasAuraType(SPELL_AURA_WATER_WALK));
    }

    if (z > ceiling)
    {
        z = ceiling;
    }
    else if (z < *floor)
    {
        z = *floor;
    }
}

static bool InCommonFrame(Occupant const& a, Occupant const& b,
                          Geometry::Placement& outA, Geometry::Placement& outB)
{

    if (a.Where().ShareFrame(b.Where()))
    {
        outA = a.Where();
        outB = b.Where();
        return true;
    }

    TransportMap* va = a.GetMap() ? a.GetMap()->AsTransport() : nullptr;
    TransportMap* vb = b.GetMap() ? b.GetMap()->AsTransport() : nullptr;

    if (!va && !vb)
    {
        outA = a.Where();
        outB = b.Where();
        return true;
    }

    if (va != vb)
    {
        return false;
    }

    const auto la = va->PositionOf(a);
    const auto lb = va->PositionOf(b);
    if (!la || !lb)
    {
        return false;
    }

    outA = *la;
    outB = *lb;
    return true;
}

bool CanInteract(Occupant const& a, Occupant const& b)
{
    Geometry::Placement pa, pb;
    return a.IsInWorld() && b.IsInWorld() &&
           InCommonFrame(a, b, pa, pb) && pa.ShareFrame(pb);
}

bool CanBeSeen(Occupant const& seen, Occupant const& viewer)
{
    if (!seen.IsInWorld() || !viewer.IsInWorld())
    {
        return false;
    }

    if (seen.Where().ShareFrame(viewer.Where()))
    {
        return true;
    }

    if (Transport* aboard = Transport::VesselOf(seen))
    {
        if (aboard->GetMap() == viewer.GetMap() || aboard == &viewer)
        {
            return true;
        }
    }

    if (Transport* watching = Transport::VesselOf(viewer))
    {
        if (watching->GetMap() == seen.GetMap() || watching == &seen)
        {
            return true;
        }
    }

    return false;
}

bool SeenWithin(Occupant const& seen, Occupant const& viewer, float dist, bool is3D)
{
    if (!CanBeSeen(seen, viewer))
    {
        return false;
    }

    if (seen.Where().ShareFrame(viewer.Where()))
    {
        return seen.Where().WithinDist(viewer.Where(), dist, is3D);
    }

    return true;
}

bool InReach(Occupant const& a, Occupant const& b, float dist, bool is3D)
{
    Geometry::Placement pa, pb;
    return CanInteract(a, b) && InCommonFrame(a, b, pa, pb) &&
           pa.WithinDist(pb, dist, is3D);
}

bool InFrontPhased(Occupant const& a, Occupant const& b, float dist, float arc)
{
    Geometry::Placement pa, pb;
    return CanInteract(a, b) && InCommonFrame(a, b, pa, pb) &&
           pa.IsInFront(pb, dist, arc);
}

bool InBackPhased(Occupant const& a, Occupant const& b, float dist, float arc)
{
    Geometry::Placement pa, pb;
    return CanInteract(a, b) && InCommonFrame(a, b, pa, pb) &&
           pa.IsInBack(pb, dist, arc);
}

bool HasLineOfSight(Occupant const& a, Geometry::Vector3 const& point)
{

    return a.GetMap()->IsInLineOfSight(a.Where().X(), a.Where().Y(), a.Where().Z() + 2.0f,
                                       point.x, point.y, point.z + 2.0f);
}

bool HasLineOfSight(Occupant const& a, Occupant const& b)
{
    if (!CanInteract(a, b))
    {
        return false;
    }

    if (TransportMap* hull = a.GetMap() ? a.GetMap()->AsTransport() : nullptr)
    {
        Geometry::Placement pa, pb;
        if (!InCommonFrame(a, b, pa, pb))
        {
            return false;
        }
        return !hull->IsBlocked(
            Geometry::Vector3(pa.X(), pa.Y(), pa.Z() + 2.0f),
            Geometry::Vector3(pb.X(), pb.Y(), pb.Z() + 2.0f));
    }

    return HasLineOfSight(a, b.Where().Pos());
}

bool IsPlaceable(Occupant const& obj)
{
    return obj.Where().IsFinite() &&
           MaNGOS::IsValidMapCoord(obj.Where().X(), obj.Where().Y(),
                                   obj.Where().Z(), obj.Where().Facing());
}
