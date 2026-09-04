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

// The list of items a player still has to write.
//
// Every item here is a bare address that is never read through: the queue is
// only allowed to look at an item once it has decided the item concerns it, and
// these tests are what says so. A queue that dereferenced first would crash the
// suite rather than quietly pass it.

#include "doctest.h"

#include "ItemSaveQueue.h"

#include <cstdint>

namespace
{
    /// An address that stands for an item and is never followed.
    Item* Token(std::uintptr_t n) { return reinterpret_cast<Item*>(n * sizeof(void*)); }

    Item* const kFirst = Token(1);
    Item* const kSecond = Token(2);
}

TEST_CASE("save queue: a fresh queue holds nothing and knows nobody")
{
    ItemSaveQueue queue;

    CHECK(queue.IsEmpty());
    CHECK_FALSE(queue.IsShut());
    CHECK_FALSE(queue.Holds(kFirst));

    // An item it does not hold has no place, and the answer is out of range
    // rather than a plausible index.
    CHECK(queue.PlaceOf(kFirst) >= queue.Waiting().size());
}

TEST_CASE("save queue: a shut queue takes nothing in")
{
    ItemSaveQueue queue;
    queue.Shut(true);

    CHECK(queue.IsShut());

    // Shut is read before the item is, so this must not follow the address.
    queue.Note(kFirst);

    CHECK(queue.IsEmpty());
    CHECK_FALSE(queue.Holds(kFirst));

    queue.Shut(false);
    CHECK_FALSE(queue.IsShut());
}

TEST_CASE("save queue: forgetting something it never held does nothing")
{
    ItemSaveQueue queue;

    // Not held is decided before the item is read, so this must not follow it.
    queue.Forget(kSecond);

    CHECK(queue.IsEmpty());
    CHECK_FALSE(queue.Holds(kSecond));
}

TEST_CASE("save queue: clearing empties both the places and what it knows")
{
    ItemSaveQueue queue;
    queue.Clear();

    CHECK(queue.IsEmpty());
    CHECK(queue.Waiting().empty());
    CHECK_FALSE(queue.Holds(kFirst));
    CHECK_FALSE(queue.Holds(kSecond));
}
