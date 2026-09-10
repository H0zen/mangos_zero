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

void Occupant::SetMap(Map* map)
{
    MANGOS_ASSERT(map);
    m_currMap = map;

    m_mapId = map->GetId();
    m_InstanceId = map->GetInstanceId();
    RefreshFrame();
}

TerrainInfo const* Occupant::GetTerrain() const
{
    MANGOS_ASSERT(m_currMap);
    return m_currMap->GetTerrain();
}

void Occupant::AddObjectToRemoveList()
{
    GetMap()->AddObjectToRemoveList(this);
}

void Occupant::UpdateVisibilityAndView()
{
    GetViewPoint().Call_UpdateVisibilityForOwner();
    UpdateObjectVisibility();
    GetViewPoint().Event_ViewPointVisibilityChanged();
}

void Occupant::UpdateObjectVisibility()
{
    CellPair p = MaNGOS::ComputeCellPair(Where().X(), Where().Y());
    Cell cell(p);

    GetMap()->UpdateObjectVisibility(this, cell, p);
}

void Occupant::AddToClientUpdateList()
{
    GetMap()->Backlog().Add(this);
}

void Occupant::RemoveFromClientUpdateList()
{
    GetMap()->Backlog().Forget(this);
}

struct OccupantChangeAccumulator
{
    UpdateDataMapType& i_updateDatas;
    Occupant& i_object;

    OccupantChangeAccumulator(Occupant& obj, UpdateDataMapType& d) : i_updateDatas(d), i_object(obj)
    {

        if (IsType(&i_object, TYPEMASK_PLAYER))
        {
            i_object.BuildUpdateDataForPlayer((Player*)&i_object, i_updateDatas);
        }
    }

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

    template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
};

void Occupant::BuildUpdateData(UpdateDataMapType& update_players)
{
    OccupantChangeAccumulator notifier(*this, update_players);
    Cell::VisitWorldObjects(this, notifier, GetMap()->GetBroadcastRadius());

    ClearUpdateMask(false);
}

bool Occupant::PrintCoordinatesError(float x, float y, float z, char const* descr) const
{
    sLog.outError("%s with invalid %s coordinates: mapid = %uu, x = %f, y = %f, z = %f", GetGuidStr().c_str(), descr, GetMapId(), x, y, z);
    return false;
}

void Occupant::SetActiveObjectState(bool active)
{
    if (m_isActiveObject == active || (IsType(this, TYPEMASK_PLAYER) && !active))
    {
        return;
    }

    if (IsInWorld() && !IsType(this, TYPEMASK_PLAYER))
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
