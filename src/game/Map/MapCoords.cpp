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

#include "MapCoords.h"

#include "DBCStores.h"
#include "GridDefines.h"
#include "GridMap.h"
#include "ObjectMgr.h"

namespace MapCoords
{
    bool Known(uint32 mapId)
    {
        MapEntry const* entry = sMapStore.LookupEntry(mapId);

        // A dungeon row with no instance template is a map that cannot be entered: the
        // template carries the party size, the reset rule and the ghost entrance.
        return entry && (!entry->IsDungeon() || ObjectMgr::GetInstanceTemplate(mapId));
    }

    bool Valid(uint32 mapId, float x, float y)
    {
        return Known(mapId) && MaNGOS::IsValidMapCoord(x, y);
    }

    bool Valid(uint32 mapId, float x, float y, float z)
    {
        return Known(mapId) && MaNGOS::IsValidMapCoord(x, y, z);
    }

    bool Valid(uint32 mapId, float x, float y, float z, float o)
    {
        return Known(mapId) && MaNGOS::IsValidMapCoord(x, y, z, o);
    }

    bool Valid(Geometry::Placement const& where)
    {
        return Valid(where.MapId(), where.X(), where.Y(), where.Z(), where.Facing());
    }

    bool TileExists(uint32 mapId, float x, float y)
    {
        const GridPair grid = MaNGOS::ComputeGridPair(x, y);

        return TerrainInfo::ExistTile(mapId, 63 - grid.x_coord, 63 - grid.y_coord);
    }
}
