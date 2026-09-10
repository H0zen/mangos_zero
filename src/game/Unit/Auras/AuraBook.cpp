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

#include "AuraBook.h"

#include "SpellAuras.h"

void AuraBook::Enter(SpellAuraHolder* holder)
{
    m_holders.insert(SpellAuraHolderMap::value_type(holder->GetId(), holder));
}

bool AuraBook::Strike(SpellAuraHolder* holder)
{

    if (m_cursor != m_holders.end() && m_cursor->second == holder)
    {
        ++m_cursor;
    }

    SpellAuraHolderBounds bounds = Of(holder->GetId());
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second == holder)
        {
            m_holders.erase(itr);
            return true;
        }
    }

    return false;
}

void AuraBook::SweepDeferred()
{
    for (Aura* aura : m_deferredAuras)
    {
        delete aura;
    }
    m_deferredAuras.clear();

    for (SpellAuraHolder* holder : m_deferredHolders)
    {
        delete holder;
    }
    m_deferredHolders.clear();
}
