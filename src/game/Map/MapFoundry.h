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

/**
 * @brief Where a map is cast, and the only place one ever is.
 *
 * The caller says what it wants, in its own terms: a continent, a vessel's deck, or the map
 * a particular player belongs on. It never says which C++ class that is -- deciding that is
 * the whole of this class's job. What comes back is already filed with MapRoster and ready
 * to be walked into.
 *
 * The bench lock is held across a whole Open, so two threads asking for the same continent
 * at once get one map rather than two. MapRoster keeps its own lock for the sheet itself;
 * the order is bench first, sheet second, never the reverse.
 */
class MapFoundry : public MaNGOS::Singleton<MapFoundry>
{
        friend class MaNGOS::Singleton<MapFoundry>;

    public:

        /// A continent or any other map everybody shares. Casts it on the first ask.
        Map* OpenWorld(uint32 mapId);

        /// A vessel's own map. Only the vessel can ask, because only she can be its owner.
        TransportMap* OpenDeck(uint32 deckMapId, Transport& vessel);

        /// The map this player belongs on: his instance if the map has copies, else the world's.
        Map* OpenFor(Player& player, uint32 mapId);

        /// Cast an unfiled dungeon copy. InstanceLedger files it; nobody else calls this.
        DungeonMap* CastDungeon(uint32 mapId, uint32 instanceId, DungeonPersistentState* save);

        /// Cast an unfiled battleground copy, bound to its battleground both ways.
        BattleGroundMap* CastBattleGround(uint32 mapId, uint32 instanceId, BattleGround* bg);

        /// How long a grid with nobody in it is kept before it is dropped.
        void SetGridCleanUpDelay(uint32 ms);
        uint32 GridCleanUpDelay() const { return m_gridCleanUpDelay; }

    private:

        MapFoundry();
        ~MapFoundry() = default;

        MapFoundry(MapFoundry const&) = delete;
        MapFoundry& operator=(MapFoundry const&) = delete;

        /// Find or cast the single shared copy of a non-instanceable map.
        Map* Shared(uint32 mapId, Transport* vessel);

        uint32 m_gridCleanUpDelay;
        std::mutex m_bench;
};

#define sMapFoundry MaNGOS::Singleton<MapFoundry>::Instance()
