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

#include "Geometry/Placement.h"
#include <cmath>
#include "Utilities/Errors.h"
#include "Utilities/MathDefines.h"
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
#include "Corpse.h"

#define OCCUPY_POS_DEPTH_FACTOR                          1.8f

namespace MaNGOS
{

    class NearUsedPosDo
    {
        public:

            NearUsedPosDo(Occupant const& obj, Occupant const* searcher, float absAngle, ObjectPosSelector& selector)
                : i_object(obj), i_searcher(searcher), i_absAngle(Geometry::Placement::NormalizeOrientation(absAngle)), i_selector(selector) {}

            void operator()(Corpse*) const {}
            void operator()(DynamicObject*) const {}

            void operator()(Creature* c) const
            {

                if (c == i_searcher || c == &i_object)
                {
                    return;
                }

                float x, y, z;

                if (c->IsStopped() || !c->GetMotionMaster()->GetDestination(x, y, z))
                {
                    x = c->Where().X();
                    y = c->Where().Y();
                }

                add(c, x, y);
            }

            template<class T>
                void operator()(T* u) const
            {

                if (u == i_searcher || u == &i_object)
                {
                    return;
                }

                float x, y;

                x = u->Where().X();
                y = u->Where().Y();

                add(u, x, y);
            }

            void add(Occupant* u, float x, float y) const
            {
                float dx = i_object.Where().X() - x;
                float dy = i_object.Where().Y() - y;
                float dist2d = sqrt((dx * dx) + (dy * dy));

                float delta = u->Where().Extent();
                if (i_selector.m_searchPosFor && i_selector.m_searchPosFor != u)
                {
                    delta += i_selector.m_searchPosFor->Where().Extent();
                }

                delta *= OCCUPY_POS_DEPTH_FACTOR;

                if (fabs(i_selector.m_searcherDist - dist2d) > delta)
                {
                    return;
                }

                float angle = i_object.Where().BearingTo(u->Where()) - i_absAngle;

                if (angle > M_PI_F)
                {
                    angle -= 2.0f * M_PI_F;
                }
                else if (angle < -M_PI_F)
                {
                    angle += 2.0f * M_PI_F;
                }

                i_selector.AddUsedArea(u, angle, dist2d);
            }
        private:
            Occupant const& i_object;
            Occupant const* i_searcher;
            float              i_absAngle;
            ObjectPosSelector& i_selector;
    };
}

Geometry::Vector3 PointNear(Occupant const& anchor, float distance2d, float absAngle)
{
    Geometry::Vector3 point = anchor.Where().PointAt(distance2d, absAngle);
    MaNGOS::NormalizeMapCoord(point.x);
    MaNGOS::NormalizeMapCoord(point.y);
    return point;
}

void FindFreeSpotNear(Occupant const& anchor, Occupant const* searcher, float& x, float& y, float& z,
                      float searcher_bounding_radius, float distance2d, float absAngle)
{
    const Geometry::Vector3 first = PointNear(anchor, distance2d, absAngle);
    x = first.x;
    y = first.y;
    const float init_z = z = anchor.Where().Z();

    if (!sWorld.getConfig(CONFIG_BOOL_DETECT_POS_COLLISION))
    {
        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }
        return;
    }

    float first_x = x;
    float first_y = y;
    bool first_los_conflict = false;

    const float dist = distance2d + searcher_bounding_radius + anchor.Where().Extent();

    ObjectPosSelector selector(anchor.Where().X(), anchor.Where().Y(), distance2d, searcher_bounding_radius, searcher);

    {
        MaNGOS::NearUsedPosDo u_do(anchor, searcher, absAngle, selector);
        MaNGOS::OccupantWorker<MaNGOS::NearUsedPosDo> worker(u_do);

        Cell::VisitAllObjects(&anchor, worker, dist);
    }

    if (selector.CheckOriginalAngle())
    {
        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }

        if (fabs(init_z - z) < dist && HasLineOfSight(anchor, Geometry::Vector3(x, y, z)))
        {
            return;
        }

        first_los_conflict = true;
    }

    selector.InitializeAngle();

    float angle;

    while (selector.NextAngle(angle))
    {
        const Geometry::Vector3 candidate = PointNear(anchor, distance2d, absAngle + angle);
        x = candidate.x;
        y = candidate.y;
        z = anchor.Where().Z();

        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }

        if (fabs(init_z - z) < dist && HasLineOfSight(anchor, Geometry::Vector3(x, y, z)))
        {
            return;
        }
    }

    if (!first_los_conflict)
    {
        x = first_x;
        y = first_y;

        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }
        return;
    }

    selector.InitializeAngle();

    while (selector.NextUsedAngle(angle))
    {
        const Geometry::Vector3 candidate = PointNear(anchor, distance2d, absAngle + angle);
        x = candidate.x;
        y = candidate.y;
        z = anchor.Where().Z();

        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }

        if (fabs(init_z - z) < dist && HasLineOfSight(anchor, Geometry::Vector3(x, y, z)))
        {
            return;
        }
    }

    x = first_x;
    y = first_y;

    if (searcher)
    {
        ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());
    }
    else
    {
        DropToGround(anchor, x, y, z);
    }
}

void ClosePointNear(Occupant const& anchor, float& x, float& y, float& z, float bounding_radius,
                    float distance2d, float angle, Occupant const* searcher)
{
    FindFreeSpotNear(anchor, searcher, x, y, z, bounding_radius,
                     Geometry::Placement::ContactSpread(distance2d, anchor.Where().Extent(), bounding_radius),
                     anchor.Where().Facing() + angle);
}

void ContactPointNear(Occupant const& anchor, Occupant const* obj, float& x, float& y, float& z,
                      float distance2d)
{
    FindFreeSpotNear(anchor, obj, x, y, z, obj->Where().Extent(),
                     Geometry::Placement::ContactSpread(distance2d, anchor.Where().Extent(),
                                                        obj->Where().Extent()),
                     anchor.Where().BearingTo(obj->Where()));
}
