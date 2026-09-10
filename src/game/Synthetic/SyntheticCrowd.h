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
#include "ObjectGuid.h"

#include <string>
#include <vector>

class Player;
class WorldSession;

namespace synthetic
{
    class SyntheticLink;

    struct CrowdReport
    {
        uint32 bots = 0;
        uint64 bytes = 0;
        uint64 packets = 0;
        uint32 bytesPerSecPeak = 0;
        uint32 bytesPerSecMean = 0;
    };

    class SyntheticCrowd
    {
        public:

            static SyntheticCrowd& Instance();

            uint32 Spawn(uint32 count, uint32 mapId, float x, float y, float z,
                         float radius, std::string& error);

            uint32 Despawn();

            void Drive(uint32 diff);

            CrowdReport Report(uint32 elapsedMs);

            uint32 Size() const { return static_cast<uint32>(m_bots.size()); }
            bool Empty() const { return m_bots.empty(); }

        private:

            SyntheticCrowd() = default;

            struct Bot
            {
                WorldSession* session = nullptr;
                SyntheticLink* link = nullptr;
                ObjectGuid guid = 0;
                float homeX = 0.f, homeY = 0.f, homeZ = 0.f;
                float angle = 0.f;
            };

            std::vector<Bot> m_bots;
            uint32 m_nextAccount = 0;
    };
}
