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

// The two things every metric here is made of.
//
// A percentile that is wrong by one rank is worse than no percentile: it reads
// plausibly and moves the wrong lever. So the ranks are pinned exactly, on
// windows small enough to count by hand.

#include "doctest.h"

#include "Metrics/Distribution.h"
#include "Metrics/Rate.h"

using metrics::Distribution;
using metrics::Rate;

TEST_CASE("An empty distribution reports nothing rather than zero-as-a-value")
{
    Distribution<8> d;

    CHECK(d.Empty());
    CHECK(d.Count() == 0);
    CHECK(d.Percentile(0.5f) == 0);
    CHECK(d.Max() == 0);
}

TEST_CASE("One sample is every percentile of itself")
{
    Distribution<8> d;
    d.Add(42);

    CHECK(d.Count() == 1);
    CHECK(d.Percentile(0.f) == 42);
    CHECK(d.Percentile(0.5f) == 42);
    CHECK(d.Percentile(0.99f) == 42);
    CHECK(d.Percentile(1.f) == 42);
    CHECK(d.Max() == 42);
}

TEST_CASE("Ranks land where counting by hand puts them")
{
    // Ten samples, 10 through 100. Nearest-rank: p50 is the fifth, p90 the
    // ninth, p100 the tenth.
    Distribution<16> d;
    for (uint32 i = 1; i <= 10; ++i)
    {
        d.Add(i * 10);
    }

    CHECK(d.Count() == 10);
    CHECK(d.Percentile(0.1f) == 10);
    CHECK(d.Percentile(0.5f) == 50);
    CHECK(d.Percentile(0.9f) == 90);
    CHECK(d.Percentile(1.f) == 100);
    CHECK(d.Max() == 100);
}

TEST_CASE("Order of arrival does not change the answer")
{
    Distribution<16> ascending;
    Distribution<16> descending;

    for (uint32 i = 1; i <= 10; ++i)
    {
        ascending.Add(i * 10);
        descending.Add((11 - i) * 10);
    }

    CHECK(ascending.Percentile(0.5f) == descending.Percentile(0.5f));
    CHECK(ascending.Percentile(0.9f) == descending.Percentile(0.9f));
    CHECK(ascending.Max() == descending.Max());
}

TEST_CASE("The window forgets its oldest sample")
{
    // What makes this report the last minute rather than the whole evening: a
    // single 300 ms tick at startup must not sit in p99 forever.
    Distribution<4> d;

    d.Add(1000);
    d.Add(1);
    d.Add(2);
    d.Add(3);
    REQUIRE(d.Max() == 1000);

    d.Add(4);   // pushes the 1000 out

    CHECK(d.Count() == 4);
    CHECK(d.Max() == 4);
    CHECK(d.Percentile(0.5f) == 2);
}

TEST_CASE("A tail hidden by a mean is visible in the percentiles")
{
    // The case the whole type exists for: ninety-nine quick ticks and one slow
    // one. The mean says 7 ms and nothing is wrong; p99 says otherwise.
    Distribution<128> d;
    for (int i = 0; i < 99; ++i)
    {
        d.Add(4);
    }
    d.Add(300);

    CHECK(d.Percentile(0.5f) == 4);
    CHECK(d.Percentile(0.99f) == 4);
    CHECK(d.Percentile(1.f) == 300);
    CHECK(d.Max() == 300);
}

TEST_CASE("Reset empties the window")
{
    Distribution<8> d;
    d.Add(5);
    d.Add(6);
    REQUIRE_FALSE(d.Empty());

    d.Reset();

    CHECK(d.Empty());
    CHECK(d.Percentile(0.5f) == 0);
    CHECK(d.Max() == 0);
}

TEST_CASE("A fresh rate has counted nothing")
{
    Rate r;

    CHECK(r.Pending() == 0);
    CHECK(r.Total() == 0);
    CHECK(r.Sample(1000) == doctest::Approx(0.f));
}

TEST_CASE("A rate is events over the window, in seconds")
{
    Rate r;
    for (int i = 0; i < 50; ++i)
    {
        r.Add();
    }

    CHECK(r.Pending() == 50);
    CHECK(r.Sample(5000) == doctest::Approx(10.f));   // fifty over five seconds
}

TEST_CASE("Sampling consumes the window but not the total")
{
    Rate r;
    r.Add(10);
    r.Sample(1000);

    CHECK(r.Pending() == 0);
    CHECK(r.Total() == 10);

    r.Add(5);
    CHECK(r.Sample(1000) == doctest::Approx(5.f));
    CHECK(r.Total() == 15);
}

TEST_CASE("A zero-length window reports zero rather than an infinity")
{
    // Sampled twice inside a millisecond, which is a caller's mistake. It must
    // not put an infinity in a log line and it must not divide by zero.
    Rate r;
    r.Add(7);

    CHECK(r.Sample(0) == doctest::Approx(0.f));

    // The events are still counted in the total, not lost.
    CHECK(r.Total() == 7);
}

TEST_CASE("Adding several at once counts them all")
{
    Rate r;
    r.Add(3);
    r.Add(4);

    CHECK(r.Pending() == 7);
    CHECK(r.Sample(1000) == doctest::Approx(7.f));
}

TEST_CASE("A total that never settles is what a leak looks like")
{
    // Total is deliberately monotonic: it is the shape a leak shows up in, so
    // it must not be reset by sampling.
    Rate r;
    for (int round = 0; round < 5; ++round)
    {
        r.Add(2);
        r.Sample(1000);
    }

    CHECK(r.Total() == 10);
}
