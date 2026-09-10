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
#include <string>

#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "ByteBuffer.h"
#include "UpdateFields.h"
#include "Mirror.h"
#include "UpdateData.h"
#include "ObjectGuid.h"

#define DEFAULT_OBJECT_SCALE        1.0f
#define DEFAULT_TAUREN_MALE_SCALE   1.35f
#define DEFAULT_TAUREN_FEMALE_SCALE 1.25f

class WorldPacket;
class UpdateData;
class WorldSession;
class Player;

typedef std::unordered_map<Player*, UpdateData> UpdateDataMapType;

class Object
{
    public:

        virtual ~Object();

        bool IsInWorld() const { return m_inWorld; }

        virtual void AddToWorld()
        {
            if (m_inWorld)
            {
                return;
            }

            m_inWorld = true;

            ClearUpdateMask(false);
        }

        virtual void RemoveFromWorld()
        {

            ClearUpdateMask(true);
            m_inWorld = false;
        }

        ObjectGuid const& GetObjectGuid() const { return GetGuidValue(OBJECT_FIELD_GUID); }

        uint32 GetGUIDLow() const { return GuidCounter(GetObjectGuid()); }

        PackedGuid const& GetPackGUID() const { return m_PackGUID; }

        std::string GetGuidStr() const { return GuidString(GetObjectGuid()); }

        uint32 GetEntry() const { return GetUInt32Value(OBJECT_FIELD_ENTRY); }

        void SetEntry(uint32 entry) { SetUInt32Value(OBJECT_FIELD_ENTRY, entry); }

        float GetObjectScale() const
        {
            float const scale = m_mirror.ReadFloat(OBJECT_FIELD_SCALE_X);
            return scale ? scale : DEFAULT_OBJECT_SCALE;
        }

        void SetObjectScale(float newScale);

        void ApplyScalePercent(float percent, bool apply)
        {
            ApplyPercentModFloatValue(OBJECT_FIELD_SCALE_X, percent, apply);
        }

        virtual void OnScaleChanged() {}

        uint8 GetTypeId() const { return m_objectTypeId; }

        virtual void BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const;
        void SendCreateUpdateToPlayer(Player* player);

        virtual void AddToClientUpdateList() = 0;
        virtual void RemoveFromClientUpdateList() = 0;
        virtual void BuildUpdateData(UpdateDataMapType& update_players) = 0;
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
        void SetUInt64Value(uint16 index, const uint64& value);
        void SetFloatValue(uint16 index,       float   value);
        void SetByteValue(uint16 index, uint8 offset, uint8 value);
        void SetUInt16Value(uint16 index, uint8 offset, uint16 value);
        void SetInt16Value(uint16 index, uint8 offset, int16 value) { SetUInt16Value(index, offset, static_cast<uint16>(value)); }
        void SetGuidValue(uint16 index, ObjectGuid const& value) { SetUInt64Value(index, value); }
        void SetStatFloatValue(uint16 index, float value);
        void SetStatInt32Value(uint16 index, int32 value);

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

        uint8 m_objectTypeId;
        uint8 m_updateFlag;

        Mirror m_mirror;

        bool m_objectUpdated;

    private:
        bool m_inWorld;
        bool m_isNewObject;

        PackedGuid m_PackGUID;

        Object(const Object&);
        Object& operator=(Object const&);

    public:
        bool PrintIndexError(uint32 index, bool set) const;
        bool PrintEntryError(char const* descr) const;
};
