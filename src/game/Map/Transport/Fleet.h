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

#include "ObjectGuid.h"
#include "Platform/Define.h"
#include "Policies/Singleton.h"

#include <map>
#include <set>

class Transport;

/**
 * @brief Every vessel on the server, and the shore each one can be reached from.
 *
 * A vessel is owned outright here and nowhere else. She sits in no cell, so no grid unload
 * ever reaches her, and her crew live in her own map's store rather than in any grid --
 * which is why Scuttle has to run while the maps are still standing, and why the fleet is
 * emptied before the roster is.
 *
 * The shore index is the fleet's other job. A vessel is visible to a whole map rather than
 * to a radius, so "which vessels does this map have?" is asked constantly -- by the relay,
 * by the create blocks a player is sent on arrival, and by anything resolving a transport
 * guid a client named. It is built once from the route and never moves: a vessel appears
 * under every map her route touches, including the ones she is not on today.
 */
class Fleet : public MaNGOS::Singleton<Fleet>
{
        friend class MaNGOS::Singleton<Fleet>;

    public:

        typedef std::set<Transport*> Vessels;

        /// Mint every vessel's deck map id and inject its Map.dbc row. Runs BEFORE the spawn
        /// tables are read, because those reject a row whose map sMapStore does not know.
        void MintDeckMaps();

        /// Build every vessel from `transports`, put her in the world and pin her route.
        void Launch();

        /// Destroy every vessel and her crew. Must run while the maps are still alive.
        void Scuttle();

        /// Hand over every vessel that reached the end of a map. Barrier work: no map runs.
        void SettleCrossings();

        /// The vessels that call at this map, whether or not one is there now.
        Vessels const& On(uint32 mapId) const;

        Vessels const& All() const { return m_vessels; }

        /// The vessel a client named by guid, looked for only where she could be.
        Transport* OnMapByGuid(uint32 mapId, ObjectGuid guid) const;

        /// The vessel of this guid anywhere on the server, for a login that names her.
        Transport* ByGuid(ObjectGuid guid) const;
        Transport* ByLowGuid(uint32 lowGuid) const;

    private:

        Fleet() = default;
        ~Fleet();

        Fleet(Fleet const&) = delete;
        Fleet& operator=(Fleet const&) = delete;

        Vessels m_vessels;
        std::map<uint32, Vessels> m_byMap;
};

#define sFleet MaNGOS::Singleton<Fleet>::Instance()
