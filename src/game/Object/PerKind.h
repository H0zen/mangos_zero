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

#include "LootClaim.h"
#include "Platform/Define.h"

class Group;
class Object;
class Occupant;

/**
 * Questions only some kinds of object answer.
 *
 * Handing out a quest, rolling for loot, having a respawn time: a creature and
 * a gameobject do these, an item starts a quest, and nothing else has anything
 * to say. Ask here and let this decide which kind it is talking to, so that the
 * roots of the hierarchy declare nothing on behalf of two of their leaves.
 */

/// Does this object offer the quest -- a questgiver, a gameobject, or an item
/// whose use starts it.
bool StartsQuest(Object const& object, uint32 questId);

/// Does this object take the quest back.
bool EndsQuest(Object const& object, uint32 questId);

/// The claim on what this object is holding, or nothing when it holds nothing
/// anyone can take. Only a corpse and a chest ever do.
LootClaim* ClaimOn(Occupant& holder);

/// Persist when this object comes back, if it comes back at all.
void SaveRespawnTime(Occupant& what);
