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

#include "Journal/QuestJournal.h"

#include "Creature.h"
#include "Group.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>

namespace
{

    template <typename Visit>
    void EachCarried(Player& who, QuestJournal& journal, Visit visit)
    {
        for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 const questId = who.GetQuestSlotQuestId(slot);
            if (!questId)
            {
                continue;
            }

            Quest const* quest = sObjectMgr.GetQuestTemplate(questId);
            if (!quest)
            {
                continue;
            }

            if (!visit(slot, questId, quest, journal.Of(questId)))
            {
                return;
            }
        }
    }

    template <typename Visit>
    void EachCarried(Player const& who, QuestJournal const& journal, Visit visit)
    {
        for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 const questId = who.GetQuestSlotQuestId(slot);
            if (!questId)
            {
                continue;
            }

            QuestStatusData const* line = journal.Find(questId);
            if (!line)
            {
                continue;
            }

            Quest const* quest = sObjectMgr.GetQuestTemplate(questId);
            if (!quest)
            {
                continue;
            }

            if (!visit(slot, questId, quest, *line))
            {
                return;
            }
        }
    }

    bool BarredByRaid(Player const& who, Quest const* quest)
    {
        Group const* group = who.GetGroup();
        return group && group->isRaidGroup() && !quest->IsAllowedInRaid();
    }
}

uint16 QuestJournal::SlotOf(uint32 questId) const
{
    for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        if (m_owner.GetQuestSlotQuestId(slot) == questId)
        {
            return slot;
        }
    }

    return MAX_QUEST_LOG_SIZE;
}

void QuestJournal::CountItemsHeld(Quest const* quest, QuestStatusData& line)
{
    if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        return;
    }

    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        uint32 const needed = quest->ReqItemCount[i];
        if (!needed)
        {
            continue;
        }

        line.m_itemcount[i] = std::min(m_owner.GetItemCount(quest->ReqItemId[i], true), needed);
        if (line.uState != QUEST_NEW)
        {
            line.uState = QUEST_CHANGED;
        }
    }
}

void QuestJournal::TellExplored(uint32 questId)
{
    if (!questId)
    {
        return;
    }

    WorldPacket data(SMSG_QUESTUPDATE_COMPLETE, 4);
    data << uint32(questId);
    m_owner.GetSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTUPDATE_COMPLETE quest = %u", questId);
}

void QuestJournal::TellItemCount(Quest const* quest, uint32 which, uint32 count)
{
    DEBUG_LOG("WORLD: Sent SMSG_QUESTUPDATE_ADD_ITEM");
    WorldPacket data(SMSG_QUESTUPDATE_ADD_ITEM, (4 + 4));
    data << quest->ReqItemId[which];
    data << count;
    m_owner.GetSession()->SendPacket(&data);
}

void QuestJournal::TellTargetCount(Quest const* quest, ObjectGuid whose, uint32 which, uint32 count)
{
    MANGOS_ASSERT(count <= quests::MOST_PER_COUNTER);

    int32 entry = quest->ReqCreatureOrGOId[which];
    if (entry < 0)

    {
        entry = (-entry) | 0x80000000;
    }

    WorldPacket data(SMSG_QUESTUPDATE_ADD_KILL, (4 * 4 + 8));
    DEBUG_LOG("WORLD: Sent SMSG_QUESTUPDATE_ADD_KILL");
    data << uint32(quest->GetQuestId());
    data << uint32(entry);
    data << uint32(count);
    data << uint32(quest->ReqCreatureOrGOCount[which]);
    data << whose;
    m_owner.GetSession()->SendPacket(&data);

    uint16 const slot = SlotOf(quest->GetQuestId());
    if (slot < MAX_QUEST_LOG_SIZE)
    {
        m_owner.SetQuestSlotCounter(slot, which, count);
    }
}

void QuestJournal::Credited(Quest const* quest, QuestStatusData& line, uint32 which, ObjectGuid whose)
{
    if (line.m_creatureOrGOcount[which] < quest->ReqCreatureOrGOCount[which])
    {
        ++line.m_creatureOrGOcount[which];
        if (line.uState != QUEST_NEW)
        {
            line.uState = QUEST_CHANGED;
        }

        TellTargetCount(quest, whose, which, line.m_creatureOrGOcount[which]);
    }

    if (m_owner.CanCompleteQuest(quest->GetQuestId()))
    {
        m_owner.CompleteQuest(quest->GetQuestId());
    }
}

