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

#include "Lifespan.h"
#include "Lifespan.h"
#include "Occupant.h"
#include "DBCEnums.h"
#include "Unit.h"

enum DynamicObjectType
{
    DYNAMIC_OBJECT_PORTAL           = 0x0,
    DYNAMIC_OBJECT_AREA_SPELL       = 0x1,
    DYNAMIC_OBJECT_FARSIGHT_FOCUS   = 0x2,
};

struct SpellEntry;

class DynamicObject : public Occupant
{
    public:
        explicit DynamicObject();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        bool Create(uint32 guidlow, Unit* caster, uint32 spellId, SpellEffectIndex effIndex, float x, float y, float z, int32 duration, float radius, DynamicObjectType type);
        void Update(uint32 update_diff, uint32 p_time) override;
        void Delete();
        uint32 GetSpellId() const { return m_spellId; }
        SpellEffectIndex GetEffIndex() const { return m_effIndex; }
        uint32 GetDuration() const { return m_life.Left(); }
        ObjectGuid GetCasterGuid() const { return GetGuidValue(DYNAMICOBJECT_CASTER); }
        Unit* GetCaster() const;
        float GetRadius() const { return m_radius; }
        DynamicObjectType GetType() const { return (DynamicObjectType)GetByteValue(DYNAMICOBJECT_BYTES, 0); }
        bool IsAffecting(Unit* unit) const { return m_affected.find(unit->GetObjectGuid()) != m_affected.end(); }
        void AddAffected(Unit* unit) { m_affected.insert(unit->GetObjectGuid()); }
        void RemoveAffected(Unit* unit) { m_affected.erase(unit->GetObjectGuid()); }
        void Delay(int32 delaytime);

        float ComputeBoundingRadius() const override
        {
            return 0.0f;
        }

        bool IsControlledByPlayer() const override
        {
            return (GetCasterGuid() != 0 && GuidHigh(GetCasterGuid()) == HIGHGUID_PLAYER);
        }

        bool IsVisibleForInState(Player const* u, Occupant const* viewPoint, bool inVisibleList) const override;

        void BindToTransport(ObjectGuid transportGuid, float lx, float ly, float lz);

        bool OnTransport() const { return bool(m_transportGuid); }

        bool IsInEffectRange(Unit const* target) const;

        GridReference<DynamicObject>& GetGridRef()
        {
            return m_gridRef;
        }

    protected:
        uint32 m_spellId;
        SpellEffectIndex m_effIndex;

        Lifespan m_life;
        float m_radius;
        bool m_positive;
        GuidSet m_affected;

        ObjectGuid m_transportGuid = 0;
        float m_transOffsetX, m_transOffsetY, m_transOffsetZ;
    private:
        GridReference<DynamicObject> m_gridRef;
};
