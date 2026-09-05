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

#include "Journal/QuestSlots.h"
#include "ObjectGuid.h"
#include "QuestDef.h"

#include <map>
#include <set>

typedef std::map<uint32, QuestStatusData> QuestStatusMap;

/**
 * Every quest a character has taken, and how far he has got with each.
 *
 * The journal remembers more than the twenty slots the client draws: a quest he
 * has finished and been paid for stays here as rewarded, so that it is never
 * offered again, long after its slot has been given to something else. The slots
 * are a view of the journal, not the journal itself.
 *
 * Quests that run out are kept apart in a set of their own. Only one may be
 * carried at a time, so the set holds at most one thing -- it is a set because
 * nothing has yet had to depend on that.
 *
 * The divider is whoever last offered to share a quest. It is a question that
 * has been put to him rather than a quest he has, which is why it is emptied the
 * moment the offer is answered.
 */
class QuestJournal
{
    public:
        /// The line for a quest, made if he has none yet.
        QuestStatusData& Of(uint32 questId) { return m_status[questId]; }

        /// The line for a quest, or nothing when he has never taken it.
        QuestStatusData const* Find(uint32 questId) const
        {
            auto itr = m_status.find(questId);
            return itr != m_status.end() ? &itr->second : nullptr;
        }

        /// Where a quest has got to, and whether it has been paid for.
        QuestStatus StatusOf(uint32 questId) const
        {
            QuestStatusData const* line = Find(questId);
            return line ? line->m_status : QUEST_STATUS_NONE;
        }

        bool IsRewarded(uint32 questId) const
        {
            QuestStatusData const* line = Find(questId);
            return line && line->m_rewarded;
        }

        QuestStatusMap& All() { return m_status; }
        QuestStatusMap const& All() const { return m_status; }

        void Clear() { m_status.clear(); }

        /// The quests that run out while he carries them.
        std::set<uint32>& Timed() { return m_timed; }
        std::set<uint32> const& Timed() const { return m_timed; }
        bool AnyTimed() const { return !m_timed.empty(); }
        void StartTiming(uint32 questId) { m_timed.insert(questId); }
        void StopTiming(uint32 questId) { m_timed.erase(questId); }

        /// Whoever last offered to share a quest with him.
        ObjectGuid Divider() const { return m_divider; }
        void Divider(ObjectGuid who) { m_divider = who; }
        void NoDivider() { m_divider.Clear(); }

    private:
        QuestStatusMap m_status;
        std::set<uint32> m_timed;
        ObjectGuid m_divider;
};
