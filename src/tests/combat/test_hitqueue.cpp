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

// The queue that keeps a caused hit from happening inside the hit that caused it.

#include "doctest.h"

#include "Combat/HitQueue.h"

using combat::Blow;
using combat::HitQueue;
using combat::PendingHit;

namespace
{
    Blow Hit(int32 base)
    {
        Blow a;
        a.amount = base;
        return a;
    }
}

TEST_CASE("A new queue is empty and has dropped nothing")
{
    HitQueue queue;

    CHECK(queue.Empty());
    CHECK(queue.Size() == 0);
    CHECK(queue.Dropped() == 0);

    PendingHit out;
    CHECK_FALSE(queue.Pop(out));
}

TEST_CASE("A hit begun by intent starts at depth zero")
{
    HitQueue queue;
    queue.PushRoot(Hit(10));

    PendingHit out;
    REQUIRE(queue.Pop(out));
    CHECK(out.depth == 0);
    CHECK(out.blow.amount == 10);
}

TEST_CASE("A caused hit is one deeper than what caused it")
{
    HitQueue queue;
    CHECK(queue.Push(Hit(5), 0));

    PendingHit out;
    REQUIRE(queue.Pop(out));
    CHECK(out.depth == 1);

    CHECK(queue.Push(Hit(5), out.depth));
    REQUIRE(queue.Pop(out));
    CHECK(out.depth == 2);
}

TEST_CASE("Hits come out in the order they went in")
{
    HitQueue queue;
    queue.PushRoot(Hit(1));
    queue.PushRoot(Hit(2));
    queue.PushRoot(Hit(3));

    PendingHit out;
    REQUIRE(queue.Pop(out));
    CHECK(out.blow.amount == 1);
    REQUIRE(queue.Pop(out));
    CHECK(out.blow.amount == 2);
    REQUIRE(queue.Pop(out));
    CHECK(out.blow.amount == 3);
    CHECK_FALSE(queue.Pop(out));
}

TEST_CASE("A chain is cut at the depth cap")
{
    HitQueue queue;

    // A branch that has already come MAX_DEPTH steps takes no more.
    CHECK(queue.Push(Hit(1), HitQueue::MAX_DEPTH - 1));
    CHECK_FALSE(queue.Push(Hit(1), HitQueue::MAX_DEPTH));
    CHECK_FALSE(queue.Push(Hit(1), HitQueue::MAX_DEPTH + 1));
}

TEST_CASE("A refused hit is counted, not silently swallowed")
{
    HitQueue queue;

    CHECK_FALSE(queue.Push(Hit(1), HitQueue::MAX_DEPTH));
    CHECK(queue.Dropped() == 1);

    CHECK_FALSE(queue.Push(Hit(1), HitQueue::MAX_DEPTH));
    CHECK(queue.Dropped() == 2);

    queue.ClearDropped();
    CHECK(queue.Dropped() == 0);
}

TEST_CASE("A refused hit does not enter the queue")
{
    HitQueue queue;
    queue.Push(Hit(1), HitQueue::MAX_DEPTH);

    CHECK(queue.Empty());
}

TEST_CASE("A chain walked to its end terminates")
{
    // The property the cap exists for: two effects that answer each other
    // cannot run forever. Drive it as the real drain would.
    HitQueue queue;
    queue.PushRoot(Hit(100));

    int dealt = 0;
    PendingHit current;
    while (queue.Pop(current))
    {
        ++dealt;

        // Every hit provokes another, as a runaway pair of auras would.
        queue.Push(Hit(current.blow.amount), current.depth);

        REQUIRE(dealt < 1000);   // a live-lock would trip this before the cap
    }

    // The root plus one per level of depth allowed.
    CHECK(dealt == HitQueue::MAX_DEPTH + 1);
    CHECK(queue.Dropped() == 1);
}

TEST_CASE("Two branches are cut independently")
{
    // Depth belongs to the branch, not to the queue: a second chain starting
    // fresh gets its own full run rather than inheriting the first's spend.
    HitQueue queue;

    for (int branch = 0; branch < 2; ++branch)
    {
        queue.ClearDropped();
        queue.PushRoot(Hit(1));

        int dealt = 0;
        PendingHit current;
        while (queue.Pop(current))
        {
            ++dealt;
            queue.Push(Hit(1), current.depth);
            REQUIRE(dealt < 1000);
        }

        CHECK(dealt == HitQueue::MAX_DEPTH + 1);
    }
}

TEST_CASE("Clear empties the queue and the count together")
{
    HitQueue queue;
    queue.PushRoot(Hit(1));
    queue.Push(Hit(1), HitQueue::MAX_DEPTH);

    REQUIRE_FALSE(queue.Empty());
    REQUIRE(queue.Dropped() == 1);

    queue.Clear();

    CHECK(queue.Empty());
    CHECK(queue.Dropped() == 0);
}

TEST_CASE("The queued attempt survives intact")
{
    HitQueue queue;

    Blow a;
    a.attacker = ObjectGuid(HIGHGUID_PLAYER, static_cast<uint32>(11));
    a.victim = ObjectGuid(HIGHGUID_UNIT, static_cast<uint32>(7), static_cast<uint32>(22));
    a.delivery = combat::Delivery::Spell;
    a.spellId = 133;
    a.effectIndex = 1;
    a.school = combat::School::Fire;
    a.amount = 250;
    a.canCrit = false;
    a.triggered = true;

    queue.PushRoot(a);

    PendingHit out;
    REQUIRE(queue.Pop(out));

    CHECK(out.blow.attacker == a.attacker);
    CHECK(out.blow.victim == a.victim);
    CHECK(out.blow.spellId == 133);
    CHECK(out.blow.effectIndex == 1);
    CHECK(out.blow.amount == 250);
    CHECK_FALSE(out.blow.canCrit);
    CHECK(out.blow.triggered);
}
