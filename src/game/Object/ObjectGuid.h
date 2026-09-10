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

#pragma once

#include "Platform/Define.h"
#include <string>
#include <vector>
#include <set>
#include <list>
#include <functional>
#include "ByteBuffer.h"

enum TypeID
{
    TYPEID_OBJECT        = 0,
    TYPEID_ITEM          = 1,
    TYPEID_CONTAINER     = 2,
    TYPEID_UNIT          = 3,
    TYPEID_PLAYER        = 4,
    TYPEID_GAMEOBJECT    = 5,
    TYPEID_DYNAMICOBJECT = 6,
    TYPEID_CORPSE        = 7
};

#define MAX_TYPE_ID        8

enum TypeMask
{
    TYPEMASK_OBJECT         = 0x0001,
    TYPEMASK_ITEM           = 0x0002,
    TYPEMASK_CONTAINER      = 0x0004,
    TYPEMASK_UNIT           = 0x0008,
    TYPEMASK_PLAYER         = 0x0010,
    TYPEMASK_GAMEOBJECT     = 0x0020,
    TYPEMASK_DYNAMICOBJECT  = 0x0040,
    TYPEMASK_CORPSE         = 0x0080,

    TYPEMASK_CREATURE_OR_GAMEOBJECT = TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT,
    TYPEMASK_CREATURE_GAMEOBJECT_OR_ITEM = TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT | TYPEMASK_ITEM,
    TYPEMASK_CREATURE_GAMEOBJECT_PLAYER_OR_ITEM = TYPEMASK_UNIT | TYPEMASK_GAMEOBJECT | TYPEMASK_ITEM | TYPEMASK_PLAYER,

    TYPEMASK_PRESENCE = TYPEMASK_UNIT | TYPEMASK_PLAYER | TYPEMASK_GAMEOBJECT | TYPEMASK_DYNAMICOBJECT | TYPEMASK_CORPSE,
};

enum HighGuid
{
    HIGHGUID_ITEM           = 0x4000,
    HIGHGUID_CONTAINER      = 0x4000,
    HIGHGUID_PLAYER         = 0x0000,
    HIGHGUID_GAMEOBJECT     = 0xF110,
    HIGHGUID_TRANSPORT      = 0xF120,
    HIGHGUID_UNIT           = 0xF130,
    HIGHGUID_PET            = 0xF140,
    HIGHGUID_DYNAMICOBJECT  = 0xF100,
    HIGHGUID_CORPSE         = 0xF101,
    HIGHGUID_MO_TRANSPORT   = 0x1FC0,
};

class PackedGuid;

typedef uint64 ObjectGuid;

inline HighGuid GuidHigh(ObjectGuid guid) { return HighGuid((guid >> 48) & 0x0000FFFF); }

uint32 GuidEntry(ObjectGuid guid);
uint32 GuidCounter(ObjectGuid guid);
uint32 GuidMaxCounter(HighGuid high);

ObjectGuid MakeGuid(HighGuid high, uint32 entry, uint32 counter);
ObjectGuid MakeGuid(HighGuid high, uint32 counter);

TypeID GuidTypeId(HighGuid high);
std::string GuidString(ObjectGuid guid);

typedef std::set<ObjectGuid> GuidSet;
typedef std::list<ObjectGuid> GuidList;
typedef std::vector<ObjectGuid> GuidVector;

#define PACKED_GUID_MIN_BUFFER_SIZE 9

struct PackedGuidReader
{
    explicit PackedGuidReader(ObjectGuid& guid) : m_guidPtr(&guid) {}
    ObjectGuid* m_guidPtr;
};

class PackedGuid
{
    friend ByteBuffer& operator<< (ByteBuffer& buf, PackedGuid const& guid);

    public:
        explicit PackedGuid() : m_packedGuid(PACKED_GUID_MIN_BUFFER_SIZE) { m_packedGuid.appendPackGUID(0); }
        explicit PackedGuid(ObjectGuid guid) : m_packedGuid(PACKED_GUID_MIN_BUFFER_SIZE) { m_packedGuid.appendPackGUID(guid); }

        void Set(ObjectGuid guid) { m_packedGuid.wpos(0); m_packedGuid.appendPackGUID(guid); }

        size_t size() const { return m_packedGuid.size(); }

    private:
        ByteBuffer m_packedGuid;
};

inline PackedGuid PackGuid(ObjectGuid guid) { return PackedGuid(guid); }
inline PackedGuidReader ReadPackedGuid(ObjectGuid& guid) { return PackedGuidReader(guid); }

template<HighGuid high>
    class ObjectGuidGenerator
{
    public:
        explicit ObjectGuidGenerator(uint32 start = 1) : m_nextGuid(start) {}

        void Set(uint32 val) { m_nextGuid = val; }
        uint32 Generate();

        uint32 GetNextAfterMaxUsed() const { return m_nextGuid; }

    private:
        uint32 m_nextGuid;
};

ByteBuffer& operator<< (ByteBuffer& buf, PackedGuid const& guid);
ByteBuffer& operator>> (ByteBuffer& buf, PackedGuidReader const& guid);
