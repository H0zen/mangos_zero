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

// Adding up what a character did in the war.
//
// The client is never sent the ledger, only what it comes to over four windows,
// and the windows overlap. Every rule below is one the server has always had and
// none of them was written down anywhere.

#include "doctest.h"

#include "Honor/HonorTally.h"
#include "ObjectGuid.h"

namespace
{
    HonorWindows Week(uint32 today, uint32 weekBegin)
    {
        HonorWindows when;
        when.today = today;
        when.thisWeekBegin = weekBegin;
        return when;
    }

    HonorEntry Won(uint32 date, float points, bool kill = true)
    {
        HonorEntry entry{};
        entry.type = HONORABLE;
        entry.date = date;
        entry.honorPoints = points;
        entry.isKill = kill;
        entry.state = HK_UNCHANGED;
        entry.victimType = TYPEID_PLAYER;
        entry.victimID = 1;
        return entry;
    }

    HonorEntry Lost(uint32 date, float points)
    {
        HonorEntry entry = Won(date, points);
        entry.type = DISHONORABLE;
        entry.victimType = TYPEID_UNIT;
        return entry;
    }
}

TEST_CASE("honour: an empty ledger comes to what was carried into it")
{
    HonorEntries none;

    HonorTally const tally = TallyHonor(none, Week(100, 98), 7, 3);

    CHECK(tally.lifetimeHonorable == 7);
    CHECK(tally.lifetimeDishonorable == 3);
    CHECK(tally.todayHonorable == 0);
    CHECK(tally.thisWeekHonor == doctest::Approx(0.0f));
}

TEST_CASE("honour: a kill today is counted in every window that holds today")
{
    HonorEntries entries{ Won(100, 50.0f) };

    // The week began two days ago, so today falls inside it as well.
    HonorTally const tally = TallyHonor(entries, Week(100, 98), 0, 0);

    CHECK(tally.todayHonorable == 1);
    CHECK(tally.thisWeekKills == 1);
    CHECK(tally.thisWeekHonor == doctest::Approx(50.0f));
    CHECK(tally.lifetimeHonorable == 1);

    // And in none of the windows that do not.
    CHECK(tally.yesterdayKills == 0);
    CHECK(tally.lastWeekKills == 0);
}

TEST_CASE("honour: points can be won without a kill, and then no kill is counted")
{
    // A battleground objective: worth points, kills nobody.
    HonorEntries entries{ Won(100, 40.0f, false) };

    HonorTally const tally = TallyHonor(entries, Week(100, 98), 0, 0);

    CHECK(tally.thisWeekHonor == doctest::Approx(40.0f));
    CHECK(tally.thisWeekKills == 0);
    CHECK(tally.todayHonorable == 0);
    CHECK(tally.lifetimeHonorable == 0);
}

TEST_CASE("honour: yesterday is the single day before today, not everything before it")
{
    HonorEntries entries{ Won(99, 10.0f), Won(98, 10.0f) };

    HonorTally const tally = TallyHonor(entries, Week(100, 95), 0, 0);

    CHECK(tally.yesterdayKills == 1);
    CHECK(tally.yesterdayHonor == doctest::Approx(10.0f));

    // Both are still inside this week.
    CHECK(tally.thisWeekKills == 2);
    CHECK(tally.thisWeekHonor == doctest::Approx(20.0f));
}

TEST_CASE("honour: the two weeks overlap on the day the older one ends")
{
    // This week runs [95..102] closed, last week [88..95) half-open. Day 95 is
    // the beginning of this week and the day last week stops short of.
    HonorEntries entries{ Won(95, 10.0f) };

    HonorTally const tally = TallyHonor(entries, Week(100, 95), 0, 0);

    CHECK(tally.thisWeekKills == 1);
    CHECK(tally.lastWeekKills == 0);

    // Day 94 is the last day last week takes, and this week does not.
    HonorEntries older{ Won(94, 10.0f) };
    HonorTally const before = TallyHonor(older, Week(100, 95), 0, 0);

    CHECK(before.lastWeekKills == 1);
    CHECK(before.thisWeekKills == 0);
}

