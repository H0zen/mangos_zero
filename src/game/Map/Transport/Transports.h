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

#include "GameObject.h"
#include "VesselRoute.h"

#include <map>
#include <set>
#include <string>

class Map;
class TransportMap;

class Transport : public GameObject
{
    public:
        explicit Transport();

        bool Create(uint32 guidlow, uint32 mapid, float x, float y, float z, float ang, uint8 animprogress);
        void Update(uint32 update_diff, uint32 p_time) override;

        bool IsCrossing() const { return m_crossing; }

        void CompleteCrossing();

        void WithdrawFromWorld();

        TransportMap* AsMap() const { return m_map; }

        static void RegisterVesselMap(uint32 goEntry, char const* vesselName);
        static uint32 VesselMapIdOf(uint32 goEntry);

        static bool IsVesselMapId(uint32 mapId);

        uint32 VesselMapId() const { return VesselMapIdOf(GetEntry()); }

        void PinRouteGrids();

        static Transport* VesselOf(Occupant const& obj);

        static Transport* GetTransport(Map const* map, ObjectGuid guid);

        uint32 GetPathProgress() const { return m_timer; }

    private:
        uint32 m_timer;

        bool m_withdrawn = false;

        uint32 m_crossingTo = 0;
        float m_crossingX = 0.0f;
        float m_crossingY = 0.0f;
        float m_crossingZ = 0.0f;
        bool m_crossing = false;

        TransportMap* m_map = nullptr;

    public:
        uint32 m_period;

        VesselRoute m_route;

    private:
        void TeleportTransport(uint32 newMapid, float x, float y, float z);
};
