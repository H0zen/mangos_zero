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

#include <cstddef>
#include <unordered_map>
#include <vector>

class Item;

/**
 * The items one player has touched since their last save.
 *
 * An item knows whether it has changed; it does not know that anyone is keeping
 * a list of the changed ones, and it does not know where in that list it sits.
 * The queue holds both, so an item can be dropped out of it in one step without
 * carrying a cursor of its own.
 *
 * A dropped item leaves an empty place rather than shifting the rest along, so
 * that a walk already in progress keeps its footing. Reading the queue therefore
 * means skipping the empty places.
 *
 * While loading a character the queue is shut: everything read from the database
 * is by definition already saved, and noting it would write it straight back.
 */
class ItemSaveQueue
{
    public:
        /// Whose queue this is. An item owned by anyone else is refused, which
        /// is why the answer belongs here rather than at each call.
        void Belongs(ObjectGuid const& owner) { m_owner = owner; }

        /// The item has changed and must be written before the player leaves.
        void Note(Item* item);

        /// The item must not be written -- it is gone, or already saved.
        void Forget(Item* item);

        bool Holds(Item const* item) const { return m_place.find(item) != m_place.end(); }

        /// Where the item sits, for the consistency check behind a GM command.
        std::size_t PlaceOf(Item const* item) const;

        void Shut(bool shut) { m_shut = shut; }
        bool IsShut() const { return m_shut; }

        /// The places, empty ones included; a reader skips the nulls.
        std::vector<Item*> const& Waiting() const { return m_waiting; }
        bool IsEmpty() const { return m_waiting.empty(); }

        void Clear();

    private:
        bool IsOurs(Item const& item, char const* what) const;

        std::vector<Item*>                        m_waiting;
        std::unordered_map<Item const*, std::size_t> m_place;
        ObjectGuid                                m_owner;
        bool                                      m_shut = false;
};
