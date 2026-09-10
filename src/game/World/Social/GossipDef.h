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
#include <string>
#include <vector>
#include "QuestDef.h"
#include "NPCHandler.h"
#include "ObjectGuid.h"

class WorldSession;

#define GOSSIP_MAX_MENU_ITEMS       32
#define DEFAULT_GOSSIP_MESSAGE      0xffffff

enum Gossip_Option
{
    GOSSIP_OPTION_NONE              = 0,
    GOSSIP_OPTION_GOSSIP            = 1,
    GOSSIP_OPTION_QUESTGIVER        = 2,
    GOSSIP_OPTION_VENDOR            = 3,
    GOSSIP_OPTION_TAXIVENDOR        = 4,
    GOSSIP_OPTION_TRAINER           = 5,
    GOSSIP_OPTION_SPIRITHEALER      = 6,
    GOSSIP_OPTION_SPIRITGUIDE       = 7,
    GOSSIP_OPTION_INNKEEPER         = 8,
    GOSSIP_OPTION_BANKER            = 9,
    GOSSIP_OPTION_PETITIONER        = 10,
    GOSSIP_OPTION_TABARDDESIGNER    = 11,
    GOSSIP_OPTION_BATTLEFIELD       = 12,
    GOSSIP_OPTION_AUCTIONEER        = 13,
    GOSSIP_OPTION_STABLEPET         = 14,
    GOSSIP_OPTION_ARMORER           = 15,
    GOSSIP_OPTION_UNLEARNTALENTS    = 16,
    GOSSIP_OPTION_UNLEARNPETSKILLS  = 17,
    GOSSIP_OPTION_MAX
};

enum GossipOptionIcon
{
    GOSSIP_ICON_CHAT                = 0,
    GOSSIP_ICON_VENDOR              = 1,
    GOSSIP_ICON_TAXI                = 2,
    GOSSIP_ICON_TRAINER             = 3,
    GOSSIP_ICON_INTERACT_1          = 4,
    GOSSIP_ICON_INTERACT_2          = 5,
    GOSSIP_ICON_MONEY_BAG           = 6,
    GOSSIP_ICON_TALK                = 7,
    GOSSIP_ICON_TABARD              = 8,
    GOSSIP_ICON_BATTLE              = 9,
    GOSSIP_ICON_DOT                 = 10,
    GOSSIP_ICON_CHAT_11             = 11,
    GOSSIP_ICON_CHAT_12             = 12,
    GOSSIP_ICON_DOT_13              = 13,
    GOSSIP_ICON_DOT_14              = 14,
    GOSSIP_ICON_DOT_15              = 15,
    GOSSIP_ICON_DOT_16              = 16,
    GOSSIP_ICON_DOT_17              = 17,
    GOSSIP_ICON_DOT_18              = 18,
    GOSSIP_ICON_DOT_19              = 19,
    GOSSIP_ICON_DOT_20              = 20,
    GOSSIP_ICON_MAX
};

enum Poi_Icon
{
    ICON_POI_GREY_AV_MINE       =   0,
    ICON_POI_RED_AV_MINE        =   1,
    ICON_POI_BLUE_AV_MINE       =   2,
    ICON_POI_BWTOMB             =   3,
    ICON_POI_SMALL_HOUSE        =   4,
    ICON_POI_GREYTOWER          =   5,
    ICON_POI_REDFLAG            =   6,
    ICON_POI_TOMBSTONE          =   7,
    ICON_POI_BWTOWER            =   8,
    ICON_POI_REDTOWER           =   9,
    ICON_POI_BLUETOWER          =   10,
    ICON_POI_RWTOWER            =   11,
    ICON_POI_REDTOMB            =   12,
    ICON_POI_RWTOMB             =   13,
    ICON_POI_BLUETOMB           =   14,
    ICON_POI_BLANK              =   15,
    ICON_POI_16                 =   16,
    ICON_POI_17                 =   17,
    ICON_POI_18                 =   18,
    ICON_POI_19                 =   19,
    ICON_POI_20                 =   20,
    ICON_POI_GREYLOGS           =   21,
    ICON_POI_BWLOGS             =   22,
    ICON_POI_BLUELOGS           =   23,
    ICON_POI_RWLOGS             =   24,
    ICON_POI_REDLOGS            =   25,
    ICON_POI_26                 =   26,
    ICON_POI_27                 =   27,
    ICON_POI_28                 =   28,
    ICON_POI_29                 =   29,
    ICON_POI_30                 =   30,
    ICON_POI_GREYHOUSE          =   31,
    ICON_POI_BWHOUSE            =   32,
    ICON_POI_BLUEHOUSE          =   33,
    ICON_POI_RWHOUSE            =   34,
    ICON_POI_REDHOUSE           =   35,
    ICON_POI_GREYHORSE          =   36,
    ICON_POI_BWHORSE            =   37,
    ICON_POI_BLUEHORSE          =   38,
    ICON_POI_RWHORSE            =   39,
    ICON_POI_REDHORSE           =   40
};

struct GossipMenuItem
{
    uint8       m_gIcon;
    bool        m_gCoded;
    std::string m_gMessage;
    uint32      m_gSender;
    uint32      m_gOptionId;
    std::string m_gBoxMessage;
    uint32      m_gBoxMoney;
};

