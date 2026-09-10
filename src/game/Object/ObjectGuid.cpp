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

#include <string>
#include "ObjectGuid.h"

#include "World.h"
#include "ObjectMgr.h"

#include <sstream>

static bool HasEntry(HighGuid high)
{
    switch (high)
    {
        case HIGHGUID_ITEM:
        case HIGHGUID_PLAYER:
        case HIGHGUID_DYNAMICOBJECT:
        case HIGHGUID_CORPSE:
        case HIGHGUID_MO_TRANSPORT:
            return false;
        default:
            return true;
    }
}

static char const* TypeName(HighGuid high)
{
    switch (high)
    {
        case HIGHGUID_ITEM:          return "Item";
        case HIGHGUID_PLAYER:        return "Player";
        case HIGHGUID_GAMEOBJECT:    return "Gameobject";
        case HIGHGUID_TRANSPORT:     return "Transport";
        case HIGHGUID_UNIT:          return "Creature";
        case HIGHGUID_PET:           return "Pet";
        case HIGHGUID_DYNAMICOBJECT: return "DynObject";
        case HIGHGUID_CORPSE:        return "Corpse";
        case HIGHGUID_MO_TRANSPORT:  return "MoTransport";
        default:                     return "<unknown>";
    }
}

uint32 GuidEntry(ObjectGuid guid)
{
    return HasEntry(GuidHigh(guid)) ? uint32((guid >> 24) & UI64LIT(0x0000000000FFFFFF)) : 0;
}

uint32 GuidCounter(ObjectGuid guid)
{
    return HasEntry(GuidHigh(guid))
           ? uint32(guid & UI64LIT(0x0000000000FFFFFF))
           : uint32(guid & UI64LIT(0x00000000FFFFFFFF));
}

uint32 GuidMaxCounter(HighGuid high)
{
    return HasEntry(high) ? uint32(0x00FFFFFF) : uint32(0xFFFFFFFF);
}

ObjectGuid MakeGuid(HighGuid high, uint32 entry, uint32 counter)
{
    return counter ? uint64(counter) | (uint64(entry) << 24) | (uint64(high) << 48) : 0;
}

ObjectGuid MakeGuid(HighGuid high, uint32 counter)
{
    return counter ? uint64(counter) | (uint64(high) << 48) : 0;
}

TypeID GuidTypeId(HighGuid high)
{
    switch (high)
    {
        case HIGHGUID_ITEM:          return TYPEID_ITEM;
        case HIGHGUID_UNIT:          return TYPEID_UNIT;
        case HIGHGUID_PET:           return TYPEID_UNIT;
        case HIGHGUID_PLAYER:        return TYPEID_PLAYER;
        case HIGHGUID_GAMEOBJECT:    return TYPEID_GAMEOBJECT;
        case HIGHGUID_DYNAMICOBJECT: return TYPEID_DYNAMICOBJECT;
        case HIGHGUID_CORPSE:        return TYPEID_CORPSE;
        case HIGHGUID_MO_TRANSPORT:  return TYPEID_GAMEOBJECT;
        case HIGHGUID_TRANSPORT:     return TYPEID_GAMEOBJECT;
        default:                     return TYPEID_OBJECT;
    }
}

std::string GuidString(ObjectGuid guid)
{
    if (!guid)
    {
        return "None";
    }

    HighGuid const high = GuidHigh(guid);

    std::ostringstream str;
    str << TypeName(high);

    if (high == HIGHGUID_PLAYER)
    {
        std::string name;
        if (sObjectMgr.GetPlayerNameByGUID(guid, name))
        {
            str << " " << name;
        }
    }

    str << " (";
    if (HasEntry(high))
    {
        str << (high == HIGHGUID_PET ? "Petnumber: " : "Entry: ") << GuidEntry(guid) << " ";
    }
    str << "Guid: " << GuidCounter(guid) << ")";
    return str.str();
}

template<HighGuid high>
uint32 ObjectGuidGenerator<high>::Generate()
{
    if (m_nextGuid >= GuidMaxCounter(high) - 1)
    {
        sLog.outError("%s guid overflow!! Can't continue, shutting down server. ", TypeName(high));
        World::StopNow(ERROR_EXIT_CODE);

        return GuidMaxCounter(high);
    }
    return m_nextGuid++;
}

ByteBuffer& operator<< (ByteBuffer& buf, PackedGuid const& guid)
{
    buf.append(guid.m_packedGuid);
    return buf;
}

ByteBuffer& operator>>(ByteBuffer& buf, PackedGuidReader const& guid)
{
    *guid.m_guidPtr = buf.readPackGUID();
    return buf;
}

template uint32 ObjectGuidGenerator<HIGHGUID_ITEM>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_PLAYER>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_GAMEOBJECT>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_UNIT>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_PET>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_DYNAMICOBJECT>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_CORPSE>::Generate();