TEST_CASE("honour: a struck-out entry counts for nothing at all")
{
    HonorEntries entries{ Won(100, 50.0f) };
    entries.front().state = HK_DELETED;

    HonorTally const tally = TallyHonor(entries, Week(100, 98), 0, 0);

    CHECK(tally.todayHonorable == 0);
    CHECK(tally.thisWeekHonor == doctest::Approx(0.0f));
    CHECK(tally.lifetimeHonorable == 0);
}

TEST_CASE("honour: a dishonourable kill counts for today and for the lifetime only")
{
    HonorEntries entries{ Lost(100, 25.0f) };

    HonorTally const tally = TallyHonor(entries, Week(100, 98), 0, 0);

    CHECK(tally.todayDishonorable == 1);
    CHECK(tally.lifetimeDishonorable == 1);

    // It never reaches the week windows, which carry honourable points alone.
    CHECK(tally.thisWeekKills == 0);
    CHECK(tally.thisWeekHonor == doctest::Approx(0.0f));
    CHECK(tally.todayHonorable == 0);
}

TEST_CASE("honour: a dishonourable kill dated ahead of today is retired")
{
    HonorEntry const ahead = Lost(101, 25.0f);
    HonorEntry const now = Lost(100, 25.0f);
    HonorEntry const honourableAhead = Won(101, 25.0f);

    CHECK(IsAheadOfToday(ahead, 100));
    CHECK_FALSE(IsAheadOfToday(now, 100));

    // Only a dishonourable one, and only when it is a kill.
    CHECK_FALSE(IsAheadOfToday(honourableAhead, 100));
}

TEST_CASE("honour: a whole ledger adds up window by window")
{
    HonorEntries entries{
        Won(100, 30.0f),          // today
        Won(99, 20.0f),           // yesterday
        Won(96, 10.0f, false),    // this week, points only
        Won(90, 40.0f),           // last week
        Lost(100, 5.0f),          // today, dishonourable
    };
    entries.push_back(Won(100, 99.0f));
    entries.back().state = HK_DELETED;

    HonorTally const tally = TallyHonor(entries, Week(100, 95), 2, 1);

    CHECK(tally.todayHonorable == 1);
    CHECK(tally.todayDishonorable == 1);
    CHECK(tally.yesterdayKills == 1);
    CHECK(tally.yesterdayHonor == doctest::Approx(20.0f));
    CHECK(tally.thisWeekKills == 2);
    CHECK(tally.thisWeekHonor == doctest::Approx(60.0f));
    CHECK(tally.lastWeekKills == 1);
    CHECK(tally.lastWeekHonor == doctest::Approx(40.0f));

    // Three honourable kills on top of the two carried in, one dishonourable on
    // top of the one carried in.
    CHECK(tally.lifetimeHonorable == 5);
    CHECK(tally.lifetimeDishonorable == 2);
}

TEST_CASE("bar: the fill is where he stands between his rank's floor and ceiling")
{
    // Halfway through a rank that runs from 5000 to 10000.
    CHECK(HonorBarFill(7500.0f, 5000.0f, 10000.0f, true) == 127);

    CHECK(HonorBarFill(5000.0f, 5000.0f, 10000.0f, true) == 0);
    CHECK(HonorBarFill(10000.0f, 5000.0f, 10000.0f, true) == 255);
}

TEST_CASE("bar: a falling rank fills the other way, from the size of the drop")
{
    // Points are held as a negative number while he is in disgrace, and the bar
    // is drawn from how far he has fallen.
    CHECK(HonorBarFill(-7500.0f, 5000.0f, 10000.0f, false) == -127);
}

TEST_CASE("bar: a rank with no span at all reads as empty")
{
    CHECK(HonorBarFill(5000.0f, 5000.0f, 5000.0f, true) == 0);
    CHECK(HonorBarFill(5000.0f, 10000.0f, 5000.0f, true) == 0);
}
