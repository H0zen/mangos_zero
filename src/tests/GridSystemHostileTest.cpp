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
 * @file GridSystemHostileTest.cpp
 * @brief The grid's reference machinery, walked the ways nobody walks it.
 *
 * Every object on a map holds a GridReference, and the grid holds them back
 * through a GridRefManager. The list is intrusive and the links are cut from
 * both ends -- by the object dying and by the grid dropping it -- so the
 * interesting cases are all about doing that twice, in the wrong order, or
 * backwards.
 */

#include "TestHarness.h"

#include "GameSystem/GridRefManager.h"
#include "GameSystem/GridReference.h"

#include <vector>

namespace
{
    /// The smallest thing a grid can hold: something that owns its own link.
    struct Dummy
    {
        GridReference<Dummy> ref;
    };
}

TEST(GridRefManager_ForwardWalkSeesEverythingLinked)
{
    GridRefManager<Dummy> manager;
    Dummy a, b, c;
    a.ref.link(&manager, &a);
    b.ref.link(&manager, &b);
    c.ref.link(&manager, &c);

    int seen = 0;
    for (GridRefManager<Dummy>::iterator it = manager.begin(); it != manager.end(); ++it)
    {
        ++seen;
    }

    CHECK(seen == 3);
    CHECK(manager.getSize() == 3);
}

TEST(GridRefManager_ReverseWalkSeesEverythingLinked)
{
    // ===== rbegin() MUST WALK BACKWARDS =====
    //
    // rbegin() hands back the LAST element wrapped in the same forward iterator
    // begin() uses, whose operator++ follows next(). Starting at the last element
    // and stepping forward reaches the end immediately, so the loop body runs
    // once and a caller iterating "in reverse" silently processes one object out
    // of however many are on the grid.
    // ========================================
    GridRefManager<Dummy> manager;
    Dummy a, b, c;
    a.ref.link(&manager, &a);
    b.ref.link(&manager, &b);
    c.ref.link(&manager, &c);

    int seen = 0;
    std::vector<Dummy*> order;
    for (auto it = manager.rbegin(); it != manager.rend(); ++it)
    {
        ++seen;
        order.push_back(it->getSource());
    }

    CHECK(seen == 3);
    REQUIRE(order.size() == 3);

    // insertFirst, so the last one linked is the first one out.
    CHECK(order[0] == &a);
    CHECK(order[2] == &c);
}

TEST(GridReference_UnlinkTwiceIsANoOp)
{
    GridRefManager<Dummy> manager;
    Dummy a;

    a.ref.link(&manager, &a);
    CHECK(a.ref.isValid());
    CHECK(manager.getSize() == 1);

    a.ref.unlink();
    CHECK(!a.ref.isValid());
    CHECK(manager.getSize() == 0);

    // The size must not go round the houses: getSize() is unsigned, so one extra
    // decrement reads back as 4294967295 rather than as -1.
    a.ref.unlink();
    CHECK(!a.ref.isValid());
    CHECK(manager.getSize() == 0);
}

TEST(GridReference_DestructorUnlinksWhileStillLinked)
{
    GridRefManager<Dummy> manager;

    {
        Dummy b;
        b.ref.link(&manager, &b);
        CHECK(manager.begin() != manager.end());
    }

    CHECK(manager.begin() == manager.end());
    CHECK(manager.getSize() == 0);
}

TEST(GridReference_RelinkingMovesTheReferenceToTheNewManager)
{
    // link() on a live reference unlinks the old one first. If it did not, the
    // old manager would keep a pointer to a reference that now belongs to
    // another list, and the next walk of either would leave the list it started in.
    GridRefManager<Dummy> first;
    GridRefManager<Dummy> second;
    Dummy a;

    a.ref.link(&first, &a);
    CHECK(first.getSize() == 1);

    a.ref.link(&second, &a);
    CHECK(first.getSize() == 0);
    CHECK(second.getSize() == 1);
    CHECK(first.begin() == first.end());
    CHECK(second.begin() != second.end());
}

TEST(GridReference_InvalidateKeepsTheSourceAndDropsTheTarget)
{
    // invalidate() is the target's way out: the grid is going away and tells
    // every reference so. The source must survive it -- callers read
    // getSource() afterwards to know what they lost.
    GridRefManager<Dummy> manager;
    Dummy a;
    a.ref.link(&manager, &a);

    a.ref.invalidate();

    CHECK(!a.ref.isValid());
    CHECK(a.ref.getSource() == &a);
    CHECK(manager.getSize() == 0);
    CHECK(manager.begin() == manager.end());
}

TEST(GridRefManager_EmptyManagerWalksNowhere)
{
    GridRefManager<Dummy> manager;

    CHECK(manager.getSize() == 0);
    CHECK(manager.begin() == manager.end());
    CHECK(manager.rbegin() == manager.rend());
    CHECK(manager.getFirst() == nullptr);
    CHECK(manager.getLast() == nullptr);
}
