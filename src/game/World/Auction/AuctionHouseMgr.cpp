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
#include <sstream>
#include "AuctionHouseMgr.h"
#include "Database/DatabaseEnv.h"
#include "SQLStorages.h"
#include "DBCStores.h"
#include "ProgressBar.h"

#include "AccountMgr.h"
#include "Item.h"
#include "Language.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Mint.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Mail.h"

#include "Policies/Singleton.h"

#include <string>
#include <vector>

AuctionHouseMgr::AuctionHouseMgr()
{
}

AuctionHouseMgr::~AuctionHouseMgr()
{
    for (ItemMap::const_iterator itr = mAitems.begin(); itr != mAitems.end(); ++itr)
    {
        delete itr->second;
    }
}

AuctionHouseObject* AuctionHouseMgr::GetAuctionsMap(AuctionHouseEntry const* house)
{
    if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION))
    {
        return &mAuctions[AUCTION_HOUSE_NEUTRAL];
    }

    switch (GetAuctionHouseTeam(house))
    {
        case ALLIANCE: return &mAuctions[AUCTION_HOUSE_ALLIANCE];
        case HORDE:    return &mAuctions[AUCTION_HOUSE_HORDE];
        default:       return &mAuctions[AUCTION_HOUSE_NEUTRAL];
    }
}

uint32 AuctionHouseMgr::GetAuctionDeposit(AuctionHouseEntry const* entry, uint32 time, Item* pItem)
{
    float deposit = float(pItem->GetProto()->SellPrice * pItem->GetCount() * (time / MIN_AUCTION_TIME));

    deposit = deposit * entry->depositPercent / 100.0f;

    float min_deposit = float(sWorld.getConfig(CONFIG_UINT32_AUCTION_DEPOSIT_MIN));

    if (deposit < min_deposit)
    {
        deposit = min_deposit;
    }

    return uint32(deposit * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_DEPOSIT));
}

