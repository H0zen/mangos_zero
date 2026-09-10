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

#include <mutex>

class BattleGround;
class BattleGroundMap;
class DungeonMap;
class DungeonPersistentState;
class Map;
class Player;
class Transport;
class TransportMap;

class MapFoundry : public MaNGOS::Singleton<MapFoundry>
{
        friend class MaNGOS::Singleton<MapFoundry>;

    public:

        Map* OpenWorld(uint32 mapId);

        TransportMap* OpenDeck(uint32 deckMapId, Transport& vessel);

        Map* OpenFor(Player& player, uint32 mapId);

        DungeonMap* CastDungeon(uint32 mapId, uint32 instanceId, DungeonPersistentState* save);

        BattleGroundMap* CastBattleGround(uint32 mapId, uint32 instanceId, BattleGround* bg);

        void SetGridCleanUpDelay(uint32 ms);
        uint32 GridCleanUpDelay() const { return m_gridCleanUpDelay; }

    private:

        MapFoundry();
        ~MapFoundry() = default;

        MapFoundry(MapFoundry const&) = delete;
        MapFoundry& operator=(MapFoundry const&) = delete;

        Map* Shared(uint32 mapId, Transport* vessel);

        uint32 m_gridCleanUpDelay;
        std::mutex m_bench;
};

#define sMapFoundry MaNGOS::Singleton<MapFoundry>::Instance()
