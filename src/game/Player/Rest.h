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

enum RestType
{
    REST_TYPE_NO                = 0,
    REST_TYPE_IN_TAVERN         = 1,
    REST_TYPE_IN_CITY           = 2
};

namespace rest
{

    inline float Gained(uint32 nextLevelXp, time_t seconds, float rate)
    {
        return float(seconds) * (nextLevelXp / 1152000.0f) * rate;
    }

    struct Rates
    {
        float inGame = 1.0f;
        float offlineInn = 1.0f;
        float offlineWilderness = 1.0f;
    };

    inline float RateFor(bool offline, bool inRestPlace, Rates const& paid)
    {
        if (!offline)
        {
            return paid.inGame;
        }

        return inRestPlace ? paid.offlineInn : paid.offlineWilderness / 4.0f;
    }

    inline float Ceiling(uint32 nextLevelXp)
    {
        return float(nextLevelXp) * 1.5f / 2.0f;
    }
}

class Rest
{
    public:

        explicit Rest(Player& who) : m_owner(who), m_bonus(0.0f), m_type(REST_TYPE_NO),
                                     m_innTrigger(0), m_enteredInn(0) {}

        float Bonus() const { return m_bonus; }

        void Bonus(float amount);

        uint32 SpendOn(uint32 xp);

        RestType Kind() const { return m_type; }

        void Kind(RestType type, uint32 areaTriggerId = 0);

        uint32 InnTrigger() const { return m_innTrigger; }

        time_t EnteredInn() const { return m_enteredInn; }
        void EnteredInn(time_t when) { m_enteredInn = when; }

        float Over(time_t seconds, bool offline = false, bool inRestPlace = false) const;

    private:

        Player& m_owner;

        float m_bonus;
        RestType m_type;
        uint32 m_innTrigger;
        time_t m_enteredInn;
};
