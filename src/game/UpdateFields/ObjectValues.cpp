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
#include "Object.h"
#include "ObjectKind.h"
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

#include <sstream>
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

void Object::SetInt32Value(uint16 index, int32 value)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));

    if (m_mirror.Write(index, uint32(value)))
    {
        MarkForClientUpdate();
    }
}

void Object::SetUInt32Value(uint16 index, uint32 value)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));

    if (m_mirror.Write(index, value))
    {
        MarkForClientUpdate();
    }
}

void Object::SetGuidValue(uint16 index, ObjectGuid value)
{
    MANGOS_ASSERT(index + 1 < GetValuesCount() || PrintIndexError(index, true));

    if (m_mirror.WritePair(index, value))
    {
        MarkForClientUpdate();
    }
}

void Object::SetFloatValue(uint16 index, float value)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));

    if (m_mirror.WriteFloat(index, value))
    {
        MarkForClientUpdate();
    }
}

void Object::SetByteValue(uint16 index, uint8 offset, uint8 value)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));
    MANGOS_ASSERT(offset < 4);

    uint32 const shift = offset * 8;
    uint32 const packed = (m_mirror.Read(index) & ~(uint32(0xFF) << shift)) | (uint32(value) << shift);

    if (m_mirror.Write(index, packed))
    {
        MarkForClientUpdate();
    }
}

void Object::SetUInt16Value(uint16 index, uint8 offset, uint16 value)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));
    MANGOS_ASSERT(offset < 2);

    uint32 const shift = offset * 16;
    uint32 const packed = (m_mirror.Read(index) & ~(uint32(0xFFFF) << shift)) | (uint32(value) << shift);

    if (m_mirror.Write(index, packed))
    {
        MarkForClientUpdate();
    }
}

void Object::SetStatFloatValue(uint16 index, float value)
{
    SetFloatValue(index, value < 0.0f ? 0.0f : value);
}

void Object::SetStatInt32Value(uint16 index, int32 value)
{
    SetUInt32Value(index, value < 0 ? 0 : uint32(value));
}

void Object::ApplyModUInt32Value(uint16 index, int32 val, bool apply)
{
    int32 cur = int32(GetUInt32Value(index)) + (apply ? val : -val);
    SetUInt32Value(index, cur < 0 ? 0 : uint32(cur));
}

void Object::ApplyModInt32Value(uint16 index, int32 val, bool apply)
{
    SetInt32Value(index, GetInt32Value(index) + (apply ? val : -val));
}

void Object::ApplyModSignedFloatValue(uint16 index, float val, bool apply)
{
    SetFloatValue(index, GetFloatValue(index) + (apply ? val : -val));
}

void Object::ApplyModPositiveFloatValue(uint16 index, float val, bool apply)
{
    float cur = GetFloatValue(index) + (apply ? val : -val);
    SetFloatValue(index, cur < 0.0f ? 0.0f : cur);
}

void Object::SetFlag(uint16 index, uint32 newFlag)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));

    if (m_mirror.Write(index, m_mirror.Read(index) | newFlag))
    {
        MarkForClientUpdate();
    }
}

void Object::RemoveFlag(uint16 index, uint32 oldFlag)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));

    if (m_mirror.Write(index, m_mirror.Read(index) & ~oldFlag))
    {
        MarkForClientUpdate();
    }
}

void Object::SetByteFlag(uint16 index, uint8 offset, uint8 newFlag)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));
    MANGOS_ASSERT(offset < 4);

    if (m_mirror.Write(index, m_mirror.Read(index) | (uint32(newFlag) << (offset * 8))))
    {
        MarkForClientUpdate();
    }
}

void Object::RemoveByteFlag(uint16 index, uint8 offset, uint8 oldFlag)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));
    MANGOS_ASSERT(offset < 4);

    if (m_mirror.Write(index, m_mirror.Read(index) & ~(uint32(oldFlag) << (offset * 8))))
    {
        MarkForClientUpdate();
    }
}

bool Object::LoadFields(char const* data, uint16 first, uint16 count)
{
    if (!data)
    {
        return false;
    }

    if (!m_mirror.IsOpen())
    {
        _InitValues();
    }

    MANGOS_ASSERT(first + count <= GetValuesCount() || PrintIndexError(first, true));

    Tokens tokens = StrSplit(data, " ");
    if (tokens.size() != count)
    {
        return false;
    }

    uint16 index = first;
    for (const auto& token : tokens)
    {
        m_mirror.Write(index++, uint32(std::strtoul(token.c_str(), nullptr, 10)));
    }

    return true;
}

std::string Object::SaveFields(uint16 first, uint16 count) const
{
    MANGOS_ASSERT(first + count <= GetValuesCount() || PrintIndexError(first, false));

    std::ostringstream out;
    for (uint16 index = first; index < first + count; ++index)
    {
        out << m_mirror.Read(index) << " ";
    }

    return out.str();
}

bool Object::PrintIndexError(uint32 index, bool set) const
{
    sLog.outError("Attempt %s nonexistent value field: %u (count: %u) for object typeid: %u type mask: %u", (set ? "set value to" : "get value from"), index, GetValuesCount(), GetTypeId(), TypeMaskFor(m_objectTypeId));

    return false;
}

bool Object::PrintEntryError(char const* descr) const
{
    sLog.outError("Object Type %u, Entry %u (lowguid %u) with invalid call for %s", GetTypeId(), GetEntry(), GuidCounter(GetObjectGuid()), descr);

    return false;
}

void Object::BuildUpdateDataForPlayer(Player* pl, UpdateDataMapType& update_players)
{
    UpdateDataMapType::iterator iter = update_players.find(pl);

    if (iter == update_players.end())
    {
        std::pair<UpdateDataMapType::iterator, bool> p = update_players.insert(UpdateDataMapType::value_type(pl, UpdateData()));
        MANGOS_ASSERT(p.second);
        iter = p.first;
    }

    BuildValuesUpdateBlockForPlayer(&iter->second, iter->first);
}

void Object::MarkForClientUpdate()
{
    if (m_inWorld)
    {
        if (!m_objectUpdated)
        {
            AddToClientUpdateList();
            m_objectUpdated = true;
        }
    }
}
