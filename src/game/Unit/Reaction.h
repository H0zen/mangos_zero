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

enum class Reaction
{
    Hostile,
    Friendly,
    Neither,
    NoOpinion,
};

Reaction OpinionOf(Player const& player, FactionTemplateEntry const* faction, bool byWarState);

Reaction AsFactionsDeclare(FactionTemplateEntry const& who, FactionTemplateEntry const& whom);

Reaction ReactionOf(Unit const& who, Unit const& whom);
Reaction ReactionOf(GameObject const& who, Unit const& whom);
Reaction ReactionOf(Corpse const& who, Unit const& whom);
Reaction ReactionOf(DynamicObject const& who, Unit const& whom);

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

bool HostileToPlayers(Unit const& who);
bool NeutralToAll(Unit const& who);
