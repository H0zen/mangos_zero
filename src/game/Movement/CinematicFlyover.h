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
 */

#pragma once

#include "CinematicFlyoverRoute.h"
#include "ObjectGuid.h"
#include <memory>

class Player;
class Creature;
class Map;

class CinematicFlyover
{
public:
    CinematicFlyover(Player* player, uint8 raceId);
    ~CinematicFlyover();

    void Begin();

    void Update(uint32 updateDiff);

    void Stop();

    bool IsActive() const { return m_active; }

private:

    Creature* ResolveBody() const;

    bool InterpolatePosition(uint32 atMs, float& x, float& y, float& z, float& o);

    Player* m_player;
    const CinematicFlyoverRoute* m_route;
    Map* m_viewerMap;
    float m_viewerRadius;
    ObjectGuid m_bodyGuid = 0;
    uint32 m_bodyEntry;
    uint32 m_elapsedMs;
    uint32 m_updateTimer;
    uint32 m_timeoutMs;
    bool m_armed;
    bool m_begun;
    bool m_active;
};
