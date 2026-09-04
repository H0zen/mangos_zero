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

/**
 * @file Object.cpp
 * @brief Base implementation for all game objects
 *
 * This file implements the Object class, which is the base class for all
 * entities in the game world. It provides:
 * - Update field management (synchronized with clients)
 * - Object GUID handling
 * - Update data building for network transmission
 * - Object visibility and spawning
 * - Type identification
 *
 * The Object class uses an array of uint32 values (update fields) that
 * mirror the client's object state. Changes to these values are sent to
 * players who can see the object.
 */



#include "Geometry/Placement.h"
#include <cmath>
#include "Utilities/Errors.h"
#include "Utilities/MathDefines.h"
#include "Presence.h"
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
#include "MapManager.h"
#include "Transports.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "movement/packet_builder.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"
#include "Corpse.h"

// how much space should be left in front of/ behind a mob that already uses a space
#define OCCUPY_POS_DEPTH_FACTOR                          1.8f

namespace MaNGOS
{

    /**
     * @brief Near used position functor
     *
     * Checks for used positions near an object for position selection.
     */
    class NearUsedPosDo
    {
        public:
            /**
             * @brief Constructor
             * @param obj Source object
             * @param searcher Object searching for position
             * @param absAngle Absolute angle
             * @param selector Position selector
             */
            NearUsedPosDo(Presence const& obj, Presence const* searcher, float absAngle, ObjectPosSelector& selector)
                : i_object(obj), i_searcher(searcher), i_absAngle(Geometry::Placement::NormalizeOrientation(absAngle)), i_selector(selector) {}

            void operator()(Corpse*) const {}
            void operator()(DynamicObject*) const {}

            /**
             * @brief Process creature
             * @param c Creature to process
             */
            void operator()(Creature* c) const
            {
                // skip self or target
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

            /**
             * @brief Process generic unit
             * @param u Unit to process
             */
            template<class T>
                void operator()(T* u) const
            {
                // skip self or target
                if (u == i_searcher || u == &i_object)
                {
                    return;
                }

                float x, y;

                x = u->Where().X();
                y = u->Where().Y();

                add(u, x, y);
            }

            /**
             * @brief Add used position
             * @param u Object to add
             * @param x X coordinate
             * @param y Y coordinate
             *
             * Adds a used position to the selector.
             */
            void add(Presence* u, float x, float y) const
            {
                float dx = i_object.Where().X() - x;
                float dy = i_object.Where().Y() - y;
                float dist2d = sqrt((dx * dx) + (dy * dy));

                // It is ok for the objects to require a bit more space
                float delta = u->Where().Extent();
                if (i_selector.m_searchPosFor && i_selector.m_searchPosFor != u)
                {
                    delta += i_selector.m_searchPosFor->Where().Extent();
                }

                delta *= OCCUPY_POS_DEPTH_FACTOR;           // Increase by factor

                // u is too near/far away from i_object. Do not consider it to occupy space
                if (fabs(i_selector.m_searcherDist - dist2d) > delta)
                {
                    return;
                }

                float angle = i_object.Where().BearingTo(u->Where()) - i_absAngle;

                // move angle to range -pi ... +pi, range before is -2Pi..2Pi
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
            Presence const& i_object;
            Presence const* i_searcher;
            float              i_absAngle;
            ObjectPosSelector& i_selector;
    };
}                                                           // namespace MaNGOS

// A point the component constructed, pulled back inside the map's coordinate bounds --
// which is the map's business, not the geometry's.
Geometry::Vector3 PointNear(Presence const& anchor, float distance2d, float absAngle)
{
    Geometry::Vector3 point = anchor.Where().PointAt(distance2d, absAngle);
    MaNGOS::NormalizeMapCoord(point.x);
    MaNGOS::NormalizeMapCoord(point.y);
    return point;
}

/**
 * @brief Finds a nearby point while accounting for collisions and line of sight.
 *
 * @param searcher The object requesting the position.
 * @param x Receives the resulting x coordinate.
 * @param y Receives the resulting y coordinate.
 * @param z Receives the resulting z coordinate.
 * @param searcher_bounding_radius The requester's bounding radius.
 * @param distance2d The desired distance from the anchor.
 * @param absAngle The preferred absolute angle.
 */
void FindFreeSpotNear(Presence const& anchor, Presence const* searcher, float& x, float& y, float& z,
                      float searcher_bounding_radius, float distance2d, float absAngle)
{
    const Geometry::Vector3 first = PointNear(anchor, distance2d, absAngle);
    x = first.x;
    y = first.y;
    const float init_z = z = anchor.Where().Z();

    // if detection disabled, return first point
    if (!sWorld.getConfig(CONFIG_BOOL_DETECT_POS_COLLISION))
    {
        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());       // update to LOS height if available
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }
        return;
    }

