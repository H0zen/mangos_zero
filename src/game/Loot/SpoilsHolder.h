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

#include "ObjectGuid.h"

class Object;
class Player;
struct Loot;

namespace spoils
{
    /**
     * @brief The thing a loot guid names.
     *
     * A body on his map, a chest, a set of bones, or a lockbox in his own bags. Nothing
     * else can hold a pile, and a guid naming anything else resolves to nothing.
     *
     * This is where the client's guid becomes an object, and the only place that has to
     * know which stores to look in. What may then be done with it is the object's own
     * answer -- Object::Spoils and Object::OpenableBy.
     */
    Object* Holder(Player& who, ObjectGuid guid);

    /// The pile that guid names, if this player may take from it right now.
    Loot* OpenedBy(Player& who, ObjectGuid guid);
}
