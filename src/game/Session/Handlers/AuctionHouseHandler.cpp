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

#include "Common/Locales.h"
#include <sstream>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "AuctionAnswers.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "AuctionHouseMgr.h"
#include "Mail.h"
#include "Util.h"
#include "Chat.h"
#include "ReputationMgr.h"
#include "SQLStorages.h"
#include "DBCStores.h"
#include "SpellMgr.h"
#include <string>
#include <vector>
#include <map>
#include <list>
#include <ctime>

void auctions::AuctionHello(Player& who, WorldPacket& recv_data)
{
    ObjectGuid auctioneerGuid = 0;
    recv_data >> auctioneerGuid;

    Creature* unit = who.GetNPCIfCanInteractWith(auctioneerGuid, UNIT_NPC_FLAG_AUCTIONEER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleAuctionHelloOpcode - %s not found or you can't interact with him.", GuidString(auctioneerGuid).c_str());
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    who.GetSession()->SendAuctionHello(unit);
}

void WorldSession::SendAuctionHello(Unit* unit)
{

    AuctionHouseEntry const* ahEntry = AuctionHouseMgr::GetAuctionHouseEntry(unit);

    WorldPacket data(MSG_AUCTION_HELLO, 12);
    data << unit->GetObjectGuid();
    data << uint32(ahEntry->houseId);
    SendPacket(&data);
}

void WorldSession::SendAuctionCommandResult(AuctionEntry* auc, AuctionAction Action, AuctionError ErrorCode, InventoryResult invError, uint32 newOutbid )
{

    if (ErrorCode == AUCTION_ERR_HIGHER_BID)
    {
        WorldPacket data(SMSG_AUCTION_COMMAND_RESULT, 16);
        data << uint32(auc ? auc->Id : 0);
        data << uint32(Action);
        data << uint32(ErrorCode);
        data << MakeGuid(HIGHGUID_PLAYER, auc->bidder);
        data << uint32(auc->bid);
        data << uint32(auc->GetAuctionOutBid());
        SendPacket(&data);
        return;
    }

    uint32 outbid = newOutbid;
    if (ErrorCode == AUCTION_OK && Action == AUCTION_BID_PLACED && !outbid)
    {
        outbid = auc->GetAuctionOutBid();
    }

    SendAuctionCommandResultData(auc ? auc->Id : 0, Action, ErrorCode, invError, outbid);
}

void WorldSession::SendAuctionCommandResultData(uint32 aucId, AuctionAction Action, AuctionError ErrorCode, InventoryResult invError, uint32 newOutbid)
{
    WorldPacket data(SMSG_AUCTION_COMMAND_RESULT, 16);
    data << uint32(aucId);
    data << uint32(Action);
    data << uint32(ErrorCode);

    switch (ErrorCode)
    {
        case AUCTION_OK:
            if (Action == AUCTION_BID_PLACED)
            {
                data << uint32(newOutbid);
            }
            break;
        case AUCTION_ERR_INVENTORY:
            data << uint32(invError);
            break;
        default:
            break;
    }

    SendPacket(&data);
}

void WorldSession::SendAuctionBidderNotification(AuctionEntry* auction, bool won)
{
    SendAuctionBidderNotificationData(auction->GetHouseId(), auction->Id, auction->bidder,
                                      auction->bid, auction->GetAuctionOutBid(),
                                      auction->itemTemplate, auction->itemRandomPropertyId, won);
}

void WorldSession::SendAuctionBidderNotificationData(uint32 houseId, uint32 id, uint32 bidder, uint32 bid, uint32 outbid, uint32 itemTemplate, int32 itemRand, bool won)
{
    WorldPacket data(SMSG_AUCTION_BIDDER_NOTIFICATION, (8 * 4));
    data << uint32(houseId);
    data << uint32(id);
    data << MakeGuid(HIGHGUID_PLAYER, bidder);

    data << uint32(won ? 0 : bid);
    data << uint32(outbid);
    data << uint32(itemTemplate);
    data << int32(itemRand);

    SendPacket(&data);
}

void WorldSession::SendAuctionOwnerNotification(AuctionEntry* auction, bool sold)
{
    SendAuctionOwnerNotificationData(auction->GetHouseId(), auction->Id, auction->bid,
                                     auction->GetAuctionOutBid(), auction->bidder,
                                     auction->itemTemplate, auction->itemRandomPropertyId, sold);
}

void WorldSession::SendAuctionOwnerNotificationData(uint32 houseId, uint32 id, uint32 bid, uint32 outbid, uint32 bidderGuidLow, uint32 itemTemplate, int32 itemRand, bool sold)
{

    (void)houseId;

    WorldPacket data(SMSG_AUCTION_OWNER_NOTIFICATION, (7 * 4));
    data << uint32(id);
    data << uint32(bid);
    data << uint32(outbid);

    ObjectGuid bidder_guid = 0;
    if (!sold)
    {
        bidder_guid = MakeGuid(HIGHGUID_PLAYER, bidderGuidLow);
    }

    data << bidder_guid;
    data << uint32(itemTemplate);
    data << int32(itemRand);

    SendPacket(&data);
}

void WorldSession::SendAuctionRemovedNotification(AuctionEntry* auction)
{
    SendAuctionRemovedNotificationData(auction->Id, auction->itemTemplate, auction->itemRandomPropertyId);
}

void WorldSession::SendAuctionRemovedNotificationData(uint32 id, uint32 itemTemplate, int32 itemRand)
{
    WorldPacket data(SMSG_AUCTION_REMOVED_NOTIFICATION, (3 * 4));
    data << uint32(id);
    data << uint32(itemTemplate);
    data << uint32(itemRand);

    SendPacket(&data);
}

void WorldSession::SendAuctionOutbiddedMail(AuctionEntry* auction)
{
    ObjectGuid oldBidder_guid = MakeGuid(HIGHGUID_PLAYER, auction->bidder);
    Player* oldBidder = sObjectMgr.GetPlayer(oldBidder_guid);

    uint32 oldBidder_accId = 0;
    if (!oldBidder)
    {
        oldBidder_accId = sObjectMgr.GetPlayerAccountIdByGUID(oldBidder_guid);
    }

    if (oldBidder || oldBidder_accId)
    {
        std::ostringstream msgAuctionOutbiddedSubject;
        msgAuctionOutbiddedSubject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_OUTBIDDED;

        if (oldBidder)
        {
            oldBidder->GetSession()->SendAuctionBidderNotification(auction, false);
        }

        MailDraft(msgAuctionOutbiddedSubject.str(),"")
            .SetMoney(auction->bid)
            .SendMailTo(MailReceiver(oldBidder, oldBidder_guid), auction, MAIL_CHECK_MASK_COPIED);
    }
}

void WorldSession::SendAuctionCancelledToBidderMail(AuctionEntry* auction)
{
    ObjectGuid bidder_guid = MakeGuid(HIGHGUID_PLAYER, auction->bidder);
    Player* bidder = sObjectMgr.GetPlayer(bidder_guid);

    uint32 bidder_accId = 0;
    if (!bidder)
    {
        bidder_accId = sObjectMgr.GetPlayerAccountIdByGUID(bidder_guid);
    }

    if (bidder || bidder_accId)
    {
        std::ostringstream msgAuctionCancelledSubject;
        msgAuctionCancelledSubject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_CANCELLED_TO_BIDDER;

        if (bidder)
        {
            bidder->GetSession()->SendAuctionRemovedNotification(auction);
        }

        MailDraft(msgAuctionCancelledSubject.str(),"")
            .SetMoney(auction->bid)
            .SendMailTo(MailReceiver(bidder, bidder_guid), auction, MAIL_CHECK_MASK_COPIED);
    }
}

AuctionHouseEntry const* WorldSession::GetCheckedAuctionHouseForAuctioneer(ObjectGuid guid)
{
    Unit* auctioneer;

    if (guid == GetPlayer()->GetObjectGuid())
    {

        if (GetPlayer()->GetAuctionAccessMode() == 0 && !ChatHandler(GetPlayer()).FindCommand("auction"))
        {
            DEBUG_LOG("%s attempt open auction in cheating way.", GuidString(guid).c_str());
            return nullptr;
        }

        auctioneer = GetPlayer();
    }

    else
    {
        auctioneer = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_AUCTIONEER);
        if (!auctioneer)
        {
            DEBUG_LOG("Auctioneer %s accessed in cheating way.", GuidString(guid).c_str());
            return nullptr;
        }
    }

    return AuctionHouseMgr::GetAuctionHouseEntry(auctioneer);
}

