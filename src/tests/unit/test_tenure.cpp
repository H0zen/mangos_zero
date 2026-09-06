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

// The term a called-up unit stays on, and the ten rules it can be granted under.

#include "doctest.h"

#include "Tenure.h"

namespace
{
    uint32 const TERM = 1000;
    uint32 const TICK = 100;

    tenure::Body Fighting()
    {
        tenure::Body how;
        how.inCombat = true;
        return how;
    }

    tenure::Body Dead()
    {
        tenure::Body how;
        how.alive = false;
        how.dead = true;
        return how;
    }

    tenure::Body Corpse()
    {
        tenure::Body how;
        how.alive = false;
        how.corpse = true;
        return how;
    }

    tenure::Body Gone()
    {
        tenure::Body how;
        how.alive = false;
        how.despawned = true;
        return how;
    }
}

TEST_CASE("tenure: a term granted under no rule never runs out")
{
    tenure::Body const well;

    auto const said = tenure::Tick(TEMPSPAWN_MANUAL_DESPAWN, TERM, TERM, TICK, well);

    CHECK_FALSE(said.vanish);
    CHECK(said.left == TERM);
}

TEST_CASE("tenure: a timed term spends what elapses and is up at nought")
{
    tenure::Body const well;

    auto said = tenure::Tick(TEMPSPAWN_TIMED_DESPAWN, TERM, TERM, TICK, well);
    CHECK_FALSE(said.vanish);
    CHECK(said.left == TERM - TICK);

    said = tenure::Tick(TEMPSPAWN_TIMED_DESPAWN, TICK, TERM, TICK, well);
    CHECK(said.vanish);
}

TEST_CASE("tenure: an out-of-combat term is measured from the last fight")
{
    tenure::Body const well;

    // the fight puts the clock back to full
    auto said = tenure::Tick(TEMPSPAWN_TIMED_OOC_DESPAWN, 200, TERM, TICK, Fighting());
    CHECK_FALSE(said.vanish);
    CHECK(said.left == TERM);

    // out of it, the clock runs again
    said = tenure::Tick(TEMPSPAWN_TIMED_OOC_DESPAWN, 200, TERM, TICK, well);
    CHECK_FALSE(said.vanish);
    CHECK(said.left == 100);

    // and a fight saves one whose time was all but spent
    said = tenure::Tick(TEMPSPAWN_TIMED_OOC_DESPAWN, 1, TERM, TICK, Fighting());
    CHECK_FALSE(said.vanish);
}

TEST_CASE("tenure: a corpse-timed term only runs while there is a corpse")
{
    tenure::Body const well;

    // alive, the clock stands still
    auto said = tenure::Tick(TEMPSPAWN_CORPSE_TIMED_DESPAWN, TERM, TERM, TICK, well);
    CHECK_FALSE(said.vanish);
    CHECK(said.left == TERM);

    // a corpse spends it
    said = tenure::Tick(TEMPSPAWN_CORPSE_TIMED_DESPAWN, TERM, TERM, TICK, Corpse());
    CHECK_FALSE(said.vanish);
    CHECK(said.left == TERM - TICK);

    said = tenure::Tick(TEMPSPAWN_CORPSE_TIMED_DESPAWN, TICK, TERM, TICK, Corpse());
    CHECK(said.vanish);

    // and a body that is gone ends it whatever the clock says
    said = tenure::Tick(TEMPSPAWN_CORPSE_TIMED_DESPAWN, TERM, TERM, TICK, Gone());
    CHECK(said.vanish);
}

TEST_CASE("tenure: a corpse term is up the moment it dies")
{
    tenure::Body const well;

    CHECK_FALSE(tenure::Tick(TEMPSPAWN_CORPSE_DESPAWN, TERM, TERM, TICK, well).vanish);
    CHECK(tenure::Tick(TEMPSPAWN_CORPSE_DESPAWN, TERM, TERM, TICK, Dead()).vanish);

    // a corpse is not the skipped-corpse state this rule reads
    CHECK_FALSE(tenure::Tick(TEMPSPAWN_CORPSE_DESPAWN, TERM, TERM, TICK, Corpse()).vanish);
}

TEST_CASE("tenure: a dead term is up when the body is gone")
{
    CHECK_FALSE(tenure::Tick(TEMPSPAWN_DEAD_DESPAWN, TERM, TERM, TICK, Corpse()).vanish);
    CHECK(tenure::Tick(TEMPSPAWN_DEAD_DESPAWN, TERM, TERM, TICK, Gone()).vanish);
}

