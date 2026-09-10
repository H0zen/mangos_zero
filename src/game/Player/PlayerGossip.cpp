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

#include <string>
#include "Utilities/PackedValues.h"
#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
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
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>

void Player::PrepareGossipMenu(Occupant* pSource, uint32 menuId)
{
    PlayerMenu* pMenu = PlayerTalkClass;
    pMenu->ClearMenus();

    pMenu->GetGossipMenu().SetMenuId(menuId);

    GossipMenuItemsMapBounds pMenuItemBounds = sObjectMgr.GetGossipMenuItemsMapBounds(menuId);

    bool canSeeQuests = menuId == GetDefaultGossipMenuForSource(pSource);

    if (pMenuItemBounds.first == pMenuItemBounds.second && canSeeQuests)
    {
        pMenuItemBounds = sObjectMgr.GetGossipMenuItemsMapBounds(0);
    }

    for (GossipMenuItemsMap::const_iterator itr = pMenuItemBounds.first; itr != pMenuItemBounds.second; ++itr)
    {
        GossipMenuItems const& gossipMenu = itr->second;
        bool hasMenuItem = true;
        bool isGMSkipConditionCheck = false;

        if (gossipMenu.conditionId && !sObjectMgr.IsPlayerMeetToCondition(gossipMenu.conditionId, this, GetMap(), pSource, CONDITION_FROM_GOSSIP_OPTION))
        {
            if (isGameMaster())
            {
                isGMSkipConditionCheck = true;
            }
            else
            {
                if (gossipMenu.option_id == GOSSIP_OPTION_QUESTGIVER)
                {
                    canSeeQuests = false;
                }
                continue;
            }
        }

        if (IsCreature(pSource))
        {
            Creature* pCreature = (Creature*)pSource;

            uint32 npcflags = pCreature->GetUInt32Value(UNIT_NPC_FLAGS);

            if (!(gossipMenu.npc_option_npcflag & npcflags))
            {
                continue;
            }

            switch (gossipMenu.option_id)
            {
                case GOSSIP_OPTION_GOSSIP:
                    break;
                case GOSSIP_OPTION_QUESTGIVER:
                    hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_ARMORER:
                    hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_SPIRITHEALER:
                    if (!IsDead())
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_VENDOR:
                {
                    VendorItemData const* vItems = pCreature->GetVendorItems();
                    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();
                    if ((!vItems || vItems->Empty()) && (!tItems || tItems->Empty()))
                    {
                        sLog.outErrorDb("Creature %u (Entry: %u) have UNIT_NPC_FLAG_VENDOR but have empty trading item list.", pCreature->GetGUIDLow(), pCreature->GetEntry());
                        hasMenuItem = false;
                    }
                    break;
                }
                case GOSSIP_OPTION_TRAINER:
                    if (!pCreature->IsTrainerOf(this, false))
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_UNLEARNTALENTS:
                    if (!pCreature->CanTrainAndResetTalentsOf(this))
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_UNLEARNPETSKILLS:
                    if (!GetPet() || GetPet()->getPetType() != HUNTER_PET || GetPet()->m_spells.size() <= 1 || pCreature->GetCreatureInfo()->TrainerType != TRAINER_TYPE_PETS || pCreature->GetCreatureInfo()->TrainerClass != CLASS_HUNTER)
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_TAXIVENDOR:
                    if (GetSession()->SendLearnNewTaxiNode(pCreature))
                    {
                        return;
                    }
                    break;
                case GOSSIP_OPTION_BATTLEFIELD:
                    if (!pCreature->CanInteractWithBattleMaster(this, false))
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_STABLEPET:
                    if (getClass() != CLASS_HUNTER)
                    {
                        hasMenuItem = false;
                    }
                    break;
                case GOSSIP_OPTION_SPIRITGUIDE:
                case GOSSIP_OPTION_INNKEEPER:
                case GOSSIP_OPTION_BANKER:
                case GOSSIP_OPTION_PETITIONER:
                case GOSSIP_OPTION_TABARDDESIGNER:
                case GOSSIP_OPTION_AUCTIONEER:
                    break;
                default:
                    sLog.outErrorDb("Creature entry %u have unknown gossip option %u for menu %u", pCreature->GetEntry(), gossipMenu.option_id, gossipMenu.menu_id);
                    hasMenuItem = false;
                    break;
            }
        }
        else if (IsGameObject(pSource))
        {
            GameObject* pGo = (GameObject*)pSource;

            switch (gossipMenu.option_id)
            {
                case GOSSIP_OPTION_QUESTGIVER:
                    hasMenuItem = false;
                    break;
                case GOSSIP_OPTION_GOSSIP:
                    if (pGo->GetGoType() != GAMEOBJECT_TYPE_QUESTGIVER && pGo->GetGoType() != GAMEOBJECT_TYPE_GOOBER)
                    {
                        hasMenuItem = false;
                    }
                    break;
                default:
                    hasMenuItem = false;
                    break;
            }
        }

        if (hasMenuItem)
        {
            std::string strOptionText = gossipMenu.option_text;
            std::string strBoxText = gossipMenu.box_text;

            int loc_idx = GetSession()->GetSessionDbLocaleIndex();

            if (loc_idx >= 0)
            {
                uint32 idxEntry = MAKE_PAIR32(menuId, gossipMenu.id);

                if (GossipMenuItemsLocale const* no = sObjectMgr.GetGossipMenuItemsLocale(idxEntry))
                {
                    if (no->OptionText.size() > (size_t)loc_idx && !no->OptionText[loc_idx].empty())
                    {
                        strOptionText = no->OptionText[loc_idx];
                    }

                    if (no->BoxText.size() > (size_t)loc_idx && !no->BoxText[loc_idx].empty())
                    {
                        strBoxText = no->BoxText[loc_idx];
                    }
                }
            }

            if (isGMSkipConditionCheck)
            {
                strOptionText.append(" (");
                strOptionText.append(GetSession()->GetMangosString(LANG_GM_ON));
                strOptionText.append(")");
            }

            pMenu->GetGossipMenu().AddMenuItem(gossipMenu.option_icon, strOptionText, 0, gossipMenu.option_id, strBoxText, gossipMenu.box_coded);
            pMenu->GetGossipMenu().AddMenuItemData(gossipMenu.action_menu_id, gossipMenu.action_poi_id, gossipMenu.action_script_id);
        }
    }

    if (canSeeQuests)
    {
        PrepareQuestMenu(pSource->GetObjectGuid());
    }

}