    // or remember first point
    float first_x = x;
    float first_y = y;
    bool first_los_conflict = false;                        // first point LOS problems

    const float dist = distance2d + searcher_bounding_radius + anchor.Where().Extent();

    // prepare selector for work
    ObjectPosSelector selector(anchor.Where().X(), anchor.Where().Y(), distance2d, searcher_bounding_radius, searcher);

    // adding used positions around object
    {
        MaNGOS::NearUsedPosDo u_do(anchor, searcher, absAngle, selector);
        MaNGOS::PresenceWorker<MaNGOS::NearUsedPosDo> worker(u_do);

        Cell::VisitAllObjects(&anchor, worker, dist);
    }

    // maybe can just place in primary position
    if (selector.CheckOriginalAngle())
    {
        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());       // update to LOS height if available
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }

        if (fabs(init_z - z) < dist && HasLineOfSight(anchor, Geometry::Vector3(x, y, z)))
        {
            return;
        }

        first_los_conflict = true;                          // first point have LOS problems
    }

    // set first used pos in lists
    selector.InitializeAngle();

    float angle;                                            // candidate of angle for free pos

    // select in positions after current nodes (selection one by one)
    while (selector.NextAngle(angle))                       // angle for free pos
    {
        const Geometry::Vector3 candidate = PointNear(anchor, distance2d, absAngle + angle);
        x = candidate.x;
        y = candidate.y;
        z = anchor.Where().Z();

        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());       // update to LOS height if available
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

    // BAD NEWS: not free pos (or used or have LOS problems)
    // Attempt find _used_ pos without LOS problem
    if (!first_los_conflict)
    {
        x = first_x;
        y = first_y;

        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());       // update to LOS height if available
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }
        return;
    }

    // set first used pos in lists
    selector.InitializeAngle();

    // select in positions after current nodes (selection one by one)
    while (selector.NextUsedAngle(angle))                   // angle for used pos but maybe without LOS problem
    {
        const Geometry::Vector3 candidate = PointNear(anchor, distance2d, absAngle + angle);
        x = candidate.x;
        y = candidate.y;
        z = anchor.Where().Z();

        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());       // update to LOS height if available
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

    // BAD BAD NEWS: all found pos (free and used) have LOS problem :(
    x = first_x;
    y = first_y;

    if (searcher)
    {
        ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());           // update to LOS height if available
    }
    else
    {
        DropToGround(anchor, x, y, z);
    }
}

void ClosePointNear(Presence const& anchor, float& x, float& y, float& z, float bounding_radius,
                    float distance2d, float angle, Presence const* searcher)
{
    FindFreeSpotNear(anchor, searcher, x, y, z, bounding_radius,
                     Geometry::Placement::ContactSpread(distance2d, anchor.Where().Extent(), bounding_radius),
                     anchor.Where().Facing() + angle);
}

void ContactPointNear(Presence const& anchor, Presence const* obj, float& x, float& y, float& z,
                      float distance2d)
{
    FindFreeSpotNear(anchor, obj, x, y, z, obj->Where().Extent(),
                     Geometry::Placement::ContactSpread(distance2d, anchor.Where().Extent(),
                                                        obj->Where().Extent()),
                     anchor.Where().BearingTo(obj->Where()));
}

