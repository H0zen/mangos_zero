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

#include "ItemSaveQueue.h"

#include "Item.h"
#include "Log.h"

/// Only what the player actually owns may be written under their name.
bool ItemSaveQueue::IsOurs(Item const& item, char const* what) const
{
    if (item.GetOwnerGuid() == m_owner)
    {
        return true;
    }

    sLog.outError("ItemSaveQueue::%s - %s is owned by %s, not by %s",
                  what, item.GetGuidStr().c_str(),
                  item.GetOwnerGuid().GetString().c_str(), m_owner.GetString().c_str());
    return false;
}

void ItemSaveQueue::Note(Item* item)
{
    if (m_shut || Holds(item) || !IsOurs(*item, "Note"))
    {
        return;
    }

    m_waiting.push_back(item);
    m_place[item] = m_waiting.size() - 1;
}

void ItemSaveQueue::Forget(Item* item)
{
    auto const at = m_place.find(item);
    if (m_shut || at == m_place.end() || !IsOurs(*item, "Forget"))
    {
        return;
    }

    m_waiting[at->second] = nullptr;
    m_place.erase(at);
}

std::size_t ItemSaveQueue::PlaceOf(Item const* item) const
{
    auto const at = m_place.find(item);
    return at == m_place.end() ? m_waiting.size() : at->second;
}

void ItemSaveQueue::Clear()
{
    m_waiting.clear();
    m_place.clear();
}
