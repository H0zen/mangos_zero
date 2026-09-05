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

#include "Inventory.h"

class Item;
class Player;

/**
 * A place an item is being moved to, and the plan for getting it there.
 *
 * Where a thing is going decides both what has to be weighed beforehand and what
 * has to be done afterwards, and those differ between a place he carries, a
 * place in the bank and a place he wears. A carried or banked place also takes a
 * list, because a stack may have to be broken across several; a worn place takes
 * one number, because only one thing goes on a hand.
 *
 * Holding the three together means a move is weighed once and carried out once
 * instead of the same three-way branch being written at every step. A swap needs
 * four weighings and two carryings, and without this that is six copies of it.
 */
class ItemDestination
{
    public:
        ItemDestination(Player& who, uint16 place) : m_who(who), m_place(place) {}

        /// Whether an item can go to a place of this kind at all. A place that
        /// is none of the three -- the buyback row, say -- takes nothing.
        bool Reachable() const;

        /**
         * Whether the item may go here, and by what plan. Nothing moves.
         *
         * Set swap when something is already there and is leaving in exchange:
         * a worn place then has to be able to give up what is on it as well as
         * take what is coming, and the plan is allowed to name a place that is
         * still occupied.
         */
        InventoryResult Weigh(Item* item, bool swap);

        /// Carries out the plan that Weigh came back OK with. What the item then
        /// does for him -- a stat, a set bonus, a cooldown -- is applied here,
        /// because only a worn place causes any of it.
        void Carry(Item* item);

    private:
        Player& m_who;
        uint16 m_place;

        /// Where the stack lands when the place is one he carries or banks.
        ItemPosCountVec m_spread;

        /// Where the piece lands when the place is one he wears.
        uint16 m_worn = 0;
};
