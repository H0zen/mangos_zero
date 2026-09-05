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

/**
 * Who may take what is inside something, and whether a group is still rolling.
 *
 * A corpse and a chest answer this the same way and always have, so the answer
 * belongs to neither of them. Whoever struck the killing blow, or opened the
 * lid, stakes the claim; if they were in a group at that moment the group holds
 * it with them, and the claim survives the group changing afterwards.
 *
 * The roll is a separate span of time laid over the claim: while it runs, the
 * group named here is deciding among themselves, and nobody may take anything.
 */
class LootClaim
{
    public:
        /**
         * Stake the claim for whoever is behind `taker` -- a pet's master, a
         * charmer's player. Passing nothing clears it.
         *
         * A unit nobody drives stakes no claim, which is how one creature
         * killing another leaves the corpse to the first player who finds it.
         *
         * @return true when a player was found and the claim now stands.
         */
        bool StakedBy(Unit* taker);

        bool IsClaimed() const { return m_groupId != 0 || !m_takerGuid.IsEmpty(); }
        bool IsGroupClaim() const { return m_groupId != 0; }

        ObjectGuid const& TakerGuid() const { return m_takerGuid; }
        uint32 GroupId() const { return m_groupId; }

        /// The player who staked it, whatever has happened to their group since.
        Player* Taker() const;

        /// The group that holds it, or nothing if it never had one or it has
        /// disbanded since.
        Group* HoldingGroup() const;

        /**
         * Who may actually take it now, which is not always who staked it: once
         * a group holds the claim, a player who has since left it loses out to
         * any member who is still in.
         */
        Player* Entitled() const;

        /// The group starts deciding among themselves, for this long.
        void StartRoll(Group* group, uint32 timer);

        /// The roll ends, however it ended.
        void StopRoll();

        bool IsRolling() const { return m_rollTimer != 0; }
        uint32 RollTimeLeft() const { return m_rollTimer; }

        /// @return true when this tick is the one the roll runs out on.
        bool TickRoll(uint32 diff);

    private:
        ObjectGuid m_takerGuid;
        uint32     m_groupId = 0;
        uint32     m_rollGroupId = 0;
        uint32     m_rollTimer = 0;
};
