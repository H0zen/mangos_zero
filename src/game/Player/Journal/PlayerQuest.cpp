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



#include "Utilities/Errors.h"
#include <algorithm>
#include <string>
#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "CinematicFlyover.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "Version.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "LFGMgr.h"
#include "DisableMgr.h"

/**
 * @brief Builds the current quest menu for a creature or game object.
 *
 * @param guid The GUID of the quest source.
 */
void Player::PrepareQuestMenu(ObjectGuid guid)
{
    QuestRelationsMapBounds rbounds;
    QuestRelationsMapBounds irbounds;

    // pets also can have quests
    if (Creature* pCreature = GetMap()->GetAnyTypeCreature(guid))
    {
        rbounds = sObjectMgr.GetCreatureQuestRelationsMapBounds(pCreature->GetEntry());
        irbounds = sObjectMgr.GetCreatureQuestInvolvedRelationsMapBounds(pCreature->GetEntry());
    }
    else
    {
        // we should obtain map pointer from GetMap() in 99% of cases. Special case
        // only for quests which cast teleport spells on player
        Map* _map = IsInWorld() ? GetMap() : sMapMgr.FindMap(GetMapId(), GetInstanceId());
        MANGOS_ASSERT(_map);

        if (GameObject* pGameObject = _map->GetGameObject(guid))
        {
            rbounds = sObjectMgr.GetGOQuestRelationsMapBounds(pGameObject->GetEntry());
            irbounds = sObjectMgr.GetGOQuestInvolvedRelationsMapBounds(pGameObject->GetEntry());
        }
        else
        {
            return;
        }
    }

    QuestMenu& qm = PlayerTalkClass->GetQuestMenu();
    qm.ClearMenu();

    for (QuestRelationsMap::const_iterator itr = irbounds.first; itr != irbounds.second; ++itr)
    {
        uint32 quest_id = itr->second;

        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
        {
            continue;
        }

        QuestStatus status = GetQuestStatus(quest_id);

        if (status == QUEST_STATUS_COMPLETE && !GetQuestRewardStatus(quest_id))
        {
            qm.AddMenuItem(quest_id, DIALOG_STATUS_REWARD_REP);
        }
        else if (status == QUEST_STATUS_INCOMPLETE)
        {
            qm.AddMenuItem(quest_id, DIALOG_STATUS_INCOMPLETE);
        }
        else if (status == QUEST_STATUS_AVAILABLE)
        {
            qm.AddMenuItem(quest_id, DIALOG_STATUS_CHAT);
        }
    }

    for (QuestRelationsMap::const_iterator itr = rbounds.first; itr != rbounds.second; ++itr)
    {
        uint32 quest_id = itr->second;

        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
        {
            continue;
        }

        QuestStatus status = GetQuestStatus(quest_id);

        if (pQuest->IsAutoComplete() && CanTakeQuest(pQuest, false))
        {
            qm.AddMenuItem(quest_id, DIALOG_STATUS_REWARD_REP);
        }
        else if (status == QUEST_STATUS_NONE && CanTakeQuest(pQuest, false))
        {
            qm.AddMenuItem(quest_id, DIALOG_STATUS_AVAILABLE);
        }
    }
}

/**
 * @brief Sends the prepared quest dialog or quest list for a source.
 *
 * @param guid The GUID of the quest source.
 */
void Player::SendPreparedQuest(ObjectGuid guid)
{
    QuestMenu& questMenu = PlayerTalkClass->GetQuestMenu();
    if (questMenu.Empty())
    {
        return;
    }

    QuestMenuItem const& qmi0 = questMenu.GetItem(0);

    uint32 status = qmi0.m_qIcon;

    // single element case
    if (questMenu.MenuItemCount() == 1)
    {
        // Auto open -- maybe also should verify there is no greeting
        uint32 quest_id = qmi0.m_qId;
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);

        if (pQuest)
        {
            if (status == DIALOG_STATUS_REWARD_REP && !GetQuestRewardStatus(quest_id))
            {
                PlayerTalkClass->SendQuestGiverRequestItems(pQuest, guid, CanRewardQuest(pQuest, false), true);
            }
            else if (status == DIALOG_STATUS_INCOMPLETE)
            {
                PlayerTalkClass->SendQuestGiverRequestItems(pQuest, guid, false, true);
            }
            // Send completable on repeatable quest if player don't have quest
            else if (pQuest->IsRepeatable())
            {
                PlayerTalkClass->SendQuestGiverRequestItems(pQuest, guid, CanCompleteRepeatableQuest(pQuest), true);
            }
            else
            {
                PlayerTalkClass->SendQuestGiverQuestDetails(pQuest, guid, true);
            }
        }
    }
    // multiply entries
    else
    {
        QEmote qe;
        qe._Delay = 0;
        qe._Emote = 0;
        std::string title = "";

        // need pet case for some quests
        if (Creature* pCreature = GetMap()->GetAnyTypeCreature(guid))
        {
            uint32 textid = GetGossipTextId(pCreature);

            GossipText const* gossiptext = sObjectMgr.GetGossipText(textid);
            if (!gossiptext)
            {
                qe._Delay = 0;                              // TEXTEMOTE_MESSAGE;              // zyg: player emote
                qe._Emote = 0;                              // TEXTEMOTE_HELLO;                // zyg: NPC emote
                title.clear();
            }
            else
            {
                qe = gossiptext->Options[0].Emotes[0];

                int loc_idx = GetSession()->GetSessionDbLocaleIndex();

                std::string title0 = gossiptext->Options[0].Text_0;
                std::string title1 = gossiptext->Options[0].Text_1;
                sObjectMgr.GetNpcTextLocaleStrings0(textid, loc_idx, &title0, &title1);

                title = !title0.empty() ? title0 : title1;
            }
        }
        PlayerTalkClass->SendQuestGiverQuestList(qe, title, guid);
    }
}

/**
 * @brief Checks whether a quest is currently active in the player's log.
 *
 * @param quest_id The quest identifier to check.
 * @return True if the quest is active; otherwise, false.
 */
bool Player::IsActiveQuest(uint32 quest_id) const
{
    auto itr = m_journal.All().find(quest_id);

    return itr != m_journal.All().end() && itr->second.m_status != QUEST_STATUS_NONE;
}

/**
 * @brief Checks whether a quest is currently active with a specific completion state.
 *
 * @param quest_id The quest identifier to check.
 * @param completed_or_not The completion-state filter.
 * @return True if the quest matches the requested state; otherwise, false.
 */