typedef std::vector<GossipMenuItem> GossipMenuItemList;

struct GossipMenuItemData
{
    int32  m_gAction_menu;
    uint32 m_gAction_poi;
    uint32 m_gAction_script;
};

typedef std::vector<GossipMenuItemData> GossipMenuItemDataList;

struct QuestMenuItem
{
    uint32      m_qId;
    uint8       m_qIcon;
};

typedef std::vector<QuestMenuItem> QuestMenuItemList;

class GossipMenu
{
    public:
        explicit GossipMenu(WorldSession* session);
        ~GossipMenu();

        void AddMenuItem(uint8 Icon, const std::string& Message, bool Coded = false);
        void AddMenuItem(uint8 Icon, const std::string& Message, uint32 dtSender, uint32 dtAction, const std::string& BoxMessage, uint32 BoxMoney = 0,  bool Coded = false);

        void AddMenuItem(uint8 Icon, char const* Message, bool Coded = false);
        void AddMenuItem(uint8 Icon, char const* Message, uint32 dtSender, uint32 dtAction, bool Coded = false);
        void AddMenuItem(uint8 Icon, char const* Message, uint32 dtSender, uint32 dtAction, char const* BoxMessage, uint32 BoxMoney = 0, bool Coded =false);
        void AddMenuItem(uint8 Icon, int32 itemText, uint32 dtSender, uint32 dtAction, int32 boxText, bool Coded = false);

        void SetMenuId(uint32 menu_id) { m_gMenuId = menu_id; }
        uint32 GetMenuId() const { return m_gMenuId; }

        void AddMenuItemData(int32 action_menu, uint32 action_poi, uint32 action_script);

        unsigned int MenuItemCount() const
        {
            return static_cast<unsigned int>(m_gItems.size());
        }

        unsigned int MenuItemDataCount() const
        {
            return static_cast<unsigned int>(m_gItemsData.size());
        }

        bool Empty() const
        {
            return m_gItems.empty();
        }

        GossipMenuItem const& GetItem(unsigned int Id) const
        {
            return m_gItems.at(Id);
        }

        GossipMenuItemData const* GetItemData(unsigned int indexId) const
        {
            if (indexId >= m_gItemsData.size())
            {
                sLog.outError("GossipMenu::GetItemData> indexId is out of bounds!");
                return nullptr;
            }
            return &m_gItemsData.at(indexId);
        }

        uint32 MenuItemSender(unsigned int ItemId) const;
        uint32 MenuItemAction(unsigned int ItemId) const;
        bool MenuItemCoded(unsigned int ItemId) const;

        void ClearMenu();

        WorldSession* GetMenuSession() const { return m_session; }

    protected:
        GossipMenuItemList      m_gItems;
        GossipMenuItemDataList  m_gItemsData;

        uint32 m_gMenuId;

    private:
        WorldSession* m_session;
};

class QuestMenu
{
    public:
        QuestMenu();
        ~QuestMenu();

        void AddMenuItem(uint32 QuestId, uint8 Icon);
        void ClearMenu();

        uint8 MenuItemCount() const
        {
            return static_cast<uint8>(m_qItems.size());
        }

        bool Empty() const
        {
            return m_qItems.empty();
        }

        bool HasItem(uint32 questid) const;

        QuestMenuItem const& GetItem(uint16 Id) const
        {
            return m_qItems.at(Id);
        }

    protected:
        QuestMenuItemList m_qItems;
};

class PlayerMenu
{
    private:
        GossipMenu mGossipMenu;
        QuestMenu  mQuestMenu;

    public:
        explicit PlayerMenu(WorldSession* Session);
        ~PlayerMenu();

        GossipMenu& GetGossipMenu()
        {
            return mGossipMenu;
        }

        QuestMenu& GetQuestMenu()
        {
            return mQuestMenu;
        }

        WorldSession* GetMenuSession() const { return mGossipMenu.GetMenuSession(); }

        bool Empty() const { return mGossipMenu.Empty() && mQuestMenu.Empty(); }

        void ClearMenus();
        uint32 GossipOptionSender(unsigned int Selection) const;
        uint32 GossipOptionAction(unsigned int Selection) const;
        bool GossipOptionCoded(unsigned int Selection) const;

        void SendGossipMenu(uint32 titleTextId, ObjectGuid objectGuid);
        void CloseGossip() const;
        void SendPointOfInterest(float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, const char* locName) const;
        void SendPointOfInterest(uint32 poi_id) const;
        void SendTalking(uint32 textID) const;
        void SendTalking(char const* title, char const* text) const;

        void SendQuestGiverStatus(uint8 questStatus, ObjectGuid npcGUID) const;

        void SendQuestGiverQuestList(QEmote eEmote, const std::string& Title, ObjectGuid npcGUID);

        void SendQuestQueryResponse(Quest const* pQuest) const;
        void SendQuestGiverQuestDetails(Quest const* pQuest, ObjectGuid npcGUID, bool ActivateAccept) const;

        void SendQuestGiverOfferReward(Quest const* pQuest, ObjectGuid npcGUID, bool EnbleNext) const;
        void SendQuestGiverRequestItems(Quest const* pQuest, ObjectGuid npcGUID, bool Completable, bool CloseOnCancel) const;
};
