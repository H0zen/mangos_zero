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

// One outcome told three ways, and the ways agreeing.
//
// Each dialect is a mapping, so the cases are the mapping. What they guard is
// drift: a blocked hit that stops procing as blocked, or an absorbed one that
// stops playing the sound, are both a single missing line here.

#include "doctest.h"

#include "Combat/Flags.h"
#include "SpellMgr.h"
#include "Unit.h"

using combat::Landing;
using combat::Outcome;
using combat::ToHitInfo;
using combat::ToProcEx;
using combat::ToVictimState;

namespace
{
    Outcome Result(Landing landing, int32 dealt = 100)
    {
        Outcome o;
        o.landing = landing;
        o.dealt = dealt;
        return o;
    }
}

TEST_CASE("A plain hit is a normal swing that procs as a normal hit")
{
    const Outcome out = Result(Landing::Hit);

    CHECK((ToHitInfo(out) & HITINFO_MISS) == 0);
    CHECK((ToHitInfo(out) & HITINFO_CRITICALHIT) == 0);
    CHECK(ToVictimState(out) == VICTIMSTATE_NORMAL);
    CHECK((ToProcEx(out) & PROC_EX_NORMAL_HIT) != 0);
}

TEST_CASE("A crit says so in all three dialects")
{
    const Outcome out = Result(Landing::Crit);

    CHECK((ToHitInfo(out) & HITINFO_CRITICALHIT) != 0);
    CHECK(ToVictimState(out) == VICTIMSTATE_NORMAL);
    CHECK((ToProcEx(out) & PROC_EX_CRITICAL_HIT) != 0);
    CHECK((ToProcEx(out) & PROC_EX_NORMAL_HIT) == 0);
}

TEST_CASE("A miss leaves the victim unaffected")
{
    const Outcome out = Result(Landing::Miss, 0);

    CHECK((ToHitInfo(out) & HITINFO_MISS) != 0);
    CHECK(ToVictimState(out) == VICTIMSTATE_UNAFFECTED);
    CHECK((ToProcEx(out) & PROC_EX_MISS) != 0);
}

TEST_CASE("Dodge and parry are victim states, not hit-info bits")
{
    const Outcome dodged = Result(Landing::Dodge, 0);
    CHECK(ToVictimState(dodged) == VICTIMSTATE_DODGE);
    CHECK((ToProcEx(dodged) & PROC_EX_DODGE) != 0);

    const Outcome parried = Result(Landing::Parry, 0);
    CHECK(ToVictimState(parried) == VICTIMSTATE_PARRY);
    CHECK((ToProcEx(parried) & PROC_EX_PARRY) != 0);
}

TEST_CASE("A block is both a hit-info bit and a victim state")
{
    Outcome out = Result(Landing::Block, 70);
    out.blocked = 30;

    CHECK((ToHitInfo(out) & HITINFO_BLOCK) != 0);
    CHECK(ToVictimState(out) == VICTIMSTATE_BLOCKS);
    CHECK((ToProcEx(out) & PROC_EX_BLOCK) != 0);
}

TEST_CASE("Glancing and crushing change the damage, not the connection")
{
    const Outcome glanced = Result(Landing::Glance, 60);
    CHECK((ToHitInfo(glanced) & HITINFO_GLANCING) != 0);
    CHECK(ToVictimState(glanced) == VICTIMSTATE_NORMAL);
    CHECK((ToProcEx(glanced) & PROC_EX_NORMAL_HIT) != 0);

    const Outcome crushed = Result(Landing::Crush, 150);
    CHECK((ToHitInfo(crushed) & HITINFO_CRUSHING) != 0);
    CHECK(ToVictimState(crushed) == VICTIMSTATE_NORMAL);
    CHECK((ToProcEx(crushed) & PROC_EX_NORMAL_HIT) != 0);
}

TEST_CASE("Evade and immunity are their own victim states")
{
    const Outcome evaded = Result(Landing::Evade, 0);
    CHECK(ToVictimState(evaded) == VICTIMSTATE_EVADES);
    CHECK((ToProcEx(evaded) & PROC_EX_EVADE) != 0);

    const Outcome immune = Result(Landing::Immune, 0);
    CHECK(ToVictimState(immune) == VICTIMSTATE_IS_IMMUNE);
    CHECK((ToProcEx(immune) & PROC_EX_IMMUNE) != 0);
}

TEST_CASE("Absorption and resistance stack on top of how it landed")
{
    // They describe what happened to the damage, so they must survive alongside
    // a crit or a block rather than replacing it.
    Outcome out = Result(Landing::Crit, 40);
    out.absorbed = 60;
    out.resisted = 20;

    CHECK((ToHitInfo(out) & HITINFO_CRITICALHIT) != 0);
    CHECK((ToHitInfo(out) & HITINFO_ABSORB) != 0);
    CHECK((ToHitInfo(out) & HITINFO_RESIST) != 0);

    CHECK((ToProcEx(out) & PROC_EX_CRITICAL_HIT) != 0);
    CHECK((ToProcEx(out) & PROC_EX_ABSORB) != 0);
    CHECK((ToProcEx(out) & PROC_EX_RESIST) != 0);
}

TEST_CASE("Nothing absorbed or resisted sets neither bit")
{
    const Outcome out = Result(Landing::Hit);

    CHECK((ToHitInfo(out) & HITINFO_ABSORB) == 0);
    CHECK((ToHitInfo(out) & HITINFO_RESIST) == 0);
    CHECK((ToProcEx(out) & PROC_EX_ABSORB) == 0);
    CHECK((ToProcEx(out) & PROC_EX_RESIST) == 0);
}

TEST_CASE("A fully resisted blow reports resist once, not twice")
{
    Outcome out = Result(Landing::Resist, 0);
    out.resisted = 100;

    const uint32 procEx = ToProcEx(out);
    CHECK((procEx & PROC_EX_RESIST) != 0);
    CHECK((procEx & PROC_EX_NORMAL_HIT) == 0);
}

TEST_CASE("Every landing produces some proc flag")
{
    // A landing that mapped to nothing would silently stop every proc that
    // depends on it, and the symptom would be an ability that just does not
    // fire rather than an error.
    const Landing all[] = {
        Landing::Hit, Landing::Crit, Landing::Miss, Landing::Dodge,
        Landing::Parry, Landing::Block, Landing::Glance, Landing::Crush,
        Landing::Resist, Landing::Immune, Landing::Evade,
    };

    for (const Landing landing : all)
    {
        CHECK(ToProcEx(Result(landing)) != PROC_EX_NONE);
    }
}
