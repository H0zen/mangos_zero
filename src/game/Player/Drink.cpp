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

#include "Drink.h"

#include "Player.h"

/// A step of sobering, and the beat it comes on.
namespace
{
    uint16 const SOBERS_BY = 256;
    uint32 const EVERY = 10 * IN_MILLISECONDS;
}

DrunkenState Drink::NameOf(uint16 amount)
{
    if (amount >= 23000)
    {
        return DRUNKEN_SMASHED;
    }

    if (amount >= 12800)
    {
        return DRUNKEN_DRUNK;
    }

    if (amount & 0xFFFE)
    {
        return DRUNKEN_TIPSY;
    }

    return DRUNKEN_SOBER;
}

void Drink::Amount(uint16 amount)
{
    m_amount = amount;
    m_owner.SetDrunkAndGender(m_amount, m_owner.getGender());

    // past drunk he makes out what only a drunk man can see
    if (NameOf(m_amount) >= DRUNKEN_DRUNK)
    {
        m_owner.SeesInvisibility(6, true);
    }
    else
    {
        m_owner.SeesInvisibility(6, false);
    }
}

void Drink::Run(uint32 elapsed)
{
    if (!m_amount)
    {
        return;
    }

    m_sobering += elapsed;

    if (m_sobering <= EVERY)
    {
        return;
    }

    m_sobering = 0;
    Amount(m_amount <= SOBERS_BY ? 0 : m_amount - SOBERS_BY);
}
