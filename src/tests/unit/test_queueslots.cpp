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

// The three places a character may hold in battleground queues.

#include "doctest.h"

#include "QueueSlots.h"

namespace
{
    BattleGroundQueueTypeId const WARSONG = BATTLEGROUND_QUEUE_WS;
    BattleGroundQueueTypeId const ARATHI = BATTLEGROUND_QUEUE_AB;
    BattleGroundQueueTypeId const ALTERAC = BATTLEGROUND_QUEUE_AV;
}

TEST_CASE("queues: a character who has queued for nothing holds nothing")
{
    QueueSlots slots;

    CHECK_FALSE(slots.AnyHeld());
    CHECK(slots.AnyFree());
    CHECK(slots.SlotOf(WARSONG) == QueueSlots::NOWHERE);
    CHECK_FALSE(slots.Holds(WARSONG));
}

TEST_CASE("queues: taking a slot puts him in that queue and no other")
{
    QueueSlots slots;
    uint32 const slot = slots.Take(WARSONG);

    CHECK(slot != QueueSlots::NOWHERE);
    CHECK(slots.AnyHeld());
    CHECK(slots.Holds(WARSONG));
    CHECK_FALSE(slots.Holds(ARATHI));
    CHECK(slots.Kind(slot) == WARSONG);
    CHECK(slots.SlotOf(WARSONG) == slot);
}

TEST_CASE("queues: standing in the same queue twice gives back the slot he holds")
{
    QueueSlots slots;
    uint32 const first = slots.Take(WARSONG);

    CHECK(slots.Take(WARSONG) == first);
    CHECK(slots.AnyFree());
}

TEST_CASE("queues: he may stand in three at once and no more")
{
    QueueSlots slots;

    CHECK(slots.Take(WARSONG) != QueueSlots::NOWHERE);
    CHECK(slots.Take(ARATHI) != QueueSlots::NOWHERE);
    CHECK(slots.Take(ALTERAC) != QueueSlots::NOWHERE);

    CHECK_FALSE(slots.AnyFree());
    CHECK(slots.Take(BattleGroundQueueTypeId(MAX_BATTLEGROUND_QUEUE_TYPES)) == QueueSlots::NOWHERE);
}

TEST_CASE("queues: giving one up frees its slot and leaves the others alone")
{
    QueueSlots slots;
    slots.Take(WARSONG);
    uint32 const arathi = slots.Take(ARATHI);
    slots.Take(ALTERAC);

    slots.Give(ARATHI);

    CHECK_FALSE(slots.Holds(ARATHI));
    CHECK(slots.Holds(WARSONG));
    CHECK(slots.Holds(ALTERAC));
    CHECK(slots.AnyFree());
    CHECK(slots.Kind(arathi) == BATTLEGROUND_QUEUE_NONE);
}

TEST_CASE("queues: giving up one he does not hold changes nothing")
{
    QueueSlots slots;
    slots.Take(WARSONG);

    slots.Give(ARATHI);

    CHECK(slots.Holds(WARSONG));
    CHECK(slots.AnyHeld());
}

TEST_CASE("queues: a slot keeps its number when a neighbour is given up")
{
    QueueSlots slots;
    uint32 const warsong = slots.Take(WARSONG);
    uint32 const arathi = slots.Take(ARATHI);

    slots.Give(WARSONG);

    CHECK(slots.SlotOf(ARATHI) == arathi);
    CHECK(arathi != warsong);
}

TEST_CASE("queues: he is not called until a battleground calls him")
{
    QueueSlots slots;
    slots.Take(WARSONG);

    CHECK_FALSE(slots.Called(WARSONG));

    slots.CalledTo(WARSONG, 42);

    CHECK(slots.Called(WARSONG));
    CHECK(slots.CalledToInstance(42));
    CHECK_FALSE(slots.CalledToInstance(43));
}

TEST_CASE("queues: a call does not reach a queue he is not standing in")
{
    QueueSlots slots;
    slots.Take(WARSONG);

    slots.CalledTo(ARATHI, 42);

    CHECK_FALSE(slots.Called(ARATHI));
    CHECK_FALSE(slots.CalledToInstance(42));
}

TEST_CASE("queues: giving up a slot takes the call in it with it")
{
    QueueSlots slots;
    slots.Take(WARSONG);
    slots.CalledTo(WARSONG, 42);

    slots.Give(WARSONG);

    CHECK_FALSE(slots.Called(WARSONG));
    CHECK_FALSE(slots.CalledToInstance(42));
}

TEST_CASE("queues: standing in a queue again clears the call that was in the slot")
{
    QueueSlots slots;
    slots.Take(WARSONG);
    slots.CalledTo(WARSONG, 42);

    slots.Take(WARSONG);

    CHECK_FALSE(slots.Called(WARSONG));
}

TEST_CASE("queues: an empty slot is not a call to no battleground")
{
    QueueSlots slots;

    CHECK_FALSE(slots.CalledToInstance(0));

    slots.Take(WARSONG);

    CHECK_FALSE(slots.CalledToInstance(0));
}
