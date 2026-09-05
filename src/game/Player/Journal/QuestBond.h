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
#include "QuestDef.h"

/**
 * What ties one spawn to a quest.
 *
 * The world data keeps two multimaps per kind of spawn, entry to quest: one for
 * the quests it hands out and one for the quests it takes back. Both a creature
 * and a gameobject ask the same two questions of them, so the asking lives here
 * rather than twice in each class.
 *
 * The range is passed in as the pair of iterators the storage hands out, so
 * nothing here needs to know which of the four maps it is reading.
 */

/// True when a range of entry-to-quest relations names this quest.
template <typename Bounds>
bool NamesQuest(Bounds const& relations, uint32 questId)
{
    for (auto itr = relations.first; itr != relations.second; ++itr)
    {
        if (itr->second == questId)
        {
            return true;
        }
    }

    return false;
}

/**
 * True when a player is carrying this quest and has not been paid for it.
 *
 * Both a quest still being worked on and one ready to turn in count: the
 * question is whether the spawn that takes the quest back still has business
 * with this player, and it has until the reward is handed over. A quest already
 * rewarded leaves nothing to come back for, even when the log still shows it as
 * complete for a repeatable.
 */
inline bool IsHandInPending(QuestStatus status, bool rewarded)
{
    return !rewarded && (status == QUEST_STATUS_INCOMPLETE || status == QUEST_STATUS_COMPLETE);
}
