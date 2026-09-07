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

#include "SpoilsHolder.h"

#include "Corpse.h"
#include "Creature.h"
#include "GameObject.h"
#include "Item.h"
#include "Map.h"
#include "Player.h"

Object* spoils::Holder(Player& who, ObjectGuid guid)
{
    switch (guid.GetHigh())
    {
        case HIGHGUID_UNIT:
            return who.GetMap()->GetCreature(guid);

        case HIGHGUID_GAMEOBJECT:
            return who.GetMap()->GetGameObject(guid);

        case HIGHGUID_CORPSE:
            return who.GetMap()->GetCorpse(guid);

        // Only ever one of his own: a lockbox is looted out of the bags it sits in.
        case HIGHGUID_ITEM:
            return who.GetItemByGuid(guid);

        default:
            return nullptr;
    }
}

Loot* spoils::OpenedBy(Player& who, ObjectGuid guid)
{
    Object* holder = Holder(who, guid);

    return holder ? holder->SpoilsFor(who) : nullptr;
}
