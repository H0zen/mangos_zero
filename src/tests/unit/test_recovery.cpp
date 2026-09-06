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

// The clocks a unit's own recovery runs on, and what one tick gives back.

#include "doctest.h"

#include "Stats/Recovery.h"

TEST_CASE("recovery: a clock that was never set is due at once")
{
    Recovery clocks;

    CHECK(clocks.Due());
    CHECK(clocks.Left() == 0);
}

TEST_CASE("recovery: a tick falls due when the clock runs out, not before")
{
    Recovery clocks;
    clocks.NextIn(2000);

    clocks.Run(500);
    CHECK_FALSE(clocks.Due());
    CHECK(clocks.Left() == 1500);

    clocks.Run(1499);
    CHECK_FALSE(clocks.Due());

    clocks.Run(1);
    CHECK(clocks.Due());
}

TEST_CASE("recovery: an overlong step spends the clock rather than wrapping it")
{
    Recovery clocks;
    clocks.NextIn(100);

    clocks.Run(5000);

    CHECK(clocks.Due());
    CHECK(clocks.Left() == 0);
}

TEST_CASE("recovery: a spent clock stays spent until it is set again")
{
    Recovery clocks;
    clocks.NextIn(100);
    clocks.Run(100);

    clocks.Run(100);
    CHECK(clocks.Due());

    clocks.NextIn(2000);
    CHECK_FALSE(clocks.Due());
}

TEST_CASE("recovery: spending mana holds the spirit-fed share back")
{
    Recovery clocks;

    CHECK_FALSE(clocks.HoldingBack());

    clocks.ManaSpent();
    CHECK(clocks.HoldingBack());

    clocks.RunHold(Recovery::HOLD - 1);
    CHECK(clocks.HoldingBack());

    clocks.RunHold(1);
    CHECK_FALSE(clocks.HoldingBack());
}

TEST_CASE("recovery: the two clocks do not spend each other")
{
    Recovery clocks;
    clocks.NextIn(2000);
    clocks.ManaSpent();

    clocks.Run(2000);

    CHECK(clocks.Due());
    CHECK(clocks.HoldingBack());
}

TEST_CASE("recovery: at rest a bar refills by a third of itself")
{
    regen::Rates const rates;

    auto const share = regen::PowerTick(POWER_MANA, 300.0f, 3000, false, false, rates);

    CHECK(share.any);
    CHECK(share.amount == doctest::Approx(1000.0f));
}

TEST_CASE("recovery: in a fight mana comes from spirit, not from the bar")
{
    regen::Rates const rates;

    auto const share = regen::PowerTick(POWER_MANA, 300.0f, 3000, true, false, rates);

    CHECK(share.any);
    CHECK(share.amount == doctest::Approx(300.0f / 5.0f + 17.0f));
}

TEST_CASE("recovery: mana just spent gives nothing back at all")
{
    regen::Rates const rates;

    auto const share = regen::PowerTick(POWER_MANA, 300.0f, 3000, true, true, rates);

    CHECK(share.any);
    CHECK(share.amount == doctest::Approx(0.0f));

    // the hold only bites on one answering to a fight or a master
    auto const resting = regen::PowerTick(POWER_MANA, 300.0f, 3000, false, true, rates);
    CHECK(resting.amount == doctest::Approx(1000.0f));
}

TEST_CASE("recovery: the world's rate scales what spirit gives")
{
    regen::Rates rates;
    rates.mana = 2.0f;

    auto const share = regen::PowerTick(POWER_MANA, 300.0f, 3000, true, false, rates);

    CHECK(share.amount == doctest::Approx((300.0f / 5.0f + 17.0f) * 2.0f));
}

TEST_CASE("recovery: energy and focus come back at a flat rate, fight or no fight")
{
    regen::Rates rates;
    rates.energy = 1.5f;
    rates.focus = 0.5f;

    CHECK(regen::PowerTick(POWER_ENERGY, 0.0f, 100, true, false, rates).amount == doctest::Approx(30.0f));
    CHECK(regen::PowerTick(POWER_ENERGY, 0.0f, 100, false, false, rates).amount == doctest::Approx(30.0f));
    CHECK(regen::PowerTick(POWER_FOCUS, 0.0f, 100, true, false, rates).amount == doctest::Approx(12.0f));
}

TEST_CASE("recovery: rage says nothing at all, because it does not come back on its own")
{
    regen::Rates const rates;

    auto const share = regen::PowerTick(POWER_RAGE, 300.0f, 1000, false, false, rates);

    CHECK_FALSE(share.any);
    CHECK(share.amount == doctest::Approx(0.0f));
}

TEST_CASE("recovery: one with no master heals a third of its bar")
{
    CHECK(regen::HealthTick(300.0f, 3000, false, false, 1.0f) == 1000);
}

TEST_CASE("recovery: one with a master heals off its spirit, less while it has mana")
{
    CHECK(regen::HealthTick(300.0f, 3000, true, true, 1.0f) == 75);
    CHECK(regen::HealthTick(300.0f, 3000, true, false, 1.0f) == 240);
}

TEST_CASE("recovery: a charmed creature with no spirit falls back to a third of its bar")
{
    // a charmed creature has spirit 0, so the sum rounds to nothing and the
    // fallback is what it actually gets
    CHECK(regen::HealthTick(0.0f, 3000, true, true, 1.0f) == 1000);

    // and so does one whose spirit is too small to round up to one
    CHECK(regen::HealthTick(3.0f, 3000, true, true, 1.0f) == 1000);
}