void AuctionHouseMgr::SendAuctionWonMail(AuctionEntry* auction)
{
    Item* pItem = GetAItem(auction->itemGuidLow);
    if (!pItem)
    {
        return;
    }

    ObjectGuid bidder_guid = MakeGuid(HIGHGUID_PLAYER, auction->bidder);
    Player* bidder = sObjectMgr.GetPlayer(bidder_guid);

    uint32 bidder_accId = 0;

    ObjectGuid ownerGuid = MakeGuid(HIGHGUID_PLAYER, auction->owner);

    if (sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
    {
        AccountTypes bidder_security;
        std::string bidder_name;
        if (bidder)
        {
            bidder_accId = bidder->GetSession()->GetAccountId();
            bidder_security = bidder->GetSession()->GetSecurity();
            bidder_name = bidder->GetName();
        }
        else
        {
            bidder_accId = sObjectMgr.GetPlayerAccountIdByGUID(bidder_guid);
            bidder_security = bidder_accId ? sAccountMgr.GetSecurity(bidder_accId) : SEC_PLAYER;

            if (bidder_security > SEC_PLAYER)
            {
                if (!sObjectMgr.GetPlayerNameByGUID(bidder_guid, bidder_name))
                {
                    bidder_name = sObjectMgr.GetMangosStringForDBCLocale(LANG_UNKNOWN);
                }
            }
        }

        if (bidder_security > SEC_PLAYER)
        {
            std::string owner_name;
            if (ownerGuid && !sObjectMgr.GetPlayerNameByGUID(ownerGuid, owner_name))
            {
                owner_name = sObjectMgr.GetMangosStringForDBCLocale(LANG_UNKNOWN);
            }

            uint32 owner_accid = sObjectMgr.GetPlayerAccountIdByGUID(ownerGuid);

            sLog.outCommand(bidder_accId, "GM %s (Account: %u) won item in auction (Entry: %u Count: %u) and pay money: %u. Original owner %s (Account: %u)",
                bidder_name.c_str(), bidder_accId, auction->itemTemplate, auction->itemCount, auction->bid, owner_name.c_str(), owner_accid);
        }
    }
    else if (!bidder)
    {
        bidder_accId = sObjectMgr.GetPlayerAccountIdByGUID(bidder_guid);
    }

    if (bidder || bidder_accId)
    {
        std::ostringstream msgAuctionWonSubject;
        msgAuctionWonSubject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_WON;

        std::ostringstream msgAuctionWonBody;
        msgAuctionWonBody.width(16);
        msgAuctionWonBody << std::right << std::hex << auction->owner;
        msgAuctionWonBody << std::dec << ":" << auction->bid << ":" << auction->buyout;
        DEBUG_LOG("AuctionWon body string : %s", msgAuctionWonBody.str().c_str());

        CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid`='%u'", auction->bidder, auction->itemGuidLow);

        if (bidder)
        {
            bidder->GetSession()->SendAuctionBidderNotification(auction, true);
        }

        RemoveAItem(auction->itemGuidLow);
        auction->itemGuidLow = 0;

        MailDraft(msgAuctionWonSubject.str(), msgAuctionWonBody.str())
            .AddItem(pItem)
            .SendMailTo(MailReceiver(bidder, bidder_guid), auction, MAIL_CHECK_MASK_COPIED);
    }

    else
    {
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", auction->itemGuidLow);
        RemoveAItem(auction->itemGuidLow);
        auction->itemGuidLow = 0;
        delete pItem;
    }
}

void AuctionHouseMgr::SendAuctionSuccessfulMail(AuctionEntry* auction)
{
    ObjectGuid owner_guid = MakeGuid(HIGHGUID_PLAYER, auction->owner);
    Player* owner = sObjectMgr.GetPlayer(owner_guid);

    uint32 owner_accId = 0;
    if (!owner)
    {
        owner_accId = sObjectMgr.GetPlayerAccountIdByGUID(owner_guid);
    }

    if (owner || owner_accId)
    {
        std::ostringstream msgAuctionSuccessfulSubject;
        msgAuctionSuccessfulSubject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_SUCCESSFUL;

        std::ostringstream auctionSuccessfulBody;
        uint32 auctionCut = auction->GetAuctionCut();

        auctionSuccessfulBody.width(16);
        auctionSuccessfulBody << std::right << std::hex << auction->bidder;
        auctionSuccessfulBody << std::dec << ":" << auction->bid << ":" << auction->buyout;
        auctionSuccessfulBody << ":" << auction->deposit << ":" << auctionCut;

        DEBUG_LOG("AuctionSuccessful body string : %s", auctionSuccessfulBody.str().c_str());

        uint32 profit = auction->bid + auction->deposit - auctionCut;

        if (owner)
        {

            owner->GetSession()->SendAuctionOwnerNotification(auction, true);
        }

        MailDraft(msgAuctionSuccessfulSubject.str(), auctionSuccessfulBody.str())
            .SetMoney(profit)
            .SendMailTo(MailReceiver(owner, owner_guid), auction, MAIL_CHECK_MASK_COPIED);
    }
}

void AuctionHouseMgr::SendAuctionExpiredMail(AuctionEntry* auction)
{

    Item* pItem = GetAItem(auction->itemGuidLow);
    if (!pItem)
    {
        sLog.outError("Auction item (GUID: %u) not found, and lost.", auction->itemGuidLow);
        return;
    }

    ObjectGuid owner_guid = MakeGuid(HIGHGUID_PLAYER, auction->owner);
    Player* owner = sObjectMgr.GetPlayer(owner_guid);

    uint32 owner_accId = 0;
    if (!owner)
    {
        owner_accId = sObjectMgr.GetPlayerAccountIdByGUID(owner_guid);
    }

    if (owner || owner_accId)
    {
        std::ostringstream subject;
        subject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_EXPIRED;

        if (owner)
        {
            owner->GetSession()->SendAuctionOwnerNotification(auction, false);
        }

        RemoveAItem(auction->itemGuidLow);
        auction->itemGuidLow = 0;

        MailDraft(subject.str(),"")
            .AddItem(pItem)
            .SendMailTo(MailReceiver(owner, owner_guid), auction, MAIL_CHECK_MASK_COPIED);
    }

    else
    {
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", auction->itemGuidLow);
        RemoveAItem(auction->itemGuidLow);
        auction->itemGuidLow = 0;
        delete pItem;
    }
}

void AuctionHouseMgr::LoadAuctionItems()
{

    QueryResult* result = CharacterDatabase.Query("SELECT `data`,`itemguid`,`item_template` FROM `auction` JOIN `item_instance` ON `itemguid` = `guid`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 auction items");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;

    Field* fields;
    do
    {
        bar.step();

        fields = result->Fetch();
        uint32 item_guid        = fields[1].GetUInt32();
        uint32 item_template    = fields[2].GetUInt32();

        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_template);

        if (!proto)
        {
            sLog.outError("AuctionHouseMgr::LoadAuctionItems: Unknown item (GUID: %u id: #%u) in auction, skipped.", item_guid, item_template);
            continue;
        }

        Item* item = NewItemOrBag(proto);

        if (!item->LoadFromDB(item_guid, fields))
        {
            delete item;
            continue;
        }
        AddAItem(item);

        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u auction items", count);
    sLog.outString();
}

void AuctionHouseMgr::LoadAuctions()
{
    QueryResult* result = CharacterDatabase.Query("SELECT COUNT(*) FROM `auction`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString();
        sLog.outString(">> Loaded 0 auctions. DB table `auction` is empty.");
        return;
    }

    Field* fields = result->Fetch();
    uint32 AuctionCount = fields[0].GetUInt32();
    delete result;

    if (!AuctionCount)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString();
        sLog.outString(">> Loaded 0 auctions. DB table `auction` is empty.");
        return;
    }

    result = CharacterDatabase.Query("SELECT `id`,`houseid`,`itemguid`,`item_template`,`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`buyguid`,`lastbid`,`startbid`,`deposit` FROM `auction`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString();
        sLog.outString(">> Loaded 0 auctions. DB table `auction` is empty.");
        return;
    }

    BarGoLink bar(AuctionCount);

    do
    {
        fields = result->Fetch();

        bar.step();

        AuctionEntry* auction = new AuctionEntry;
        auction->Id = fields[0].GetUInt32();
        uint32 houseid  = fields[1].GetUInt32();
        auction->itemGuidLow = fields[2].GetUInt32();
        auction->itemTemplate = fields[3].GetUInt32();
        auction->itemCount = fields[4].GetUInt32();
        auction->itemRandomPropertyId = fields[5].GetUInt32();
        auction->owner = fields[6].GetUInt32();
        auction->buyout = fields[7].GetUInt32();
        auction->expireTime = time_t(fields[8].GetUInt64());
        auction->bidder = fields[9].GetUInt32();
        auction->bid = fields[10].GetUInt32();
        auction->startbid = fields[11].GetUInt32();
        auction->deposit = fields[12].GetUInt32();

        auction->auctionHouseEntry = nullptr;

        Item* pItem = GetAItem(auction->itemGuidLow);
        if (!pItem)
        {
            auction->DeleteFromDB();
            sLog.outError("Auction %u has not a existing item : %u, deleted", auction->Id, auction->itemGuidLow);
            delete auction;
            continue;
        }

        if ((auction->itemTemplate != pItem->GetEntry()) ||
            (auction->itemCount != pItem->GetCount()) ||
            (auction->itemRandomPropertyId != pItem->GetItemRandomPropertyId()))
        {
            auction->itemTemplate = pItem->GetEntry();
            auction->itemCount    = pItem->GetCount();
            auction->itemRandomPropertyId = pItem->GetItemRandomPropertyId();

            CharacterDatabase.PExecute("UPDATE `auction` SET `item_template` = %u, "
                "`item_count` = %u, `item_randompropertyid` = %i WHERE "
                "`itemguid` = %u", auction->itemTemplate, auction->itemCount,
                auction->itemRandomPropertyId, auction->itemGuidLow);
        }

        auction->auctionHouseEntry = sAuctionHouseStore.LookupEntry(houseid);

        if (!auction->auctionHouseEntry)
        {

            auction->auctionHouseEntry = sAuctionHouseStore.LookupEntry(7);

            std::ostringstream msgAuctionCanceledOwner;
            msgAuctionCanceledOwner << auction->itemTemplate << ":"
                << auction->itemRandomPropertyId << ":" << AUCTION_CANCELED;

            if (auction->itemGuidLow)
            {
                RemoveAItem(auction->itemGuidLow);
                auction->itemGuidLow = 0;

                MailDraft(msgAuctionCanceledOwner.str(), "")
                    .AddItem(pItem)
                    .SendMailTo(MailReceiver(MakeGuid(HIGHGUID_PLAYER, auction->owner)),
                        auction, MAIL_CHECK_MASK_COPIED);
            }

            auction->DeleteFromDB();
            delete auction;

            continue;
        }

        GetAuctionsMap(auction->auctionHouseEntry)->AddAuction(auction);
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u auctions", AuctionCount);
    sLog.outString();
}

void AuctionHouseMgr::AddAItem(Item* it)
{
    MANGOS_ASSERT(it);
    MANGOS_ASSERT(mAitems.find(it->GetGUIDLow()) == mAitems.end());
    mAitems[it->GetGUIDLow()] = it;
}

bool AuctionHouseMgr::RemoveAItem(uint32 id)
{
    ItemMap::iterator i = mAitems.find(id);
    if (i == mAitems.end())
    {
        return false;
    }
    mAitems.erase(i);
    return true;
}

void AuctionHouseMgr::Update()
{
    for (int i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
    {
        mAuctions[i].Update();
    }
}

uint32 AuctionHouseMgr::GetAuctionHouseTeam(AuctionHouseEntry const* house)
{

    switch (house->houseId)
    {
        case 1: case 2: case 3:
            return ALLIANCE;
        case 4: case 5: case 6:
            return HORDE;
        case 7:
        default:
            return 0;
    }
}

AuctionHouseEntry const* AuctionHouseMgr::GetAuctionHouseEntry(Unit* unit)
{
    uint32 houseid = 1;

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION))
    {
        if (IsCreature(unit))
        {

            uint32 factionTemplateId = unit->getFaction();
            switch (factionTemplateId)
            {
                case   12: houseid = 1; break;
                case   29: houseid = 6; break;
                case   55: houseid = 2; break;
                case   68: houseid = 4; break;
                case   80: houseid = 3; break;
                case  104: houseid = 5; break;
                case  120: houseid = 7; break;
                case  474: houseid = 7; break;
                case  534: houseid = 2; break;
                case  855: houseid = 7; break;
                default:
                {
                    FactionTemplateEntry const* u_entry = sFactionTemplateStore.LookupEntry(factionTemplateId);
                    if (!u_entry)
                    {
                        houseid = 7;
                    }
                    else if (u_entry->FactionGroup & FACTION_MASK_ALLIANCE)
                    {
                        houseid = 1;
                    }
                    else if (u_entry->FactionGroup & FACTION_MASK_HORDE)
                    {
                        houseid = 6;
                    }
                    else
                    {
                        houseid = 7;
                    }
                    break;
                }
            }
        }
        else
        {
            Player* player = (Player*)unit;
            if (player->GetAuctionAccessMode() > 0)
            {
                houseid = 7;
            }
            else
            {
                switch (((Player*)unit)->GetTeam())
                {
                    case ALLIANCE: houseid = player->GetAuctionAccessMode() == 0 ? 1 : 6; break;
                    case HORDE:    houseid = player->GetAuctionAccessMode() == 0 ? 6 : 1; break;
                    default: break;
                }
            }
        }
    }

    return sAuctionHouseStore.LookupEntry(houseid);
}

void AuctionHouseObject::Update()
{
    time_t curTime = sWorld.GetGameTime();

    for (AuctionEntryMap::iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end();)
    {
        AuctionEntryMap::iterator old = itr++;
        if (curTime > old->second->expireTime)
        {

            if (old->second->bid)
            {
                old->second->AuctionBidWinning();
            }

            else
            {
                sAuctionMgr.SendAuctionExpiredMail(old->second);

                old->second->DeleteFromDB();
                sAuctionMgr.RemoveAItem(old->second->itemGuidLow);
                delete old->second;
                AuctionsMap.erase(old);
                continue;

            }
        }
    }
}

void AuctionHouseObject::BuildListBidderItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount)
{
    for (AuctionEntryMap::const_iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end(); ++itr)
    {
        AuctionEntry* Aentry = itr->second;
        if (Aentry->bidder == player->GetGUIDLow())
        {
            if (itr->second->BuildAuctionInfo(data))
            {
                ++count;
            }
            ++totalcount;
        }
    }
}

