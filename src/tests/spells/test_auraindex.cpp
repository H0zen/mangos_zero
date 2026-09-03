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

// What replaces 192 linked lists per unit has to keep the one promise those
// lists made for free: a walk stays valid while auras come and go underneath
// it, because that is precisely what procs and effect handlers do.
//
// The index never dereferences an Aura, so the cases use distinct addresses
// rather than real auras.

#include "doctest.h"

#include "Object/AuraIndex.h"

#include <vector>
#include <cstdint>

namespace
{
    /// Distinct, never-dereferenced stand-ins for auras.
    Aura* Tag(uintptr_t n) { return reinterpret_cast<Aura*>(n * sizeof(void*)); }

    std::vector<Aura*> Walk(const auras::Index::Range& range)
    {
        std::vector<Aura*> seen;
        for (Aura* aura : range)
        {
            seen.push_back(aura);
        }
        return seen;
    }
}

TEST_CASE("an empty index yields nothing for any type")
{
    const auras::Index index;

    CHECK(index.Of(SPELL_AURA_MOD_DAMAGE_DONE).empty());
    CHECK(index.Of(SPELL_AURA_SCHOOL_ABSORB).size() == 0);
    CHECK(index.Total() == 0);
    CHECK(Walk(index.Of(SPELL_AURA_DUMMY)).empty());
}

TEST_CASE("auras come back grouped by their type")
{
    auras::Index index;

    index.Add(SPELL_AURA_SCHOOL_ABSORB, Tag(1));
    index.Add(SPELL_AURA_DUMMY, Tag(2));
    index.Add(SPELL_AURA_SCHOOL_ABSORB, Tag(3));

    CHECK(Walk(index.Of(SPELL_AURA_SCHOOL_ABSORB)) == std::vector<Aura*>{Tag(1), Tag(3)});
    CHECK(Walk(index.Of(SPELL_AURA_DUMMY)) == std::vector<Aura*>{Tag(2)});
    CHECK(index.Total() == 3);

    // A type nobody touched stays absent rather than becoming empty-but-present.
    CHECK(index.Of(SPELL_AURA_MOD_STUN).empty());
}

TEST_CASE("removal reports whether it found anything")
{
    auras::Index index;
    index.Add(SPELL_AURA_DUMMY, Tag(1));

    CHECK(index.Remove(SPELL_AURA_DUMMY, Tag(1)));
    CHECK_FALSE(index.Remove(SPELL_AURA_DUMMY, Tag(1)));
    CHECK_FALSE(index.Remove(SPELL_AURA_MOD_STUN, Tag(1)));
    CHECK(index.Total() == 0);
    CHECK(index.Of(SPELL_AURA_DUMMY).empty());
}

TEST_CASE("a removed aura leaves no trace in a later walk")
{
    auras::Index index;
    index.Add(SPELL_AURA_DUMMY, Tag(1));
    index.Add(SPELL_AURA_DUMMY, Tag(2));
    index.Add(SPELL_AURA_DUMMY, Tag(3));

    index.Remove(SPELL_AURA_DUMMY, Tag(2));

    CHECK(Walk(index.Of(SPELL_AURA_DUMMY)) == std::vector<Aura*>{Tag(1), Tag(3)});
    CHECK(index.Of(SPELL_AURA_DUMMY).size() == 2);
}

TEST_CASE("a hole is reused instead of growing the block")
{
    auras::Index index;
    index.Add(SPELL_AURA_DUMMY, Tag(1));
    index.Add(SPELL_AURA_DUMMY, Tag(2));
    index.Remove(SPELL_AURA_DUMMY, Tag(1));
    index.Add(SPELL_AURA_DUMMY, Tag(3));

    // Tag(3) took Tag(1)'s slot, so it comes first.
    CHECK(Walk(index.Of(SPELL_AURA_DUMMY)) == std::vector<Aura*>{Tag(3), Tag(2)});
    CHECK(index.Total() == 2);
}

