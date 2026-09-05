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

class Occupant;
class Player;
struct CreatureInfo;
struct FactionEntry;

typedef std::map<uint32, QuestStatusData> QuestStatusMap;

/**
 * Every quest a character has taken, and how far he has got with each.
 *
 * The journal remembers more than the twenty slots the client draws: a quest he
 * has finished and been paid for stays here as rewarded, so that it is never
 * offered again, long after its slot has been given to something else. The slots
 * are a view of the journal, not the journal itself.
 *
 * What he carries right now is exactly what stands in those twenty slots, which
 * is why everything that reports progress -- a kill, a spell cast, a word with
 * someone, an item gained or lost, money, reputation, a place walked into --
 * walks the slots and looks up the line each one names.
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
        explicit QuestJournal(Player& owner) : m_owner(owner) {}

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

        /// Which of the twenty slots a quest stands in, or MAX_QUEST_LOG_SIZE.
        uint16 SlotOf(uint32 questId) const;

        /// Fills the item counts of a fresh line from what he already carries.
        void CountItemsHeld(Quest const* quest, QuestStatusData& line);

        /// A place walked into, or an event a script says has happened.
        void Explored(uint32 questId);

        /// The same, for every living group member near the object.
        void ExploredWithGroup(uint32 questId, Occupant const* what);

        void ItemGained(uint32 entry, uint32 count);
        void ItemLost(uint32 entry, uint32 count);

        /// A creature killed, crediting its own entry and its shared credits.
        void CreatureKilled(CreatureInfo const* what, ObjectGuid whose);

        void KillCredited(uint32 entry, ObjectGuid whose = ObjectGuid());
        void CastCredited(uint32 entry, ObjectGuid whose, uint32 spellId, bool originalCaster = true);
        void TalkCredited(uint32 entry, ObjectGuid whose);

        void MoneyNowIs(uint32 amount);
        void ReputationNowIs(FactionEntry const* faction);

        /// Whether an item still counts towards something he carries.
        bool NeedsItem(uint32 itemId) const;

        /// Whether a gameobject is still an objective of something he carries.
        bool NeedsGameObject(int32 goId) const;

    private:
        /// One step against the objective at `which`, and the word for it.
        void Credited(Quest const* quest, QuestStatusData& line, uint32 which, ObjectGuid whose);

        void TellExplored(uint32 questId);
        void TellItemCount(Quest const* quest, uint32 which, uint32 count);
        void TellTargetCount(Quest const* quest, ObjectGuid whose, uint32 which, uint32 count);

        Player& m_owner;
        QuestStatusMap m_status;
        std::set<uint32> m_timed;
        ObjectGuid m_divider;
};