bool Player::IsCurrentQuest(uint32 quest_id, uint8 completed_or_not) const
{
    auto itr = m_journal.All().find(quest_id);
    if (itr == m_journal.All().end())
    {
        return false;
    }

    QuestStatusData const& questStatus = itr->second;

    switch (completed_or_not)
    {
        case 1:
            return questStatus.m_status == QUEST_STATUS_INCOMPLETE;
        case 2:
            return questStatus.m_status == QUEST_STATUS_COMPLETE && !questStatus.m_rewarded;
        default:
            return questStatus.m_status == QUEST_STATUS_INCOMPLETE || (questStatus.m_status == QUEST_STATUS_COMPLETE && !questStatus.m_rewarded);
    }
}

/**
 * @brief Finds the next quest in a chain offered by a specific source.
 *
 * @param guid The GUID of the quest source.
 * @param pQuest The current quest in the chain.
 * @return The next quest in the chain, or null if unavailable.
 */
Quest const* Player::GetNextQuest(ObjectGuid guid, Quest const* pQuest)
{
    QuestRelationsMapBounds rbounds;

    if (Creature* pCreature = GetMap()->GetAnyTypeCreature(guid))
    {
        rbounds = sObjectMgr.GetCreatureQuestRelationsMapBounds(pCreature->GetEntry());
    }
    else
    {
        // we should obtain map pointer from GetMap() in 99% of cases. Special case
        // only for quests which cast teleport spells on player
        Map* _map = IsInWorld() ? GetMap() : sMapMgr.FindMap(GetMapId(), GetInstanceId());
        MANGOS_ASSERT(_map);

        if (GameObject* pGameObject = _map->GetGameObject(guid))
        {
            rbounds = sObjectMgr.GetGOQuestRelationsMapBounds(pGameObject->GetEntry());
        }
        else
        {
            return nullptr;
        }
    }

    uint32 nextQuestID = pQuest->GetNextQuestInChain();
    for (QuestRelationsMap::const_iterator itr = rbounds.first; itr != rbounds.second; ++itr)
    {
        if (itr->second == nextQuestID)
        {
            return sObjectMgr.GetQuestTemplate(nextQuestID);
        }
    }

    return nullptr;
}

/**
 * Check if a player could see a start quest
 * Basic Quest-taking requirements: Class, Race, Skill, Quest-Line, ...
 * Check if the quest-level is not too high (related config value CONFIG_INT32_QUEST_HIGH_LEVEL_HIDE_DIFF)
 */
bool Player::CanSeeStartQuest(Quest const* pQuest) const
{
    if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_QUEST, pQuest->GetQuestId(), this) &&
        SatisfyQuestClass(pQuest, false) && SatisfyQuestRace(pQuest, false) && SatisfyQuestSkill(pQuest, false) &&
        SatisfyQuestExclusiveGroup(pQuest, false) && SatisfyQuestReputation(pQuest, false) &&
        SatisfyQuestPreviousQuest(pQuest, false) && SatisfyQuestNextChain(pQuest, false) &&
        SatisfyQuestPrevChain(pQuest, false) &&
        pQuest->IsActive())
    {
        int32 highLevelDiff = sWorld.getConfig(CONFIG_INT32_QUEST_HIGH_LEVEL_HIDE_DIFF);
        if (highLevelDiff < 0)
        {
            return true;
        }
        return getLevel() + uint32(highLevelDiff) >= pQuest->GetMinLevel();
    }

    return false;
}

/**
 * @brief Checks whether the player can accept a quest.
 *
 * @param pQuest The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if the quest can be taken; otherwise, false.
 */
bool Player::CanTakeQuest(Quest const* pQuest, bool msg) const
{
    return !DisableMgr::IsDisabledFor(DISABLE_TYPE_QUEST, pQuest->GetQuestId(), this) &&
        SatisfyQuestStatus(pQuest, msg) &&
        SatisfyQuestExclusiveGroup(pQuest, msg) &&
        SatisfyQuestClass(pQuest, msg) &&
        SatisfyQuestRace(pQuest, msg) &&
        SatisfyQuestLevel(pQuest, msg) &&
        SatisfyQuestSkill(pQuest, msg) &&
        SatisfyQuestReputation(pQuest, msg) &&
        SatisfyQuestPreviousQuest(pQuest, msg) &&
        SatisfyQuestTimed(pQuest, msg) &&
        SatisfyQuestNextChain(pQuest, msg) &&
        SatisfyQuestPrevChain(pQuest, msg) &&
        pQuest->IsActive();
}

/**
 * @brief Checks whether the player can add a quest to the quest log.
 *
 * @param pQuest The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if the quest can be added; otherwise, false.
 */
bool Player::CanAddQuest(Quest const* pQuest, bool msg) const
{
    if (!SatisfyQuestLog(msg))
    {
        return false;
    }

    if (!CanGiveQuestSourceItemIfNeed(pQuest))
    {
        return false;
    }

    return true;
}

/**
 * @brief Checks whether the player currently meets all completion requirements for a quest.
 *
 * @param quest_id The quest identifier to validate.
 * @return True if the quest can be completed; otherwise, false.
 */
bool Player::CanCompleteQuest(uint32 quest_id) const
{
    if (!quest_id)
    {
        return false;
    }

    auto q_itr = m_journal.All().find(quest_id);

    // some quests can be auto taken and auto completed in one step
    QuestStatus status = q_itr != m_journal.All().end() ? q_itr->second.m_status : QUEST_STATUS_NONE;

    if (status == QUEST_STATUS_COMPLETE)
    {
        return false; // not allow re-complete quest
    }

    Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest_id);

    if (!qInfo)
    {
        return false;
    }

    // only used for "flag" quests and not real in-game quests
    if (qInfo->HasQuestFlag(QUEST_FLAGS_AUTO_REWARDED))
    {
        // a few checks, not all "satisfy" is needed
        if (SatisfyQuestPreviousQuest(qInfo, false) && SatisfyQuestLevel(qInfo, false) &&
            SatisfyQuestSkill(qInfo, false) && SatisfyQuestRace(qInfo, false) && SatisfyQuestClass(qInfo, false))
        {
            return true;
        }
        return false;
    }

    // auto complete quest
    if (qInfo->IsAutoComplete() && CanTakeQuest(qInfo, false))
    {
        return true;
    }

    if (status != QUEST_STATUS_INCOMPLETE)
    {
        return false;
    }

    // incomplete quest have status data
    QuestStatusData const& q_status = q_itr->second;

    if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            if (qInfo->ReqItemCount[i] != 0 && q_status.m_itemcount[i] < qInfo->ReqItemCount[i])
            {
                return false;
            }
        }
    }

    if (qInfo->HasSpecialFlag(QuestSpecialFlags(QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO)))
    {
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            if (qInfo->ReqCreatureOrGOId[i] == 0)
            {
                continue;
            }

            if (qInfo->ReqCreatureOrGOCount[i] != 0 && q_status.m_creatureOrGOcount[i] < qInfo->ReqCreatureOrGOCount[i])
            {
                return false;
            }
        }
    }

    if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT) && !q_status.m_explored)
    {
        return false;
    }

    if (qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED) && q_status.m_timer == 0)
    {
        return false;
    }

    if (qInfo->GetRewOrReqMoney() < 0)
    {
        if (GetMoney() < uint32(-qInfo->GetRewOrReqMoney()))
        {
            return false;
        }
    }

    uint32 repFacId = qInfo->GetRepObjectiveFaction();
    if (repFacId && GetReputationMgr().GetReputation(repFacId) < qInfo->GetRepObjectiveValue())
    {
        return false;
    }

    return true;
}

