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

#include "GridDefines.h"
#include "ObjectGuid.h"
#include "Policies/Singleton.h"
#include "Utilities/ConcurrentRegistry.h"

class Corpse;
class Map;

class CorpseManager : public MaNGOS::Singleton<CorpseManager>
{
        friend class MaNGOS::Singleton<CorpseManager>;

    public:

        Corpse* Find(ObjectGuid corpseGuid) const;

        Corpse* FindInMap(ObjectGuid corpseGuid, uint32 mapId) const;

        Corpse* FindForPlayer(ObjectGuid playerGuid) const;

        void AddObject(Corpse* corpse);
        void RemoveObject(Corpse* corpse);

        void Add(Corpse* corpse);

        void Remove(Corpse* corpse);

        void AddCorpsesToGrid(GridPair const& gridpair, GridType& grid, Map* map);

        Corpse* ConvertCorpseForPlayer(ObjectGuid playerGuid, bool insignia = false);

        void RemoveOldCorpses();

    private:

        CorpseManager() = default;
        ~CorpseManager();

        void RecordCell(Corpse* corpse);
        void ForgetCell(Corpse* corpse);

        MaNGOS::ConcurrentRegistry<ObjectGuid, Corpse> m_corpses;
        MaNGOS::ConcurrentRegistry<ObjectGuid, Corpse> m_byOwner;
};

#define sCorpseManager MaNGOS::Singleton<CorpseManager>::Instance()
