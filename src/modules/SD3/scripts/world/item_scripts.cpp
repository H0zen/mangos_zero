/**
 * ScriptDev3 is an extension for mangos providing enhanced features for
 * area triggers, creatures, game objects, instances, items, and spells beyond
 * the default database scripting in mangos.
 *
 * Copyright (C) 2006-2013 ScriptDev2 <http://www.scriptdev2.com/>
 * Copyright (C) 2014-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * ScriptData
 * SDName:      Item_Scripts
 * SD%Complete: 100
 * SDComment: Items for a range of different items. See content below (in script)
 * SDCategory:  Items
 * EndScriptData
 */

/**
 * ContentData
 * item_gossip_test                    Demonstrates every gossip icon on an item gossip menu
 * EndContentData
 */

#include "precompiled.h"

/**
 * In order to work properly you must use a false spell to trigger the OnUse Method => you can assign spell 18282 to the object bound to this script.
 */
struct item_gossip_test : public ItemScript
{
    item_gossip_test() : ItemScript("item_gossip_test") {}

    bool OnUse(Player* pPlayer, Item* pItem, const SpellCastTargets& pTargets) override
    {
        // Logging
        sLog.outString("Item [item_gossip_test] %s was used by %s ! ", pItem->GetProto()->Name1, pPlayer->GetName());

        pPlayer->PlayerTalkClass->ClearMenus();

        uint32 gossip_menu_id = 1;

        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_INTERACT_1, "Option with GOSSIP_ICON_INTERACT_1", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT, " Option with GOSSIP_ICON_CHAT", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_VENDOR, " Option with GOSSIP_ICON_VENDOR", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_TAXI, " Option with GOSSIP_ICON_TAXI", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_TRAINER, " Option with GOSSIP_ICON_TRAINER", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_INTERACT_1, " Option with GOSSIP_ICON_INTERACT_1", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_INTERACT_2, " Option with GOSSIP_ICON_INTERACT_2", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_MONEY_BAG, " Option with GOSSIP_ICON_MONEY_BAG", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_TALK, "Option with GOSSIP_ICON_TALK", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_TABARD, "Option with GOSSIP_ICON_TABARD", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_BATTLE, "Option with GOSSIP_ICON_BATTLE", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_DOT, "Option with GOSSIP_ICON_DOT", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT_11, "Option with GOSSIP_ICON_CHAT_11", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT_12, "Option with GOSSIP_ICON_CHAT_12", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_DOT_13, "Option with GOSSIP_ICON_DOT_13", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
        // Max gossip options seem to be 15
        /**  pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_DOT_14, "Option with GOSSIP_ICON_DOT_14", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
         * pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_DOT_15, "Option with GOSSIP_ICON_DOT_15", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
         * pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_DOT_16, "Option with GOSSIP_ICON_DOT_16", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
         * pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_DOT_17, "Option with GOSSIP_ICON_DOT_17", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
         * pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_DOT_18, "Option with GOSSIP_ICON_DOT_18", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
         * pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_DOT_19, "Option with GOSSIP_ICON_DOT_19", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
         * pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_DOT_20, "Option with GOSSIP_ICON_DOT_20", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
         */

        // pPlayer->PlayerTalkClass->GetGossipMenu().AddMenuItem(GOSSIP_ICON_INTERACT_1, "Code test option", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2, true);

        pPlayer->SEND_GOSSIP_MENU(gossip_menu_id, pItem->GetObjectGuid());

        return false; // return FALSE because item would be stuck (spell is not processed) !
    }

    bool OnGossipSelect(Player* pPlayer, Item* pItem, uint32 uiSender, uint32 uiAction)
    {
        switch (uiAction)
        {
            case GOSSIP_ACTION_INFO_DEF + 1:
            {
                uint32 playerGUID = pPlayer->GetGUIDLow();

                sLog.outString("Item [item_gossip_test] %s was used by %s and choose action %u ", pItem->GetProto()->Name1, pPlayer->GetName(), uiAction);

                pPlayer->CLOSE_GOSSIP_MENU();

                break;
            }
        }
        return true;
    }

    bool OnGossipSelectWithCode(Player* pPlayer, Item* pItem, uint32 uiSender, uint32 uiAction, const char* code)
    {
        sLog.outString("Item [item_gossip_test] %s was used by %s and choose action %u with code %s ", pItem->GetProto()->Name1, pPlayer->GetName(), uiAction, code);

        switch (uiAction)
        {
            case GOSSIP_ACTION_INFO_DEF + 2:
            {
                sLog.outString("OK GOSSIP_ACTION_INFO_DEF + 2 !");

                break;
            }
        }

        pPlayer->CLOSE_GOSSIP_MENU();
        return true;
    }
};

void AddSC_item_scripts()
{
    Script* s;
    s = new item_gossip_test();
    s->RegisterSelf();
}
