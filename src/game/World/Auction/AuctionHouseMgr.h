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

#include <unordered_map>
#include <utility>
#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include <ctime>
#include <map>
#include "Policies/Singleton.h"
#include "DBCStructure.h"

#include <string>
#include <vector>

class Item;
class Player;
class Unit;
class WorldPacket;

#define MIN_AUCTION_TIME (2*HOUR)

enum AuctionError
{
    AUCTION_OK                          = 0,
    AUCTION_ERR_INVENTORY               = 1,
    AUCTION_ERR_DATABASE                = 2,
    AUCTION_ERR_NOT_ENOUGH_MONEY        = 3,
    AUCTION_ERR_ITEM_NOT_FOUND          = 4,
    AUCTION_ERR_HIGHER_BID              = 5,
    AUCTION_ERR_BID_INCREMENT           = 7,
    AUCTION_ERR_BID_OWN                 = 10,
    AUCTION_ERR_RESTRICTED_ACCOUNT      = 13
};

enum AuctionAction
{
    AUCTION_STARTED     = 0,
    AUCTION_REMOVED     = 1,
    AUCTION_BID_PLACED  = 2
};

struct AuctionEntry
{
    uint32 Id;
    uint32 itemGuidLow;
    uint32 itemTemplate;
    uint32 itemCount;
    int32 itemRandomPropertyId;
    uint32 owner;
    uint32 startbid;
    uint32 bid;
    uint32 buyout;
    time_t expireTime;
    uint32 bidder;
    uint32 deposit;
    AuctionHouseEntry const* auctionHouseEntry;

    uint32 GetHouseId() const { return auctionHouseEntry->houseId; }
    uint32 GetHouseFaction() const { return auctionHouseEntry->faction; }
    uint32 GetAuctionCut() const;
    uint32 GetAuctionOutBid() const;
    bool BuildAuctionInfo(WorldPacket& data) const;
    void DeleteFromDB() const;
    void SaveToDB() const;
    void AuctionBidWinning(Player* bidder = nullptr);
    bool UpdateBid(uint32 newbid, Player* newbidder = nullptr);
};

class AuctionHouseObject
{
    public:
        AuctionHouseObject() {}
        ~AuctionHouseObject()
        {
            for (AuctionEntryMap::const_iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end(); ++itr)
            {
                delete itr->second;
            }
        }

        typedef std::map<uint32, AuctionEntry*> AuctionEntryMap;
        typedef std::pair<AuctionEntryMap::const_iterator, AuctionEntryMap::const_iterator> AuctionEntryMapBounds;

        uint32 GetCount()
        {
            return AuctionsMap.size();
        }

        AuctionEntryMap const& GetAuctions() const { return AuctionsMap; }
        AuctionEntryMapBounds GetAuctionsBounds() const {return AuctionEntryMapBounds(AuctionsMap.begin(), AuctionsMap.end()); }

        void AddAuction(AuctionEntry* ah)
        {
            MANGOS_ASSERT(ah);
            AuctionsMap[ah->Id] = ah;
        }

        AuctionEntry* GetAuction(uint32 id) const
        {
            AuctionEntryMap::const_iterator itr = AuctionsMap.find(id);
            return itr != AuctionsMap.end() ? itr->second : nullptr;
        }

        bool RemoveAuction(uint32 id)
        {
            return AuctionsMap.erase(id);
        }

        void Update();

        void BuildListBidderItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount);
        void BuildListOwnerItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount);
        void BuildListAuctionItems(WorldPacket& data, Player* player,
            std::wstring const& searchedname, uint32 listfrom, uint32 levelmin, uint32 levelmax, uint32 usable,
            uint32 inventoryType, uint32 itemClass, uint32 itemSubClass, uint32 quality,
            uint32& count, uint32& totalcount);

        void BuildListForKind(uint8 kind, WorldPacket& data, Player* player,
            const std::wstring& wname, uint32 listfrom, uint32 levelmin, uint32 levelmax,
            uint32 usable, uint32 invType, uint32 itemClass, uint32 itemSubClass,
            uint32 quality, const std::vector<uint32>& clientOutbidIds,
            uint32& count, uint32& totalcount);

        AuctionEntry* AddAuction(AuctionHouseEntry const* auctionHouseEntry, Item* newItem, uint32 etime, uint32 bid, uint32 buyout = 0, uint32 deposit = 0, Player* pl = nullptr, bool ownTransaction = true);
        AuctionEntry* AddAuctionByGuid(AuctionHouseEntry const* auctionHouseEntry, Item* newItem, uint32 etime, uint32 bid, uint32 buyout, uint32 lowguid);
    private:
        AuctionEntryMap AuctionsMap;
};

enum AuctionHouseType
{
    AUCTION_HOUSE_ALLIANCE  = 0,
    AUCTION_HOUSE_HORDE     = 1,
    AUCTION_HOUSE_NEUTRAL   = 2
};

#define MAX_AUCTION_HOUSE_TYPE 3

class AuctionHouseMgr
{
    public:
        AuctionHouseMgr();
        ~AuctionHouseMgr();

        typedef std::unordered_map<uint32, Item*> ItemMap;

        AuctionHouseObject* GetAuctionsMap(AuctionHouseType houseType) { return &mAuctions[houseType]; }
        AuctionHouseObject* GetAuctionsMap(AuctionHouseEntry const* house);

        Item* GetAItem(uint32 id)
        {
            ItemMap::const_iterator itr = mAitems.find(id);
            if (itr != mAitems.end())
            {
                return itr->second;
            }
            return nullptr;
        }

        void SendAuctionWonMail(AuctionEntry* auction);
        void SendAuctionSuccessfulMail(AuctionEntry* auction);
        void SendAuctionExpiredMail(AuctionEntry* auction);

        static uint32 GetAuctionDeposit(AuctionHouseEntry const* entry, uint32 time, Item* pItem);

        static uint32 GetAuctionHouseTeam(AuctionHouseEntry const* house);
        static AuctionHouseEntry const* GetAuctionHouseEntry(Unit* unit);

    public:

        void LoadAuctionItems();
        void LoadAuctions();

        void AddAItem(Item* it);
        bool RemoveAItem(uint32 id);

        void Update();

    private:
        AuctionHouseObject  mAuctions[MAX_AUCTION_HOUSE_TYPE];

        ItemMap             mAitems;
};

#define sAuctionMgr MaNGOS::Singleton<AuctionHouseMgr>::Instance()

void AhAppendClientOutbidsForTest(const std::vector<uint32>& clientIds,
                                  std::vector<uint32>& outOrder);
