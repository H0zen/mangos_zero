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

#include "MapKey.h"
#include "Policies/Singleton.h"

#include <map>
#include <mutex>

class Map;

/**
 * @brief The sheet of maps that are open right now, and nothing else.
 *
 * It answers "is this map open?", holds the pointer while it is, and destroys the map when
 * it is struck off. It casts nothing -- MapFoundry builds maps -- ticks nothing --
 * MapTicker does that -- and knows about instances only as the number a copy is filed under.
 *
 * Find, Enrol and Retire are guarded: a map thread can ask for a map by id while the world
 * thread is opening another one. Walking the sheet belongs to the world thread alone.
 */
class MapRoster : public MaNGOS::Singleton<MapRoster>
{
        friend class MaNGOS::Singleton<MapRoster>;

    public:

        typedef std::map<MapKey, Map*> Sheet;

        /// The open map filed under this id, or nullptr.
        Map* Find(uint32 mapId, uint32 instanceId = 0) const;

        /// File a freshly cast map. The roster owns it from here on.
        void Enrol(MapKey const& key, Map* map);

        /// Unload the map, destroy it, strike it off. Every pointer to it is dead after this.
        void Retire(MapKey const& key);

        /// The whole sheet, unloaded and destroyed. The shutdown path.
        void RetireAll();

        Sheet const& All() const { return m_sheet; }

        uint32 Count() const { return uint32(m_sheet.size()); }

        /// Visit every open copy of one map id: one continent, or forty copies of a dungeon.
        template<typename Visit>
        void EachOnMap(uint32 mapId, Visit&& visit) const
        {
            auto const first = m_sheet.lower_bound(MapKey(mapId, 0));
            auto const last = m_sheet.lower_bound(MapKey(mapId + 1, 0));

            for (auto itr = first; itr != last; ++itr)
            {
                visit(itr->second);
            }
        }

        /// Visit every open map, whatever it is.
        template<typename Visit>
        void Each(Visit&& visit) const
        {
            for (auto const& filed : m_sheet)
            {
                visit(filed.second);
            }
        }

    private:

        MapRoster() = default;
        ~MapRoster();

        MapRoster(MapRoster const&) = delete;
        MapRoster& operator=(MapRoster const&) = delete;

        Sheet m_sheet;
        mutable std::mutex m_lock;
};

#define sMapRoster MaNGOS::Singleton<MapRoster>::Instance()
