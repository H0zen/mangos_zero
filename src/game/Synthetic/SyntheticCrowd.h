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

// A crowd that is not there, to find out what a crowd costs.
//
// Five hundred players standing in one city is the case the whole visibility
// design is argued about, and nobody here has ever measured it. These are real
// Players in the real map, seen by the real sweep, sent real update packets --
// only the socket at the end is a counter instead of a client.
//
// They move, because a still crowd measures nothing: retransmission fan-out is
// quadratic in the number of movers, and it is the fan-out that decides whether
// a city tick fits in its budget.
//
// NOT FOR A LIVE REALM. Spawning is refused unless the configuration says this
// server is a test one, and these players are never written to the character
// database -- five hundred bots logging out would otherwise write five hundred
// rows of nonsense.

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
        uint64 bytes = 0;           ///< over the window, across all of them
        uint64 packets = 0;
        uint32 bytesPerSecPeak = 0; ///< the worst-off single session
        uint32 bytesPerSecMean = 0;
    };

    class SyntheticCrowd
    {
        public:

            static SyntheticCrowd& Instance();

            /**
             * @brief Put `count` synthetic players on a map at a point.
             *
             * They are scattered inside `radius` so they are not all in one
             * cell, which would measure a case the grid never sees.
             *
             * Returns how many were placed, and why if fewer than asked.
             */
            uint32 Spawn(uint32 count, uint32 mapId, float x, float y, float z,
                         float radius, std::string& error);

            /// Remove them all. Safe to call when there are none.
            uint32 Despawn();

            /// One step of movement for every bot, fed in as the packets a
            /// client would have sent.
            void Drive(uint32 diff);

            /// Consume the window's counters.
            CrowdReport Report(uint32 elapsedMs);

            uint32 Size() const { return uint32(m_bots.size()); }
            bool Empty() const { return m_bots.empty(); }

        private:

            SyntheticCrowd() = default;

            struct Bot
            {
                WorldSession* session = nullptr;
                SyntheticLink* link = nullptr;   ///< owned by the session's shared_ptr
                ObjectGuid guid;
                float homeX = 0.f, homeY = 0.f, homeZ = 0.f;
                float angle = 0.f;               ///< where it is on its little circle
            };

            std::vector<Bot> m_bots;
            uint32 m_nextAccount = 0;
    };
}
