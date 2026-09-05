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

#include <unordered_map>
#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Utilities/MathDefines.h"
#include <ctime>
#include <vector>
#include <string>
#include <map>
#include "ByteBuffer.h"
#include "UpdateFields.h"
#include "Mirror.h"
#include "UpdateData.h"
#include "ObjectGuid.h"
#include "Camera.h"
#include "GameTime.h"
#include "Geometry/Placement.h"

#include <set>

#define DEFAULT_OBJECT_SCALE        1.0f                    // non-Tauren player/item scale as default, npc/go from database, pets from dbc
#define DEFAULT_TAUREN_MALE_SCALE   1.35f                   // Tauren male player scale by default
#define DEFAULT_TAUREN_FEMALE_SCALE 1.25f                   // Tauren female player scale by default

class WorldPacket;
class UpdateData;
class WorldSession;
class Creature;
class GameObject;
class Player;
class Unit;
class Group;
class Map;
class InstanceData;
class TerrainInfo;
struct MangosStringLocale;

typedef std::unordered_map<Player*, UpdateData> UpdateDataMapType;

/**
 * @brief Position structure
 *
 * Stores 3D position coordinates and orientation.
 */
struct Position
{

    /**
     * @brief Default constructor
     */
    Position() : x(0.0f), y(0.0f), z(0.0f), o(0.0f) {}
    Position(float _x, float _y, float _z, float _o) : x(_x), y(_y), z(_z), o(_o) {}

    float x; ///< X-coordinate
    float y; ///< Y-coordinate
    float z; ///< Z-coordinate
    float o; ///< Orientation (radians)
};

/**
 * @brief Base class for all objects in the MaNGOS world
 *
 * The Object class is the fundamental base class for all entities that exist
 * in the game world, including players, creatures, game objects, items, etc.
 * It provides core functionality for GUID management, update fields, and world state.
 *
 * This class handles:
 * - Object identification and GUID management
 * - Update field system for client synchronization
 * - World state management (in/out of world)
 * - Type casting helpers for safe downcasting
 * - Value accessors for different data types
 *
 * @note This is an abstract base class and should not be instantiated directly
 * @note All derived classes must implement virtual methods appropriately
 */
class Object
{
    public:
        /**
         * @brief Virtual destructor for proper cleanup of derived classes
         */
        virtual ~Object();

        /**
         * @brief Check if object is currently in the game world
         * @return true if object is in world, false otherwise
         */
        bool IsInWorld() const { return m_inWorld; }

        /**
         * @brief Add object to the game world
         *
         * This method initializes the object's world state and prepares it for
         * client updates. Should be called when object becomes active in world.
         *
         * @note If object is already in world, this method does nothing
         * @note Clears update mask to prevent sending stale data
         */
        virtual void AddToWorld()
        {
            if (m_inWorld)
            {
                return;
            }

            m_inWorld = true;

            // synchronize values mirror with values array (changes will send in updatecreate opcode any way
            ClearUpdateMask(false);                         // false - we can't have update data in update queue before adding to world
        }

        /**
         * @brief Remove object from the game world
         *
         * This method cleans up the object's world state and prevents further
         * client updates. Should be called when object becomes inactive.
         *
         * @note Clears update mask to prevent sending updates after removal
         */
        virtual void RemoveFromWorld()
        {
            // if we remove from world then sending changes not required
            ClearUpdateMask(true);
            m_inWorld = false;
        }

        /**
         * @brief Get the object's unique GUID
         * @return Reference to the object's GUID
         */
        ObjectGuid const& GetObjectGuid() const { return GetGuidValue(OBJECT_FIELD_GUID); }

        /**
         * @brief Get the low part of the object's GUID
         * @return Low 32 bits of the GUID counter
         */
        uint32 GetGUIDLow() const { return GetObjectGuid().GetCounter(); }

        /**
         * @brief Get the packed GUID representation
         * @return Reference to packed GUID for network transmission
         */
        PackedGuid const& GetPackGUID() const { return m_PackGUID; }

        /**
         * @brief Get the GUID as a string
         * @return String representation of the GUID
         */
        std::string GetGuidStr() const { return GetObjectGuid().GetString(); }

        /**
         * @brief Get the object's entry ID from DBC
         * @return Entry ID from appropriate DBC file
         */
        uint32 GetEntry() const { return GetUInt32Value(OBJECT_FIELD_ENTRY); }

