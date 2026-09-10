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

class GameObjectBehaviour
{
    public:

        struct Casting
        {
            uint32 spellId = 0;
            Unit* caster = nullptr;
            bool triggered = false;
        };

        explicit GameObjectBehaviour(GameObject& it) : m_it(it) {}

        virtual ~GameObjectBehaviour() = default;

        GameObjectBehaviour(GameObjectBehaviour const&) = delete;
        GameObjectBehaviour& operator=(GameObjectBehaviour const&) = delete;

        enum class Tick
        {
            Carry,
            Rest,
            Stop
        };

        virtual Casting UsedBy(Unit* user, bool scriptSaidYes);

        virtual void Arming() {}

        virtual Tick TimedOut() { return Tick::Carry; }

        virtual Tick Standing() { return Tick::Carry; }

        virtual void InUse(uint32 elapsed) { (void)elapsed; }

        virtual Tick Spent() { return Tick::Carry; }

        virtual void Respawning() {}

    protected:

        GameObject& It() const { return m_it; }

        GameObjectInfo const& Data() const;

    private:
        GameObject& m_it;
};

std::unique_ptr<GameObjectBehaviour> BehaviourOf(GameObject& it);
