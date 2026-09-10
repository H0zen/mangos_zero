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

#include "doctest.h"

#include "Cast/Roster/Roster.h"

namespace
{
    ObjectGuid Player(uint32 counter)
    {
        return MakeGuid(HIGHGUID_PLAYER, counter);
    }

    ObjectGuid Creature(uint32 counter)
    {
        return MakeGuid(HIGHGUID_UNIT, static_cast<uint32>(1), counter);
    }

    ObjectGuid Chest(uint32 counter)
    {
        return MakeGuid(HIGHGUID_GAMEOBJECT, static_cast<uint32>(2), counter);
    }

    cast::UnitTarget Unit(ObjectGuid guid, uint8 slots, uint64 arrivesInMs = 0)
    {
        cast::UnitTarget target;
        target.guid = guid;
        target.slots = slots;
        target.arrivesInMs = arrivesInMs;
        return target;
    }

    cast::ObjectTarget Object(ObjectGuid guid, uint8 slots, uint64 arrivesInMs = 0)
    {
        cast::ObjectTarget target;
        target.guid = guid;
        target.slots = slots;
        target.arrivesInMs = arrivesInMs;
        return target;
    }
}

TEST_CASE("a unit named twice is one line on the roster")
{
    cast::Roster roster;
    roster.Enrol(Unit(Creature(7), 1 << 0));

    cast::UnitTarget* enrolled = roster.FindUnit(Creature(7));
    REQUIRE(enrolled != nullptr);
    enrolled->slots |= 1 << 2;

    CHECK(roster.Units().size() == 1u);
    CHECK(roster.Units()[0].slots == ((1 << 0) | (1 << 2)));
}

TEST_CASE("a unit not on the roster is not found")
{
    cast::Roster roster;
    roster.Enrol(Unit(Creature(7), 1 << 0));

    CHECK(roster.FindUnit(Creature(8)) == nullptr);
    CHECK(roster.FindUnit(Player(7)) == nullptr);
}

TEST_CASE("a slot is served when any kind of target wants it")
{
    cast::Roster roster;

    roster.Enrol(Unit(Player(1), 1 << 0));
    CHECK(roster.ServesSlot(0));
    CHECK_FALSE(roster.ServesSlot(1));
    CHECK_FALSE(roster.ServesSlot(2));

    roster.Enrol(Object(Chest(3), 1 << 1));
    CHECK(roster.ServesSlot(1));

    cast::ItemTarget item;
    item.item = nullptr;
    item.slots = 1 << 2;
    roster.Enrol(item);
    CHECK(roster.ServesSlot(2));
}

TEST_CASE("a unit that is immune to every slot serves none")
{
    cast::Roster roster;
    roster.Enrol(Unit(Player(1), 0));

    CHECK(roster.Units().size() == 1u);
    CHECK_FALSE(roster.ServesSlot(0));
    CHECK_FALSE(roster.ServesSlot(1));
    CHECK_FALSE(roster.ServesSlot(2));
}

TEST_CASE("the soonest arrival is the shortest flight, units and objects together")
{
    cast::Roster roster;
    CHECK(roster.SoonestArrivalMs() == 0u);

    roster.Enrol(Unit(Player(1), 1 << 0, 800));
    CHECK(roster.SoonestArrivalMs() == 800u);

    roster.Enrol(Unit(Creature(2), 1 << 0, 1200));
    CHECK(roster.SoonestArrivalMs() == 800u);

    roster.Enrol(Object(Chest(3), 1 << 0, 300));
    CHECK(roster.SoonestArrivalMs() == 300u);
}

TEST_CASE("a target that is reached at once does not shorten the wait")
{
    cast::Roster roster;
    roster.Enrol(Unit(Player(1), 1 << 0, 500));
    roster.Enrol(Unit(Creature(2), 1 << 0));

    CHECK(roster.SoonestArrivalMs() == 500u);
}

TEST_CASE("a roster of instant targets waits for nothing")
{
    cast::Roster roster;
    roster.Enrol(Unit(Player(1), 1 << 0));
    roster.Enrol(Object(Chest(3), 1 << 1));

    CHECK(roster.SoonestArrivalMs() == 0u);
}

TEST_CASE("clearing the roster empties every kind at once")
{
    cast::Roster roster;
    roster.Enrol(Unit(Player(1), 1 << 0, 700));
    roster.Enrol(Object(Chest(3), 1 << 1));

    cast::ItemTarget item;
    item.slots = 1 << 2;
    roster.Enrol(item);

    roster.Clear();

    CHECK(roster.Units().empty());
    CHECK(roster.Objects().empty());
    CHECK(roster.Items().empty());
    CHECK(roster.SoonestArrivalMs() == 0u);
    CHECK_FALSE(roster.ServesSlot(0));
}

TEST_CASE("an item is found by the item it names")
{
    cast::Roster roster;

    Item* const first = reinterpret_cast<Item*>(0x10);
    Item* const second = reinterpret_cast<Item*>(0x20);

    cast::ItemTarget target;
    target.item = first;
    target.slots = 1 << 0;
    roster.Enrol(target);

    REQUIRE(roster.FindItem(first) != nullptr);
    CHECK(roster.FindItem(first)->slots == (1 << 0));
    CHECK(roster.FindItem(second) == nullptr);
}

TEST_CASE("a fresh line on the roster has not been served")
{
    cast::Roster roster;
    roster.Enrol(Unit(Player(1), 1 << 0));
    roster.Enrol(Object(Chest(3), 1 << 0));

    CHECK_FALSE(roster.Units()[0].served);
    CHECK_FALSE(roster.Objects()[0].served);
    CHECK(roster.Units()[0].verdict == SPELL_MISS_NONE);
    CHECK(roster.Units()[0].reflectedVerdict == SPELL_MISS_NONE);
    CHECK(roster.Units()[0].damage == 0u);
    CHECK(roster.Units()[0].hitInfo == 0u);
}