void Player::SendPreparedGossip(Occupant* pSource)
{
    if (!pSource)
    {
        return;
    }

    if (IsCreature(pSource))
    {

        if (!static_cast<Creature*>(pSource)->HasNpcFlag(UNIT_NPC_FLAG_GOSSIP) && !PlayerTalkClass->GetQuestMenu().Empty())
        {
            SendPreparedQuest(pSource->GetObjectGuid());
            return;
        }
    }
    else if (IsGameObject(pSource))
    {

        if (!PlayerTalkClass->GetGossipMenu().GetMenuId() && !PlayerTalkClass->GetQuestMenu().Empty())
        {
            SendPreparedQuest(pSource->GetObjectGuid());
            return;
        }
    }

    uint32 textId = GetGossipTextId(pSource);

    if (uint32 menuId = PlayerTalkClass->GetGossipMenu().GetMenuId())
    {
        textId = GetGossipTextId(menuId, pSource);
    }

    PlayerTalkClass->SendGossipMenu(textId, pSource->GetObjectGuid());
}

void Player::OnGossipSelect(Occupant* pSource, uint32 gossipListId)
{
    GossipMenu& gossipmenu = PlayerTalkClass->GetGossipMenu();

    if (gossipListId >= gossipmenu.MenuItemCount())
    {
        return;
    }

    GossipMenuItem const&  menu_item = gossipmenu.GetItem(gossipListId);

    uint32 gossipOptionId = menu_item.m_gOptionId;
    ObjectGuid guid = pSource->GetObjectGuid();

    if (IsGameObject(pSource))
    {
        if (gossipOptionId > GOSSIP_OPTION_QUESTGIVER)
        {
            sLog.outError("Player guid %u request invalid gossip option for GameObject entry %u", GetGUIDLow(), pSource->GetEntry());
            return;
        }
    }

    GossipMenuItemData const* pMenuData = gossipmenu.GetItemData(gossipListId);
    GossipMenuItemData menuData = {};

    if (pMenuData)
    {
        menuData = *pMenuData;
    }

    switch (gossipOptionId)
    {
        case GOSSIP_OPTION_GOSSIP:
        {
            if (menuData.m_gAction_poi)
            {
                PlayerTalkClass->SendPointOfInterest(menuData.m_gAction_poi);
            }

            if (menuData.m_gAction_menu > 0)
            {
                PrepareGossipMenu(pSource, uint32(menuData.m_gAction_menu));
                SendPreparedGossip(pSource);
            }
            else if (menuData.m_gAction_menu < 0)
            {
                PlayerTalkClass->CloseGossip();
                m_journal.TalkCredited(pSource->GetEntry(), pSource->GetObjectGuid());
            }

            break;
        }
        case GOSSIP_OPTION_SPIRITHEALER:
            if (IsDead())
            {
                ((Creature*)pSource)->CastSpell(((Creature*)pSource), 17251, true, nullptr, nullptr, GetObjectGuid());
            }
            break;
        case GOSSIP_OPTION_QUESTGIVER:
            PrepareQuestMenu(guid);
            SendPreparedQuest(guid);
            break;
        case GOSSIP_OPTION_VENDOR:
        case GOSSIP_OPTION_ARMORER:
            GetSession()->SendListInventory(guid);
            break;
        case GOSSIP_OPTION_STABLEPET:
            GetSession()->SendStablePet(guid);
            break;
        case GOSSIP_OPTION_TRAINER:
            GetSession()->SendTrainerList(guid);
            break;
        case GOSSIP_OPTION_UNLEARNTALENTS:
            PlayerTalkClass->CloseGossip();
            SendTalentWipeConfirm(guid);
            break;
        case GOSSIP_OPTION_UNLEARNPETSKILLS:
            PlayerTalkClass->CloseGossip();
            SendPetSkillWipeConfirm();
            break;
        case GOSSIP_OPTION_TAXIVENDOR:
            GetSession()->SendTaxiMenu(((Creature*)pSource));
            break;
        case GOSSIP_OPTION_INNKEEPER:
            PlayerTalkClass->CloseGossip();
            SetBindPoint(guid);
            break;
        case GOSSIP_OPTION_BANKER:
            GetSession()->SendShowBank(guid);
            break;
        case GOSSIP_OPTION_PETITIONER:
            PlayerTalkClass->CloseGossip();
            GetSession()->SendPetitionShowList(guid);
            break;
        case GOSSIP_OPTION_TABARDDESIGNER:
            PlayerTalkClass->CloseGossip();
            GetSession()->SendTabardVendorActivate(guid);
            break;
        case GOSSIP_OPTION_AUCTIONEER:
            GetSession()->SendAuctionHello(((Creature*)pSource));
            break;
        case GOSSIP_OPTION_SPIRITGUIDE:
            PrepareGossipMenu(pSource);
            SendPreparedGossip(pSource);
            break;
        case GOSSIP_OPTION_BATTLEFIELD:
        {
            BattleGroundTypeId bgTypeId = sBattleGroundMgr.GetBattleMasterBG(pSource->GetEntry());

            if (bgTypeId == BATTLEGROUND_TYPE_NONE)
            {
                sLog.outError("a user (guid %u) requested battlegroundlist from a npc who is no battlemaster", GetGUIDLow());
                return;
            }

            GetSession()->SendBattlegGroundList(guid, bgTypeId);
            break;
        }
    }

    if (pMenuData && menuData.m_gAction_script)
    {
        if (IsCreature(pSource))
        {
            GetMap()->Scripts().Start(DBS_ON_GOSSIP, menuData.m_gAction_script, pSource, this, SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE);
        }
        else if (IsGameObject(pSource))
        {
            GetMap()->Scripts().Start(DBS_ON_GOSSIP, menuData.m_gAction_script, this, pSource, SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET);
        }
    }
}

