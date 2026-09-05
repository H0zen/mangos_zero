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



#include "Utilities/Errors.h"
#include "Object.h"
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

/// Every write goes the same way: the mirror answers whether the value it now
/// holds is different from the one it held, and only a difference is worth
/// telling anybody about.

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

/// Stores without announcing. The caller takes on saying so.
void Object::UpdateUInt32Value(uint16 index, uint32 value)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));

    m_mirror.Write(index, value);
    m_mirror.Touch(index);
}

void Object::SetUInt64Value(uint16 index, const uint64& value)
{
    MANGOS_ASSERT(index + 1 < GetValuesCount() || PrintIndexError(index, true));

    bool const low = m_mirror.Write(index, uint32(value));
    bool const high = m_mirror.Write(index + 1, uint32(value >> 32));

    if (low || high)
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

/// A stat never goes below nothing.
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

void Object::SetShortFlag(uint16 index, bool highpart, uint16 newFlag)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));

    if (m_mirror.Write(index, m_mirror.Read(index) | (uint32(newFlag) << (highpart ? 16 : 0))))
    {
        MarkForClientUpdate();
    }
}

void Object::RemoveShortFlag(uint16 index, bool highpart, uint16 oldFlag)
{
    MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index, true));

    if (m_mirror.Write(index, m_mirror.Read(index) & ~(uint32(oldFlag) << (highpart ? 16 : 0))))
    {
        MarkForClientUpdate();
    }
}

/**
 * @brief Print index error
 * @param index Field index that caused error
 * @param set If true, was a set operation; if false, was a get operation
 * @return Always false
 *
 * Logs an error when attempting to access a nonexistent field.
 */
/**
 * @brief Read a run of fields back from their stored text form
 * @param data Space-separated decimal values
 * @param first Index of the first field the text describes
 * @param count How many fields it must describe
 * @return false when the text does not hold exactly that many values
 *
 * The storage format happens to be the field array written out, which is why a
 * range is enough for both callers: a whole item, or one run of a character's
 * explored zones.
 */
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

/**
 * @brief Write a run of fields out in the form LoadFields reads
 * @param first Index of the first field
 * @param count How many fields to write
 */
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
    sLog.outError("Attempt %s nonexistent value field: %u (count: %u) for object typeid: %u type mask: %u", (set ? "set value to" : "get value from"), index, GetValuesCount(), GetTypeId(), m_objectType);

    // ASSERT must fail after function call
    return false;
}

/**
 * @brief Print entry error
 * @param descr Description of the invalid operation
 * @return Always false
 *
 * Logs an error when an invalid operation is performed on this object.
 */
bool Object::PrintEntryError(char const* descr) const
{
    sLog.outError("Object Type %u, Entry %u (lowguid %u) with invalid call for %s", GetTypeId(), GetEntry(), GetObjectGuid().GetCounter(), descr);

    // always false for continue assert fail
    return false;
}

/**
 * @brief Build update data for player
 * @param pl Target player
 * @param update_players Map of players to their update data
 *
 * Builds update data for the specified player, adding them
 * to the update map if not already present.
 */
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

/**
 * @brief Add to client update list
 *
 * Base implementation logs error and asserts.
 * Derived classes should override this method.
 */
void Object::AddToClientUpdateList()
{
    sLog.outError("Unexpected call of Object::AddToClientUpdateList for object (TypeId: %u Update fields: %u)", GetTypeId(), GetValuesCount());
    MANGOS_ASSERT(false);
}

/**
 * @brief Remove from client update list
 *
 * Base implementation logs error and asserts.
 * Derived classes should override this method.
 */
void Object::RemoveFromClientUpdateList()
{
    sLog.outError("Unexpected call of Object::RemoveFromClientUpdateList for object (TypeId: %u Update fields: %u)", GetTypeId(), GetValuesCount());
    MANGOS_ASSERT(false);
}

/**
 * @brief Build update data
 * @param update_players Map of players to their update data
 *
 * Base implementation logs error and asserts.
 * Derived classes should override this method.
 */
void Object::BuildUpdateData(UpdateDataMapType& /*update_players */)
{
    sLog.outError("Unexpected call of Object::BuildUpdateData for object (TypeId: %u Update fields: %u)", GetTypeId(), GetValuesCount());
    MANGOS_ASSERT(false);
}

/**
 * @brief Mark object for client update
 *
 * Adds the object to the client update list if it's in world
 * and not already marked for update.
 */
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
