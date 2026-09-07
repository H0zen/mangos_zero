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

#include "MapRoster.h"

#include "Map.h"

MapRoster::~MapRoster()
{
    // The shutdown path empties the sheet through RetireAll while the vessels are still
    // afloat and everything else is still standing. Anything left here is a process that
    // never got that far, and the maps go without ceremony.
    for (auto const& filed : m_sheet)
    {
        delete filed.second;
    }

    m_sheet.clear();
}

Map* MapRoster::Find(uint32 mapId, uint32 instanceId) const
{
    std::lock_guard<std::mutex> guard(m_lock);

    auto const filed = m_sheet.find(MapKey(mapId, instanceId));

    return filed == m_sheet.end() ? nullptr : filed->second;
}

void MapRoster::Enrol(MapKey const& key, Map* map)
{
    if (!map)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(m_lock);

    m_sheet[key] = map;
}

void MapRoster::Retire(MapKey const& key)
{
    Map* leaving = nullptr;

    {
        std::lock_guard<std::mutex> guard(m_lock);

        auto const filed = m_sheet.find(key);
        if (filed == m_sheet.end())
        {
            return;
        }

        leaving = filed->second;
        m_sheet.erase(filed);
    }

    // Struck off first, then emptied: unloading a map runs scripts and destructors that ask
    // the roster what is open, and what they must not be told is the map being torn down.
    leaving->UnloadAll(true);
    delete leaving;
}

void MapRoster::RetireAll()
{
    Sheet leaving;

    {
        std::lock_guard<std::mutex> guard(m_lock);
        leaving.swap(m_sheet);
    }

    for (auto const& filed : leaving)
    {
        filed.second->UnloadAll(true);
    }

    for (auto const& filed : leaving)
    {
        delete filed.second;
    }
}