void AuctionHouseObject::BuildListOwnerItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount)
{
    for (AuctionEntryMap::const_iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end(); ++itr)
    {
        AuctionEntry* Aentry = itr->second;
        if (Aentry->owner == player->GetGUIDLow())
        {
            if (Aentry->BuildAuctionInfo(data))
            {
                ++count;
            }
            ++totalcount;
        }
    }
}

void AuctionHouseObject::BuildListAuctionItems(WorldPacket& data, Player* player,
    std::wstring const& wsearchedname, uint32 listfrom, uint32 levelmin, uint32 levelmax, uint32 usable,
    uint32 inventoryType, uint32 itemClass, uint32 itemSubClass, uint32 quality,
    uint32& count, uint32& totalcount)
{
    int loc_idx = player->GetSession()->GetSessionDbLocaleIndex();

    for (AuctionEntryMap::const_iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end(); ++itr)
    {
        AuctionEntry* Aentry = itr->second;
        Item* item = sAuctionMgr.GetAItem(Aentry->itemGuidLow);
        if (!item)
        {
            continue;
        }

        {
            ItemPrototype const* proto = item->GetProto();

            if (itemClass != 0xffffffff && proto->Class != itemClass)
            {
                continue;
            }

            if (itemSubClass != 0xffffffff && proto->SubClass != itemSubClass)
            {
                continue;
            }

            if (inventoryType != 0xffffffff && proto->InventoryType != inventoryType)
            {
                continue;
            }

            if (quality != 0xffffffff && proto->Quality < quality)
            {
                continue;
            }

            if (levelmin != 0x00 && (proto->RequiredLevel < levelmin || (levelmax != 0x00 && proto->RequiredLevel > levelmax)))
            {
                continue;
            }

            if (usable != 0x00)
            {
                if (player->CanUseItem(item) != EQUIP_ERR_OK)
                {
                    continue;
                }

                if (proto->Class == ITEM_CLASS_RECIPE)
                {
                    if (SpellEntry const* spell = sSpellStore.LookupEntry(proto->Spells[0].SpellId))
                    {
                        if (player->HasSpell(spell->EffectTriggerSpell[EFFECT_INDEX_0]))
                        {
                            continue;
                        }
                    }
                }
            }

            std::string name = proto->Name1;
            sObjectMgr.GetItemLocaleStrings(proto->ItemId, loc_idx, &name);

            if (!wsearchedname.empty() && !Utf8FitTo(name, wsearchedname))
            {
                continue;
            }

            if (count < 50 && totalcount >= listfrom)
            {
                ++count;
                Aentry->BuildAuctionInfo(data);
            }
        }

        ++totalcount;
    }
}

