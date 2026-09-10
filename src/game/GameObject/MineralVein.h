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

#include <unordered_map>

class MineralVeins
{
    public:
        void Holds(uint32 entry, uint32 poorer, uint32 richer);
        void GroundHolds(uint32 zone, uint32 entry);
        void Clear();

        uint32 PoorerThan(uint32 entry) const;

        uint32 RicherThan(uint32 entry) const;

        uint32 InZone(uint32 zone) const;

        uint32 SpawnedAs(uint32 entry, uint32 zone, bool zoneRoll, bool poorer, bool richer) const;

        std::size_t Ladders() const { return m_ladder.size(); }
        std::size_t Grounds() const { return m_ground.size(); }

    private:
        struct Ladder
        {
            uint32 poorer = 0;
            uint32 richer = 0;
        };

        std::unordered_map<uint32, Ladder> m_ladder;
        std::unordered_map<uint32, uint32> m_ground;
};

extern MineralVeins sMineralVeins;

void LoadMineralVeins();
