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

class Group;
class Object;
class WorldObject;

/**
 * Handing out quests, and rolling for loot.
 *
 * Neither is a property of "an object". Both used to be virtual on the root of
 * the hierarchy, answering false or doing nothing for the five kinds that have
 * no business with them, so that the two or three kinds that do could override.
 * A base class that declares what its leaves might one day want is the same
 * inversion the field serializer had, and it has the same cure: ask here, and
 * let this decide which kind it is talking to.
 */

/// Does this object offer the quest -- a questgiver, a gameobject, or an item
/// whose use starts it.
bool StartsQuest(Object const& object, uint32 questId);

/// Does this object take the quest back.
bool EndsQuest(Object const& object, uint32 questId);

/// Begin the group's roll on what this object is holding, and end it.
void StartGroupLoot(WorldObject& holder, Group* group, uint32 timer);
void StopGroupLoot(WorldObject& holder);
