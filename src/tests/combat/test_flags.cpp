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

using combat::Outcome;
using combat::Result;
using combat::Strike;
using combat::ToHitInfo;
using combat::ToProcEx;
using combat::ToVictimState;

namespace
{
    /// A blow that ended without landing.
    Outcome Ended(Result result)
    {
        Outcome o;
        o.strike.result = result;
        return o;
    }

    /// A blow that landed, with whichever amplifiers the case is about.
    Outcome Landed(int32 dealt = 100, bool crit = false, bool glancing = false,
                   bool crushing = false, bool blocked = false)
    {
        Outcome o;
        o.strike.result = Result::Landed;
        o.strike.crit = crit;
        o.strike.glancing = glancing;
        o.strike.crushing = crushing;
        o.strike.blocked = blocked;
        o.dealt = dealt;
        return o;
    }
}

TEST_CASE("A plain hit is a normal swing that procs as a normal hit")
{
    const Outcome out = Landed();

    CHECK((ToHitInfo(out) & HITINFO_MISS) == 0);
    CHECK((ToHitInfo(out) & HITINFO_CRITICALHIT) == 0);
    CHECK(ToVictimState(out) == VICTIMSTATE_NORMAL);
    CHECK((ToProcEx(out) & PROC_EX_NORMAL_HIT) != 0);
}

TEST_CASE("A critical hit is not a normal hit to a proc")
{
    const Outcome out = Landed(200, /*crit*/ true);

    CHECK((ToHitInfo(out) & HITINFO_CRITICALHIT) != 0);
    CHECK((ToProcEx(out) & PROC_EX_CRITICAL_HIT) != 0);
    CHECK((ToProcEx(out) & PROC_EX_NORMAL_HIT) == 0);
}

TEST_CASE("A miss is unaffected, not normal")
{
    const Outcome out = Ended(Result::Missed);

    CHECK((ToHitInfo(out) & HITINFO_MISS) != 0);
    CHECK(ToVictimState(out) == VICTIMSTATE_UNAFFECTED);
    CHECK((ToProcEx(out) & PROC_EX_MISS) != 0);
}

TEST_CASE("Dodge and parry are their own victim states")
{
    const Outcome dodged = Ended(Result::Dodged);
    CHECK(ToVictimState(dodged) == VICTIMSTATE_DODGE);
    CHECK((ToProcEx(dodged) & PROC_EX_DODGE) != 0);

    const Outcome parried = Ended(Result::Parried);
    CHECK(ToVictimState(parried) == VICTIMSTATE_PARRY);
    CHECK((ToProcEx(parried) & PROC_EX_PARRY) != 0);
}

TEST_CASE("A partial block still lands, and still says blocked in all three")
{
    // This is the case the whole file exists for: the blow connected, so it
    // must proc as a hit, and the shield took part of it, so it must also proc
    // as blocked. Folding the two into one value is what loses the second.
    Outcome out = Landed(70, false, false, false, /*blocked*/ true);
    out.blocked = 30;

    CHECK((ToHitInfo(out) & HITINFO_BLOCK) != 0);
    CHECK(ToVictimState(out) == VICTIMSTATE_BLOCKS);
    CHECK((ToProcEx(out) & PROC_EX_BLOCK) != 0);
    CHECK((ToProcEx(out) & PROC_EX_NORMAL_HIT) != 0);
}

TEST_CASE("A block that stopped everything is a block and nothing else")
{
    Outcome out = Ended(Result::Blocked);
    out.blocked = 100;

    CHECK((ToHitInfo(out) & HITINFO_BLOCK) != 0);
    CHECK(ToVictimState(out) == VICTIMSTATE_BLOCKS);
    CHECK((ToProcEx(out) & PROC_EX_BLOCK) != 0);
    CHECK((ToProcEx(out) & PROC_EX_NORMAL_HIT) == 0);
}

TEST_CASE("Glancing and crushing change the damage, not the connection")
{
    const Outcome glanced = Landed(60, false, /*glancing*/ true);
    CHECK((ToHitInfo(glanced) & HITINFO_GLANCING) != 0);
    CHECK(ToVictimState(glanced) == VICTIMSTATE_NORMAL);
    CHECK((ToProcEx(glanced) & PROC_EX_NORMAL_HIT) != 0);

    const Outcome crushed = Landed(150, false, false, /*crushing*/ true);
    CHECK((ToHitInfo(crushed) & HITINFO_CRUSHING) != 0);
    CHECK(ToVictimState(crushed) == VICTIMSTATE_NORMAL);
    CHECK((ToProcEx(crushed) & PROC_EX_NORMAL_HIT) != 0);
}

TEST_CASE("Evade and immunity are their own victim states")
{
    const Outcome evaded = Ended(Result::Evaded);
    CHECK(ToVictimState(evaded) == VICTIMSTATE_EVADES);
    CHECK((ToProcEx(evaded) & PROC_EX_EVADE) != 0);

    const Outcome immune = Ended(Result::Immune);
    CHECK(ToVictimState(immune) == VICTIMSTATE_IS_IMMUNE);
    CHECK((ToProcEx(immune) & PROC_EX_IMMUNE) != 0);
}

TEST_CASE("Deflect and reflect carry their own state and proc flag")
{
    // The client holds a word for each of these, so a blow can end that way and
    // the mapping has to say which one rather than falling through to normal.
    const Outcome deflected = Ended(Result::Deflected);
    CHECK(ToVictimState(deflected) == VICTIMSTATE_DEFLECTS);
    CHECK((ToProcEx(deflected) & PROC_EX_DEFLECT) != 0);

    const Outcome reflected = Ended(Result::Reflected);
    CHECK((ToProcEx(reflected) & PROC_EX_REFLECT) != 0);
}

TEST_CASE("Absorption and resistance stack on top of how it landed")
{
    // They describe what happened to the damage, so they must survive alongside
    // a crit or a block rather than replacing it.
    Outcome out = Landed(40, /*crit*/ true);
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
    const Outcome out = Landed();

    CHECK((ToHitInfo(out) & HITINFO_ABSORB) == 0);
    CHECK((ToHitInfo(out) & HITINFO_RESIST) == 0);
    CHECK((ToProcEx(out) & PROC_EX_ABSORB) == 0);
    CHECK((ToProcEx(out) & PROC_EX_RESIST) == 0);
}

TEST_CASE("A fully resisted blow reports resist once, not twice")
{
    Outcome out = Ended(Result::Resisted);
    out.resisted = 100;

    const uint32 procEx = ToProcEx(out);
    CHECK((procEx & PROC_EX_RESIST) != 0);
    CHECK((procEx & PROC_EX_NORMAL_HIT) == 0);
}

TEST_CASE("A fully absorbed blow reports absorb once, not twice")
{
    Outcome out = Ended(Result::Absorbed);
    out.absorbed = 100;

    const uint32 procEx = ToProcEx(out);
    CHECK((procEx & PROC_EX_ABSORB) != 0);
    CHECK((procEx & PROC_EX_NORMAL_HIT) == 0);
}

TEST_CASE("Every ending produces some proc flag")
{
    // An ending that mapped to nothing would silently stop every proc that
    // depends on it, and the symptom would be an ability that just does not
    // fire rather than an error.
    const Result all[] = {
        Result::Landed, Result::Missed, Result::Dodged, Result::Parried,
        Result::Blocked, Result::Evaded, Result::Immune, Result::Deflected,
        Result::Absorbed, Result::Resisted, Result::Reflected,
    };

    for (const Result result : all)
    {
        CHECK(ToProcEx(Ended(result)) != PROC_EX_NONE);
    }
}
