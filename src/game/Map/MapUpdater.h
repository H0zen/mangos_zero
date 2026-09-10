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

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

class Map;

class MapUpdater
{
    public:

        MapUpdater();
        ~MapUpdater();

        MapUpdater(const MapUpdater&) = delete;
        MapUpdater& operator=(const MapUpdater&) = delete;

        int schedule_update(Map& map, uint32 diff);

        int wait();

        int activate(size_t num_threads);

        int deactivate();

        bool activated();

    private:

        typedef std::pair<Map*, uint32> Task;

        void workerLoop();

        std::vector<std::thread> m_workers;
        std::queue<Task>         m_tasks;

        std::mutex              m_mutex;
        std::condition_variable m_taskAdded;
        std::condition_variable m_taskDone;

        size_t m_pending;
        bool   m_stop;
};