        /**
         * @brief Set the object's entry ID
         * @param entry Entry ID from DBC file
         */
        void SetEntry(uint32 entry) { SetUInt32Value(OBJECT_FIELD_ENTRY, entry); }

        float GetObjectScale() const
        {
            float const scale = m_mirror.ReadFloat(OBJECT_FIELD_SCALE_X);
            return scale ? scale : DEFAULT_OBJECT_SCALE;
        }

        void SetObjectScale(float newScale);

        /// Grow or shrink by a percentage of what the object already is.
        void ApplyScalePercent(float percent, bool apply)
        {
            ApplyPercentModFloatValue(OBJECT_FIELD_SCALE_X, percent, apply);
        }

        /// Scale feeds the spatial extent of anything that has one. A hook, not a
        /// downcast: Object must not learn that Occupant exists.
        virtual void OnScaleChanged() {}

        uint8 GetTypeId() const { return m_objectTypeId; }
        bool isType(TypeMask mask) const { return (mask & m_objectType); }

        virtual void BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const;
        void SendCreateUpdateToPlayer(Player* player);

        // must be overwrite in appropriate subclasses (Occupant, Item currently), or will crash
        virtual void AddToClientUpdateList();
        virtual void RemoveFromClientUpdateList();
        virtual void BuildUpdateData(UpdateDataMapType& update_players);
        void MarkForClientUpdate();
        void SendForcedObjectUpdate();

        void BuildValuesUpdateBlockForPlayer(UpdateData* data, Player* target) const;
        void BuildOutOfRangeUpdateBlock(UpdateData* data) const;

        virtual void DestroyForPlayer(Player* target) const;

