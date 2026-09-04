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

// A school, and a set of schools.
//
// The pair exists because a blow has exactly one kind while a shield covers
// several, and the cases below are that asymmetry: what a blow carries cannot be
// empty or plural, and what a defence covers is asked only about membership.

#include "doctest.h"

#include "Combat/School.h"

using combat::FirstSchoolIn;
using combat::IsPhysical;
using combat::School;
using combat::SchoolFromIndex;
using combat::SchoolSet;
using combat::WireIndex;

TEST_CASE("The wire index of a school round-trips")
{
    const School all[] = {
        School::Physical, School::Holy, School::Fire, School::Nature,
        School::Frost, School::Shadow, School::Arcane,
    };

    for (const School school : all)
    {
        CHECK(SchoolFromIndex(WireIndex(school)) == school);
    }

    CHECK(WireIndex(School::Physical) == 0);
    CHECK(WireIndex(School::Arcane) == 6);
}

TEST_CASE("An index past the last school reads as physical")
{
    // Absent and malformed have always meant physical, and a school that came
    // back out of range would otherwise index past the resistance table.
    CHECK(SchoolFromIndex(7) == School::Physical);
    CHECK(SchoolFromIndex(0xFFFFFFFFu) == School::Physical);
}

TEST_CASE("Only physical goes through armour")
{
    CHECK(IsPhysical(School::Physical));
    CHECK_FALSE(IsPhysical(School::Holy));
    CHECK_FALSE(IsPhysical(School::Shadow));
}

TEST_CASE("A set built from one school contains that one and no other")
{
    const SchoolSet frost(School::Frost);

    CHECK(frost.Contains(School::Frost));
    CHECK_FALSE(frost.Contains(School::Fire));
    CHECK_FALSE(frost.Contains(School::Physical));
    CHECK_FALSE(frost.Empty());
}

TEST_CASE("An empty set covers nothing and a full set covers everything")
{
    CHECK(SchoolSet::None().Empty());
    CHECK_FALSE(SchoolSet::None().Contains(School::Fire));

    CHECK(SchoolSet::All().Contains(School::Physical));
    CHECK(SchoolSet::All().Contains(School::Arcane));
    CHECK(SchoolSet::All().ToMask() == 0x7F);
}

TEST_CASE("A set round-trips through the mask spell data stores")
{
    SchoolSet set;
    set.Add(School::Fire).Add(School::Shadow);

    const SchoolSet again = SchoolSet::FromMask(set.ToMask());

    CHECK(again == set);
    CHECK(again.Contains(School::Fire));
    CHECK(again.Contains(School::Shadow));
    CHECK_FALSE(again.Contains(School::Frost));
}

TEST_CASE("Bits above the seven schools are dropped when a mask is read")
{
    const SchoolSet set = SchoolSet::FromMask(0xFFFFFFFFu);

    CHECK(set == SchoolSet::All());
    CHECK(set.ToMask() == 0x7F);
}

TEST_CASE("Removing a school leaves the rest of the set alone")
{
    SchoolSet set = SchoolSet::All();
    set.Remove(School::Holy);

    CHECK_FALSE(set.Contains(School::Holy));
    CHECK(set.Contains(School::Physical));
    CHECK(set.Contains(School::Arcane));
}

TEST_CASE("Narrowing a mask to one school takes the lowest bit")
{
    // Spell data stores a mask where one school is meant. The narrowing happens
    // in one place so the loss is visible, and it picks the same school the
    // client's own table would.
    CHECK(FirstSchoolIn(0x01) == School::Physical);
    CHECK(FirstSchoolIn(0x04) == School::Fire);
    CHECK(FirstSchoolIn(0x20) == School::Shadow);

    // Fire and frost together: the lower bit wins.
    CHECK(FirstSchoolIn(0x14) == School::Fire);

    // Nothing set at all is physical, not an invalid school.
    CHECK(FirstSchoolIn(0) == School::Physical);
}