void AuctionHouseObject::BuildListForKind(uint8 kind, WorldPacket& data, Player* player,
    const std::wstring& wname, uint32 listfrom, uint32 levelmin, uint32 levelmax,
    uint32 usable, uint32 invType, uint32 itemClass, uint32 itemSubClass,
    uint32 quality, const std::vector<uint32>& clientOutbidIds,
    uint32& count, uint32& totalcount)
{
    switch (kind)
    {
        case 1:
            BuildListOwnerItems(data, player, count, totalcount);
            break;
        case 2:
        {

            for (size_t i = 0; i < clientOutbidIds.size(); ++i)
            {
                AuctionEntry* a = GetAuction(clientOutbidIds[i]);
                if (a && a->BuildAuctionInfo(data))
                {
                    ++totalcount;
                    ++count;
                }
            }
            BuildListBidderItems(data, player, count, totalcount);
            break;
        }
        case 0:
        default:
            BuildListAuctionItems(data, player, wname, listfrom, levelmin, levelmax,
                usable, invType, itemClass, itemSubClass, quality, count, totalcount);
            break;
    }
}

void AhAppendClientOutbidsForTest(const std::vector<uint32>& clientIds,
                                  std::vector<uint32>& outOrder)
{
    for (size_t i = 0; i < clientIds.size(); ++i)
    {
        outOrder.push_back(clientIds[i]);
    }
}

