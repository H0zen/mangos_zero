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

class Fleet : public MaNGOS::Singleton<Fleet>
{
        friend class MaNGOS::Singleton<Fleet>;

    public:

        typedef std::set<Transport*> Vessels;

        void MintDeckMaps();

        void Launch();

        void Scuttle();

        void SettleCrossings();

        Vessels const& On(uint32 mapId) const;

        Vessels const& All() const { return m_vessels; }

        Transport* OnMapByGuid(uint32 mapId, ObjectGuid guid) const;

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
