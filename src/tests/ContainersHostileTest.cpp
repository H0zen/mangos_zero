/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
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

/**
 * @file ContainersHostileTest.cpp
 * @brief The intrusive list and the reference protocol built on it.
 *
 * These two carry the group memberships, the threat lists, the follower links
 * and the map references -- every long-lived relationship between two objects
 * that can die independently. The failure mode is never a wrong value; it is a
 * node still pointed at after it is gone.
 */

#include "TestHarness.h"

#include "Utilities/LinkedList.h"
#include "Utilities/LinkedReference/RefManager.h"
#include "Utilities/LinkedReference/Reference.h"
#include "Utilities/ConcurrentRegistry.h"

#include <vector>

namespace
{
    struct Node : public LinkedListElement
    {
        explicit Node(int v) : value(v) {}
        int value;
    };

    /// Counts every hook the protocol delivers, so a second delivery is visible.
    struct Target;
    struct Source;

    struct CountingRef : public Reference<Target, Source>
    {
        void targetObjectBuildLink() override { ++builds; }
        void targetObjectDestroyLink() override { ++destroys; }
        void sourceObjectDestroyLink() override { ++invalidates; }

        int builds = 0;
        int destroys = 0;
        int invalidates = 0;
    };

    struct Target : public RefManager<Target, Source> {};
    struct Source { CountingRef link; };
}

// ===== the intrusive list =====

TEST(LinkedList_ForwardAndReverseWalkAgreeOnTheContents)
{
    LinkedListHead list;
    Node a(1), b(2), c(3);
    list.insertLast(&a);
    list.insertLast(&b);
    list.insertLast(&c);

    std::vector<int> forward;
    for (LinkedListElement* e = list.getFirst(); e; e = e->next())
    {
        forward.push_back(static_cast<Node*>(e)->value);
    }

    std::vector<int> backward;
    for (LinkedListElement* e = list.getLast(); e; e = e->prev())
    {
        backward.push_back(static_cast<Node*>(e)->value);
    }

    REQUIRE(forward.size() == 3);
    REQUIRE(backward.size() == 3);
    CHECK(forward[0] == 1 && forward[1] == 2 && forward[2] == 3);
    CHECK(backward[0] == 3 && backward[1] == 2 && backward[2] == 1);
}

TEST(LinkedList_DelinkTwiceIsANoOp)
{
    LinkedListHead list;
    Node a(1), b(2), c(3);
    list.insertLast(&a);
    list.insertLast(&b);
    list.insertLast(&c);

    b.delink();
    CHECK(!b.isInList());
    CHECK(list.getSize() == 2);

    // The second one must not walk the stale iNext/iPrev it no longer owns.
    b.delink();
    CHECK(!b.isInList());
    CHECK(list.getSize() == 2);
    CHECK(static_cast<Node*>(list.getFirst())->value == 1);
    CHECK(static_cast<Node*>(list.getLast())->value == 3);
}

TEST(LinkedList_DestroyingANodeUnlinksIt)
{
    LinkedListHead list;
    Node a(1);
    list.insertLast(&a);

    {
        Node b(2);
        list.insertLast(&b);
        CHECK(list.getSize() == 2);
    }

    // b is gone. If its destructor had not delinked, getLast() would now be a
    // pointer into a dead stack frame.
    CHECK(list.getSize() == 1);
    CHECK(static_cast<Node*>(list.getFirst())->value == 1);
    CHECK(static_cast<Node*>(list.getLast())->value == 1);
}

TEST(LinkedList_EmptyListHasNoEnds)
{
    LinkedListHead list;

    CHECK(list.isEmpty());
    CHECK(list.getFirst() == nullptr);
    CHECK(list.getLast() == nullptr);
    CHECK(list.getSize() == 0);
}

TEST(LinkedList_InsertFirstAndInsertLastPutTheNodeWhereTheySay)
{
    LinkedListHead list;
    Node a(1), b(2), c(3);

    list.insertFirst(&b);
    list.insertFirst(&a);
    list.insertLast(&c);

    CHECK(static_cast<Node*>(list.getFirst())->value == 1);
    CHECK(static_cast<Node*>(list.getLast())->value == 3);
    CHECK(list.getSize() == 3);
}