/**
 * @brief Checks whether a repeatable quest can currently be turned in.
 *
 * @param pQuest The repeatable quest to validate.
 * @return True if the quest can be completed; otherwise, false.
 */
bool Player::CanCompleteRepeatableQuest(Quest const* pQuest) const
{
    // Solve problem that player don't have the quest and try complete it.
    // if repeatable she must be able to complete event if player don't have it.
    // Seem that all repeatable quest are DELIVER Flag so, no need to add more.
    if (!CanTakeQuest(pQuest, false))
    {
        return false;
    }

    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            if (pQuest->ReqItemId[i] && pQuest->ReqItemCount[i] && !HasItemCount(pQuest->ReqItemId[i], pQuest->ReqItemCount[i]))
            {
                return false;
            }
        }
    }

    if (!CanRewardQuest(pQuest, false))
    {
        return false;
    }

    return true;
}

/**
 * @brief Checks whether a completed quest can currently be rewarded.
 *
 * @param pQuest The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if the quest reward can be claimed; otherwise, false.
 */
bool Player::CanRewardQuest(Quest const* pQuest, bool msg) const
{
    // not auto complete quest and not completed quest (only cheating case, then ignore without message)
    if (!pQuest->IsAutoComplete() && GetQuestStatus(pQuest->GetQuestId()) != QUEST_STATUS_COMPLETE)
    {
        return false;
    }

    // rewarded and not repeatable quest (only cheating case, then ignore without message)
    if (GetQuestRewardStatus(pQuest->GetQuestId()))
    {
        return false;
    }

    // prevent receive reward with quest items in bank
    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            if (pQuest->ReqItemCount[i] != 0 &&
                GetItemCount(pQuest->ReqItemId[i]) < pQuest->ReqItemCount[i])
            {
                if (msg)
                {
                    SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr, pQuest->ReqItemId[i]);
                }

                return false;
            }
        }
    }

    // prevent receive reward with low money and GetRewOrReqMoney() < 0
    if (pQuest->GetRewOrReqMoney() < 0 && GetMoney() < uint32(-pQuest->GetRewOrReqMoney()))
    {
        return false;
    }

    return true;
}

/**
 * @brief Checks whether a specific quest reward choice can be granted.
 *
 * @param pQuest The quest to validate.
 * @param reward The selected optional reward index.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if the selected reward can be granted; otherwise, false.
 */
bool Player::CanRewardQuest(Quest const* pQuest, uint32 reward, bool msg) const
{
    bool result;
    uint32 numOptionalRewards;
    uint32 numRewards;
    uint32 requiredSlots;
    InventoryResult iRes;

    requiredSlots = 0;
    result = CanRewardQuest(pQuest, msg);
    if (result)
    {
        ItemPosCountVec destActual;
        numOptionalRewards = pQuest->GetRewChoiceItemsCount();
        numRewards = pQuest->GetRewItemsCount();
        if (numOptionalRewards > 0)
        {
            requiredSlots = numRewards + 1; // Only ONE optional reward can be selected
        }
        else
        {
            requiredSlots = numRewards;
        }

        if (numRewards > 0 || numOptionalRewards > 0)
        {
            if (pQuest->RewChoiceItemId[reward])
            {
                ItemPosCountVec dest;
                iRes = CanStoreNewItem(0, 0, dest, pQuest->RewChoiceItemId[reward], pQuest->RewChoiceItemCount[reward]);
                if (iRes != EQUIP_ERR_OK)
                {
                    goto CANT_EQUIP;
                }
            }
            for (uint32 i = 0; i < numRewards; ++i)
            {
                if (pQuest->RewItemId[i])
                {
                    ItemPosCountVec dest;
                    iRes = CanStoreNewItem(0, 0, dest, pQuest->RewItemId[i], pQuest->RewItemCount[i]);
                    if (iRes != EQUIP_ERR_OK)
                    {
                        goto CANT_EQUIP;
                    }
                }
            }
            // We use 2586 (Gamemaster's Robes) as the item ID so that we can verify that the slots can be filled for all selected quest rewards
            iRes = CanStoreNewItem(0, 0, destActual, 2586, requiredSlots);
            CANT_EQUIP:
            if (iRes != EQUIP_ERR_OK)
            {
                SendEquipError(iRes, 0, 0);
                result = false;
            }
        }
    }
    return result;
}

/**
 * @brief Sends a pet taming failure reason to the client.
 *
 * @param reason The taming failure reason code.
 */
