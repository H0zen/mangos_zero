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
#include "Geometry/Placement.h"

class Hearth
{
    public:

        uint32 MapId() const { return m_mapId; }
        uint16 AreaId() const { return m_areaId; }

        float X() const { return m_at.x; }
        float Y() const { return m_at.y; }
        float Z() const { return m_at.z; }

        Geometry::Placement Where() const
        {
            return Geometry::Placement::Somewhere(m_mapId, m_at, 0.0f);
        }

        void SetTo(uint32 mapId, uint16 areaId, float x, float y, float z)
        {
            m_mapId = mapId;
            m_areaId = areaId;
            m_at = Geometry::Vector3(x, y, z);
        }

        uint32 Countdown() const { return m_countdown; }
        void Countdown(uint32 left) { m_countdown = left; }

    private:

        uint32 m_mapId = 0;
        uint16 m_areaId = 0;
        Geometry::Vector3 m_at;
        uint32 m_countdown = 0;
};
