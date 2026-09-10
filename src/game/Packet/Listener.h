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

#include "IWorldGateway.h"

#include "net/Server.hpp"

#include <cstdint>
#include <string>

namespace proto
{

    class Listener
    {
        public:

            explicit Listener(IWorldGateway& gateway);
            ~Listener();

            Listener(const Listener&) = delete;
            Listener& operator=(const Listener&) = delete;

            bool Start(uint16_t port, const std::string& bindIp = std::string());

            void Stop();

        private:

            IWorldGateway& m_gateway;
            net::Server    m_server;
            bool           m_running;
    };
}
