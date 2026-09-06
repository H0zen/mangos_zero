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

/**
 * What a unit's spells have left standing in the world.
 *
 * Two kinds, and the spell that made each is how it is found again. An area is
 * the ground a spell keeps working on -- a blizzard, a rain of fire, a
 * consecration -- and lives only as long as the spell does. An object is a thing
 * put down where it can be seen and used: a soul well, a portal, a fishing
 * bobber, a duel's flag.
 *
 * The unit holds an area by guid and looks it up in the map, because the map may
 * take one away without asking; an object it holds outright, and is the one who
 * takes it away.
 */
class Conjurations
{
    public:

        explicit Conjurations(Unit& whose) : m_owner(whose) {}

        /// The ground one of his spells is working on.
        void AddArea(DynamicObject* area);
        DynamicObject* AreaOf(uint32 spellId);
        DynamicObject* AreaOf(uint32 spellId, SpellEffectIndex effect);

        /// Lets go of an area that has already gone.
        void Forget(ObjectGuid area) { m_areas.remove(area); }

        /// Ends the areas that spell is working on, or every one of them.
        void RemoveAreas(uint32 spellId);
        void RemoveAllAreas();

        /// A thing one of his spells has put down.
        void AddObject(GameObject* object);
        void RemoveObject(GameObject* object, bool destroy);
        void RemoveObjects(uint32 spellId, bool destroy);
        void RemoveAllObjects();

        /// Clears away the objects that have gone from the world on their own.
        void RemoveDespawnedObjects();

        /// Whether nothing of his is left standing.
        bool Empty() const { return m_areas.empty() && m_objects.empty(); }

    private:

        Unit& m_owner;

        GuidList m_areas;
        std::list<GameObject*> m_objects;
};
