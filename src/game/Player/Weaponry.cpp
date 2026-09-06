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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Weaponry.h"

#include "Player.h"

void Weaponry::CanParry(bool can)
{
    if (m_canParry == can)
    {
        return;
    }

    m_canParry = can;
    m_owner.UpdateParryPercentage();
}

void Weaponry::CanBlock(bool can)
{
    if (m_canBlock == can)
    {
        return;
    }

    m_canBlock = can;
    m_owner.UpdateBlockPercentage();
}
