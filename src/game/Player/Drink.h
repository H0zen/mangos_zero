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

/// The four names cut out of the range a character can drink into.
enum DrunkenState
{
    DRUNKEN_SOBER               = 0,
    DRUNKEN_TIPSY               = 1,
    DRUNKEN_DRUNK               = 2,
    DRUNKEN_SMASHED             = 3
};

#define MAX_DRUNKEN             4

class Player;

/**
 * How much a character has had to drink.
 *
 * The amount is a number from nothing to twenty-three thousand and more, and
 * four names are cut out of that range: sober, tipsy, drunk, smashed. Only the
 * name reaches the client, packed into the same field as his gender.
 *
 * He sobers by two hundred and fifty-six every ten seconds, whatever he drank,
 * so a full skinful takes about a quarter of an hour to wear off.
 *
 * Past drunk he begins to see what is otherwise hidden: the sixth kind of
 * invisibility is pirates and ghosts that only a drunk man can make out.
 */
class Drink
{
    public:

        explicit Drink(Player& who) : m_owner(who), m_amount(0), m_sobering(0) {}

        uint16 Amount() const { return m_amount; }

        /// Sets how much is in him and settles what follows from it.
        void Amount(uint16 amount);

        /// Which of the four names the amount falls under.
        static DrunkenState NameOf(uint16 amount);

        /// Sobers him by a step every ten seconds.
        void Run(uint32 elapsed);

    private:

        Player& m_owner;

        uint16 m_amount;
        uint32 m_sobering;
};
