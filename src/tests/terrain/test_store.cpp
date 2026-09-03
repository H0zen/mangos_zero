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

// Store: the array that either owns its bytes or looks at a mapping.
//
// What matters is that a reader cannot tell the difference, and that a move of
// an owning Store does not leave a pointer to the vector's old buffer -- which
// is the failure a cached data() would introduce and the reason there is none.

#include "doctest.h"

#include "terrain/Store.hpp"

#include <cstdint>
#include <utility>
#include <vector>

using world::terrain::Store;

TEST_CASE("A default Store is empty and owns nothing to read")
{
    Store<float> store;

    CHECK(store.empty());
    CHECK(store.size() == 0);
    CHECK(store.Owns());
    CHECK(store.begin() == store.end());
}

TEST_CASE("An adopted vector reads back through the same interface")
{
    Store<float> store;
    store.Adopt(std::vector<float>{1.f, 2.f, 3.f});

    CHECK(store.Owns());
    REQUIRE(store.size() == 3);
    CHECK(store[0] == doctest::Approx(1.f));
    CHECK(store[2] == doctest::Approx(3.f));
    CHECK_FALSE(store.empty());
}

TEST_CASE("A view reads the memory it was pointed at, without copying")
{
    const std::vector<float> backing{10.f, 20.f, 30.f, 40.f};

    Store<float> store;
    store.View(backing.data(), backing.size());

    CHECK_FALSE(store.Owns());
    REQUIRE(store.size() == 4);
    CHECK(store[1] == doctest::Approx(20.f));

    // Same memory, not a copy of it.
    CHECK(store.data() == backing.data());
}

TEST_CASE("Reads look identical whether the bytes are owned or viewed")
{
    const std::vector<uint16_t> backing{7, 8, 9};

    Store<uint16_t> owning;
    owning.Adopt(std::vector<uint16_t>{7, 8, 9});

    Store<uint16_t> viewing;
    viewing.View(backing.data(), backing.size());

    REQUIRE(owning.size() == viewing.size());
    for (size_t i = 0; i < owning.size(); ++i)
    {
        CHECK(owning[i] == viewing[i]);
    }
}

TEST_CASE("Moving an owning Store leaves no pointer to the old buffer")
{
    // The reason data() is recomputed instead of cached. A cached pointer would
    // survive the move and address a vector that has handed its buffer away.
    Store<float> first;
    first.Adopt(std::vector<float>{5.f, 6.f, 7.f});

    Store<float> second = std::move(first);

    REQUIRE(second.size() == 3);
    CHECK(second[0] == doctest::Approx(5.f));
    CHECK(second[2] == doctest::Approx(7.f));
    CHECK(second.data() != nullptr);
    CHECK(second.Owns());
}

TEST_CASE("Moving a viewing Store keeps looking at the same memory")
{
    const std::vector<float> backing{1.f, 2.f};

    Store<float> first;
    first.View(backing.data(), backing.size());

    Store<float> second = std::move(first);

    CHECK_FALSE(second.Owns());
    CHECK(second.data() == backing.data());
    REQUIRE(second.size() == 2);
    CHECK(second[1] == doctest::Approx(2.f));
}

TEST_CASE("Building on a Store that was a view takes it back into ownership")
{
    // Mapped tile bytes are read-only and shared. A build call must not quietly
    // write through the view, so it drops it and starts owning instead.
    const std::vector<float> backing{99.f, 98.f};

    Store<float> store;
    store.View(backing.data(), backing.size());
    REQUIRE_FALSE(store.Owns());

    store.assign(3, 1.5f);

    CHECK(store.Owns());
    REQUIRE(store.size() == 3);
    CHECK(store[0] == doctest::Approx(1.5f));
    CHECK(store.data() != backing.data());

    // And the memory it used to look at is untouched.
    CHECK(backing[0] == doctest::Approx(99.f));
}

TEST_CASE("push_back and reserve build the owned side")
{
    Store<uint32_t> store;
    store.reserve(4);
    store.push_back(1);
    store.push_back(2);

    CHECK(store.Owns());
    REQUIRE(store.size() == 2);
    CHECK(store[0] == 1);
    CHECK(store[1] == 2);
}

TEST_CASE("resize grows the owned side")
{
    Store<uint8_t> store;
    store.resize(5);

    CHECK(store.size() == 5);
    CHECK(store.Owns());
}

TEST_CASE("clear empties either kind")
{
    const std::vector<float> backing{1.f};

    Store<float> viewing;
    viewing.View(backing.data(), backing.size());
    viewing.clear();
    CHECK(viewing.empty());
    CHECK(viewing.Owns());

    Store<float> owning;
    owning.Adopt(std::vector<float>{1.f, 2.f});
    owning.clear();
    CHECK(owning.empty());
}

TEST_CASE("swap exchanges with a plain vector")
{
    Store<float> store;
    store.Adopt(std::vector<float>{1.f, 2.f});

    std::vector<float> other{9.f};
    store.swap(other);

    REQUIRE(store.size() == 1);
    CHECK(store[0] == doctest::Approx(9.f));
    REQUIRE(other.size() == 2);
    CHECK(other[0] == doctest::Approx(1.f));
}

TEST_CASE("Iteration covers exactly the elements")
{
    Store<int32_t> store;
    store.Adopt(std::vector<int32_t>{1, 2, 3, 4});

    int32_t sum = 0;
    size_t seen = 0;
    for (const int32_t value : store)
    {
        sum += value;
        ++seen;
    }

    CHECK(seen == 4);
    CHECK(sum == 10);
}
