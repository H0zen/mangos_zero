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

#include "Rest.h"

#include "Log.h"
#include "Player.h"
#include "World.h"

void Rest::Bonus(float amount)
{
    // there is no next level to store a fraction of
    if (m_owner.getLevel() >= sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        amount = 0.0f;
    }

    if (amount < 0.0f)
    {
        amount = 0.0f;
    }

    float const ceiling = rest::Ceiling(m_owner.GetUInt32Value(PLAYER_NEXT_LEVEL_XP));
    m_bonus = amount > ceiling ? ceiling : amount;

    if (m_bonus > 10.0f)
    {
        m_owner.SetByteValue(PLAYER_BYTES_2, 3, REST_STATE_RESTED);
    }
    else if (m_bonus <= 1.0f)
    {
        m_owner.SetByteValue(PLAYER_BYTES_2, 3, REST_STATE_NORMAL);
    }

    m_owner.SetUInt32Value(PLAYER_REST_STATE_EXPERIENCE, uint32(m_bonus));
}

uint32 Rest::SpendOn(uint32 xp)
{
    // rest can at most double the experience, never more
    uint32 spent = uint32(m_bonus);
    if (spent > xp)
    {
        spent = xp;
    }

    Bonus(m_bonus - spent);

    DETAIL_LOG("Player gain %u xp (+ %u Rested Bonus). Rested points=%f", xp + spent, spent, m_bonus);
    return spent;
}

void Rest::Kind(RestType type, uint32 areaTriggerId /*= 0*/)
{
    m_type = type;

    if (m_type == REST_TYPE_NO)
    {
        m_owner.RemovePlayerFlag(PLAYER_FLAGS_RESTING);

        // outside a resting place he is fair game where the realm says so
        if (sWorld.IsFFAPvPRealm())
        {
            m_owner.SetFFAPvP(true);
        }

        return;
    }

    m_owner.SetPlayerFlag(PLAYER_FLAGS_RESTING);

    m_innTrigger = areaTriggerId;
    m_enteredInn = time(nullptr);

    if (sWorld.IsFFAPvPRealm())
    {
        m_owner.SetFFAPvP(false);
    }
}

float Rest::Over(time_t seconds, bool offline /*= false*/, bool inRestPlace /*= false*/) const
{
    float rate = sWorld.getConfig(CONFIG_FLOAT_RATE_REST_INGAME);

    if (offline)
    {
        rate = inRestPlace
             ? sWorld.getConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_TAVERN_OR_CITY)
             : sWorld.getConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_WILDERNESS) / 4.0f;
    }

    return rest::Gained(m_owner.GetUInt32Value(PLAYER_NEXT_LEVEL_XP), seconds, rate);
}