void Player::SendPetTameFailure(PetTameFailureReason reason)
{
    WorldPacket data(SMSG_PET_TAME_FAILURE, 1);
    data << uint8(reason);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Adds a quest to the player's log and initializes its tracking state.
 *
 * @param pQuest The quest to add.
 * @param questGiver The object that granted the quest.
 */
void Player::AddQuest(Quest const* pQuest, Object* questGiver)
{
    uint16 log_slot = m_journal.SlotOf(0);
    MANGOS_ASSERT(log_slot < MAX_QUEST_LOG_SIZE);

    uint32 quest_id = pQuest->GetQuestId();

    // if not exist then created with set uState==NEW and rewarded=false
    QuestStatusData& questStatusData = m_journal.Of(quest_id);

    // check for repeatable quests status reset
    questStatusData.m_status = QUEST_STATUS_INCOMPLETE;
    questStatusData.m_explored = false;

    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER))
    {
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            questStatusData.m_itemcount[i] = 0;
        }
    }

    if (pQuest->HasSpecialFlag(QuestSpecialFlags(QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO)))
    {
        for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            questStatusData.m_creatureOrGOcount[i] = 0;
        }
    }

    if (pQuest->GetRepObjectiveFaction())
    {
        if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(pQuest->GetRepObjectiveFaction()))
        {
            GetReputationMgr().SetVisible(factionEntry);
        }
    }

    uint32 qtime = 0;
    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
    {
        uint32 limittime = pQuest->GetLimitTime();

        // shared timed quest
        if (questGiver && questGiver->IsPlayer())
        {
            limittime = ((Player*)questGiver)->getQuestStatusMap()[quest_id].m_timer / IN_MILLISECONDS;
        }

        AddTimedQuest(quest_id);
        questStatusData.m_timer = limittime * IN_MILLISECONDS;
        qtime = static_cast<uint32>(time(nullptr)) + limittime;
    }
    else
    {
        questStatusData.m_timer = 0;
    }

    SetQuestSlot(log_slot, quest_id, qtime);

    if (questStatusData.uState != QUEST_NEW)
    {
        questStatusData.uState = QUEST_CHANGED;
    }

    // quest accept scripts
    if (questGiver)
    {
        switch (questGiver->GetTypeId())
        {
            case TYPEID_UNIT:
                sScriptMgr.OnQuestAccept(this, (Creature*)questGiver, pQuest);
                break;
            case TYPEID_ITEM:
            case TYPEID_CONTAINER:
                sScriptMgr.OnQuestAccept(this, (Item*)questGiver, pQuest);
                break;
            case TYPEID_GAMEOBJECT:
                sScriptMgr.OnQuestAccept(this, (GameObject*)questGiver, pQuest);
                break;
        }

        // starting initial DB quest script
        if (pQuest->GetQuestStartScript() != 0)
        {
            GetMap()->Scripts().Start(DBS_ON_QUEST_START, pQuest->GetQuestStartScript(), questGiver, this, SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE);
        }
    }

    // remove start item if not need
    if (questGiver && questGiver->isType(TYPEMASK_ITEM))
    {
        // destroy not required for quest finish quest starting item
        bool notRequiredItem = true;
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
        {
            if (pQuest->ReqItemId[i] == questGiver->GetEntry())
            {
                notRequiredItem = false;
                break;
            }
        }

        if (pQuest->GetSrcItemId() == questGiver->GetEntry())
        {
            notRequiredItem = false;
        }

        if (notRequiredItem)
        {
            DestroyItem(((Item*)questGiver)->GetBagSlot(), ((Item*)questGiver)->GetSlot(), true);
        }
    }

    GiveQuestSourceItemIfNeed(pQuest);

    m_journal.CountItemsHeld(pQuest, questStatusData);

    // Some spells applied at quest activation
    uint32 zone, area;
    GetTerrain()->GetZoneAndAreaId(zone, area, Where().X(), Where().Y(), Where().Z());
    SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(zone);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
    {
        itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, true);
    }
    if (area != zone)
    {
        saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(area);
        for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        {
            itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, true);
        }
    }
    saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(0);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
    {
        itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, true);
    }

    UpdateForQuestObjects();

    if (sWorld.getConfig(CONFIG_BOOL_ENABLE_QUEST_TRACKER)) // check if Quest Tracker is enabled
    {
        DEBUG_LOG("QUEST TRACKER: Quest Added.");

        static SqlStatementID CHAR_INS_QUEST_TRACK;
        // prepare Quest Tracker datas
        SqlStatement stmt = CharacterDatabase.CreateStatement(CHAR_INS_QUEST_TRACK, "INSERT INTO `quest_tracker` (`id`, `character_guid`, `quest_accept_time`, `core_hash`, `core_revision`) VALUES (?, ?, NOW(), ?, ?)");
        stmt.addUInt32(quest_id);
        stmt.addUInt32(GetGUIDLow());
        stmt.addString(MangosVersion::Hash());
        stmt.addString(MangosVersion::Date());

        // add to Quest Tracker
        stmt.Execute();
    }
}

/**
 * @brief Marks a quest as complete and updates quest tracker data.
 *
 * @param quest_id The quest identifier to complete.
 * @param status The completion status to apply.
 */
void Player::CompleteQuest(uint32 quest_id, QuestStatus status)
{
    if (quest_id)
    {
        SetQuestStatus(quest_id, status);

        uint16 log_slot = m_journal.SlotOf(quest_id);
        if (log_slot < MAX_QUEST_LOG_SIZE)
        {
            SetQuestSlotState(log_slot, QUEST_STATE_COMPLETE);
        }

        if (Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest_id))
        {
            if (qInfo->HasQuestFlag(QUEST_FLAGS_AUTO_REWARDED))
            {
                RewardQuest(qInfo, 0, this, false);
            }
        }
    }

    if (sWorld.getConfig(CONFIG_BOOL_ENABLE_QUEST_TRACKER)) // check if Quest Tracker is enabled
    {
        DEBUG_LOG("QUEST TRACKER: Quest Completed.");
        static SqlStatementID CHAR_UPD_QUEST_TRACK_COMPLETE_TIME;
        // prepare Quest Tracker datas
        SqlStatement stmt = CharacterDatabase.CreateStatement(CHAR_UPD_QUEST_TRACK_COMPLETE_TIME, "UPDATE `quest_tracker` SET `quest_complete_time` = NOW() WHERE `id` = ? AND `character_guid` = ? ORDER BY `quest_accept_time` DESC LIMIT 1");
        stmt.addUInt32(quest_id);
        stmt.addUInt32(GetGUIDLow());

        // add to Quest Tracker
        stmt.Execute();
    }
}

/**
 * @brief Restores a quest to the incomplete state.
 *
 * @param quest_id The quest identifier to update.
 */
void Player::IncompleteQuest(uint32 quest_id)
{
    if (quest_id)
    {
        SetQuestStatus(quest_id, QUEST_STATUS_INCOMPLETE);

        uint16 log_slot = m_journal.SlotOf(quest_id);
        if (log_slot < MAX_QUEST_LOG_SIZE)
        {
            RemoveQuestSlotState(log_slot, QUEST_STATE_COMPLETE);
        }
    }
}

/**
 * @brief Grants quest rewards, updates quest state, and triggers reward-side effects.
 *
 * @param pQuest The rewarded quest.
 * @param reward The selected optional reward index.
 * @param questGiver The object granting the reward.
 * @param announce True to send the quest reward packet to the client.
 */
