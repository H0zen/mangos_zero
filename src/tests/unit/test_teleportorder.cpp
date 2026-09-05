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

// An order to send a character somewhere: whether it may wait for the tick to
// finish, and whether it is still worth making when the tick ends.

#include "doctest.h"

#include "Teleport/TeleportOrder.h"

TEST_CASE("order: a fresh one is not in the air and owes nothing")
{
    TeleportOrder order;

    CHECK_FALSE(order.InFlight());
    CHECK_FALSE(order.InFlightNear());
    CHECK_FALSE(order.InFlightFar());
    CHECK(order.Owed() == 0);
    CHECK(order.Options() == 0);
}

TEST_CASE("order: either flight counts as being in the air")
{
    TeleportOrder near;
    near.FlyingNear(true);
    CHECK(near.InFlight());
    CHECK(near.InFlightNear());
    CHECK_FALSE(near.InFlightFar());

    TeleportOrder far;
    far.FlyingFar(true);
    CHECK(far.InFlight());
    CHECK(far.InFlightFar());
    CHECK_FALSE(far.InFlightNear());
}

TEST_CASE("order: aiming keeps both the place and the terms")
{
    TeleportOrder order;
    order.Aim(Geometry::Placement::Somewhere(571, Geometry::Vector3(1.0f, 2.0f, 3.0f), 1.5f), 0x04);

    CHECK(order.To().MapId() == 571);
    CHECK(order.To().X() == doctest::Approx(1.0f));
    CHECK(order.To().Facing() == doctest::Approx(1.5f));
    CHECK(order.Options() == 0x04);
}

TEST_CASE("waiting: an order given when none may wait is made at once")
{
    TeleportOrder order;
    order.MayWait(false);

    CHECK_FALSE(order.WaitIfItMay(true));
    CHECK_FALSE(order.Waits(true));
}

TEST_CASE("waiting: an order given during a tick waits for the end of it")
{
    TeleportOrder order;
    order.MayWait(true);

    CHECK(order.WaitIfItMay(true));
    CHECK(order.Waits(true));
}

TEST_CASE("waiting: one given to a living character is dropped if he dies first")
{
    TeleportOrder order;
    order.MayWait(true);
    order.WaitIfItMay(true);

    // He became a ghost during the same tick; sending him would pull him off the
    // graveyard he was just put at.
    CHECK_FALSE(order.Waits(false));
}

TEST_CASE("waiting: one given to a character already dead is made either way")
{
    TeleportOrder order;
    order.MayWait(true);
    order.WaitIfItMay(false);

    CHECK(order.Waits(false));
    CHECK(order.Waits(true));
}

TEST_CASE("arrival: what is owed adds up and is settled in one go")
{
    TeleportOrder order;

    order.OnArrival(DELAYED_RESURRECT_PLAYER);
    order.OnArrival(DELAYED_SAVE_PLAYER);

    CHECK(order.Owes(DELAYED_RESURRECT_PLAYER));
    CHECK(order.Owes(DELAYED_SAVE_PLAYER));
    CHECK_FALSE(order.Owes(DELAYED_SPELL_CAST_DESERTER));
    CHECK(order.Owed() != 0);

    order.Settled();

    CHECK(order.Owed() == 0);
    CHECK_FALSE(order.Owes(DELAYED_RESURRECT_PLAYER));
}

TEST_CASE("arrival: nothing beyond the three kinds is ever taken on")
{
    TeleportOrder order;

    order.OnArrival(DELAYED_END);
    CHECK(order.Owed() == 0);

    order.OnArrival(0x40);
    CHECK(order.Owed() == 0);
}
