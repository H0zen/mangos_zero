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

class QuestJournal
{
    public:
        explicit QuestJournal(Player& owner) : m_owner(owner) {}

        QuestStatusData& Of(uint32 questId) { return m_status[questId]; }

        QuestStatusData const* Find(uint32 questId) const
        {
            auto itr = m_status.find(questId);
            return itr != m_status.end() ? &itr->second : nullptr;
        }

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

        std::set<uint32>& Timed() { return m_timed; }
        std::set<uint32> const& Timed() const { return m_timed; }
        bool AnyTimed() const { return !m_timed.empty(); }
        void StartTiming(uint32 questId) { m_timed.insert(questId); }
        void StopTiming(uint32 questId) { m_timed.erase(questId); }

        ObjectGuid Divider() const { return m_divider; }
        void Divider(ObjectGuid who) { m_divider = who; }
        void NoDivider() { m_divider = 0; }

        uint16 SlotOf(uint32 questId) const;

        void CountItemsHeld(Quest const* quest, QuestStatusData& line);

        void Explored(uint32 questId);

        void ExploredWithGroup(uint32 questId, Occupant const* what);

        void ItemGained(uint32 entry, uint32 count);
        void ItemLost(uint32 entry, uint32 count);

        void CreatureKilled(CreatureInfo const* what, ObjectGuid whose);

        void KillCredited(uint32 entry, ObjectGuid whose = 0);
        void CastCredited(uint32 entry, ObjectGuid whose, uint32 spellId, bool originalCaster = true);
        void TalkCredited(uint32 entry, ObjectGuid whose);

        void MoneyNowIs(uint32 amount);
        void ReputationNowIs(FactionEntry const* faction);

        bool NeedsItem(uint32 itemId) const;

        bool NeedsGameObject(int32 goId) const;

    private:

        void Credited(Quest const* quest, QuestStatusData& line, uint32 which, ObjectGuid whose);

        void TellExplored(uint32 questId);
        void TellItemCount(Quest const* quest, uint32 which, uint32 count);
        void TellTargetCount(Quest const* quest, ObjectGuid whose, uint32 which, uint32 count);

        Player& m_owner;
        QuestStatusMap m_status;
        std::set<uint32> m_timed;
        ObjectGuid m_divider = 0;
};