void Player::RewardQuest(Quest const* pQuest, uint32 reward, Object* questGiver, bool announce)
{
    uint32 quest_id = pQuest->GetQuestId();

    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        if (pQuest->ReqItemId[i])
        {
            DestroyItemCount(pQuest->ReqItemId[i], pQuest->ReqItemCount[i], true);
        }
    }

    RemoveTimedQuest(quest_id);

    if (BattleGround* bg = Battle().Ground())
    {
        if (bg->GetTypeID() == BATTLEGROUND_AV)
        {
            ((BattleGroundAV*)bg)->HandleQuestComplete(pQuest->GetQuestId(), this);
        }
    }

    if (pQuest->GetRewChoiceItemsCount() > 0)
    {
        if (uint32 itemId = pQuest->RewChoiceItemId[reward])
        {
            ItemPosCountVec dest;
            if (CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, pQuest->RewChoiceItemCount[reward]) == EQUIP_ERR_OK)
            {
                Item* item = StoreNewItem(dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId));
                SendNewItem(item, pQuest->RewChoiceItemCount[reward], true, false, false, false);
            }
        }
    }

    if (pQuest->GetRewItemsCount() > 0)
    {
        for (uint32 i = 0; i < pQuest->GetRewItemsCount(); ++i)
        {
            if (uint32 itemId = pQuest->RewItemId[i])
            {
                ItemPosCountVec dest;
                if (CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, pQuest->RewItemCount[i]) == EQUIP_ERR_OK)
                {
                    Item* item = StoreNewItem(dest, itemId, true, Item::GenerateItemRandomPropertyId(itemId));
                    SendNewItem(item, pQuest->RewItemCount[i], true, false, false, false);
                }
            }
        }
    }

    RewardReputation(pQuest);

    uint16 log_slot = m_journal.SlotOf(quest_id);
    if (log_slot < MAX_QUEST_LOG_SIZE)
    {
        SetQuestSlot(log_slot, 0);
    }

    QuestStatusData& q_status = m_journal.Of(quest_id);

    // Used for client inform but rewarded only in case not max level
    uint32 xp = uint32(pQuest->XPValue(this) * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_QUEST));

    if (getLevel() < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        GiveXP(xp , nullptr);
    }
    else
    {
        ModifyMoney(int32(pQuest->GetRewMoneyMaxLevel() * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY)));
    }

    // Give player extra money if GetRewOrReqMoney > 0 and get ReqMoney if negative
    ModifyMoney(pQuest->GetRewOrReqMoney());

    // Send reward mail
    if (uint32 mail_template_id = pQuest->GetRewMailTemplateId())
    {
        MailDraft(mail_template_id).SendMailTo(this, questGiver, MAIL_CHECK_MASK_HAS_BODY, pQuest->GetRewMailDelaySecs());
    }

    if (!pQuest->IsRepeatable())
    {
        SetQuestStatus(quest_id, QUEST_STATUS_COMPLETE);
    }
    else
    {
        SetQuestStatus(quest_id, QUEST_STATUS_NONE);
    }

    q_status.m_rewarded = true;
    if (q_status.uState != QUEST_NEW)
    {
        q_status.uState = QUEST_CHANGED;
    }

    if (announce)
    {
        SendQuestReward(pQuest, xp);
    }

    bool handled = false;

    switch (questGiver->GetTypeId())
    {
        case TYPEID_UNIT:
            handled = sScriptMgr.OnQuestRewarded(this, (Creature*)questGiver, pQuest, reward);
            break;
        case TYPEID_GAMEOBJECT:
            handled = sScriptMgr.OnQuestRewarded(this, (GameObject*)questGiver, pQuest, reward);
            break;
    }

    if (!handled && pQuest->GetQuestCompleteScript() != 0)
    {
        GetMap()->Scripts().Start(DBS_ON_QUEST_END, pQuest->GetQuestCompleteScript(), questGiver, this, SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE);
    }

    // cast spells after mark quest complete (some spells have quest completed state reqyurements in spell_area data)
    if (pQuest->GetRewSpellCast() > 0)
    {
        CastSpell(this, pQuest->GetRewSpellCast(), true);
    }
    else if (pQuest->GetRewSpell() > 0)
    {
        CastSpell(this, pQuest->GetRewSpell(), true);
    }

    // remove auras from spells with quest reward state limitations
    // Some spells applied at quest reward
    uint32 zone, area;
    GetTerrain()->GetZoneAndAreaId(zone, area, Where().X(), Where().Y(), Where().Z());
    SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(zone);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
    {
        itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, false);
    }
    if (area != zone)
    {
        saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(area);
        for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        {
            itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, false);
        }
    }
    saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(0);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
    {
        itr->second->ApplyOrRemoveSpellIfCan(this, zone, area, false);
    }
}

// TODO be more specific at callers about quest fail reason. Also, quest "fails" when either picking up or giving out is unsuccessful.
void Player::FailQuest(uint32 questId)
{
    if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(questId))
    {
        SetQuestStatus(questId, QUEST_STATUS_FAILED);

        uint16 log_slot = m_journal.SlotOf(questId);

        if (log_slot < MAX_QUEST_LOG_SIZE)
        {
            SetQuestSlotTimer(log_slot, 1);
            SetQuestSlotState(log_slot, QUEST_STATE_FAIL);
        }

        if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
        {
            QuestStatusData& q_status = m_journal.Of(questId);

            RemoveTimedQuest(questId);
            q_status.m_timer = 0;

            SendQuestTimerFailed(questId);
        }
        else
        {
            SendQuestFailed(questId);
        }
    }
}

/**
 * @brief Checks whether the player meets a quest's required skill threshold.
 *
 * @param qInfo The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if the skill requirement is met; otherwise, false.
 */
bool Player::SatisfyQuestSkill(Quest const* qInfo, bool msg) const
{
    uint32 skill = qInfo->GetRequiredSkill();

    // skip 0 case RequiredSkill
    if (skill == 0)
    {
        return true;
    }

    // check skill value
    if (GetSkillValue(skill) < qInfo->GetRequiredSkillValue())
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
        }

        return false;
    }

    return true;
}

/**
 * @brief Checks whether the player meets a quest's minimum level requirement.
 *
 * @param qInfo The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if the level requirement is met; otherwise, false.
 */
bool Player::SatisfyQuestLevel(Quest const* qInfo, bool msg) const
{
    if (getLevel() < qInfo->GetMinLevel())
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
        }

        return false;
    }

    return true;
}

/**
 * @brief Checks whether the player has free space in the quest log.
 *
 * @param msg True to emit failure feedback when checks fail.
 * @return True if a free quest log slot exists; otherwise, false.
 */
bool Player::SatisfyQuestLog(bool msg) const
{
    // exist free slot
    if (m_journal.SlotOf(0) < MAX_QUEST_LOG_SIZE)
    {
        return true;
    }

    if (msg)
    {
        WorldPacket data(SMSG_QUESTLOG_FULL, 0);
        GetSession()->SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_QUESTLOG_FULL");
    }
    return false;
}

