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
#include <vector>
#include <set>
#include "Camera.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Log.h"
#include "Errors.h"
#include "Player.h"
#include "TransportMap.h"

Camera::Camera(Player* pl) : m_owner(*pl), m_source(pl)
{
    m_source->GetViewPoint().Attach(this);
}

Camera::~Camera()
{

    MANGOS_ASSERT(m_source == &m_owner);

    m_source->GetViewPoint().Detach(this);
}

void Camera::ReceivePacket(WorldPacket* data)
{
    m_owner.SendDirectMessage(data);
}

void Camera::UpdateForCurrentViewPoint()
{
    m_gridRef.unlink();

    if (GridType* grid = m_source->GetViewPoint().m_grid)
    {
        grid->AddWorldObject(this);
    }

    UpdateVisibilityForOwner();
}

void Camera::SetView(Occupant* obj, bool update_far_sight_field )
{
    MANGOS_ASSERT(obj);

    if (m_source == obj)
    {
        return;
    }

    if (!CanBeSeen(*obj, m_owner))
    {
        sLog.outError("Camera::SetView, viewpoint is not in map with camera's owner");
        return;
    }

    if (!IsType(obj, TypeMask(TYPEMASK_DYNAMICOBJECT | TYPEMASK_UNIT)))
    {
        sLog.outError("Camera::SetView, viewpoint type is not available for client");
        return;
    }

    m_source->GetViewPoint().Detach(this);
    if (!m_source->IsActiveObject())
    {
        m_source->GetMap()->RemoveFromActive(m_source);
    }

    m_source = obj;

    if (!m_source->IsActiveObject())
    {
        m_source->GetMap()->AddToActive(m_source);
    }

    m_source->GetViewPoint().Attach(this);

    if (update_far_sight_field)
    {
        m_owner.SetGuidValue(PLAYER_FARSIGHT, (m_source == &m_owner ? 0 : m_source->GetObjectGuid()));
    }

    UpdateForCurrentViewPoint();
}

void Camera::Event_ViewPointVisibilityChanged()
{
    if (!m_owner.HaveAtClient(m_source))
    {
        ResetView();
    }
}

void Camera::ResetView(bool update_far_sight_field )
{
    SetView(&m_owner, update_far_sight_field);
}

void Camera::Event_AddedToWorld(InitialWorldUpdateBatch* batch)
{
    GridType* grid = m_source->GetViewPoint().m_grid;
    MANGOS_ASSERT(grid);
    grid->AddWorldObject(this);

    UpdateVisibilityForOwnerInBatch(batch);
}

void Camera::Event_RemovedFromWorld()
{
    if (m_source == &m_owner)
    {
        m_gridRef.unlink();
        return;
    }

    ResetView();
}

void Camera::Event_Moved()
{
    m_gridRef.unlink();
    m_source->GetViewPoint().m_grid->AddWorldObject(this);
}

void Camera::UpdateVisibilityOf(Occupant* target)
{
    m_owner.UpdateVisibilityOf(m_source, target);
}

void Camera::UpdateVisibilityOf(Occupant* target, UpdateData& data, std::set<Occupant*>& vis)
{
    m_owner.UpdateVisibilityOf(m_source, target, data, vis);
}

void Camera::UpdateVisibilityForOwner()
{
    UpdateVisibilityForOwnerInBatch(nullptr);
}

void Camera::UpdateVisibilityForOwnerInBatch(InitialWorldUpdateBatch* batch)
{

    float visibilityDistance = m_source->GetVisibilityDistanceOverride();
    if (visibilityDistance <= 0.0f)
    {
        visibilityDistance = m_source->GetMap()->GetVisibilityDistance();
    }

    MaNGOS::VisibleNotifier notifier(*this, batch);
    Cell::VisitAllObjects(m_source, notifier, visibilityDistance, false);

    std::vector<RelaySource> relayed;
    TransportMap::CollectRelaySources(m_source, visibilityDistance, relayed);
    for (RelaySource const& src : relayed)
    {
        if (src.radius > 0.0f)
        {
            Cell::VisitAllObjects(src.x, src.y, src.map, notifier, src.radius, false);
        }
        else
        {
            Cell::VisitAllObjectsInGrid(src.x, src.y, src.map, notifier, false);
        }
    }

    notifier.Notify();
}

ViewPoint::~ViewPoint()
{
    if (!m_cameras.empty())
    {
        sLog.outError("ViewPoint destructor called, but some cameras referenced to it");
    }
}
