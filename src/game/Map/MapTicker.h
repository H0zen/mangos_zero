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

/**
 * @brief The clock that gives every open map its turn, and the barrier at the end of it.
 *
 * One round is: every world map updated, in parallel when there are worker threads; then
 * the barrier, where nothing is running and the vessels between two maps are handed over;
 * then the maps that nobody needs any more are retired.
 *
 * A vessel's deck is never in the round. It belongs to the vessel, which ticks it nested
 * inside the tick of the map she sails, and it is never retired: no player enters it to
 * keep it awake and her crew have nowhere else to be.
 */
class MapTicker : public MaNGOS::Singleton<MapTicker>
{
        friend class MaNGOS::Singleton<MapTicker>;

    public:

        /// Put the worker threads to work. Zero threads means every map ticks on this one.
        void Start(uint32 threads);

        /// Drain what is queued, then stop and join the workers.
        void Halt();

        /// How long a round waits before the next one. Capped, so a map cannot go stale.
        void SetInterval(uint32 ms);

        /// One round, if the interval has come round.
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