/**
 * @brief Checks whether previous-quest requirements for a quest are satisfied.
 *
 * @param qInfo The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if previous-quest requirements are met; otherwise, false.
 */
bool Player::SatisfyQuestPreviousQuest(Quest const* qInfo, bool msg) const
{
    // No previous quest (might be first quest in a series)
    if (qInfo->prevQuests.empty())
    {
        return true;
    }

    for (Quest::PrevQuests::const_iterator iter = qInfo->prevQuests.begin(); iter != qInfo->prevQuests.end(); ++iter)
    {
        uint32 prevId = abs(*iter);

        auto i_prevstatus = m_journal.All().find(prevId);
        Quest const* qPrevInfo = sObjectMgr.GetQuestTemplate(prevId);

        if (qPrevInfo && i_prevstatus != m_journal.All().end())
        {
            // If any of the positive previous quests completed, return true
            if (*iter > 0 && i_prevstatus->second.m_rewarded)
            {
                // skip one-from-all exclusive group
                if (qPrevInfo->GetExclusiveGroup() >= 0)
                {
                    return true;
                }

                // each-from-all exclusive group ( < 0)
                // given a group with 2+ quests, and one of those has a branch that is not restricted by the group, return true
                if (qInfo->GetPrevQuestId() != 0 && qPrevInfo->GetNextQuestId() != qInfo->GetPrevQuestId())
                {
                    return true;
                }

                // can be start if only all quests in prev quest exclusive group completed and rewarded
                ExclusiveQuestGroupsMapBounds bounds = sObjectMgr.GetExclusiveQuestGroupsMapBounds(qPrevInfo->GetExclusiveGroup());

                MANGOS_ASSERT(bounds.first != bounds.second); // always must be found if qPrevInfo->ExclusiveGroup != 0

                for (ExclusiveQuestGroupsMap::const_iterator iter2 = bounds.first; iter2 != bounds.second; ++iter2)
                {
                    uint32 exclude_Id = iter2->second;

                    // skip checked quest id, only state of other quests in group is interesting
                    if (exclude_Id == prevId)
                    {
                        continue;
                    }

                    auto i_exstatus = m_journal.All().find(exclude_Id);

                    // alternative quest from group also must be completed and rewarded(reported)
                    if (i_exstatus == m_journal.All().end() || !i_exstatus->second.m_rewarded)
                    {
                        if (msg)
                        {
                            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
                        }

                        return false;
                    }
                }
                return true;
            }
            // If any of the negative previous quests active, return true
            if (*iter < 0 && IsCurrentQuest(prevId))
            {
                // skip one-from-all exclusive group
                if (qPrevInfo->GetExclusiveGroup() >= 0)
                {
                    return true;
                }

                // each-from-all exclusive group ( < 0)
                // given a group with 2+ quests, and one of those has a branch that is not restricted by the group, return true
                if (qInfo->GetPrevQuestId() != 0 && qPrevInfo->GetNextQuestId() != abs(qInfo->GetPrevQuestId()))
                {
                    return true;
                }

                // can be start if only all quests in prev quest exclusive group active
                ExclusiveQuestGroupsMapBounds bounds = sObjectMgr.GetExclusiveQuestGroupsMapBounds(qPrevInfo->GetExclusiveGroup());

                MANGOS_ASSERT(bounds.first != bounds.second); // always must be found if qPrevInfo->ExclusiveGroup != 0

                for (ExclusiveQuestGroupsMap::const_iterator iter2 = bounds.first; iter2 != bounds.second; ++iter2)
                {
                    uint32 exclude_Id = iter2->second;

                    // skip checked quest id, only state of other quests in group is interesting
                    if (exclude_Id == prevId)
                    {
                        continue;
                    }

                    // alternative quest from group also must be active
                    if (!IsCurrentQuest(exclude_Id))
                    {
                        if (msg)
                        {
                            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
                        }

                        return false;
                    }
                }
                return true;
            }
        }
    }

    // Has only positive prev. quests in non-rewarded state
    // and negative prev. quests in non-active state
    if (msg)
    {
        SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
    }

    return false;
}

/**
 * @brief Checks whether the player's class can accept a quest.
 *
 * @param qInfo The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if the class requirement is met; otherwise, false.
 */
bool Player::SatisfyQuestClass(Quest const* qInfo, bool msg) const
{
    uint32 reqClass = qInfo->GetRequiredClasses();

    if (reqClass == 0)
    {
        return true;
    }

    if ((reqClass & getClassMask()) == 0)
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
        }

        return false;
    }

    return true;
}

/**
 * @brief Checks whether the player's race can accept a quest.
 *
 * @param qInfo The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if the race requirement is met; otherwise, false.
 */
bool Player::SatisfyQuestRace(Quest const* qInfo, bool msg) const
{
    uint32 reqraces = qInfo->GetRequiredRaces();

    if (reqraces == 0)
    {
        return true;
    }

    if ((reqraces & getRaceMask()) == 0)
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_FAILED_WRONG_RACE);
        }

        return false;
    }

    return true;
}

/**
 * @brief Checks whether the player's reputation meets quest requirements.
 *
 * @param qInfo The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if reputation requirements are met; otherwise, false.
 */
bool Player::SatisfyQuestReputation(Quest const* qInfo, bool msg) const
{
    uint32 fIdMin = qInfo->GetRequiredMinRepFaction();      // Min required rep
    if (fIdMin && GetReputationMgr().GetReputation(fIdMin) < qInfo->GetRequiredMinRepValue())
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
        }

        return false;
    }

    uint32 fIdMax = qInfo->GetRequiredMaxRepFaction();      // Max required rep
    if (fIdMax && GetReputationMgr().GetReputation(fIdMax) >= qInfo->GetRequiredMaxRepValue())
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
        }

        return false;
    }

    return true;
}

/**
 * @brief Checks whether the quest is not already active in the player's log.
 *
 * @param qInfo The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if the quest status allows accepting it; otherwise, false.
 */
bool Player::SatisfyQuestStatus(Quest const* qInfo, bool msg) const
{
    auto itr = m_journal.All().find(qInfo->GetQuestId());

    if (itr != m_journal.All().end() && itr->second.m_status != QUEST_STATUS_NONE)
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_ALREADY_ON);
        }

        return false;
    }

    return true;
}

/**
 * @brief Checks whether the player can accept another timed quest.
 *
 * @param qInfo The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if timed-quest rules allow acceptance; otherwise, false.
 */
bool Player::SatisfyQuestTimed(Quest const* qInfo, bool msg) const
{
    if (!m_journal.Timed().empty() && qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_QUEST_ONLY_ONE_TIMED);
        }

        return false;
    }

    return true;
}