// ===== the reference protocol =====

TEST(Reference_LinkDeliversBuildOnce)
{
    Target target;
    Source source;

    source.link.link(&target, &source);

    CHECK(source.link.isValid());
    CHECK(source.link.builds == 1);
    CHECK(source.link.getTarget() == &target);
    CHECK(source.link.getSource() == &source);
}

TEST(Reference_UnlinkTwiceDeliversDestroyOnce)
{
    // ===== THE HOOK RUNS ON A NULL TARGET =====
    //
    // unlink() calls targetObjectDestroyLink() before it checks anything, and
    // the first unlink already set iRefTo to NULL. Subclasses that reach through
    // getTarget() in that hook -- GroupReference, FollowerReference,
    // HostileReference all do -- therefore dereference NULL on the second call.
    //
    // It is reachable, not theoretical: Player::RemoveFromGroup unlinks
    // m_group, and ~GroupReference() unlinks it again when the player is
    // destroyed.
    // ==========================================
    Target target;
    Source source;

    source.link.link(&target, &source);
    source.link.unlink();
    CHECK(!source.link.isValid());
    CHECK(source.link.destroys == 1);

    source.link.unlink();
    CHECK(!source.link.isValid());
    CHECK(source.link.destroys == 1);
}

TEST(Reference_InvalidateTwiceDeliversInvalidateOnce)
{
    Target target;
    Source source;

    source.link.link(&target, &source);
    source.link.invalidate();
    CHECK(!source.link.isValid());
    CHECK(source.link.invalidates == 1);

    source.link.invalidate();
    CHECK(source.link.invalidates == 1);
}

TEST(Reference_InvalidateKeepsTheSource)
{
    // The whole point of invalidate() over unlink(): the target is gone but the
    // source is still there and still has to be told which one it lost.
    Target target;
    Source source;

    source.link.link(&target, &source);
    source.link.invalidate();

    CHECK(source.link.getTarget() == nullptr);
    CHECK(source.link.getSource() == &source);
}

TEST(Reference_UnlinkOnANeverLinkedReferenceIsANoOp)
{
    Source source;

    CHECK(!source.link.isValid());
    source.link.unlink();
    CHECK(!source.link.isValid());
    CHECK(source.link.destroys == 0);
}

TEST(Reference_RelinkingUnlinksTheOldTargetFirst)
{
    Target first;
    Target second;
    Source source;

    source.link.link(&first, &source);
    source.link.link(&second, &source);

    CHECK(source.link.getTarget() == &second);
    CHECK(source.link.builds == 2);
    CHECK(source.link.destroys == 1);
}

// ===== the registry =====

TEST(ConcurrentRegistry_FindsInsertsAndRemoves)
{
    int a = 1;
    int b = 2;
    MaNGOS::ConcurrentRegistry<int, int> registry;

    registry.Insert(1, &a);
    registry.Insert(2, &b);
    CHECK(registry.Size() == 2);
    CHECK(registry.Find(1) == &a);
    CHECK(registry.Find(2) == &b);

    registry.Remove(1);
    CHECK(registry.Find(1) == nullptr);
    CHECK(registry.Size() == 1);
}

TEST(ConcurrentRegistry_MissingKeyIsNullNotAnInsertion)
{
    MaNGOS::ConcurrentRegistry<int, int> registry;

    CHECK(registry.Find(7) == nullptr);
    CHECK(registry.Size() == 0);

    // Removing something that was never there must not create it, nor
    // underflow the size.
    registry.Remove(7);
    CHECK(registry.Size() == 0);
}

TEST(ConcurrentRegistry_InsertOnAnExistingKeyReplaces)
{
    int a = 1;
    int b = 2;
    MaNGOS::ConcurrentRegistry<int, int> registry;

    registry.Insert(1, &a);
    registry.Insert(1, &b);

    CHECK(registry.Size() == 1);
    CHECK(registry.Find(1) == &b);
}
