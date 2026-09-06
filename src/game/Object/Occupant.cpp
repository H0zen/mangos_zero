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
#include "MapManager.h"
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

/**
 * @brief Assigns the current map context to the world object.
 *
 * @param map The map to assign.
 */
void Occupant::SetMap(Map* map)
{
    MANGOS_ASSERT(map);
    m_currMap = map;
    // lets save current map's Id/instanceId
    m_mapId = map->GetId();
    m_InstanceId = map->GetInstanceId();
    RefreshFrame();
}

TerrainInfo const* Occupant::GetTerrain() const
{
    MANGOS_ASSERT(m_currMap);
    return m_currMap->GetTerrain();
}

/**
 * @brief Schedules the object for removal from the map.
 */
void Occupant::AddObjectToRemoveList()
{
    GetMap()->AddObjectToRemoveList(this);
}

/**
 * @brief Refreshes both visibility and viewpoint-dependent visibility state.
 */
void Occupant::UpdateVisibilityAndView()
{
    GetViewPoint().Call_UpdateVisibilityForOwner();
    UpdateObjectVisibility();
    GetViewPoint().Event_ViewPointVisibilityChanged();
}

/**
 * @brief Recomputes this object's visibility for nearby clients.
 */
void Occupant::UpdateObjectVisibility()
{
    CellPair p = MaNGOS::ComputeCellPair(Where().X(), Where().Y());
    Cell cell(p);

    GetMap()->UpdateObjectVisibility(this, cell, p);
}

/**
 * @brief Adds the world object to the map's update queue.
 */
void Occupant::AddToClientUpdateList()
{
    GetMap()->Backlog().Add(this);
}

/**
 * @brief Remove from client update list
 *
 * Removes this object from the map's update list.
 */
void Occupant::RemoveFromClientUpdateList()
{
    GetMap()->Backlog().Forget(this);
}

/**
 * @brief World object change accumulator
 *
 * Accumulates update data for a world object and nearby players.
 */
struct OccupantChangeAccumulator
{
    UpdateDataMapType& i_updateDatas; ///< Update data map
    Occupant& i_object; ///< World object

    /**
     * @brief Constructor
     * @param obj World object
     * @param d Update data map
     */
    OccupantChangeAccumulator(Occupant& obj, UpdateDataMapType& d) : i_updateDatas(d), i_object(obj)
    {
        // send self fields changes in another way, otherwise
        // with new camera system when player's camera too far from player, camera wouldn't receive packets and changes from player
        if (i_object.isType(TYPEMASK_PLAYER))
        {
            i_object.BuildUpdateDataForPlayer((Player*)&i_object, i_updateDatas);
        }
    }

    /**
     * @brief Visit cameras
     * @param m Camera map
     *
     * Builds update data for all camera owners that can see this object.
     */
    void Visit(CameraMapType& m)
    {
        for (CameraMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
        {
            Player* owner = iter->getSource()->GetOwner();
            if (owner != &i_object && owner->HaveAtClient(&i_object))
            {
                i_object.BuildUpdateDataForPlayer(owner, i_updateDatas);
            }
        }
    }

    /**
     * @brief Visit other grid references (no-op)
     */
    template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
};

/**
 * @brief Build update data
 * @param update_players Map of players to their update data
 *
 * Builds update data for all players who can see this object.
 */
void Occupant::BuildUpdateData(UpdateDataMapType& update_players)
{
    OccupantChangeAccumulator notifier(*this, update_players);
    Cell::VisitWorldObjects(this, notifier, GetMap()->GetBroadcastRadius());

    ClearUpdateMask(false);
}

/**
 * @brief Print coordinates error
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param descr Description of the operation
 * @return Always false
 *
 * Logs an error when invalid coordinates are encountered.
 */
bool Occupant::PrintCoordinatesError(float x, float y, float z, char const* descr) const
{
    sLog.outError("%s with invalid %s coordinates: mapid = %uu, x = %f, y = %f, z = %f", GetGuidStr().c_str(), descr, GetMapId(), x, y, z);
    return false;                                           // always false for continue assert fail
}

/**
 * @brief Set active object state
 * @param active If true, set as active object
 *
 * Sets whether this object is an active object (updated even when no players nearby).
 */
void Occupant::SetActiveObjectState(bool active)
{
    if (m_isActiveObject == active || (isType(TYPEMASK_PLAYER) && !active))  // player shouldn't became inactive, never
    {
        return;
    }

    // player's update implemented in a different from other active occupant's way
    // it's considered to use generic way in future
    if (IsInWorld() && !isType(TYPEMASK_PLAYER))
    {
        if (IsActiveObject() && !active)
        {
            GetMap()->RemoveFromActive(this);
        }
        else if (IsActiveObject() && active)
        {
            GetMap()->AddToActive(this);
        }
    }
    m_isActiveObject = active;
}