/**
 * @brief Checks whether exclusive-group rules allow the quest to be accepted.
 *
 * @param qInfo The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if exclusive-group requirements are met; otherwise, false.
 */
bool Player::SatisfyQuestExclusiveGroup(Quest const* qInfo, bool msg) const
{
    // non positive exclusive group, if > 0 then can be start if any other quest in exclusive group already started/completed
    if (qInfo->GetExclusiveGroup() <= 0)
    {
        return true;
    }

    ExclusiveQuestGroupsMapBounds bounds = sObjectMgr.GetExclusiveQuestGroupsMapBounds(qInfo->GetExclusiveGroup());

    MANGOS_ASSERT(bounds.first != bounds.second);           // must always be found if qInfo->ExclusiveGroup != 0

    for (ExclusiveQuestGroupsMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
    {
        uint32 exclude_Id = iter->second;

        // skip checked quest id, only state of other quests in group is interesting
        if (exclude_Id == qInfo->GetQuestId())
        {
            continue;
        }

        auto i_exstatus = m_journal.All().find(exclude_Id);

        // alternative quest already started or completed
        if (i_exstatus != m_journal.All().end() &&
            (i_exstatus->second.m_status == QUEST_STATUS_COMPLETE || i_exstatus->second.m_status == QUEST_STATUS_INCOMPLETE))
        {
            if (msg)
            {
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
            }

            return false;
        }
    }

    return true;
}

/**
 * @brief Checks whether later quests in the chain do not block acceptance.
 *
 * @param qInfo The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if next-chain requirements are met; otherwise, false.
 */
bool Player::SatisfyQuestNextChain(Quest const* qInfo, bool msg) const
{
    if (!qInfo->GetNextQuestInChain())
    {
        return true;
    }

    // next quest in chain already started or completed
    auto itr = m_journal.All().find(qInfo->GetNextQuestInChain());
    if (itr != m_journal.All().end() &&
        (itr->second.m_status == QUEST_STATUS_COMPLETE || itr->second.m_status == QUEST_STATUS_INCOMPLETE))
    {
        if (msg)
        {
            SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
        }

        return false;
    }

    // check for all quests further up the chain
    // only necessary if there are quest chains with more than one quest that can be skipped
    // return SatisfyQuestNextChain( qInfo->GetNextQuestInChain(), msg );
    return true;
}

/**
 * @brief Checks whether previous-chain quests do not block acceptance.
 *
 * @param qInfo The quest to validate.
 * @param msg True to emit failure feedback when checks fail.
 * @return True if previous-chain requirements are met; otherwise, false.
 */
bool Player::SatisfyQuestPrevChain(Quest const* qInfo, bool msg) const
{
    // No previous quest in chain
    if (qInfo->prevChainQuests.empty())
    {
        return true;
    }

    for (Quest::PrevChainQuests::const_iterator iter = qInfo->prevChainQuests.begin(); iter != qInfo->prevChainQuests.end(); ++iter)
    {
        uint32 prevId = *iter;

        // If any of the previous quests in chain active, return false
        if (IsCurrentQuest(prevId))
        {
            if (msg)
            {
                SendCanTakeQuestResponse(INVALIDREASON_DONT_HAVE_REQ);
            }

            return false;
        }

        // check for all quests further down the chain
        // only necessary if there are quest chains with more than one quest that can be skipped
        // if ( !SatisfyQuestPrevChain( prevId, msg ) )
        //    return false;
    }

    // No previous quest in chain active
    return true;
}

/**
 * @brief Checks whether required quest source items can be granted to the player.
 *
 * @param pQuest The quest whose source item is being evaluated.
 * @param dest Optional output describing where the item can be stored.
 * @return True if the source item can be granted or is not needed; otherwise, false.
 */
bool Player::CanGiveQuestSourceItemIfNeed(Quest const* pQuest, ItemPosCountVec* dest) const
{
    if (uint32 srcitem = pQuest->GetSrcItemId())
    {
        uint32 count = pQuest->GetSrcItemCount();

        // player already have max amount required item (including bank), just report success
        uint32 has_count = GetItemCount(srcitem, true);
        if (has_count >= count)
        {
            return true;
        }

        count -= has_count;                                 // real need amount

        InventoryResult msg;
        if (!dest)
        {
            ItemPosCountVec destTemp;
            msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, destTemp, srcitem, count);
        }
        else
        {
            msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, *dest, srcitem, count);
        }

        if (msg == EQUIP_ERR_OK)
        {
            return true;
        }
        else
        {
            SendEquipError(msg, nullptr, nullptr, srcitem);
        }
        return false;
    }

    return true;
}

/**
 * @brief Grants quest source items required when a quest is accepted.
 *
 * @param pQuest The quest whose source item should be granted.
 */
void Player::GiveQuestSourceItemIfNeed(Quest const* pQuest)
{
    ItemPosCountVec dest;
    if (CanGiveQuestSourceItemIfNeed(pQuest, &dest) && !dest.empty())
    {
        uint32 count = 0;
        for (ItemPosCountVec::const_iterator c_itr = dest.begin(); c_itr != dest.end(); ++c_itr)
        {
            count += c_itr->count;
        }

        Item* item = StoreNewItem(dest, pQuest->GetSrcItemId(), true);
        SendNewItem(item, count, true, false);
    }
}

/**
 * @brief Removes a quest source item when the quest flow requires it.
 *
 * @param quest_id The quest identifier whose source item should be removed.
 * @param msg True to emit failure feedback when removal is not possible.
 * @return True if the source item was removed or not needed; otherwise, false.
 */
bool Player::TakeQuestSourceItem(uint32 quest_id, bool msg)
{
    Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest_id);
    if (qInfo)
    {
        uint32 srcitem = qInfo->GetSrcItemId();
        if (srcitem > 0)
        {
            uint32 count = qInfo->GetSrcItemCount();
            if (count <= 0)
            {
                count = 1;
            }

            // exist one case when destroy source quest item not possible:
            // non un-equippable item (equipped non-empty bag, for example)
            InventoryResult res = CanUnequipItems(srcitem, count);
            if (res != EQUIP_ERR_OK)
            {
                if (msg)
                {
                    SendEquipError(res, nullptr, nullptr, srcitem);
                }
                return false;
            }

            DestroyItemCount(srcitem, count, true, true);
        }
    }
    return true;
}

/**
 * @brief Checks whether a quest reward has already been claimed.
 *
 * @param quest_id The quest identifier to query.
 * @return True if the quest reward is marked as claimed; otherwise, false.
 */
