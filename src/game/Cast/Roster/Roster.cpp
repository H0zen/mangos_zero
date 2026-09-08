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

#include "Roster.h"

namespace cast
{
    void Roster::Clear()
    {
        m_units.clear();
        m_objects.clear();
        m_items.clear();
    }

    UnitTarget* Roster::FindUnit(ObjectGuid guid)
    {
        for (auto& target : m_units)
        {
            if (target.guid == guid)
            {
                return &target;
            }
        }

        return nullptr;
    }

    ObjectTarget* Roster::FindObject(ObjectGuid guid)
    {
        for (auto& target : m_objects)
        {
            if (target.guid == guid)
            {
                return &target;
            }
        }

        return nullptr;
    }

    ItemTarget* Roster::FindItem(const Item* item)
    {
        for (auto& target : m_items)
        {
            if (target.item == item)
            {
                return &target;
            }
        }

        return nullptr;
    }

    bool Roster::ServesSlot(uint8 slot) const
    {
        const uint8 bit = uint8(1 << slot);

        for (const auto& target : m_units)
        {
            if (target.slots & bit)
            {
                return true;
            }
        }

        for (const auto& target : m_objects)
        {
            if (target.slots & bit)
            {
                return true;
            }
        }

        for (const auto& target : m_items)
        {
            if (target.slots & bit)
            {
                return true;
            }
        }

        return false;
    }

    uint64 Roster::SoonestArrivalMs() const
    {
        uint64 soonest = 0;

        for (const auto& target : m_units)
        {
            if (target.arrivesInMs != 0 && (soonest == 0 || target.arrivesInMs < soonest))
            {
                soonest = target.arrivesInMs;
            }
        }

        for (const auto& target : m_objects)
        {
            if (target.arrivesInMs != 0 && (soonest == 0 || target.arrivesInMs < soonest))
            {
                soonest = target.arrivesInMs;
            }
        }

        return soonest;
    }
}
