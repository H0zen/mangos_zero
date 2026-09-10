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

#pragma once

#include "DBCEnums.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <list>

class DynamicObject;
class GameObject;
class Unit;

class Conjurations
{
    public:

        explicit Conjurations(Unit& whose) : m_owner(whose) {}

        void AddArea(DynamicObject* area);
        DynamicObject* AreaOf(uint32 spellId);
        DynamicObject* AreaOf(uint32 spellId, SpellEffectIndex effect);

        void Forget(ObjectGuid area) { m_areas.remove(area); }

        void RemoveAreas(uint32 spellId);
        void RemoveAllAreas();

        void AddObject(GameObject* object);
        void RemoveObject(GameObject* object, bool destroy);
        void RemoveObjects(uint32 spellId, bool destroy);
        void RemoveAllObjects();

        void RemoveDespawnedObjects();

        bool Empty() const { return m_areas.empty() && m_objects.empty(); }

    private:

        Unit& m_owner;

        GuidList m_areas;
        std::list<GameObject*> m_objects;
};