AuctionEntry* AuctionHouseObject::AddAuction(AuctionHouseEntry const* auctionHouseEntry, Item* newItem, uint32 etime, uint32 bid, uint32 buyout, uint32 deposit, Player* pl , bool ownTransaction )
{
    uint32 auction_time = uint32(etime * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_TIME));

    AuctionEntry* AH = new AuctionEntry;
    AH->Id = sMint.AuctionIds().Next();
    AH->itemGuidLow = GuidCounter(newItem->GetObjectGuid());
    AH->itemTemplate = newItem->GetEntry();
    AH->itemCount = newItem->GetCount();
    AH->itemRandomPropertyId = newItem->GetItemRandomPropertyId();
    AH->owner = pl ? pl->GetGUIDLow() : 0;
    AH->startbid = bid;
    AH->bidder = 0;
    AH->bid = 0;
    AH->buyout = buyout;
    AH->expireTime = time(nullptr) + auction_time;
    AH->deposit = deposit;
    AH->auctionHouseEntry = auctionHouseEntry;

    AddAuction(AH);

    sAuctionMgr.AddAItem(newItem);

    if (pl)
    {
        pl->MoveItemFromInventory(newItem->GetBagSlot(), newItem->GetSlot(), true);
    }

    if (ownTransaction)
    {
        CharacterDatabase.BeginTransaction();
    }

    if (pl)
    {
        newItem->DeleteFromInventoryDB();
    }

    newItem->SaveToDB();
    AH->SaveToDB();

    if (pl)
    {
        pl->SaveInventoryAndGoldToDB();
    }

    if (ownTransaction)
    {
        CharacterDatabase.CommitTransaction();
    }

    return AH;
}

