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

#include "ObjectKind.h"

uint16 TypeMaskFor(uint8 typeId)
{
    static uint16 const masks[MAX_TYPE_ID] =
    {
        TYPEMASK_OBJECT,
        TYPEMASK_OBJECT | TYPEMASK_ITEM,
        TYPEMASK_OBJECT | TYPEMASK_ITEM | TYPEMASK_CONTAINER,
        TYPEMASK_OBJECT | TYPEMASK_UNIT,
        TYPEMASK_OBJECT | TYPEMASK_UNIT | TYPEMASK_PLAYER,
        TYPEMASK_OBJECT | TYPEMASK_GAMEOBJECT,
        TYPEMASK_OBJECT | TYPEMASK_DYNAMICOBJECT,
        TYPEMASK_OBJECT | TYPEMASK_CORPSE,
    };

    MANGOS_ASSERT(typeId < MAX_TYPE_ID);
    return masks[typeId];
}
