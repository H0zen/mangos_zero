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

#include <ctime>

class Player;

/// Where a character is resting, if he is.
enum RestType
{
    REST_TYPE_NO                = 0,
    REST_TYPE_IN_TAVERN         = 1,
    REST_TYPE_IN_CITY           = 2
};

/**
 * The arithmetic of rest, which depends on nothing but the numbers given.
 *
 * A bubble is a twentieth of a level and fills in eight hours of resting, so a
 * full bar is twenty bubbles and takes a hundred and sixty hours. The client
 * doubles whatever it is told, which is why what is kept here is half of what
 * is drawn.
 */
namespace rest
{
    /// A level's worth of experience divided over the eight hours a bubble takes,
    /// halved for the client, and multiplied by whatever rate is in force.
    inline float Gained(uint32 nextLevelXp, time_t seconds, float rate)
    {
        return float(seconds) * (nextLevelXp / 1152000.0f) * rate;
    }

    /// The most that can be held: a level and a half of drawn rest, which is
    /// thirty bubbles, kept as three quarters of a level because the client
    /// doubles it.
    inline float Ceiling(uint32 nextLevelXp)
    {
        return float(nextLevelXp) * 1.5f / 2.0f;
    }
}

/**
 * The rest a character has stored up, and where he is storing it.
 *
 * It is spent on the next experience he earns, one point of rest for one point
 * of experience, so a rested character advances at double rate until it runs
 * out. What is spent is taken off at once; nothing is kept in reserve.
 *
 * Resting is a place, not an action: an inn or a city. Standing in one sets the
 * flag the client draws and takes him out of free-for-all combat where the realm
 * has it; walking out puts him back.
 *
 * At the level ceiling nothing accrues at all, since there is no next level to
 * store a fraction of.
 */
class Rest
{
    public:

        explicit Rest(Player& who) : m_owner(who), m_bonus(0.0f), m_type(REST_TYPE_NO),
                                     m_innTrigger(0), m_enteredInn(0) {}

        float Bonus() const { return m_bonus; }

        /// Stores rest, holding it between nothing and the ceiling, and tells the
        /// client which of the two states he is in.
        void Bonus(float amount);

        /// Takes as much rest as the experience can use and hands it back.
        uint32 SpendOn(uint32 xp);

        RestType Kind() const { return m_type; }

        /// Says where he is resting, REST_TYPE_NO for nowhere.
        void Kind(RestType type, uint32 areaTriggerId = 0);

        uint32 InnTrigger() const { return m_innTrigger; }

        time_t EnteredInn() const { return m_enteredInn; }
        void EnteredInn(time_t when) { m_enteredInn = when; }

        /// What a stretch of time is worth to him, in rest.
        float Over(time_t seconds, bool offline = false, bool inRestPlace = false) const;

    private:

        Player& m_owner;

        float m_bonus;
        RestType m_type;
        uint32 m_innTrigger;
        time_t m_enteredInn;
};