void auctions::AuctionSellItem(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionSellItem");

    ObjectGuid auctioneerGuid = 0;
    ObjectGuid itemGuid = 0;
    uint32 etime, bid, buyout;

    recv_data >> auctioneerGuid;
    recv_data >> itemGuid;
    recv_data >> bid;
    recv_data >> buyout;
    recv_data >> etime;

    if (!bid || !etime)
    {
        return;
    }

    Player* pl = session.GetPlayer();

    AuctionHouseEntry const* auctionHouseEntry = session.GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    etime *= MINUTE;

    switch (etime)
    {
        case 1*MIN_AUCTION_TIME:
        case 4*MIN_AUCTION_TIME:
        case 12*MIN_AUCTION_TIME:
            break;
        default:
            return;
    }

    if (session.GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        session.GetPlayer()->RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    if (!itemGuid)
    {
        return;
    }

    Item* it = pl->GetItemByGuid(itemGuid);

    if (sAuctionMgr.GetAItem(GuidCounter(itemGuid)))
    {
        sLog.outError("AuctionError, %s is sending %s, but item is already in another auction", pl->GetGuidStr().c_str(), GuidString(itemGuid).c_str());
        session.SendAuctionCommandResult(nullptr, AUCTION_STARTED, AUCTION_ERR_INVENTORY, EQUIP_ERR_ITEM_NOT_FOUND);
        return;
    }

    if (!it)
    {
        session.SendAuctionCommandResult(nullptr, AUCTION_STARTED, AUCTION_ERR_INVENTORY, EQUIP_ERR_ITEM_NOT_FOUND);
        return;
    }

    if (!it->CanBeTraded())
    {
        session.SendAuctionCommandResult(nullptr, AUCTION_STARTED, AUCTION_ERR_INVENTORY, EQUIP_ERR_ITEM_NOT_FOUND);
        return;
    }

    if ((it->GetProto()->Flags & ITEM_FLAG_CONJURED) || it->GetUInt32Value(ITEM_FIELD_DURATION))
    {
        session.SendAuctionCommandResult(nullptr, AUCTION_STARTED, AUCTION_ERR_INVENTORY, EQUIP_ERR_ITEM_NOT_FOUND);
        return;
    }

    uint32 deposit = AuctionHouseMgr::GetAuctionDeposit(auctionHouseEntry, etime, it);
    if (pl->GetMoney() < deposit)
    {
        session.SendAuctionCommandResult(nullptr, AUCTION_STARTED, AUCTION_ERR_NOT_ENOUGH_MONEY);
        return;
    }

    if (session.GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
    {
        sLog.outCommand(session.GetAccountId(), "GM %s (Account: %u) create auction: %s (Entry: %u Count: %u)",
            session.GetPlayerName(), session.GetAccountId(), it->GetProto()->Name1, it->GetEntry(), it->GetCount());
    }

    char numTotalOwned = 0;
    for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionHouse->GetAuctions().begin(); itr != auctionHouse->GetAuctions().end(); ++itr)
    {
        AuctionEntry* Aentry = itr->second;
        if (Aentry->owner == pl->GetGUIDLow())
        {
            Item *pItem = sAuctionMgr.GetAItem(Aentry->itemGuidLow);
            if (!pItem)
            {
                sLog.outError("%s:%d:\tItem %id doesn't exist!", __FILE__, __LINE__, Aentry->itemGuidLow);
            }
            else
            {
                numTotalOwned++;
                if (numTotalOwned == 50)
                {

                    return session.SendAuctionCommandResult(nullptr, AUCTION_STARTED, AUCTION_ERR_DATABASE, EQUIP_ERR_OK);
                }
            }
        }
    }

    pl->ModifyMoney(-int32(deposit));

    AuctionEntry* AH = auctionHouse->AddAuction(auctionHouseEntry, it, etime, bid, buyout, deposit, pl);

    DETAIL_LOG("selling %s to auctioneer %s with initial bid %u with buyout %u and with time %u (in sec) in auctionhouse %u",
        GuidString(itemGuid).c_str(), GuidString(auctioneerGuid).c_str(), bid, buyout, etime, auctionHouseEntry->houseId);

    session.SendAuctionCommandResult(AH, AUCTION_STARTED, AUCTION_OK);

}

void auctions::AuctionPlaceBid(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionPlaceBid");

    ObjectGuid auctioneerGuid = 0;
    uint32 auctionId;
    uint32 price;
    recv_data >> auctioneerGuid;
    recv_data >> auctionId >> price;

    if (!auctionId || !price)
    {
        return;
    }

    AuctionHouseEntry const* auctionHouseEntry = who.GetSession()->GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    AuctionEntry* auction = auctionHouse->GetAuction(auctionId);
    Player* pl = &who;

    if (!auction || auction->owner == pl->GetGUIDLow())
    {

        who.GetSession()->SendAuctionCommandResult(nullptr, AUCTION_BID_PLACED, AUCTION_ERR_BID_OWN);
        return;
    }

    ObjectGuid ownerGuid = MakeGuid(HIGHGUID_PLAYER, auction->owner);

    Player* auction_owner = sObjectMgr.GetPlayer(ownerGuid);
    if (!auction_owner && sObjectMgr.GetPlayerAccountIdByGUID(ownerGuid) == pl->GetSession()->GetAccountId())
    {

        who.GetSession()->SendAuctionCommandResult(nullptr, AUCTION_BID_PLACED, AUCTION_ERR_BID_OWN);
        return;
    }

    if (price <= auction->bid)
    {

        who.GetSession()->SendAuctionCommandResult(auction, AUCTION_BID_PLACED, AUCTION_ERR_HIGHER_BID);
        return;
    }

    if ((price < auction->buyout || auction->buyout == 0) &&
        price < auction->bid + auction->GetAuctionOutBid())
    {

        who.GetSession()->SendAuctionCommandResult(auction, AUCTION_BID_PLACED, AUCTION_ERR_BID_INCREMENT);
        return;
    }

    if (price > pl->GetMoney())
    {

        return;
    }

    if (price < auction->startbid)
    {
        return;
    }

    uint32 newOutbid = (price / 100) * 5;
    if (!newOutbid)
    {
        newOutbid = 1;
    }

    who.GetSession()->SendAuctionCommandResult(auction, AUCTION_BID_PLACED, AUCTION_OK, EQUIP_ERR_OK, newOutbid);

    auction->UpdateBid(price, pl);
}

void auctions::AuctionRemoveItem(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionRemoveItem");

    ObjectGuid auctioneerGuid = 0;
    uint32 auctionId;
    recv_data >> auctioneerGuid;
    recv_data >> auctionId;

    AuctionHouseEntry const* auctionHouseEntry = who.GetSession()->GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    AuctionEntry* auction = auctionHouse->GetAuction(auctionId);
    Player* pl = &who;

    if (!auction || auction->owner != pl->GetGUIDLow())
    {
        who.GetSession()->SendAuctionCommandResult(nullptr, AUCTION_REMOVED, AUCTION_ERR_DATABASE);
        sLog.outError("CHEATER : %u, he tried to cancel auction (id: %u) of another player, or auction is nullptr", pl->GetGUIDLow(), auctionId);
        return;
    }

    Item* pItem = sAuctionMgr.GetAItem(auction->itemGuidLow);
    if (!pItem)
    {
        sLog.outError("Auction id: %u has nonexistent item (item guid : %u)!!!", auction->Id, auction->itemGuidLow);
        who.GetSession()->SendAuctionCommandResult(nullptr, AUCTION_REMOVED, AUCTION_ERR_INVENTORY, EQUIP_ERR_ITEM_NOT_FOUND);
        return;
    }

    uint32 cutDebited = 0;

    CharacterDatabase.BeginTransaction();

    if (auction->bid)
    {
        uint32 auctionCut = auction->GetAuctionCut();
        if (pl->GetMoney() < auctionCut)
        {
            CharacterDatabase.RollbackTransaction();
            return;
        }

        if (auction->bidder)
        {
            who.GetSession()->SendAuctionCancelledToBidderMail(auction);
        }

        pl->ModifyMoney(-int32(auctionCut));
        cutDebited = auctionCut;
    }

    std::ostringstream msgAuctionCanceledOwner;
    msgAuctionCanceledOwner << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_CANCELED;

    MailDraft(msgAuctionCanceledOwner.str(),"")
        .AddItem(pItem)
        .SendMailTo(pl, auction, MAIL_CHECK_MASK_COPIED);

    auction->DeleteFromDB();
    pl->SaveInventoryAndGoldToDB();

    if (!CharacterDatabase.CommitTransactionChecked())
    {

        pl->ModifyMoney(int32(cutDebited));
        who.GetSession()->SendAuctionCommandResult(auction, AUCTION_REMOVED, AUCTION_ERR_DATABASE);
        sLog.outError("AH: cancel txn rolled back for auction %u; cut restored", auction->Id);
        return;
    }

    who.GetSession()->SendAuctionCommandResult(auction, AUCTION_REMOVED, AUCTION_OK);
    sAuctionMgr.RemoveAItem(auction->itemGuidLow);
    auctionHouse->RemoveAuction(auction->Id);

    delete auction;
}

void auctions::AuctionListBidderItems(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionListBidderItems");

    ObjectGuid auctioneerGuid = 0;
    uint32 listfrom;
    uint32 outbiddedCount;

    recv_data >> auctioneerGuid;
    recv_data >> listfrom;
    recv_data >> outbiddedCount;
    if (recv_data.size() != (16 + outbiddedCount * 4))
    {
        sLog.outError("Client sent bad opcode!!! with count: %u and size : %u (must be: %u)", outbiddedCount, (uint32)recv_data.size(), (16 + outbiddedCount * 4));
        outbiddedCount = 0;
    }

    AuctionHouseEntry const* auctionHouseEntry = who.GetSession()->GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    Player* pl = &who;

    std::vector<uint32> clientOutbidIds;
    while (outbiddedCount > 0)
    {
        --outbiddedCount;
        uint32 outbiddedAuctionId;
        recv_data >> outbiddedAuctionId;
        clientOutbidIds.push_back(outbiddedAuctionId);
    }

    WorldPacket data(SMSG_AUCTION_BIDDER_LIST_RESULT, (4 + 4 + 4));
    data << uint32(0);
    uint32 count = 0;
    uint32 totalcount = 0;
    for (size_t i = 0; i < clientOutbidIds.size(); ++i)
    {
        AuctionEntry* auction = auctionHouse->GetAuction(clientOutbidIds[i]);
        if (auction && auction->BuildAuctionInfo(data))
        {
            ++totalcount;
            ++count;
        }
    }

    auctionHouse->BuildListBidderItems(data, pl, count, totalcount);
    data.put<uint32>(0, count);
    data << uint32(totalcount);
    who.GetSession()->SendPacket(&data);
}

void auctions::AuctionListOwnerItems(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionListOwnerItems");

    ObjectGuid auctioneerGuid = 0;
    uint32 listfrom;

    recv_data >> auctioneerGuid;
    recv_data >> listfrom;

    AuctionHouseEntry const* auctionHouseEntry = who.GetSession()->GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    WorldPacket data(SMSG_AUCTION_OWNER_LIST_RESULT, (4 + 4));
    data << (uint32) 0;

    uint32 count = 0;
    uint32 totalcount = 0;

    auctionHouse->BuildListOwnerItems(data, &who, count, totalcount);
    data.put<uint32>(0, count);
    data << uint32(totalcount);
    who.GetSession()->SendPacket(&data);
}

void auctions::AuctionListItems(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionListItems");

    ObjectGuid auctioneerGuid = 0;
    std::string searchedname;
    uint8 levelmin, levelmax, usable;
    uint32 listfrom, auctionSlotID, auctionMainCategory, auctionSubCategory, quality;

    recv_data >> auctioneerGuid;
    recv_data >> listfrom;
    recv_data >> searchedname;

    recv_data >> levelmin >> levelmax;
    recv_data >> auctionSlotID >> auctionMainCategory >> auctionSubCategory >> quality;
    recv_data >> usable;

    AuctionHouseEntry const* auctionHouseEntry = who.GetSession()->GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    WorldPacket data(SMSG_AUCTION_LIST_RESULT, (4 + 4));
    uint32 count = 0;
    uint32 totalcount = 0;
    data << uint32(0);

    std::wstring wsearchedname;
    if (!Utf8toWStr(searchedname, wsearchedname))
    {

        data.put<uint32>(0, count);
        data << uint32(totalcount);
        who.GetSession()->SendPacket(&data);
        return;
    }

    wstrToLower(wsearchedname);

    auctionHouse->BuildListAuctionItems(data, &who,
        wsearchedname, listfrom, levelmin, levelmax, usable,
        auctionSlotID, auctionMainCategory, auctionSubCategory, quality,
        count, totalcount);

    data.put<uint32>(0, count);
    data << uint32(totalcount);
    who.GetSession()->SendPacket(&data);
}
