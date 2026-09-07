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

#include "Geometry/Placement.h"
#include "Platform/Define.h"

/**
 * @brief Whether a place named by a row, a command or a packet is a place at all.
 *
 * These answer from the data stores alone -- Map.dbc, the instance templates, the terrain
 * tiles on disk -- and never from what happens to be open. A coordinate is checked long
 * before any map exists for it: a spawn table is read at start-up, a teleport command is
 * typed at a console, a movement packet arrives naming a map nobody is on.
 */
namespace MapCoords
{
    /// Is this a map the server can open? A dungeon also needs its instance template.
    bool Known(uint32 mapId);

    bool Valid(uint32 mapId, float x, float y);
    bool Valid(uint32 mapId, float x, float y, float z);
    bool Valid(uint32 mapId, float x, float y, float z, float o);
    bool Valid(Geometry::Placement const& where);

    /// Is there terrain on disk under this spot? Asked of the start-up sanity check.
    bool TileExists(uint32 mapId, float x, float y);
}