        int32 GetInt32Value(uint16 index) const
        {
            MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index , false));
            return int32(m_mirror.Read(index));
        }

        uint32 GetUInt32Value(uint16 index) const
        {
            MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index , false));
            return m_mirror.Read(index);
        }

        /// A guid spans two dwords and is read in place, which is why this one
        /// hands back a reference into the block rather than a copy.
        const uint64& GetUInt64Value(uint16 index) const
        {
            MANGOS_ASSERT(index + 1 < GetValuesCount() || PrintIndexError(index , false));
            return *reinterpret_cast<uint64 const*>(m_mirror.At(index));
        }

        float GetFloatValue(uint16 index) const
        {
            MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index , false));
            return m_mirror.ReadFloat(index);
        }

        uint8 GetByteValue(uint16 index, uint8 offset) const
        {
            MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index , false));
            MANGOS_ASSERT(offset < 4);
            return uint8(m_mirror.Read(index) >> (offset * 8));
        }

        uint16 GetUInt16Value(uint16 index, uint8 offset) const
        {
            MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index , false));
            MANGOS_ASSERT(offset < 2);
            return uint16(m_mirror.Read(index) >> (offset * 16));
        }

        ObjectGuid const& GetGuidValue(uint16 index) const { return *reinterpret_cast<ObjectGuid const*>(&GetUInt64Value(index)); }

        void SetInt32Value(uint16 index,        int32  value);
        void SetUInt32Value(uint16 index,       uint32  value);
        void UpdateUInt32Value(uint16 index,    uint32  value);
        void SetUInt64Value(uint16 index, const uint64& value);
        void SetFloatValue(uint16 index,       float   value);
        void SetByteValue(uint16 index, uint8 offset, uint8 value);
        void SetUInt16Value(uint16 index, uint8 offset, uint16 value);
        void SetInt16Value(uint16 index, uint8 offset, int16 value) { SetUInt16Value(index, offset, static_cast<uint16>(value)); }
        void SetGuidValue(uint16 index, ObjectGuid const& value) { SetUInt64Value(index, value.GetRawValue()); }
        void SetStatFloatValue(uint16 index, float value);
        void SetStatInt32Value(uint16 index, int32 value);

        /// Send this field again although its stored value did not change --
        /// because what it means to an observer did.
        void ResendField(uint16 index);

        void ApplyModUInt32Value(uint16 index, int32 val, bool apply);
        void ApplyModInt32Value(uint16 index, int32 val, bool apply);
        void ApplyModPositiveFloatValue(uint16 index, float val, bool apply);
        void ApplyModSignedFloatValue(uint16 index, float val, bool apply);

        void ApplyPercentModFloatValue(uint16 index, float val, bool apply)
        {
            val = val != -100.0f ? val : -99.9f ;
            SetFloatValue(index, GetFloatValue(index) * (apply ? (100.0f + val) / 100.0f : 100.0f / (100.0f + val)));
        }

        void SetFlag(uint16 index, uint32 newFlag);
        void RemoveFlag(uint16 index, uint32 oldFlag);

        void ToggleFlag(uint16 index, uint32 flag)
        {
            if (HasFlag(index, flag))
            {
                RemoveFlag(index, flag);
            }
            else
            {
                SetFlag(index, flag);
            }
        }

        /**
         * Checks if a certain flag is set.
         * @param index The index to check, values may originate from at least \ref EUnitFields
         * @param flag Which flag to check, value may originate from a lot of places, see code
         * for examples of what
         * @return true if the flag is set, false otherwise
         * \todo More info on these flags and where they come from, also, which indexes can be used?
         */
        bool HasFlag(uint16 index, uint32 flag) const
        {
            MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index , false));
            return (m_mirror.Read(index) & flag) != 0;
        }

        void ApplyModFlag(uint16 index, uint32 flag, bool apply)
        {
            if (apply)
            {
                SetFlag(index, flag);
            }
            else
            {
                RemoveFlag(index, flag);
            }
        }

        void SetByteFlag(uint16 index, uint8 offset, uint8 newFlag);
        void RemoveByteFlag(uint16 index, uint8 offset, uint8 newFlag);

        void ToggleByteFlag(uint16 index, uint8 offset, uint8 flag)
        {
            if (HasByteFlag(index, offset, flag))
            {
                RemoveByteFlag(index, offset, flag);
            }
            else
            {
                SetByteFlag(index, offset, flag);
            }
        }

        bool HasByteFlag(uint16 index, uint8 offset, uint8 flag) const
        {
            MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index , false));
            MANGOS_ASSERT(offset < 4);
            return (GetByteValue(index, offset) & flag) != 0;
        }

        void ApplyModByteFlag(uint16 index, uint8 offset, uint32 flag, bool apply)
        {
            if (apply)
            {
                SetByteFlag(index, offset, flag);
            }
            else
            {
                RemoveByteFlag(index, offset, flag);
            }
        }

        void SetShortFlag(uint16 index, bool highpart, uint16 newFlag);
        void RemoveShortFlag(uint16 index, bool highpart, uint16 oldFlag);

        void ToggleShortFlag(uint16 index, bool highpart, uint8 flag)
        {
            if (HasShortFlag(index, highpart, flag))
            {
                RemoveShortFlag(index, highpart, flag);
            }
            else
            {
                SetShortFlag(index, highpart, flag);
            }
        }

        bool HasShortFlag(uint16 index, bool highpart, uint8 flag) const
        {
            MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index , false));
            return (GetUInt16Value(index, highpart ? 1 : 0) & flag) != 0;
        }

        void ApplyModShortFlag(uint16 index, bool highpart, uint32 flag, bool apply)
        {
            if (apply)
            {
                SetShortFlag(index, highpart, flag);
            }
            else
            {
                RemoveShortFlag(index, highpart, flag);
            }
        }

        void SetFlag64(uint16 index, uint64 newFlag)
        {
            uint64 oldval = GetUInt64Value(index);
            uint64 newval = oldval | newFlag;
            SetUInt64Value(index, newval);
        }

        void RemoveFlag64(uint16 index, uint64 oldFlag)
        {
            uint64 oldval = GetUInt64Value(index);
            uint64 newval = oldval & ~oldFlag;
            SetUInt64Value(index, newval);
        }

        void ToggleFlag64(uint16 index, uint64 flag)
        {
            if (HasFlag64(index, flag))
            {
                RemoveFlag64(index, flag);
            }
            else
            {
                SetFlag64(index, flag);
            }
        }

        bool HasFlag64(uint16 index, uint64 flag) const
        {
            MANGOS_ASSERT(index < GetValuesCount() || PrintIndexError(index , false));
            return (GetUInt64Value(index) & flag) != 0;
        }

        void ApplyModFlag64(uint16 index, uint64 flag, bool apply)
        {
            if (apply)
            {
                SetFlag64(index, flag);
            }
            else
            {
                RemoveFlag64(index, flag);
            }
        }

        void ClearUpdateMask(bool remove);

        bool LoadFields(char const* data, uint16 first, uint16 count);
        std::string SaveFields(uint16 first, uint16 count) const;

        uint16 GetValuesCount() const { return m_mirror.Count(); }

        void _ReCreate(uint32 entry);
        void SetAsNewObject(bool isNew) { m_isNewObject = isNew; }

    protected:
        Object();

        void _InitValues();
        void _Create(uint32 guidlow, uint32 entry, HighGuid guidhigh);

        void BuildMovementUpdate(ByteBuffer* data, uint8 updateFlags) const;
        void BuildValuesUpdate(uint8 updatetype, ByteBuffer* data, Player* target) const;
        void BuildUpdateDataForPlayer(Player* pl, UpdateDataMapType& update_players);

        uint16 m_objectType;

        uint8 m_objectTypeId;
        uint8 m_updateFlag;

        Mirror m_mirror;

        bool m_objectUpdated;

    private:
        bool m_inWorld;
        bool m_isNewObject;

        PackedGuid m_PackGUID;

        Object(const Object&);                              // prevent generation copy constructor
        Object& operator=(Object const&);                   // prevent generation assigment operator

    public:
        /// It is a player, which on the wire is TypeID 4.
        bool IsPlayer() const { return GetTypeId() == TYPEID_PLAYER; }

        /// A unit that is not a player. There is no creature TypeID: see the note
        /// on ToCreature below for why that is the whole of the question.
        bool IsCreature() const { return GetTypeId() == TYPEID_UNIT; }

        /// Anything alive, a player included: the mask carries the whole chain.
        bool IsUnit() const { return isType(TYPEMASK_UNIT); }

        bool IsGameObject() const { return GetTypeId() == TYPEID_GAMEOBJECT; }
        bool IsCorpse() const { return GetTypeId() == TYPEID_CORPSE; }
        bool IsDynObject() const { return GetTypeId() == TYPEID_DYNAMICOBJECT; }

        // for output helpfull error messages from ASSERTs
        bool PrintIndexError(uint32 index, bool set) const;
        bool PrintEntryError(char const* descr) const;
};

