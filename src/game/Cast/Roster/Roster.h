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
#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <vector>

class Item;

namespace cast
{

    struct UnitTarget
    {
        ObjectGuid guid = 0;
        uint64 arrivesInMs = 0;
        uint32 hitInfo = 0;
        uint32 damage = 0;
        SpellMissInfo verdict = SPELL_MISS_NONE;
        SpellMissInfo reflectedVerdict = SPELL_MISS_NONE;
        uint8 slots = 0;
        bool served = false;
    };

    struct ObjectTarget
    {
        ObjectGuid guid = 0;
        uint64 arrivesInMs = 0;
        uint8 slots = 0;
        bool served = false;
    };

    struct ItemTarget
    {
        Item* item = nullptr;
        uint8 slots = 0;
    };

    class Roster
    {
        public:
            void Clear();

            UnitTarget* FindUnit(ObjectGuid guid);
            ObjectTarget* FindObject(ObjectGuid guid);
            ItemTarget* FindItem(const Item* item);

            void Enrol(const UnitTarget& target) { m_units.push_back(target); }
            void Enrol(const ObjectTarget& target) { m_objects.push_back(target); }
            void Enrol(const ItemTarget& target) { m_items.push_back(target); }

            std::vector<UnitTarget>& Units() { return m_units; }
            const std::vector<UnitTarget>& Units() const { return m_units; }

            std::vector<ObjectTarget>& Objects() { return m_objects; }
            const std::vector<ObjectTarget>& Objects() const { return m_objects; }

            std::vector<ItemTarget>& Items() { return m_items; }
            const std::vector<ItemTarget>& Items() const { return m_items; }

            bool ServesSlot(uint8 slot) const;

            uint64 SoonestArrivalMs() const;

        private:
            std::vector<UnitTarget> m_units;
            std::vector<ObjectTarget> m_objects;
            std::vector<ItemTarget> m_items;
    };
}
