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

// What two faction templates make of each other.
//
// This is the floor of the whole question: once nobody is anybody's pet, nobody
// is swinging at anybody, and no player standing has a view, the templates are
// what is left. Three answers come out of it, and the third one is the point --
// a pair that names neither side is not a pair of friends.

#include "doctest.h"

#include "Reaction.h"
#include "DBCStructure.h"

namespace
{
    /// A template that names nobody and belongs to no group.
    FactionTemplateEntry Blank(uint32 faction)
    {
        FactionTemplateEntry entry{};
        entry.ID = faction;
        entry.Faction = faction;
        return entry;
    }

    uint32 const kAlliance = 2;
    uint32 const kHorde = 4;
}

TEST_CASE("faction templates: naming a faction outright settles it either way")
{
    FactionTemplateEntry stormwind = Blank(72);
    FactionTemplateEntry orgrimmar = Blank(76);

    // Nothing is named yet, and no groups overlap.
    CHECK(AsFactionsDeclare(stormwind, orgrimmar) == Reaction::Neither);

    stormwind.Enemies[0] = orgrimmar.Faction;
    CHECK(AsFactionsDeclare(stormwind, orgrimmar) == Reaction::Hostile);

    // Enmity is read before amity, so naming the same faction on both lists
    // still comes out hostile.
    stormwind.Friend[0] = orgrimmar.Faction;
    CHECK(AsFactionsDeclare(stormwind, orgrimmar) == Reaction::Hostile);

    stormwind.Enemies[0] = 0;
    CHECK(AsFactionsDeclare(stormwind, orgrimmar) == Reaction::Friendly);
}

TEST_CASE("faction templates: groups answer when no faction is named")
{
    FactionTemplateEntry guard = Blank(11);
    FactionTemplateEntry traveller = Blank(12);

    guard.FactionGroup = kAlliance;
    traveller.FactionGroup = kHorde;

    CHECK(AsFactionsDeclare(guard, traveller) == Reaction::Neither);

    guard.EnemyGroup = kHorde;
    CHECK(AsFactionsDeclare(guard, traveller) == Reaction::Hostile);

    guard.EnemyGroup = 0;
    guard.FriendGroup = kHorde;
    CHECK(AsFactionsDeclare(guard, traveller) == Reaction::Friendly);

    // Amity read from the other side counts the same.
    guard.FriendGroup = 0;
    traveller.FriendGroup = kAlliance;
    CHECK(AsFactionsDeclare(guard, traveller) == Reaction::Friendly);

    // A row that says both is malformed, and enmity wins so that a caller
    // asking whether it may strike is never wrongly told yes.
    guard.EnemyGroup = kHorde;
    CHECK(AsFactionsDeclare(guard, traveller) == Reaction::Hostile);
}

TEST_CASE("faction templates: a named faction outranks the groups")
{
    FactionTemplateEntry beast = Blank(14);
    FactionTemplateEntry player = Blank(1);

    beast.FactionGroup = kAlliance;
    beast.EnemyGroup = kHorde;
    player.FactionGroup = kHorde;
    CHECK(AsFactionsDeclare(beast, player) == Reaction::Hostile);

    // Naming it a friend settles the question before the groups are reached.
    beast.Friend[2] = player.Faction;
    CHECK(AsFactionsDeclare(beast, player) == Reaction::Friendly);

    // A template with no faction of its own cannot be named, so the groups
    // decide even when the lists hold entries.
    FactionTemplateEntry nameless = Blank(0);
    nameless.FactionGroup = kHorde;
    CHECK(AsFactionsDeclare(beast, nameless) == Reaction::Hostile);
}

TEST_CASE("faction templates: a side is friendly to itself only if it says so")
{
    FactionTemplateEntry mine = Blank(21);
    mine.FactionGroup = kAlliance;

    CHECK(AsFactionsDeclare(mine, mine) == Reaction::Neither);

    mine.FriendGroup = kAlliance;
    CHECK(AsFactionsDeclare(mine, mine) == Reaction::Friendly);
}
