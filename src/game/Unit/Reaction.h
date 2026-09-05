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

#pragma once

#include "Platform/Define.h"

class Corpse;
class DynamicObject;
class GameObject;
class Object;
class Player;
class Unit;
struct FactionTemplateEntry;

/// What one side makes of another.
enum class Reaction
{
    Hostile,        ///< settled: they are enemies
    Friendly,       ///< settled: they are friends
    Neither,        ///< settled: neither, and the faction templates do not get a say
    NoOpinion,      ///< unsettled: the player holds no view, so the templates decide
};

/**
 * What a player makes of a faction, from their own standing with it.
 *
 * A forced reaction settles the question outright. Failing that, the answer
 * comes from one of two places, and which one depends on who is asking about
 * whom: a player judging a creature's faction goes by whether they have
 * declared war on it, while a creature or gameobject judging a player goes by
 * the reputation rank that player has earned. The two are not the same question
 * and do not always agree, so `byWarState` picks between them.
 *
 * `NoOpinion` means the faction is not one the player can hold a standing with;
 * the caller falls back to comparing the faction templates.
 */
Reaction OpinionOf(Player const& player, FactionTemplateEntry const* faction, bool byWarState);

/**
 * What two factions have declared about each other in the game's own data.
 *
 * This is the standing between the sides themselves, not between two particular
 * creatures: nothing that has happened in the world is taken into account. A
 * template can name another faction as both friend and enemy, or belong to
 * groups named on both sides. Enmity is read first, because a caller who may not
 * attack is worse served by a wrong yes than a caller who may not heal is by a
 * wrong no.
 */
Reaction AsFactionsDeclare(FactionTemplateEntry const& who, FactionTemplateEntry const& whom);

/**
 * What one thing in the world makes of a unit.
 *
 * The relation belongs to neither of the two, so it is asked of the pair rather
 * than of one of them. Three answers are possible and all three occur: two
 * players of opposing sides, only one of whom is flagged for war, are neither
 * friends nor enemies, and the client draws that name yellow.
 *
 * Ownership is followed first: a pet answers for its master, and a gameobject,
 * a corpse and a spell's lingering effect answer for whoever put them there.
 */
Reaction ReactionOf(Unit const& who, Unit const& whom);
Reaction ReactionOf(GameObject const& who, Unit const& whom);
Reaction ReactionOf(Corpse const& who, Unit const& whom);
Reaction ReactionOf(DynamicObject const& who, Unit const& whom);

/// For a caller holding something it has not identified.
Reaction ReactionOf(Object const& who, Unit const& whom);

template <typename T>
inline bool IsHostile(T const& who, Unit const& whom)
{
    return ReactionOf(who, whom) == Reaction::Hostile;
}

template <typename T>
inline bool IsFriendly(T const& who, Unit const& whom)
{
    return ReactionOf(who, whom) == Reaction::Friendly;
}

/**
 * What a unit's own faction says about everyone else, with no second party.
 *
 * A faction a player can hold a standing with is never either of these: the
 * standing decides instead, so the template is not consulted.
 */
bool HostileToPlayers(Unit const& who);
bool NeutralToAll(Unit const& who);
