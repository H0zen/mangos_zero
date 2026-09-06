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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Conjurations.h"

#include "DynamicObject.h"
#include "GameObject.h"
#include "Map.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "Utilities/Errors.h"

void Conjurations::AddArea(DynamicObject* area)
{
    m_areas.push_back(area->GetObjectGuid());
}

DynamicObject* Conjurations::AreaOf(uint32 spellId)
{
    for (GuidList::iterator itr = m_areas.begin(); itr != m_areas.end();)
    {
        DynamicObject* area = m_owner.GetMap()->GetDynamicObject(*itr);
        if (!area)
        {
            itr = m_areas.erase(itr);                       // the map has already taken it
            continue;
        }

        if (area->GetSpellId() == spellId)
        {
            return area;
        }

        ++itr;
    }

    return nullptr;
}

DynamicObject* Conjurations::AreaOf(uint32 spellId, SpellEffectIndex effect)
{
    for (GuidList::iterator itr = m_areas.begin(); itr != m_areas.end();)
    {
        DynamicObject* area = m_owner.GetMap()->GetDynamicObject(*itr);
        if (!area)
        {
            itr = m_areas.erase(itr);                       // the map has already taken it
            continue;
        }

        if (area->GetSpellId() == spellId && area->GetEffIndex() == effect)
        {
            return area;
        }

        ++itr;
    }

    return nullptr;
}

void Conjurations::RemoveAreas(uint32 spellId)
{
    for (GuidList::iterator itr = m_areas.begin(); itr != m_areas.end();)
    {
        DynamicObject* area = m_owner.GetMap()->GetDynamicObject(*itr);
        if (!area)
        {
            itr = m_areas.erase(itr);
        }
        else if (spellId == 0 || area->GetSpellId() == spellId)
        {
            area->Delete();
            itr = m_areas.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void Conjurations::RemoveAllAreas()
{
    while (!m_areas.empty())
    {
        if (DynamicObject* area = m_owner.GetMap()->GetDynamicObject(*m_areas.begin()))
        {
            area->Delete();
        }

        m_areas.erase(m_areas.begin());
    }
}

void Conjurations::AddObject(GameObject* object)
{
    MANGOS_ASSERT(object && !object->GetOwnerGuid());

    m_objects.push_back(object);
    object->SetOwnerGuid(m_owner.GetObjectGuid());

    if (!m_owner.IsPlayer() || !object->GetSpellId())
    {
        return;
    }

    // A spell that stands only while its object stands cannot be cast again
    // until the object goes. Item cooldowns and charge mods are not considered;
    // no such spell is known.
    SpellEntry const* madeBy = sSpellStore.LookupEntry(object->GetSpellId());
    if (madeBy && madeBy->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE))
    {
        static_cast<Player&>(m_owner).AddSpellAndCategoryCooldowns(madeBy, 0, nullptr, true);
    }
}

void Conjurations::RemoveObject(GameObject* object, bool destroy)
{
    MANGOS_ASSERT(object && object->GetOwnerGuid() == m_owner.GetObjectGuid());

    object->SetOwnerGuid(ObjectGuid());

    if (uint32 spellId = object->GetSpellId())
    {
        m_owner.RemoveAuras(spellId);

        if (m_owner.IsPlayer())
        {
            SpellEntry const* madeBy = sSpellStore.LookupEntry(spellId);
            if (madeBy && madeBy->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE))
            {
                static_cast<Player&>(m_owner).SendCooldownEvent(madeBy);
            }
        }
    }

    m_objects.remove(object);

    if (destroy)
    {
        object->SetRespawnTime(0);
        object->Delete();
    }
}

void Conjurations::RemoveObjects(uint32 spellId, bool destroy)
{
    for (auto itr = m_objects.begin(); itr != m_objects.end();)
    {
        if (spellId != 0 && (*itr)->GetSpellId() != spellId)
        {
            ++itr;
            continue;
        }

        (*itr)->SetOwnerGuid(ObjectGuid());
        if (destroy)
        {
            (*itr)->SetRespawnTime(0);
            (*itr)->Delete();
        }

        itr = m_objects.erase(itr);
    }
}

void Conjurations::RemoveAllObjects()
{
    for (auto itr = m_objects.begin(); itr != m_objects.end();)
    {
        (*itr)->SetOwnerGuid(ObjectGuid());
        (*itr)->SetRespawnTime(0);
        (*itr)->Delete();

        itr = m_objects.erase(itr);
    }
}

void Conjurations::RemoveDespawnedObjects()
{
    for (auto itr = m_objects.begin(); itr != m_objects.end();)
    {
        if ((*itr)->isSpawned())
        {
            ++itr;
            continue;
        }

        (*itr)->SetOwnerGuid(ObjectGuid());
        (*itr)->SetRespawnTime(0);
        (*itr)->Delete();

        itr = m_objects.erase(itr);
    }
}