TEST_CASE("tenure: out-of-combat time, or the moment it dies")
{
    tenure::Body const well;
    auto const rule = TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN;

    CHECK(tenure::Tick(rule, TERM, TERM, TICK, Dead()).vanish);

    auto said = tenure::Tick(rule, 200, TERM, TICK, Fighting());
    CHECK_FALSE(said.vanish);
    CHECK(said.left == TERM);

    said = tenure::Tick(rule, TICK, TERM, TICK, well);
    CHECK(said.vanish);
}

TEST_CASE("tenure: out-of-combat time, or when the body is gone")
{
    tenure::Body const well;
    auto const rule = TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN;

    CHECK(tenure::Tick(rule, TERM, TERM, TICK, Gone()).vanish);

    // a corpse is neither fighting nor gone, and its clock stands at full
    auto said = tenure::Tick(rule, 200, TERM, TICK, Corpse());
    CHECK_FALSE(said.vanish);
    CHECK(said.left == TERM);

    said = tenure::Tick(rule, TICK, TERM, TICK, well);
    CHECK(said.vanish);
}

TEST_CASE("tenure: timed, or the moment it dies")
{
    tenure::Body const well;
    auto const rule = TEMPSPAWN_TIMED_OR_CORPSE_DESPAWN;

    CHECK(tenure::Tick(rule, TERM, TERM, TICK, Dead()).vanish);

    // a fight does not stop this clock
    auto said = tenure::Tick(rule, TERM, TERM, TICK, Fighting());
    CHECK_FALSE(said.vanish);
    CHECK(said.left == TERM - TICK);

    CHECK(tenure::Tick(rule, TICK, TERM, TICK, well).vanish);
}

TEST_CASE("tenure: timed, or when the body is gone")
{
    tenure::Body const well;
    auto const rule = TEMPSPAWN_TIMED_OR_DEAD_DESPAWN;

    CHECK(tenure::Tick(rule, TERM, TERM, TICK, Gone()).vanish);
    CHECK_FALSE(tenure::Tick(rule, TERM, TERM, TICK, Corpse()).vanish);
    CHECK(tenure::Tick(rule, TICK, TERM, TICK, well).vanish);
}

TEST_CASE("tenure: a term that ends keeps nothing on the clock")
{
    tenure::Body const well;

    auto const said = tenure::Tick(TEMPSPAWN_TIMED_DESPAWN, 40, TERM, TICK, well);

    CHECK(said.vanish);
    CHECK(said.left == 40);
}

TEST_CASE("tenure: a granted term starts full and knows who granted it")
{
    Tenure held;

    CHECK_FALSE(held.Bounded());
    CHECK(held.Rule() == TEMPSPAWN_MANUAL_DESPAWN);
    CHECK(held.Left() == 0);

    held.SummonedBy(ObjectGuid(HIGHGUID_UNIT, uint32(42), uint32(7)));
    held.Grant(TEMPSPAWN_TIMED_DESPAWN, TERM);

    CHECK(held.Bounded());
    CHECK(held.Left() == TERM);
    CHECK(held.Granted() == TERM);
    CHECK(held.Summoner().GetCounter() == 7);
}

TEST_CASE("tenure: a term of nought is no term at all")
{
    Tenure held;
    held.Grant(TEMPSPAWN_TIMED_DESPAWN, 0);

    CHECK_FALSE(held.Bounded());
}

TEST_CASE("tenure: running the term down spends the clock it keeps")
{
    tenure::Body const well;

    Tenure held;
    held.Grant(TEMPSPAWN_TIMED_DESPAWN, TERM);

    CHECK_FALSE(held.RunsOut(TICK, well));
    CHECK(held.Left() == TERM - TICK);

    CHECK_FALSE(held.RunsOut(TICK, well));
    CHECK(held.Left() == TERM - 2 * TICK);
}

TEST_CASE("tenure: a term granted again starts over")
{
    tenure::Body const well;

    Tenure held;
    held.Grant(TEMPSPAWN_TIMED_DESPAWN, TERM);
    held.RunsOut(TICK, well);

    held.Grant(TEMPSPAWN_TIMED_OOC_DESPAWN, 500);

    CHECK(held.Rule() == TEMPSPAWN_TIMED_OOC_DESPAWN);
    CHECK(held.Left() == 500);
    CHECK(held.Granted() == 500);
}
