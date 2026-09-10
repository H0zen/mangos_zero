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

class BattleGround;
class Map;
class Player;

class InstanceLedger : public MaNGOS::Singleton<InstanceLedger>
{
        friend class MaNGOS::Singleton<InstanceLedger>;

    public:

        void PrimeMaxId();

        uint32 MintId() { return ++m_maxId; }

        Map* OpenFor(Player& player, uint32 mapId);

        Map* OpenBattleGround(uint32 mapId, BattleGround* bg);

        uint32 OpenDungeons() const;
        uint32 PlayersInside() const;

    private:

        InstanceLedger() = default;
        ~InstanceLedger() = default;

        InstanceLedger(InstanceLedger const&) = delete;
        InstanceLedger& operator=(InstanceLedger const&) = delete;

        uint32 m_maxId = 0;
};

#define sInstanceLedger MaNGOS::Singleton<InstanceLedger>::Instance()