bool Player::GetQuestRewardStatus(uint32 quest_id) const
{
    Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest_id);
    if (qInfo)
    {
        // for repeatable quests: rewarded field is set after first reward only to prevent getting XP more than once
        auto itr = m_journal.All().find(quest_id);
        if (itr != m_journal.All().end() && itr->second.m_status != QUEST_STATUS_NONE &&
            !qInfo->IsRepeatable())
        {
            return itr->second.m_rewarded;
        }

        return false;
    }
    return false;
}

/**
 * @brief Gets the player's current status for a quest.
 *
 * @param quest_id The quest identifier to query.
 * @return The current quest status.
 */
QuestStatus Player::GetQuestStatus(uint32 quest_id) const
{
    if (quest_id)
    {
        auto itr = m_journal.All().find(quest_id);
        if (itr != m_journal.All().end())
        {
            if (itr->second.m_status == QUEST_STATUS_FORCE_COMPLETE)
            {
                return QUEST_STATUS_COMPLETE;
            }
            return itr->second.m_status;
        }
    }
    return QUEST_STATUS_NONE;
}

/**
 * @brief Checks whether a quest can currently be shared with other players.
 *
 * @param quest_id The quest identifier to query.
 * @return True if the quest is active and sharable; otherwise, false.
 */
bool Player::CanShareQuest(uint32 quest_id) const
{
    if (Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest_id))
    {
        if (qInfo->HasQuestFlag(QUEST_FLAGS_SHARABLE))
        {
            return IsCurrentQuest(quest_id);
        }
    }
    return false;
}

/**
 * @brief Updates the stored status for a quest and refreshes quest world objects.
 *
 * @param quest_id The quest identifier to update.
 * @param status The new quest status.
 */
void Player::SetQuestStatus(uint32 quest_id, QuestStatus status)
{
    if (sObjectMgr.GetQuestTemplate(quest_id))
    {
        QuestStatusData& q_status = m_journal.Of(quest_id);

        q_status.m_status = status;

        if (q_status.uState != QUEST_NEW)
        {
            q_status.uState = QUEST_CHANGED;
        }
    }

    UpdateForQuestObjects();
}

/**
 * @brief Sends the quest reward summary packet to the client.
 *
 * @param pQuest The rewarded quest.
 * @param XP The experience amount shown in the reward packet.
 */
void Player::SendQuestReward(Quest const* pQuest, uint32 XP)
{
    uint32 questid = pQuest->GetQuestId();
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_COMPLETE quest = %u", questid);
    WorldPacket data(SMSG_QUESTGIVER_QUEST_COMPLETE, (4 + 4 + 4 + 4 + 4 + pQuest->GetRewItemsCount() * 8));
    data << uint32(questid);
    data << uint32(0x03);

    if (getLevel() < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        data << uint32(XP);
        data << uint32(pQuest->GetRewOrReqMoney());
    }
    else
    {
        data << uint32(0);
        data << uint32(pQuest->GetRewOrReqMoney() + int32(pQuest->GetRewMoneyMaxLevel() * sWorld.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY)));
    }
    data << uint32(pQuest->GetRewItemsCount());             // max is 5

    for (uint32 i = 0; i < pQuest->GetRewItemsCount(); ++i)
    {
        if (pQuest->RewItemId[i] > 0)
        {
            data << pQuest->RewItemId[i] << pQuest->RewItemCount[i];
        }
        else
        {
            data << uint32(0) << uint32(0);
        }
    }
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends a quest failure update to the client.
 *
 * @param quest_id The failed quest identifier.
 */
void Player::SendQuestFailed(uint32 quest_id)
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTUPDATE_FAILED, 4);
        data << uint32(quest_id);
        GetSession()->SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_QUESTUPDATE_FAILED");
    }
}

/**
 * @brief Sends a timed quest failure update to the client.
 *
 * @param quest_id The failed timed quest identifier.
 */
void Player::SendQuestTimerFailed(uint32 quest_id)
{
    if (quest_id)
    {
        WorldPacket data(SMSG_QUESTUPDATE_FAILEDTIMER, 4);
        data << uint32(quest_id);
        GetSession()->SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_QUESTUPDATE_FAILEDTIMER");
    }
}

/**
 * @brief Sends a quest acceptance failure reason to the client.
 *
 * @param msg The invalid-reason code to report.
 */
void Player::SendCanTakeQuestResponse(uint32 msg) const
{
    WorldPacket data(SMSG_QUESTGIVER_QUEST_INVALID, 4);
    data << uint32(msg);
    GetSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_INVALID");
}

/**
 * @brief Sends a quest-share acceptance confirmation prompt to another player.
 *
 * @param pQuest The shared quest.
 * @param pReceiver The player receiving the confirmation prompt.
 */
void Player::SendQuestConfirmAccept(const Quest* pQuest, Player* pReceiver)
{
    if (pReceiver)
    {
        int loc_idx = pReceiver->GetSession()->GetSessionDbLocaleIndex();
        std::string title = pQuest->GetTitle();
        sObjectMgr.GetQuestLocaleStrings(pQuest->GetQuestId(), loc_idx, &title);

        WorldPacket data(SMSG_QUEST_CONFIRM_ACCEPT, (4 + title.size() + 8));
        data << uint32(pQuest->GetQuestId());
        data << title;
        data << GetObjectGuid();
        pReceiver->GetSession()->SendPacket(&data);

        DEBUG_LOG("WORLD: Sent SMSG_QUEST_CONFIRM_ACCEPT");
    }
}

/**
 * @brief Sends the result of a quest share push to the client.
 *
 * @param pPlayer The player targeted by the share attempt.
 * @param msg The quest share result code.
 */
void Player::SendPushToPartyResponse(Player* pPlayer, uint8 msg)
{
    if (pPlayer)
    {
        WorldPacket data(MSG_QUEST_PUSH_RESULT, (8 + 1));
        data << pPlayer->GetObjectGuid();
        data << uint8(msg);                   // enum QuestShareMessages
        GetSession()->SendPacket(&data);
        DEBUG_LOG("WORLD: Sent MSG_QUEST_PUSH_RESULT");
    }
}

/**
 * @brief Refreshes visible quest-related world objects for the player.
 */
void Player::UpdateForQuestObjects()
{
    if (m_clientGUIDs.empty())
    {
        return;
    }

    for (GuidSet::const_iterator itr = m_clientGUIDs.begin(); itr != m_clientGUIDs.end(); ++itr)
    {
        if (itr->IsGameObject())
        {
            if (GameObject* obj = GetMap()->GetGameObject(*itr))
            {
                obj->SendCreateUpdateToPlayer(this); //[-ZERO] we must send create packet because of GAMEOBJECT_FLAGS change (not dynamic) - probably incorrect
            }
        }
    }
}