void QuestJournal::Explored(uint32 questId)
{
    if (!questId)
    {
        return;
    }

    uint16 const slot = SlotOf(questId);
    if (slot < MAX_QUEST_LOG_SIZE)
    {
        QuestStatusData& line = Of(questId);

        if (!line.m_explored)
        {
            m_owner.SetQuestSlotState(slot, QUEST_STATE_COMPLETE);
            TellExplored(questId);
            line.m_explored = true;

            if (line.uState != QUEST_NEW)
            {
                line.uState = QUEST_CHANGED;
            }
        }
    }

    if (m_owner.CanCompleteQuest(questId))
    {
        m_owner.CompleteQuest(questId);
    }
}

void QuestJournal::ExploredWithGroup(uint32 questId, Occupant const* what)
{
    Group* group = m_owner.GetGroup();
    if (!group)
    {
        Explored(questId);
        return;
    }

    for (auto itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->getSource();

        if (member && member->IsAtGroupRewardDistance(what) && !member->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        {
            member->Journal().Explored(questId);
        }
    }
}

void QuestJournal::ItemGained(uint32 entry, uint32 count)
{
    bool matched = false;
    bool refresh = false;

    EachCarried(m_owner, *this, [&](uint16, uint32 questId, Quest const* quest, QuestStatusData& line)
    {
        if (line.m_status != QUEST_STATUS_INCOMPLETE)
        {
            return true;
        }

        if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
        {
            return true;
        }

        for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            if (quest->ReqItemId[j] != entry)
            {
                continue;
            }

            matched = true;

            uint32 const needed = quest->ReqItemCount[j];
            uint32 const held = line.m_itemcount[j];
            if (held < needed)
            {
                uint32 const gained = quests::CountsTowards(held, needed, count);
                line.m_itemcount[j] += gained;
                if (line.uState != QUEST_NEW)
                {
                    line.uState = QUEST_CHANGED;
                }

                TellItemCount(quest, j, gained);
            }

            if (m_owner.CanCompleteQuest(questId))
            {
                m_owner.CompleteQuest(questId);
                return false;
            }

            if (needed == line.m_itemcount[j])
            {
                refresh = true;
            }

            return false;
        }

        return true;
    });

    if (!matched || refresh)
    {
        m_owner.UpdateForQuestObjects();
    }
}

void QuestJournal::ItemLost(uint32 entry, uint32 count)
{
    bool matched = false;

    EachCarried(m_owner, *this, [&](uint16, uint32 questId, Quest const* quest, QuestStatusData& line)
    {
        if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
        {
            return true;
        }

        for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            if (quest->ReqItemId[j] != entry)
            {
                continue;
            }

            matched = true;

            uint32 const needed = quest->ReqItemCount[j];
            uint32 const held = (line.m_status != QUEST_STATUS_COMPLETE
                                 ? line.m_itemcount[j]
                                 : m_owner.GetItemCount(entry, true));

            if (held < needed + count)
            {
                line.m_itemcount[j] = held - quests::CountsAgainst(held, needed, count);
                if (line.uState != QUEST_NEW)
                {
                    line.uState = QUEST_CHANGED;
                }

                m_owner.IncompleteQuest(questId);
            }

            return false;
        }

        return true;
    });

    if (!matched)
    {
        m_owner.UpdateForQuestObjects();
    }
}

void QuestJournal::CreatureKilled(CreatureInfo const* what, ObjectGuid whose)
{
    if (what->Entry)
    {
        KillCredited(what->Entry, whose);
    }

    for (int i = 0; i < MAX_KILL_CREDIT; ++i)
    {
        if (what->KillCredit[i])
        {
            KillCredited(what->KillCredit[i], whose);
        }
    }
}

void QuestJournal::KillCredited(uint32 entry, ObjectGuid whose)
{
    EachCarried(m_owner, *this, [&](uint16, uint32, Quest const* quest, QuestStatusData& line)
    {
        if (line.m_status != QUEST_STATUS_INCOMPLETE || BarredByRaid(m_owner, quest))
        {
            return true;
        }

        if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_KILL_OR_CAST))
        {
            return true;
        }

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {

            if (quest->ReqCreatureOrGOId[j] <= 0 || quest->ReqSpell[j] != 0)
            {
                continue;
            }

            if (uint32(quest->ReqCreatureOrGOId[j]) == entry)
            {
                Credited(quest, line, j, whose);
            }
        }

        return true;
    });
}

void QuestJournal::CastCredited(uint32 entry, ObjectGuid whose, uint32 spellId, bool originalCaster)
{
    bool const onCreature = (GuidHigh(whose) == HIGHGUID_UNIT);

    EachCarried(m_owner, *this, [&](uint16, uint32, Quest const* quest, QuestStatusData& line)
    {
        if (!originalCaster && !quest->HasQuestFlag(QUEST_FLAGS_SHARABLE))
        {
            return true;
        }

        if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_KILL_OR_CAST))
        {
            return true;
        }

        if (line.m_status != QUEST_STATUS_INCOMPLETE)
        {
            return true;
        }

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            if (quest->ReqSpell[j] != spellId)
            {
                continue;
            }

            int32 const target = quest->ReqCreatureOrGOId[j];
            uint32 const wanted = (onCreature ? (target > 0 ? uint32(target) : 0)
                                              : (target < 0 ? uint32(-target) : 0));

            if (wanted != entry)
            {
                continue;
            }

            Credited(quest, line, j, whose);

            break;
        }

        return true;
    });
}