TEST_CASE("removing the aura ahead of a walk keeps the walk valid")
{
    auras::Index index;
    for (uintptr_t i = 1; i <= 4; ++i)
    {
        index.Add(SPELL_AURA_DUMMY, Tag(i));
    }

    std::vector<Aura*> seen;
    for (Aura* aura : index.Of(SPELL_AURA_DUMMY))
    {
        seen.push_back(aura);
        if (aura == Tag(1))
        {
            index.Remove(SPELL_AURA_DUMMY, Tag(3));   // one the walk has not reached
        }
    }

    CHECK(seen == std::vector<Aura*>{Tag(1), Tag(2), Tag(4)});
}

TEST_CASE("removing the aura a walk is standing on keeps the walk valid")
{
    auras::Index index;
    for (uintptr_t i = 1; i <= 3; ++i)
    {
        index.Add(SPELL_AURA_DUMMY, Tag(i));
    }

    std::vector<Aura*> seen;
    for (Aura* aura : index.Of(SPELL_AURA_DUMMY))
    {
        seen.push_back(aura);
        index.Remove(SPELL_AURA_DUMMY, aura);
    }

    CHECK(seen == std::vector<Aura*>{Tag(1), Tag(2), Tag(3)});
    CHECK(index.Total() == 0);
}

TEST_CASE("removing an aura of another type does not disturb a walk")
{
    auras::Index index;
    index.Add(SPELL_AURA_DUMMY, Tag(1));
    index.Add(SPELL_AURA_DUMMY, Tag(2));
    for (uintptr_t i = 10; i < 40; ++i)
    {
        index.Add(SPELL_AURA_SCHOOL_ABSORB, Tag(i));
    }

    std::vector<Aura*> seen;
    for (Aura* aura : index.Of(SPELL_AURA_DUMMY))
    {
        seen.push_back(aura);
        index.Remove(SPELL_AURA_SCHOOL_ABSORB, Tag(10));
    }

    CHECK(seen == std::vector<Aura*>{Tag(1), Tag(2)});
}

TEST_CASE("a walk survives its own block growing past its capacity")
{
    auras::Index index;
    index.Add(SPELL_AURA_DUMMY, Tag(1));

    // Appending during the walk is what a proc applying an aura of the same
    // type does. The block reallocates; the walk must not be left behind, and
    // it sees what was appended ahead of it.
    std::vector<Aura*> seen;
    for (Aura* aura : index.Of(SPELL_AURA_DUMMY))
    {
        seen.push_back(aura);
        if (seen.size() < 40)
        {
            index.Add(SPELL_AURA_DUMMY, Tag(seen.size() + 1));
        }
    }

    CHECK(seen.size() == 40);
    CHECK(seen.front() == Tag(1));
    CHECK(seen.back() == Tag(40));
}

TEST_CASE("adding a new type during a walk does not disturb it")
{
    auras::Index index;
    index.Add(SPELL_AURA_MOD_STUN, Tag(1));
    index.Add(SPELL_AURA_MOD_STUN, Tag(2));

    std::vector<Aura*> seen;
    for (Aura* aura : index.Of(SPELL_AURA_MOD_STUN))
    {
        seen.push_back(aura);
        // Sorts before MOD_STUN, so it is inserted ahead of that block in the
        // list of blocks -- which must not move the block being walked.
        index.Add(SPELL_AURA_DUMMY, Tag(100));
    }

    CHECK(seen == std::vector<Aura*>{Tag(1), Tag(2)});
    CHECK(index.Of(SPELL_AURA_DUMMY).size() == 2);
}

TEST_CASE("clearing drops every type")
{
    auras::Index index;
    index.Add(SPELL_AURA_DUMMY, Tag(1));
    index.Add(SPELL_AURA_SCHOOL_ABSORB, Tag(2));

    index.Clear();

    CHECK(index.Total() == 0);
    CHECK(index.Of(SPELL_AURA_DUMMY).empty());
    CHECK(index.Of(SPELL_AURA_SCHOOL_ABSORB).empty());
}

TEST_CASE("front reads the first live aura, not the first slot")
{
    auras::Index index;
    index.Add(SPELL_AURA_DUMMY, Tag(1));
    index.Add(SPELL_AURA_DUMMY, Tag(2));
    index.Remove(SPELL_AURA_DUMMY, Tag(1));

    CHECK(index.Of(SPELL_AURA_DUMMY).front() == Tag(2));
}
