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

#include "Immunities.h"

#include <algorithm>

void Immunities::Grant(uint32 spellId, uint32 op, uint32 type)
{
    SpellImmuneList& list = m_lists[op];

    // One source per kind: whatever was holding this kind open lets go now.
    list.remove_if([type](SpellImmune const& held) { return held.type == type; });

    SpellImmune granted;
    granted.spellId = spellId;
    granted.type = type;
    list.push_back(granted);
}

void Immunities::Revoke(uint32 spellId, uint32 op)
{
    SpellImmuneList& list = m_lists[op];

    for (auto itr = list.begin(); itr != list.end(); ++itr)
    {
        if (itr->spellId == spellId)
        {
            list.erase(itr);
            break;
        }
    }
}

void Immunities::Clear()
{
    for (uint32 op = 0; op < MAX_SPELL_IMMUNITY; ++op)
    {
        m_lists[op].clear();
    }
}

bool Immunities::AnyOf(uint32 op, uint32 mask) const
{
    for (SpellImmune const& held : m_lists[op])
    {
        if (held.type & mask)
        {
            return true;
        }
    }

    return false;
}

bool Immunities::Exactly(uint32 op, uint32 type) const
{
    for (SpellImmune const& held : m_lists[op])
    {
        if (held.type == type)
        {
            return true;
        }
    }

    return false;
}
