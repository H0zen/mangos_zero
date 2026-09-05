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

// The watch kept over work that promised to touch only what it owns.

#include "doctest.h"

#include "Threading/WorkSentry.h"

#include <string>
#include <thread>

TEST_CASE("sentry: nothing is watched until a watch is set")
{
    WorkSentry::Forget();

    CHECK(WorkSentry::Watched() == nullptr);

    WorkSentry::Reached("a question put to the database");

    CHECK(WorkSentry::Offences() == 0);
}

TEST_CASE("sentry: a watch names the work under it and steps down at the end")
{
    WorkSentry::Forget();

    {
        WorkSentry watch("CMSG_PETITION_SIGN");

        CHECK(std::string(WorkSentry::Watched()) == "CMSG_PETITION_SIGN");
    }

    CHECK(WorkSentry::Watched() == nullptr);
}

TEST_CASE("sentry: watches nest and the outer one comes back")
{
    WorkSentry::Forget();

    WorkSentry outer("CMSG_CAST_SPELL");
    {
        WorkSentry inner("CMSG_PETITION_SIGN");

        CHECK(std::string(WorkSentry::Watched()) == "CMSG_PETITION_SIGN");
    }

    CHECK(std::string(WorkSentry::Watched()) == "CMSG_CAST_SPELL");
}

TEST_CASE("sentry: a breach under watch is counted")
{
    WorkSentry::Forget();

    WorkSentry watch("CMSG_PETITION_SIGN");
    WorkSentry::Reached("a question put to the database");

    CHECK(WorkSentry::Offences() == 1);
}

TEST_CASE("sentry: the same breach is told once and no more")
{
    WorkSentry::Forget();

    WorkSentry watch("CMSG_PETITION_SIGN");
    for (int again = 0; again < 100; ++again)
    {
        WorkSentry::Reached("a question put to the database");
    }

    CHECK(WorkSentry::Offences() == 1);
}

TEST_CASE("sentry: one work reaching two things is two breaches")
{
    WorkSentry::Forget();

    WorkSentry watch("CMSG_PETITION_SIGN");
    WorkSentry::Reached("a question put to the database");
    WorkSentry::Reached("the roster of everyone online");

    CHECK(WorkSentry::Offences() == 2);
}

TEST_CASE("sentry: two works reaching the same thing are two breaches")
{
    WorkSentry::Forget();

    {
        WorkSentry watch("CMSG_PETITION_SIGN");
        WorkSentry::Reached("a question put to the database");
    }
    {
        WorkSentry watch("CMSG_BUG");
        WorkSentry::Reached("a question put to the database");
    }

    CHECK(WorkSentry::Offences() == 2);
}

TEST_CASE("sentry: what one thread watches is not what another watches")
{
    WorkSentry::Forget();

    WorkSentry mine("CMSG_CAST_SPELL");

    char const* seen = "not run";
    std::thread other([&seen]()
    {
        seen = WorkSentry::Watched() ? WorkSentry::Watched() : "nothing";
    });
    other.join();

    CHECK(std::string(seen) == "nothing");
    CHECK(std::string(WorkSentry::Watched()) == "CMSG_CAST_SPELL");
}

TEST_CASE("sentry: breaches from two threads are counted together")
{
    WorkSentry::Forget();

    std::thread first([]()
    {
        WorkSentry watch("CMSG_PETITION_SIGN");
        WorkSentry::Reached("a question put to the database");
    });
    std::thread second([]()
    {
        WorkSentry watch("CMSG_BUG");
        WorkSentry::Reached("a write queued for the database");
    });
    first.join();
    second.join();

    CHECK(WorkSentry::Offences() == 2);
}