void QuestJournal::TalkCredited(uint32 entry, ObjectGuid whose)
{
    EachCarried(m_owner, *this, [&](uint16, uint32, Quest const* quest, QuestStatusData& line)
    {
        if (line.m_status != QUEST_STATUS_INCOMPLETE)
        {
            return true;
        }

        if (!quest->HasSpecialFlag(QuestSpecialFlags(QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO)))
        {
            return true;
        }

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {

            if (quest->ReqSpell[j] > 0 || quest->ReqCreatureOrGOId[j] < 0)
            {
                continue;
            }

            if (uint32(quest->ReqCreatureOrGOId[j]) == entry)
            {
                Credited(quest, line, j, whose);
            }
        }

        return true;
    });
}

void QuestJournal::MoneyNowIs(uint32 amount)
{
    EachCarried(m_owner, *this, [&](uint16, uint32 questId, Quest const* quest, QuestStatusData& line)
    {
        int32 const owed = -quest->GetRewOrReqMoney();
        if (owed <= 0)
        {
            return true;
        }

        if (line.m_status == QUEST_STATUS_INCOMPLETE)
        {
            if (int32(amount) >= owed && m_owner.CanCompleteQuest(questId))
            {
                m_owner.CompleteQuest(questId);
            }
        }
        else if (line.m_status == QUEST_STATUS_COMPLETE)
        {
            if (int32(amount) < owed)
            {
                m_owner.IncompleteQuest(questId);
            }
        }

        return true;
    });
}

void QuestJournal::ReputationNowIs(FactionEntry const* faction)
{
    EachCarried(m_owner, *this, [&](uint16, uint32 questId, Quest const* quest, QuestStatusData& line)
    {
        if (quest->GetRepObjectiveFaction() != faction->ID)
        {
            return true;
        }

        int32 const standing = m_owner.GetReputationMgr().GetReputation(faction);

        if (line.m_status == QUEST_STATUS_INCOMPLETE)
        {
            if (standing >= quest->GetRepObjectiveValue() && m_owner.CanCompleteQuest(questId))
            {
                m_owner.CompleteQuest(questId);
            }
        }
        else if (line.m_status == QUEST_STATUS_COMPLETE)
        {
            if (standing < quest->GetRepObjectiveValue())
            {
                m_owner.IncompleteQuest(questId);
            }
        }

        return true;
    });
}

bool QuestJournal::NeedsItem(uint32 itemId) const
{
    bool needed = false;

    EachCarried(m_owner, *this, [&](uint16, uint32, Quest const* quest, QuestStatusData const& line)
    {
        if (line.m_status != QUEST_STATUS_INCOMPLETE)
        {
            return true;
        }

        if (BarredByRaid(m_owner, quest) && !m_owner.Battle().InOne())
        {
            return true;
        }

        for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            if (itemId == quest->ReqItemId[j] && line.m_itemcount[j] < quest->ReqItemCount[j])
            {
                needed = true;
                return false;
            }
        }

        for (int j = 0; j < QUEST_SOURCE_ITEM_IDS_COUNT; ++j)
        {
            if (quest->ReqSourceId[j] != itemId)
            {
                continue;
            }

            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemId);
            uint32 const held = m_owner.GetItemCount(itemId, true);

            if (proto->MaxCount && held < proto->MaxCount)
            {
                needed = true;
                return false;
            }

            uint32 const ceiling = (quest->ReqSourceCount[j] ? quest->ReqSourceCount[j] : proto->Stackable);
            if (held < ceiling)
            {
                needed = true;
                return false;
            }
        }

        return true;
    });

    return needed;
}

bool QuestJournal::NeedsGameObject(int32 goId) const
{
    bool needed = false;

    EachCarried(m_owner, *this, [&](uint16, uint32, Quest const* quest, QuestStatusData const& line)
    {
        if (line.m_status != QUEST_STATUS_INCOMPLETE || BarredByRaid(m_owner, quest))
        {
            return true;
        }

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            if (quest->ReqCreatureOrGOId[j] >= 0)
            {
                continue;
            }

            if (-goId == quest->ReqCreatureOrGOId[j] && line.m_creatureOrGOcount[j] < quest->ReqCreatureOrGOCount[j])
            {
                needed = true;
                return false;
            }
        }

        return true;
    });

    return needed;
}
