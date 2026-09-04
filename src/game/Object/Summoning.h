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
#include "Presence.h"

class Creature;
class GameObject;
class Presence;

/**
 * Putting a new thing on a map.
 *
 * The summoner is an input in three roles -- it names the owner, it supplies
 * the map, and it is the anchor when no position is given -- and a creature is
 * what comes out. Two of the steps depend on what kind of thing is summoning:
 * a player lends its team, and a creature's AI is told what it made.
 *
 * @param summoner  Who is summoning, and where from.
 * @param id        Creature template, or gameobject entry.
 * @param x,y,z,ang Where. All zero means "in front of the summoner".
 * @param spwtype   When the summon despawns.
 * @param despwtime Despawn delay, in milliseconds.
 * @param asActiveObject Keep the summon updating with no players nearby.
 * @param setRun    Move at a run rather than a walk.
 */
Creature* SummonCreature(Presence& summoner, uint32 id, float x, float y, float z, float ang,
                         TempSpawnType spwtype, uint32 despwtime,
                         bool asActiveObject = false, bool setRun = false);

GameObject* SummonGameObject(Presence& summoner, uint32 id,
                             float x, float y, float z, float angle, uint32 despwtime);