uint32 Player::GetGossipTextId(Occupant* pSource)
{
    if (!pSource || !IsCreature(pSource))
    {
        return DEFAULT_GOSSIP_MESSAGE;
    }

    if (uint32 pos = sObjectMgr.GetNpcGossipTextId(((Creature*)pSource)->GetEntry()))
    {
        return pos;
    }

    return DEFAULT_GOSSIP_MESSAGE;
}

uint32 Player::GetGossipTextId(uint32 menuId, Occupant* pSource)
{
    uint32 textId = DEFAULT_GOSSIP_MESSAGE;

    if (!menuId)
    {
        return textId;
    }

    uint32 scriptId = 0;
    uint32 lastConditionId = 0;

    GossipMenusMapBounds pMenuBounds = sObjectMgr.GetGossipMenusMapBounds(menuId);
    for (GossipMenusMap::const_iterator itr = pMenuBounds.first; itr != pMenuBounds.second; ++itr)
    {
        GossipMenus const& gossipMenu = itr->second;

        if ((!gossipMenu.conditionId && !lastConditionId) ||
            (gossipMenu.conditionId > lastConditionId && sObjectMgr.IsPlayerMeetToCondition(gossipMenu.conditionId, this, GetMap(), pSource, CONDITION_FROM_GOSSIP_MENU)))
        {
            lastConditionId = gossipMenu.conditionId;
            textId = gossipMenu.text_id;
            scriptId = gossipMenu.script_id;
        }
    }

    if (scriptId)
    {
        GetMap()->Scripts().Start(DBS_ON_GOSSIP, scriptId, this, pSource, SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET);
    }

    return textId;
}

uint32 Player::GetDefaultGossipMenuForSource(Occupant* pSource)
{
    if (IsCreature(pSource))
    {
        return ((Creature*)pSource)->GetCreatureInfo()->GossipMenuId;
    }
    else if (IsGameObject(pSource))
    {
        return((GameObject*)pSource)->GetGOInfo()->GetGossipMenuId();
    }

    return 0;
}
