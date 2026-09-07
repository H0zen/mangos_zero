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

/**
 * @brief Which copy of a map a player belongs in, and the numbers those copies are known by.
 *
 * A dungeon or a battleground exists many times over, and the question "which one is his?"
 * is answered from his holds and his battleground stay -- never from the map, which has no
 * way of knowing. This ledger asks him, then has MapFoundry cast the copy if it is not open
 * yet and files it with MapRoster.
 *
 * The copy numbers are minted here and continue the run of numbers already in the database,
 * so a copy that was saved before the restart keeps the number its saved state names.
 */
class InstanceLedger : public MaNGOS::Singleton<InstanceLedger>
{
        friend class MaNGOS::Singleton<InstanceLedger>;

    public:

        /// Continue the run of copy numbers from the highest one the database has kept.
        void PrimeMaxId();

        /// The next unused copy number.
        uint32 MintId() { return ++m_maxId; }

        /// The copy of an instanceable map this player belongs in, opened if it is not.
        Map* OpenFor(Player& player, uint32 mapId);

        /// A fresh copy for a battleground about to start. The match owns it, not a bind.
        Map* OpenBattleGround(uint32 mapId, BattleGround* bg);

        /// How many dungeon copies are open, and how many people are inside them.
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
