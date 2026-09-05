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

#include <memory>

class GameObject;
struct GameObjectInfo;
class Unit;

/**
 * What one kind of gameobject does, and the state only that kind needs.
 *
 * A gameobject is the same thing whatever kind it is: a guid, a place, a block of
 * fields, a map it belongs to, a clock that says when it comes and goes. None of
 * that varies. What varies is what happens when somebody clicks it, what it does
 * on its own tick, and what it has to remember in order to do either -- and that
 * is what lives here, one class per kind.
 *
 * The kind is a column in `gameobject_template`, so it is read once, when the
 * template is fixed to the object, and never asked again. A door does not ask
 * what it is every time it is opened.
 *
 * This is the shape the client uses: its dispatcher at 0x5F7098 allocates a
 * behaviour object per type and hangs it off the gameobject rather than deriving
 * the gameobject itself, which is why the lift's facing comes out through a slot
 * of the behaviour's own vtable and not the object's.
 */
class GameObjectBehaviour
{
    public:
        /// What a use asks to be cast, if it asks for anything.
        struct Casting
        {
            uint32 spellId = 0;
            Unit* caster = nullptr;
            bool triggered = false;
        };

        /// Made by the factory alone, and only for an object whose template is set.
        explicit GameObjectBehaviour(GameObject& it) : m_it(it) {}

        virtual ~GameObjectBehaviour() = default;

        GameObjectBehaviour(GameObjectBehaviour const&) = delete;
        GameObjectBehaviour& operator=(GameObjectBehaviour const&) = delete;

        /**
         * @brief Somebody clicked it.
         *
         * @param user Whoever clicked.
         * @param scriptSaidYes What the script hook made of it, which some kinds obey.
         * @return The spell the use asks for, or nothing.
         */
        virtual Casting UsedBy(Unit* user, bool scriptSaidYes);

    protected:
        /// The object this behaviour belongs to. It never outlives it.
        GameObject& It() const { return m_it; }

        /// Its template, which is what chose this behaviour in the first place.
        GameObjectInfo const& Data() const;

    private:
        GameObject& m_it;
};

/// The behaviour of the kind the object's template names. Never null: a kind with
/// nothing of its own gets one that does nothing, so no caller has to check.
std::unique_ptr<GameObjectBehaviour> BehaviourOf(GameObject& it);
