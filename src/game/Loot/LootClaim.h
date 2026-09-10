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

class Group;
class Player;
class Unit;

class LootClaim
{
    public:

        bool StakedBy(Unit* taker);

        bool IsClaimed() const { return m_groupId != 0 || !(m_takerGuid == 0); }
        bool IsGroupClaim() const { return m_groupId != 0; }

        ObjectGuid const& TakerGuid() const { return m_takerGuid; }
        uint32 GroupId() const { return m_groupId; }

        Player* Taker() const;

        Group* HoldingGroup() const;

        Player* Entitled() const;

        void StartRoll(Group* group, uint32 timer);

        void StopRoll();

        bool IsRolling() const { return m_rollTimer != 0; }
        uint32 RollTimeLeft() const { return m_rollTimer; }

        bool TickRoll(uint32 diff);

    private:
        ObjectGuid m_takerGuid = 0;
        uint32     m_groupId = 0;
        uint32     m_rollGroupId = 0;
        uint32     m_rollTimer = 0;
};