/**
 * Asking an object to be one of the kinds it might be.
 *
 * THERE IS NO CREATURE ON THE WIRE. The client's own hierarchy has CGUnit_C at
 * TypeID 3 and CGPlayer_C at 4, with nothing in between: a creature IS a unit,
 * and what makes it a creature arrives separately, in the record the client
 * caches from SMSG_CREATURE_QUERY_RESPONSE. This core agrees without meaning to
 * -- Unit's constructor sets TYPEID_UNIT and Creature never touches it -- so a
 * creature goes out as TypeID 3 like anything else alive that is not a player.
 *
 * Which is why ToCreature tests IsCreature, and IsCreature reads "a unit that is
 * not a player". The name is the invariant it rests on: in this core every Unit
 * is either a Player or a Creature, and nothing else derives from Unit.
 *
 * Each of these is a template so that the cast is compiled where the caller
 * stands, with the derived type complete, rather than being taken on trust in a
 * header that has only heard the name.
 */

template <typename T = GameObject>
inline T* ToGameObject(Object* object)
{
    return object && object->IsGameObject() ? static_cast<T*>(object) : nullptr;
}

template <typename T = GameObject>
inline T const* ToGameObject(Object const* object)
{
    return object && object->IsGameObject() ? static_cast<T const*>(object) : nullptr;
}

template <typename T = Unit>
inline T* ToUnit(Object* object)
{
    return object && object->IsUnit() ? static_cast<T*>(object) : nullptr;
}

template <typename T = Unit>
inline T const* ToUnit(Object const* object)
{
    return object && object->IsUnit() ? static_cast<T const*>(object) : nullptr;
}

template <typename T = Creature>
inline T* ToCreature(Object* object)
{
    return object && object->IsCreature() ? static_cast<T*>(object) : nullptr;
}

template <typename T = Creature>
inline T const* ToCreature(Object const* object)
{
    return object && object->IsCreature() ? static_cast<T const*>(object) : nullptr;
}

template <typename T = Player>
inline T* ToPlayer(Object* object)
{
    return object && object->IsPlayer() ? static_cast<T*>(object) : nullptr;
}

template <typename T = Player>
inline T const* ToPlayer(Object const* object)
{
    return object && object->IsPlayer() ? static_cast<T const*>(object) : nullptr;
}

template <typename T = Corpse>
inline T const* ToCorpse(Object const* object)
{
    return object && object->IsCorpse() ? static_cast<T const*>(object) : nullptr;
}

template <typename T = DynamicObject>
inline T const* ToDynObject(Object const* object)
{
    return object && object->IsDynObject() ? static_cast<T const*>(object) : nullptr;
}
