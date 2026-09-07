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
#include "Policies/Singleton.h"

class Map;

/**
 * @brief The part of the world that is awake with nobody watching it.
 *
 * Two jobs, and they are the same job seen from two ends. Awaken opens the continents at
 * start-up, and PinActiveGrids holds down the grids a freshly opened map needs before any
 * player is near them: the cells of creatures marked active, or every spawn on the map when
 * it is configured to be force-loaded. A grid that expires under an active creature takes
 * the creature with it, silently, and only shows up as a quest giver who is not there.
 */
class LivingWorld : public MaNGOS::Singleton<LivingWorld>
{
        friend class MaNGOS::Singleton<LivingWorld>;

    public:

        /// What opening the continents cost, for the one line that reports it.
        struct Awakening
        {
            uint32 forcedMaps = 0;
            uint32 grids = 0;
            uint32 newlyLoaded = 0;
        };

        /// Open the continents. Everything else opens when somebody walks into it.
        Awakening Awaken();

        /// Hold down the grids this map cannot afford to lose.
        void PinActiveGrids(Map& map);

    private:

        LivingWorld() = default;
        ~LivingWorld() = default;

        LivingWorld(LivingWorld const&) = delete;
        LivingWorld& operator=(LivingWorld const&) = delete;

        /// Set only while Awaken runs: per-map lines are worth printing at start-up and
        /// worth nothing on the hundredth dungeon copy of the evening.
        bool m_awakening = false;
        Awakening m_tally;
};

#define sLivingWorld MaNGOS::Singleton<LivingWorld>::Instance()