AuctionEntry* AuctionHouseObject::AddAuctionByGuid(AuctionHouseEntry const* auctionHouseEntry, Item* newItem, uint32 etime, uint32 bid, uint32 buyout, uint32 lowguid)
{
    uint32 auction_time = uint32(etime * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_TIME));

    AuctionEntry* AH = new AuctionEntry;
    AH->Id = sMint.AuctionIds().Next();
    AH->itemGuidLow = GuidCounter(newItem->GetObjectGuid());
    AH->itemTemplate = newItem->GetEntry();
    AH->itemCount = newItem->GetCount();
    AH->itemRandomPropertyId = newItem->GetItemRandomPropertyId();
    AH->owner = lowguid;
    AH->startbid = bid;
    AH->bidder = 0;
    AH->bid = 0;
    AH->buyout = buyout;
    AH->expireTime = time(nullptr) + auction_time;
    AH->deposit = 0;
    AH->auctionHouseEntry = auctionHouseEntry;

    AddAuction(AH);

    sAuctionMgr.AddAItem(newItem);

    CharacterDatabase.BeginTransaction();

    newItem->SaveToDB();
    AH->SaveToDB();

    CharacterDatabase.CommitTransaction();

    return AH;
}

bool AuctionEntry::BuildAuctionInfo(WorldPacket& data) const
{
    Item* pItem = sAuctionMgr.GetAItem(itemGuidLow);
    if (!pItem)
    {
        sLog.outError("auction to item, that doesn't exist !!!!");
        return false;
    }
    data << uint32(Id);
    data << uint32(pItem->GetEntry());

    data << uint32(pItem->GetEnchantmentId(EnchantmentSlot(PERM_ENCHANTMENT_SLOT)));

    data << uint32(pItem->GetItemRandomPropertyId());
    data << uint32(pItem->GetItemSuffixFactor());
    data << uint32(pItem->GetCount());
    data << uint32(pItem->GetSpellCharges());
    data << MakeGuid(HIGHGUID_PLAYER, owner);
    data << uint32(startbid);
    data << uint32(bid ? GetAuctionOutBid() : 0);
    data << uint32(buyout);
    data << uint32((expireTime - time(nullptr))*IN_MILLISECONDS);
    data << MakeGuid(HIGHGUID_PLAYER, bidder);
    data << uint32(bid && startbid > bid ? startbid : bid);

    return true;
}

