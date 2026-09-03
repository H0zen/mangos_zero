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

// Carrying out what a resolution decided.
//
// The plan says which shields gave way and by how much, and what it cost in
// mana. This is where that is taken. It is deliberately the only writer: as long
// as deciding and taking stay apart, a resolution can be read, logged and tested
// without anything in the world having moved yet.

#include "Combat/Attempt.h"

class Unit;

namespace combat
{
    /**
      * Step one of applying a blow: the shields give way.
      *
      * Absorbing auras come down by what they stopped, the exhausted ones are
      * removed, and any mana their absorption costs is drawn from the bearer.
      *
      * Not named for spending. Spending is a choice an actor makes in order to
      * act, and belongs to the caster paying for a cast; a shield is consumed by
      * what struck it.
      */
    void ConsumeShields(Unit& victim, const Outcome& outcome);
}
