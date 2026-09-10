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

#include "Utterance.h"
#include "Platform/Define.h"
#include "World.h"
#include "ObjectLookup.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "Transports.h"
#include "TransportMap.h"
#include "GridNotifiersImpl.h"
#include "SpellMgr.h"
#include "DBCStores.h"
#include "Cast/Recipe/RecipeBook.h"

DynamicObject::DynamicObject() : Occupant()
{
    m_objectTypeId = TYPEID_DYNAMICOBJECT;
    m_updateFlag = (UPDATEFLAG_ALL | UPDATEFLAG_HAS_POSITION);

    m_transOffsetX = m_transOffsetY = m_transOffsetZ = 0.0f;
}

void DynamicObject::BindToTransport(ObjectGuid transportGuid, float lx, float ly, float lz)
{
    m_transportGuid = transportGuid;
    m_transOffsetX = lx;
    m_transOffsetY = ly;
    m_transOffsetZ = lz;
}

bool DynamicObject::IsInEffectRange(Unit const* target) const
{
    if (m_transportGuid)
    {
        Transport* named = Transport::GetTransport(GetMap(), m_transportGuid);
        TransportMap* vessel = named ? named->AsMap() : nullptr;
        if (!vessel)
        {
            return false;
        }

        const auto local = vessel->PositionOf(*target);
        if (!local)
        {
            return false;
        }

        const float dx = local->X() - m_transOffsetX;
        const float dy = local->Y() - m_transOffsetY;
        const float dz = local->Z() - m_transOffsetZ;
        return dx * dx + dy * dy + dz * dz <= GetRadius() * GetRadius();
    }

    return InReach(*this, *target, GetRadius());
}

void DynamicObject::AddToWorld()
{

    if (!IsInWorld())
    {
        GetMap()->GetObjectsStore().insert<DynamicObject>(GetObjectGuid(), (DynamicObject*)this);
    }

    Object::AddToWorld();
}

void DynamicObject::RemoveFromWorld()
{

    if (IsInWorld())
    {
        GetMap()->GetObjectsStore().erase<DynamicObject>(GetObjectGuid(), (DynamicObject*)nullptr);
        GetViewPoint().Event_RemovedFromWorld();
    }

    Object::RemoveFromWorld();
}

bool DynamicObject::Create(uint32 guidlow, Unit* caster, uint32 spellId, SpellEffectIndex effIndex, float x, float y, float z, int32 duration, float radius, DynamicObjectType type)
{
    Occupant::_Create(guidlow, HIGHGUID_DYNAMICOBJECT);
    SetMap(caster->GetMap());
    Place().MoveTo(x, y, z, 0);

    if (!IsPlaceable(*this))
    {
        sLog.outError("DynamicObject (spell %u eff %u) not created. Suggested coordinates isn't valid (X: %f Y: %f)", spellId, effIndex, Where().X(), Where().Y());
        return false;
    }

    SetEntry(spellId);
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    SetGuidValue(DYNAMICOBJECT_CASTER, caster->GetObjectGuid());

    SetByteValue(DYNAMICOBJECT_BYTES, 0, type);

    SetUInt32Value(DYNAMICOBJECT_SPELLID, spellId);
    SetFloatValue(DYNAMICOBJECT_RADIUS, radius);
    SetFloatValue(DYNAMICOBJECT_POS_X, x);
    SetFloatValue(DYNAMICOBJECT_POS_Y, y);
    SetFloatValue(DYNAMICOBJECT_POS_Z, z);

    SpellEntry const* spellProto = sSpellStore.LookupEntry(spellId);
    if (!spellProto)
    {
        sLog.outError("DynamicObject (spell %u) not created. Spell not exist!", spellId);
        return false;
    }

    m_life.Grant(uint32(duration > 0 ? duration : 0));
    m_radius = radius;
    m_effIndex = effIndex;
    m_spellId = spellId;
    m_positive = cast::RecipeOf(*spellProto).IsPositiveAt(m_effIndex);

    return true;
}

Unit* DynamicObject::GetCaster() const
{

    return ObjectLookup::GetUnit(*this, GetCasterGuid());
}

void DynamicObject::Update(uint32 , uint32 p_time)
{

    Unit* caster = GetCaster();
    if (!caster)
    {
        Delete();
        return;
    }

    const bool spent = m_life.Spend(p_time);

    if (m_radius)
    {

        MaNGOS::DynamicObjectUpdater notifier(*this, caster, m_positive);
        Cell::VisitAllObjects(this, notifier, m_radius);
    }

    if (spent)
    {
        caster->Conjured().Forget(GetObjectGuid());
        Delete();
    }
}

void DynamicObject::Delete()
{
    SendDespawnAnimation(*this);
    AddObjectToRemoveList();
}

namespace
{

    bool HoldsAnotherArea(SpellAuraHolder const& holder, SpellEffectIndex after)
    {
        SpellEntry const* spell = holder.GetSpellProto();

        for (uint32 i = after + 1; i < MAX_EFFECT_INDEX; ++i)
        {
            bool const area = spell->Effect[i] == SPELL_EFFECT_PERSISTENT_AREA_AURA ||
                              spell->Effect[i] == SPELL_EFFECT_ADD_FARSIGHT;

            if (area && holder.m_auras[i])
            {
                return true;
            }
        }

        return false;
    }
}

void DynamicObject::Delay(int32 delaytime)
{
    m_life.Shorten(delaytime);

    for (auto iter = m_affected.begin(); iter != m_affected.end();)
    {
        Unit* target = GetMap()->GetUnit(*iter);

        if (!target)
        {
            iter = m_affected.erase(iter);
            continue;
        }

        ++iter;

        SpellAuraHolder* holder = target->GetSpellAuraHolder(m_spellId, GetCasterGuid());
        if (holder && !HoldsAnotherArea(*holder, m_effIndex))
        {
            target->DelaySpellAuraHolder(m_spellId, delaytime, GetCasterGuid());
        }
    }
}

bool DynamicObject::IsVisibleForInState(Player const* u, Occupant const* viewPoint, bool inVisibleList) const
{
    if (!IsInWorld() || !u->IsInWorld())
    {
        return false;
    }

    if (GetCasterGuid() == u->GetObjectGuid())
    {
        return true;
    }

    return SeenWithin(*this, *viewPoint, GetMap()->GetVisibilityDistance() + (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f), false);
}
