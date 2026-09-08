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
    /// One unit a cast reaches. The verdict is reckoned when the unit is written
    /// down, long before the spell arrives, because the client is told the whole
    /// outcome in the same message that starts the missile flying.
    struct UnitTarget
    {
        ObjectGuid guid;
        uint64 arrivesInMs = 0;             ///< flight time from the caster, zero for a spell that lands at once
        uint32 hitInfo = 0;                 ///< what the weapon blow reports, for the spells that swing one
        uint32 damage = 0;                  ///< carried from the launch to the arrival for delayed spells
        SpellMissInfo verdict = SPELL_MISS_NONE;
        SpellMissInfo reflectedVerdict = SPELL_MISS_NONE;   ///< what happens to the caster when this unit reflects
        uint8 slots = 0;                    ///< bit per recipe slot that applies to this unit
        bool served = false;                ///< the slots have already been run on it
    };

    /// One gameobject a cast reaches. A gameobject neither dodges nor resists, so
    /// it carries no verdict.
    struct ObjectTarget
    {
        ObjectGuid guid;
        uint64 arrivesInMs = 0;
        uint8 slots = 0;
        bool served = false;
    };

    /// One item a cast reaches. The item is in a bag the caster owns, so nothing
    /// flies to it and nothing can go wrong on the way.
    struct ItemTarget
    {
        Item* item = nullptr;
        uint8 slots = 0;
    };

    /// Everyone and everything one cast reaches. The roster is written while the
    /// targets are chosen and read again when the spell lands: a target named
    /// twice by two slots is one line with two bits, not two lines.
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

            /// Does any line on the roster want this recipe slot run on it?
            bool ServesSlot(uint8 slot) const;

            /// The shortest flight time on the roster, zero when everything lands
            /// at once. This is when the cast next needs waking.
            uint64 SoonestArrivalMs() const;

        private:
            std::vector<UnitTarget> m_units;
            std::vector<ObjectTarget> m_objects;
            std::vector<ItemTarget> m_items;
    };
}
