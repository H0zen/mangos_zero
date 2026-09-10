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

#include "Chat.h"
#include "ObjectMgr.h"
#include "World.h"
#include "SQLStorages.h"

bool ChatHandler::HandleQuestAddCommand(char* args)
{
    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 entry;
    if (!ExtractUint32KeyFromLink(&args, "Hquest", entry))
    {
        return false;
    }

    Quest const* pQuest = sObjectMgr.GetQuestTemplate(entry);
    if (!pQuest)
    {
        PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
        SetSentErrorMessage(true);
        return false;
    }

    for (uint32 id = 0; id < sItemStorage.GetMaxEntry(); ++id)
    {
        ItemPrototype const* pProto = sItemStorage.LookupEntry<ItemPrototype>(id);
        if (!pProto)
        {
            continue;
        }

        if (pProto->StartQuest == entry)
        {
            PSendSysMessage(LANG_COMMAND_QUEST_STARTFROMITEM, entry, pProto->ItemId);
            SetSentErrorMessage(true);
            return false;
        }
    }

    if (player->CanAddQuest(pQuest, true))
    {
        player->AddQuest(pQuest, nullptr);

        if (player->CanCompleteQuest(entry))
        {
            player->CompleteQuest(entry);
        }
    }

    return true;
}

bool ChatHandler::HandleQuestRemoveCommand(char* args)
{
    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 entry;
    if (!ExtractUint32KeyFromLink(&args, "Hquest", entry))
    {
        return false;
    }

    Quest const* pQuest = sObjectMgr.GetQuestTemplate(entry);

    if (!pQuest)
    {
        PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
        SetSentErrorMessage(true);
        return false;
    }

    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 quest = player->GetQuestSlotQuestId(slot);
        if (quest == entry)
        {
            player->SetQuestSlot(slot, 0);

            player->TakeQuestSourceItem(quest, false);
        }
    }

    player->SetQuestStatus(entry, QUEST_STATUS_NONE);

    player->getQuestStatusMap()[entry].m_rewarded = false;

    SendSysMessage(LANG_COMMAND_QUEST_REMOVED);
    return true;
}

bool ChatHandler::HandleQuestCompleteCommand(char* args)
{
    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 entry;
    if (!ExtractUint32KeyFromLink(&args, "Hquest", entry))
    {
        return false;
    }

    Quest const* pQuest = sObjectMgr.GetQuestTemplate(entry);

    if (!pQuest || player->GetQuestStatus(entry) == QUEST_STATUS_NONE)
    {
        PSendSysMessage(LANG_COMMAND_QUEST_NOTFOUND, entry);
        SetSentErrorMessage(true);
        return false;
    }

    for (uint8 x = 0; x < QUEST_ITEM_OBJECTIVES_COUNT; ++x)
    {
        uint32 id = pQuest->ReqItemId[x];
        uint32 count = pQuest->ReqItemCount[x];
        if (!id || !count)
        {
            continue;
        }

        uint32 curItemCount = player->GetItemCount(id, true);

        ItemPosCountVec dest;
        uint8 msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, id, count - curItemCount);
        if (msg == EQUIP_ERR_OK)
        {
            Item* item = player->StoreNewItem(dest, id, true);
            player->SendNewItem(item, count - curItemCount, true, false);
        }
    }

    for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        int32 creature = pQuest->ReqCreatureOrGOId[i];
        uint32 creaturecount = pQuest->ReqCreatureOrGOCount[i];

        if (uint32 spell_id = pQuest->ReqSpell[i])
        {
            for (uint16 z = 0; z < creaturecount; ++z)
            {
                player->Journal().CastCredited(creature, 0, spell_id);
            }
        }
        else if (creature > 0)
        {
            if (CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(creature))
            {
                for (uint16 z = 0; z < creaturecount; ++z)
                {
                    player->Journal().CreatureKilled(cInfo, 0);
                }
            }
        }
        else if (creature < 0)
        {
            for (uint16 z = 0; z < creaturecount; ++z)
            {
                player->Journal().CastCredited(-creature, 0, 0);
            }
        }
    }

    if (uint32 repFaction = pQuest->GetRepObjectiveFaction())
    {
        uint32 repValue = pQuest->GetRepObjectiveValue();
        uint32 curRep = player->GetReputationMgr().GetReputation(repFaction);
        if (curRep < repValue)
        {
            if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(repFaction))
            {
                player->GetReputationMgr().SetReputation(factionEntry, repValue);
            }
        }
    }

    int32 ReqOrRewMoney = pQuest->GetRewOrReqMoney();
    if (ReqOrRewMoney < 0)
    {
        player->ModifyMoney(-ReqOrRewMoney);
    }

    if (sWorld.getConfig(CONFIG_BOOL_ENABLE_QUEST_TRACKER))
    {
        DEBUG_LOG("QUEST TRACKER: Quest Completed by GM.");
        static SqlStatementID CHAR_UPD_QUEST_TRACK_GM_COMPLETE;

        SqlStatement stmt = CharacterDatabase.CreateStatement(CHAR_UPD_QUEST_TRACK_GM_COMPLETE, "UPDATE `quest_tracker` SET `completed_by_gm` = 1 WHERE `id` = ? AND `character_guid` = ? ORDER BY `quest_accept_time` DESC LIMIT 1");
        stmt.addUInt32(pQuest->GetQuestId());
        stmt.addUInt32(player->GetGUIDLow());

        stmt.Execute();
    }

    player->CompleteQuest(entry, QUEST_STATUS_FORCE_COMPLETE);
    return true;
}