uint32 AuctionEntry::GetAuctionCut() const
{
    return uint32(auctionHouseEntry->cutPercent * bid * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_CUT) / 100.0f);
}

uint32 AuctionEntry::GetAuctionOutBid() const
{
    uint32 outbid = (bid / 100) * 5;
    if (!outbid)
    {
        outbid = 1;
    }
    return outbid;
}

void AuctionEntry::DeleteFromDB() const
{

    CharacterDatabase.PExecute("DELETE FROM `auction` WHERE `id` = '%u'", Id);
}

void AuctionEntry::SaveToDB() const
{

    CharacterDatabase.PExecute("INSERT INTO `auction` (`id`,`houseid`,`itemguid`,`item_template`,`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`buyguid`,`lastbid`,`startbid`,`deposit`) "
        "VALUES ('%u', '%u', '%u', '%u', '%u', '%i', '%u', '%u', '" UI64FMTD "', '%u', '%u', '%u', '%u')",
        Id, auctionHouseEntry->houseId, itemGuidLow, itemTemplate, itemCount, itemRandomPropertyId, owner, buyout, (uint64)expireTime, bidder, bid, startbid, deposit);
}

void AuctionEntry::AuctionBidWinning(Player* newbidder)
{
    sAuctionMgr.SendAuctionSuccessfulMail(this);
    sAuctionMgr.SendAuctionWonMail(this);

    sAuctionMgr.RemoveAItem(this->itemGuidLow);
    sAuctionMgr.GetAuctionsMap(this->auctionHouseEntry)->RemoveAuction(this->Id);

    CharacterDatabase.BeginTransaction();
    this->DeleteFromDB();
    if (newbidder)
    {
        newbidder->SaveInventoryAndGoldToDB();
    }
    CharacterDatabase.CommitTransaction();

    delete this;
}

bool AuctionEntry::UpdateBid(uint32 newbid, Player* newbidder )
{
    Player* auction_owner = owner ? sObjectMgr.GetPlayer(MakeGuid(HIGHGUID_PLAYER, owner)) : nullptr;

    if (buyout && newbid > buyout)
    {
        newbid = buyout;
    }

    if (newbidder && newbidder->GetGUIDLow() == bidder)
    {
        newbidder->ModifyMoney(-int32(newbid - bid));
    }
    else
    {
        if (newbidder)
        {
            newbidder->ModifyMoney(-int32(newbid));
        }

        if (bidder)
        {
            WorldSession::SendAuctionOutbiddedMail(this);
        }
    }

    bidder = newbidder ? newbidder->GetGUIDLow() : 0;
    bid = newbid;

    if ((newbid < buyout) || (buyout == 0))
    {

        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("UPDATE `auction` SET `buyguid` = '%u', `lastbid` = '%u' WHERE `id` = '%u'", bidder, bid, Id);
        if (newbidder)
        {
            newbidder->SaveInventoryAndGoldToDB();
        }
        CharacterDatabase.CommitTransaction();
        return true;
    }
    else
    {
        AuctionBidWinning(newbidder);
        return false;
    }
}
