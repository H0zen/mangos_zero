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

#include "Object.h"
#include "ObjectKind.h"
#include "Utilities/Errors.h"
#include "Log.h"

Object::Object()
{
    m_objectTypeId      = TYPEID_OBJECT;

    m_inWorld           = false;
    m_objectUpdated     = false;
}

Object::~Object()
{
    if (IsInWorld())
    {

        sLog.outError("Object::~Object (GUID: %u TypeId: %u) deleted but still in world!!", GetGUIDLow(), GetTypeId());
        MANGOS_ASSERT(false);
    }

    if (m_objectUpdated)
    {
        sLog.outError("Object::~Object (GUID: %u TypeId: %u) deleted but still have updated status!!", GetGUIDLow(), GetTypeId());
        MANGOS_ASSERT(false);
    }

}

void Object::_InitValues()
{
    m_mirror.Open(m_objectTypeId);

    m_objectUpdated = false;
}

void Object::_Create(uint32 guidlow, uint32 entry, HighGuid guidhigh)
{
    if (!m_mirror.IsOpen())
    {
        _InitValues();
    }

    ObjectGuid guid = MakeGuid(guidhigh, entry, guidlow);
    SetGuidValue(OBJECT_FIELD_GUID, guid);
    SetUInt32Value(OBJECT_FIELD_TYPE, TypeMaskFor(m_objectTypeId));
    m_PackGUID.Set(guid);
}

void Object::_ReCreate(uint32 entry)
{
    if (!m_mirror.IsOpen())
    {
        _InitValues();
    }

    SetUInt32Value(OBJECT_FIELD_TYPE, TypeMaskFor(m_objectTypeId));
    SetUInt32Value(OBJECT_FIELD_ENTRY, entry);
}

void Object::SetObjectScale(float newScale)
{
    SetFloatValue(OBJECT_FIELD_SCALE_X, newScale);
    OnScaleChanged();
}

void Object::ResendField(uint16 index)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));

    m_mirror.Touch(index);
    MarkForClientUpdate();
}
