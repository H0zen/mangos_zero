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

#include "MapUpdater.h"
#include "Policies/Singleton.h"
#include "Utilities/Timer.h"

class MapTicker : public MaNGOS::Singleton<MapTicker>
{
        friend class MaNGOS::Singleton<MapTicker>;

    public:

        void Start(uint32 threads);

        void Halt();

        void SetInterval(uint32 ms);

        void Run(uint32 diff);

    private:

        MapTicker();
        ~MapTicker() = default;

        MapTicker(MapTicker const&) = delete;
        MapTicker& operator=(MapTicker const&) = delete;

        IntervalTimer m_timer;
        MapUpdater m_pool;
};

#define sMapTicker MaNGOS::Singleton<MapTicker>::Instance()
