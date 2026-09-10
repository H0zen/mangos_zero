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

#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "TradeAnswers.h"
#include "World.h"
#include "PlayerRegistry.h"
#include "Log.h"
#include "Opcodes.h"
#include "Player.h"
#include "Item.h"
#include "Spell.h"
#include "SocialMgr.h"
#include "DBCStores.h"

void WorldSession::SendTradeStatus(const TradeStatusInfo& info)
{
    WorldPacket data(SMSG_TRADE_STATUS, 13);
    data << uint32(info.Status);

    switch (info.Status)
    {
        case TRADE_STATUS_BEGIN_TRADE:
            data << info.TraderGuid;
            break;
        case TRADE_STATUS_CLOSE_WINDOW:
            data << uint32(info.Result);
            data << uint8(info.IsTargetResult);
            data << uint32(info.ItemLimitCategoryId);
            break;
        case TRADE_STATUS_WRONG_REALM:
            data << uint8(info.Slot);
            break;
        default:
            break;
    }

    SendPacket(&data);
}

void trade::IgnoreTrade(Player& who, WorldPacket& )
{
    DEBUG_LOG("WORLD: Ignore Trade %u", who.GetGUIDLow());

}

void trade::BusyTrade(Player& who, WorldPacket& )
{
    DEBUG_LOG("WORLD: Busy Trade %u", who.GetGUIDLow());

}

void WorldSession::SendUpdateTrade(bool trader_state )
{
    TradeData* view_trade = trader_state ? _player->GetTradeData()->GetTraderData() : _player->GetTradeData();

    WorldPacket data(SMSG_TRADE_STATUS_EXTENDED, (100));
    data << uint8(trader_state ? 1 : 0);
    data << uint32(TRADE_SLOT_COUNT);
    data << uint32(TRADE_SLOT_COUNT);
    data << uint32(view_trade->GetMoney());
    data << uint32(view_trade->GetSpell());

    for (uint8 i = 0; i < TRADE_SLOT_COUNT; ++i)
    {
        data << uint8(i);

        if (Item* item = view_trade->GetItem(TradeSlots(i)))
        {
            data << uint32(item->GetProto()->ItemId);
            data << uint32(item->GetProto()->DisplayInfoID);
            data << uint32(item->GetCount());

            data << uint32(item->HasItemFlag(ITEM_DYNFLAG_WRAPPED) ? 1 : 0);
            data << item->GetGiftCreatorGuid();

            data << uint32(item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT));
            data << item->GetCreatorGuid();
            data << uint32(item->GetSpellCharges());
            data << uint32(item->GetItemSuffixFactor());
            data << uint32(item->GetItemRandomPropertyId());
            data << uint32(item->GetProto()->LockID);

            data << uint32(item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY));

            data << uint32(item->GetUInt32Value(ITEM_FIELD_DURABILITY));
        }
        else
        {
            for (uint8 j = 0; j < 15; ++j)
            {
                data << uint32(0);
            }
        }
    }

    SendPacket(&data);
}

void trade::MoveItems(Player& who, Item* myItems[], Item* hisItems[])
{
    Player* trader = who.GetTrader();
    if (!trader)
    {
        return;
    }

    for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
    {
        ItemPosCountVec traderDst;
        ItemPosCountVec playerDst;
        bool traderCanTrade = (myItems[i] == nullptr || trader->CanStoreItem(NULL_BAG, NULL_SLOT, traderDst, myItems[i], false) == EQUIP_ERR_OK);
        bool playerCanTrade = (hisItems[i] == nullptr || who.CanStoreItem(NULL_BAG, NULL_SLOT, playerDst, hisItems[i], false) == EQUIP_ERR_OK);
        if (traderCanTrade && playerCanTrade)
        {

            if (myItems[i])
            {

                DEBUG_LOG("partner storing: %s", myItems[i]->GetGuidStr().c_str());
                if (who.GetSession()->GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
                {
                    sLog.outCommand(who.GetSession()->GetAccountId(), "GM %s (Account: %u) trade: %s (Entry: %d Count: %u) to player: %s (Account: %u)",
                        who.GetName(), who.GetSession()->GetAccountId(),
                        myItems[i]->GetProto()->Name1, myItems[i]->GetEntry(), myItems[i]->GetCount(),
                        trader->GetName(), trader->GetSession()->GetAccountId());
                }

                trader->MoveItemToInventory(traderDst, myItems[i], true, true);
            }

            if (hisItems[i])
            {

                DEBUG_LOG("player storing: %s", hisItems[i]->GetGuidStr().c_str());
                if (trader->GetSession()->GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
                {
                    sLog.outCommand(trader->GetSession()->GetAccountId(), "GM %s (Account: %u) trade: %s (Entry: %d Count: %u) to player: %s (Account: %u)",
                        trader->GetName(), trader->GetSession()->GetAccountId(),
                        hisItems[i]->GetProto()->Name1, hisItems[i]->GetEntry(), hisItems[i]->GetCount(),
                        who.GetName(), who.GetSession()->GetAccountId());
                }

                who.MoveItemToInventory(playerDst, hisItems[i], true, true);
            }
        }
        else
        {

            if (myItems[i])
            {
                if (!traderCanTrade)
                {
                    sLog.outError("trader can't store item: %s", myItems[i]->GetGuidStr().c_str());
                }
                if (who.CanStoreItem(NULL_BAG, NULL_SLOT, playerDst, myItems[i], false) == EQUIP_ERR_OK)
                {
                    who.MoveItemToInventory(playerDst, myItems[i], true, true);
                }
                else
                {
                    sLog.outError("player can't take item back: %s", myItems[i]->GetGuidStr().c_str());
                }
            }

            if (hisItems[i])
            {
                if (!playerCanTrade)
                {
                    sLog.outError("player can't store item: %s", hisItems[i]->GetGuidStr().c_str());
                }
                if (trader->CanStoreItem(NULL_BAG, NULL_SLOT, traderDst, hisItems[i], false) == EQUIP_ERR_OK)
                {
                    trader->MoveItemToInventory(traderDst, hisItems[i], true, true);
                }
                else
                {
                    sLog.outError("trader can't take item back: %s", hisItems[i]->GetGuidStr().c_str());
                }
            }
        }
    }
}

static void setAcceptTradeMode(TradeData* myTrade, TradeData* hisTrade, Item** myItems, Item** hisItems)
{
    myTrade->SetInAcceptProcess(true);
    hisTrade->SetInAcceptProcess(true);

    for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
    {
        if (Item* item = myTrade->GetItem(TradeSlots(i)))
        {
            DEBUG_LOG("player trade %s bag: %u slot: %u", item->GetGuidStr().c_str(), item->GetBagSlot(), item->GetSlot());

            myItems[i] = item;
            myItems[i]->SetInTrade();
        }

        if (Item* item = hisTrade->GetItem(TradeSlots(i)))
        {
            DEBUG_LOG("partner trade %s bag: %u slot: %u", item->GetGuidStr().c_str(), item->GetBagSlot(), item->GetSlot());
            hisItems[i] = item;
            hisItems[i]->SetInTrade();
        }
    }
}

static void clearAcceptTradeMode(TradeData* myTrade, TradeData* hisTrade)
{
    myTrade->SetInAcceptProcess(false);
    hisTrade->SetInAcceptProcess(false);
}

static void clearAcceptTradeMode(Item** myItems, Item** hisItems)
{

    for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
    {
        if (myItems[i])
        {
            myItems[i]->SetInTrade(false);
        }
        if (hisItems[i])
        {
            hisItems[i]->SetInTrade(false);
        }
    }
}

void trade::AcceptTrade(WorldSession& session, WorldPacket& recvPacket)
{
    recvPacket.read_skip<uint32>();

    TradeData* my_trade = session.GetPlayer()->GetTradeData();
    if (!my_trade)
    {
        return;
    }

    Player* trader = my_trade->GetTrader();

    TradeData* his_trade = trader->GetTradeData();
    if (!his_trade)
    {
        return;
    }

    Item* myItems[TRADE_SLOT_TRADED_COUNT]  = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    Item* hisItems[TRADE_SLOT_TRADED_COUNT] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    my_trade->SetAccepted(true);

    TradeStatusInfo info;
    if (!InReach(*session.GetPlayer(), *trader, TRADE_DISTANCE, false))
    {
        info.Status = TRADE_STATUS_TARGET_TO_FAR;
        session.SendTradeStatus(info);
        my_trade->SetAccepted(false);
        return;
    }

    if (my_trade->GetMoney() > session.GetPlayer()->GetMoney())
    {
        info.Status = TRADE_STATUS_CLOSE_WINDOW;
        info.Result = EQUIP_ERR_NOT_ENOUGH_MONEY;
        session.SendTradeStatus(info);
        my_trade->SetAccepted(false, true);
        return;
    }

    if (his_trade->GetMoney() > trader->GetMoney())
    {
        info.Status = TRADE_STATUS_CLOSE_WINDOW;
        info.Result = EQUIP_ERR_NOT_ENOUGH_MONEY;
        trader->GetSession()->SendTradeStatus(info);
        his_trade->SetAccepted(false, true);
        return;
    }

    for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
    {
        if (Item* item = my_trade->GetItem(TradeSlots(i)))
        {
            if (!item->CanBeTraded())
            {
                info.Status = TRADE_STATUS_TRADE_CANCELED;
                session.SendTradeStatus(info);
                return;
            }
        }

        if (Item* item  = his_trade->GetItem(TradeSlots(i)))
        {
            if (!item->CanBeTraded())
            {
                info.Status = TRADE_STATUS_TRADE_CANCELED;
                session.SendTradeStatus(info);
                return;
            }
        }
    }

    if (his_trade->IsAccepted())
    {
        setAcceptTradeMode(my_trade, his_trade, myItems, hisItems);

        Spell* my_spell = nullptr;
        SpellCastTargets my_targets;

        Spell* his_spell = nullptr;
        SpellCastTargets his_targets;

        if (uint32 my_spell_id = my_trade->GetSpell())
        {
            SpellEntry const* spellEntry = sSpellStore.LookupEntry(my_spell_id);
            Item* castItem = my_trade->GetSpellCastItem();

            if (!spellEntry || !his_trade->GetItem(TRADE_SLOT_NONTRADED) ||
                (my_trade->HasSpellCastItem() && !castItem))
            {
                clearAcceptTradeMode(my_trade, his_trade);
                clearAcceptTradeMode(myItems, hisItems);

                my_trade->SetSpell(0);
                return;
            }

            my_spell = new Spell(session.GetPlayer(), spellEntry, true);
            my_spell->m_CastItem = castItem;
            my_targets.setTradeItemTarget(session.GetPlayer());
            my_spell->m_targets = my_targets;

            SpellCastResult res = my_spell->CheckCast(true);
            if (res != SPELL_CAST_OK)
            {
                my_spell->SendCastResult(res);

                clearAcceptTradeMode(my_trade, his_trade);
                clearAcceptTradeMode(myItems, hisItems);

                delete my_spell;
                my_trade->SetSpell(0);
                return;
            }
        }

        if (uint32 his_spell_id = his_trade->GetSpell())
        {
            SpellEntry const* spellEntry = sSpellStore.LookupEntry(his_spell_id);
            Item* castItem = his_trade->GetSpellCastItem();

            if (!spellEntry || !my_trade->GetItem(TRADE_SLOT_NONTRADED) ||
                (his_trade->HasSpellCastItem() && !castItem))
            {
                delete my_spell;
                his_trade->SetSpell(0);

                clearAcceptTradeMode(my_trade, his_trade);
                clearAcceptTradeMode(myItems, hisItems);
                return;
            }

            his_spell = new Spell(trader, spellEntry, true);
            his_spell->m_CastItem = castItem;
            his_targets.setTradeItemTarget(trader);
            his_spell->m_targets = his_targets;

            SpellCastResult res = his_spell->CheckCast(true);
            if (res != SPELL_CAST_OK)
            {
                his_spell->SendCastResult(res);

                clearAcceptTradeMode(my_trade, his_trade);
                clearAcceptTradeMode(myItems, hisItems);

                delete my_spell;
                delete his_spell;

                his_trade->SetSpell(0);
                return;
            }
        }

        info.Status = TRADE_STATUS_TRADE_ACCEPT;
        trader->GetSession()->SendTradeStatus(info);

        TradeStatusInfo myCanCompleteInfo, hisCanCompleteInfo;
        hisCanCompleteInfo.Result = trader->CanStoreItems(myItems, TRADE_SLOT_TRADED_COUNT);
        myCanCompleteInfo.Result = session.GetPlayer()->CanStoreItems(hisItems, TRADE_SLOT_TRADED_COUNT);

        clearAcceptTradeMode(myItems, hisItems);

        if (myCanCompleteInfo.Result != EQUIP_ERR_OK)
        {
            clearAcceptTradeMode(my_trade, his_trade);

            myCanCompleteInfo.Status = TRADE_STATUS_CLOSE_WINDOW;
            trader->GetSession()->SendTradeStatus(myCanCompleteInfo);
            myCanCompleteInfo.IsTargetResult = true;
            session.SendTradeStatus(myCanCompleteInfo);
            my_trade->SetAccepted(false);
            his_trade->SetAccepted(false);
            return;
        }
        else if (hisCanCompleteInfo.Result != EQUIP_ERR_OK)
        {
            clearAcceptTradeMode(my_trade, his_trade);

            hisCanCompleteInfo.Status = TRADE_STATUS_CLOSE_WINDOW;
            session.SendTradeStatus(hisCanCompleteInfo);
            hisCanCompleteInfo.IsTargetResult = true;
            trader->GetSession()->SendTradeStatus(hisCanCompleteInfo);
            my_trade->SetAccepted(false);
            his_trade->SetAccepted(false);
            return;
        }

        for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
        {
            if (Item* item = myItems[i])
            {
                item->SetGiftCreatorGuid(session.GetPlayer()->GetObjectGuid());
                session.GetPlayer()->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
            }
            if (Item* item = hisItems[i])
            {
                item->SetGiftCreatorGuid(trader->GetObjectGuid());
                trader->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
            }
        }

        MoveItems(*session.GetPlayer(), myItems, hisItems);

        if (sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
        {
            if (session.GetPlayer()->GetSession()->GetSecurity() > SEC_PLAYER && my_trade->GetMoney() > 0)
            {
                sLog.outCommand(session.GetPlayer()->GetSession()->GetAccountId(), "GM %s (Account: %u) give money (Amount: %u) to player: %s (Account: %u)",
                    session.GetPlayer()->GetName(), session.GetPlayer()->GetSession()->GetAccountId(),
                    my_trade->GetMoney(),
                    trader->GetName(), trader->GetSession()->GetAccountId());
            }
            if (trader->GetSession()->GetSecurity() > SEC_PLAYER && his_trade->GetMoney() > 0)
            {
                sLog.outCommand(trader->GetSession()->GetAccountId(), "GM %s (Account: %u) give money (Amount: %u) to player: %s (Account: %u)",
                    trader->GetName(), trader->GetSession()->GetAccountId(),
                    his_trade->GetMoney(),
                    session.GetPlayer()->GetName(), session.GetPlayer()->GetSession()->GetAccountId());
            }
        }

        session.GetPlayer()->ModifyMoney(-int32(my_trade->GetMoney()));
        session.GetPlayer()->ModifyMoney(his_trade->GetMoney());
        trader->ModifyMoney(-int32(his_trade->GetMoney()));
        trader->ModifyMoney(my_trade->GetMoney());

        if (my_spell)
        {
            my_spell->prepare(&my_targets);
        }

        if (his_spell)
        {
            his_spell->prepare(&his_targets);
        }

        clearAcceptTradeMode(my_trade, his_trade);
        session.GetPlayer()->DropTrade();
        trader->DropTrade();

        CharacterDatabase.BeginTransaction();
        session.GetPlayer()->SaveInventoryAndGoldToDB();
        trader->SaveInventoryAndGoldToDB();
        CharacterDatabase.CommitTransaction();

        info.Status = TRADE_STATUS_TRADE_COMPLETE;
        trader->GetSession()->SendTradeStatus(info);
        session.SendTradeStatus(info);
    }
    else
    {
        info.Status = TRADE_STATUS_TRADE_ACCEPT;
        trader->GetSession()->SendTradeStatus(info);
    }
}

void trade::UnacceptTrade(Player& who, WorldPacket& )
{
    TradeData* my_trade = who.GetTradeData();
    if (!my_trade)
    {
        return;
    }

    my_trade->SetAccepted(false, true);
}

void trade::BeginTrade(Player& who, WorldPacket& )
{
    TradeData* my_trade = who.GetTradeData();
    if (!my_trade)
    {
        return;
    }

    TradeStatusInfo info;
    info.Status = TRADE_STATUS_OPEN_WINDOW;
    my_trade->GetTrader()->GetSession()->SendTradeStatus(info);
    who.GetSession()->SendTradeStatus(info);
}

void WorldSession::SendCancelTrade()
{
    if (m_playerRecentlyLogout)
    {
        return;
    }

    TradeStatusInfo info;
    info.Status = TRADE_STATUS_TRADE_CANCELED;
    SendTradeStatus(info);
}

void trade::CancelTrade(WorldSession& session, WorldPacket& )
{

    if (session.GetPlayer())
    {
        session.GetPlayer()->TradeCancel(true);
    }
}

void trade::InitiateTrade(WorldSession& session, WorldPacket& recvPacket)
{
    ObjectGuid otherGuid = 0;
    recvPacket >> otherGuid;

    if (session.GetPlayer()->GetTradeData())
    {
        return;
    }

    TradeStatusInfo info;
    if (!session.GetPlayer()->IsAlive())
    {
        info.Status = TRADE_STATUS_YOU_DEAD;
        session.SendTradeStatus(info);
        return;
    }

    if (session.GetPlayer()->hasUnitState(UNIT_STAT_STUNNED))
    {
        info.Status = TRADE_STATUS_YOU_STUNNED;
        session.SendTradeStatus(info);
        return;
    }

    if (session.isLogingOut())
    {
        info.Status = TRADE_STATUS_YOU_LOGOUT;
        session.SendTradeStatus(info);
        return;
    }

    if (session.GetPlayer()->IsTaxiFlying())
    {
        info.Status = TRADE_STATUS_TARGET_TO_FAR;
        session.SendTradeStatus(info);
        return;
    }

    Player* pOther = sPlayerRegistry.Find(otherGuid);

    if (!pOther)
    {
        info.Status = TRADE_STATUS_NO_TARGET;
        session.SendTradeStatus(info);
        return;
    }

    if (pOther == session.GetPlayer() || pOther->GetTradeData())
    {
        info.Status = TRADE_STATUS_BUSY;
        session.SendTradeStatus(info);
        return;
    }

    if (!pOther->IsAlive())
    {
        info.Status = TRADE_STATUS_TARGET_DEAD;
        session.SendTradeStatus(info);
        return;
    }

    if (pOther->IsTaxiFlying())
    {
        info.Status = TRADE_STATUS_TARGET_TO_FAR;
        session.SendTradeStatus(info);
        return;
    }

    if (pOther->hasUnitState(UNIT_STAT_STUNNED))
    {
        info.Status = TRADE_STATUS_TARGET_STUNNED;
        session.SendTradeStatus(info);
        return;
    }

    if (pOther->GetSession()->isLogingOut())
    {
        info.Status = TRADE_STATUS_TARGET_LOGOUT;
        session.SendTradeStatus(info);
        return;
    }

    if (pOther->GetSocial()->HasIgnore(session.GetPlayer()->GetObjectGuid()))
    {
        info.Status = TRADE_STATUS_IGNORE_YOU;
        session.SendTradeStatus(info);
        return;
    }

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_TRADE) && pOther->GetTeam() != session.GetPlayer()->GetTeam() && session.GetSecurity() == SEC_PLAYER)
    {
        info.Status = TRADE_STATUS_WRONG_FACTION;
        session.SendTradeStatus(info);
        return;
    }

    if (!InReach(*pOther, *(session.GetPlayer()), TRADE_DISTANCE, false))
    {
        info.Status = TRADE_STATUS_TARGET_TO_FAR;
        session.SendTradeStatus(info);
        return;
    }

    if (session.GetSecurity() > SEC_PLAYER && session.GetPlayer()->GetVisibility() == VISIBILITY_OFF &&
        (pOther->GetSession()->GetSecurity() < session.GetSecurity() ||
        (pOther->GetSession()->GetSecurity() > session.GetSecurity() && pOther->GetVisibility() == VISIBILITY_OFF)
        ))
    {
        info.Status = TRADE_STATUS_TRADE_CANCELED;
        session.SendTradeStatus(info);
        return;
    }

    session.GetPlayer()->OpenTradeWith(pOther);
    pOther->OpenTradeWith(session.GetPlayer());

    info.Status = TRADE_STATUS_BEGIN_TRADE;
    info.TraderGuid = session.GetPlayer()->GetObjectGuid();
    pOther->GetSession()->SendTradeStatus(info);
}

void trade::SetTradeGold(Player& who, WorldPacket& recvPacket)
{
    uint32 gold;

    recvPacket >> gold;

    TradeData* my_trade = who.GetTradeData();
    if (!my_trade)
    {
        return;
    }

    my_trade->SetMoney(gold);
}

void trade::SetTradeItem(Player& who, WorldPacket& recvPacket)
{

    uint8 tradeSlot;
    uint8 bag;
    uint8 slot;

    recvPacket >> tradeSlot;
    recvPacket >> bag;
    recvPacket >> slot;

    TradeData* my_trade = who.GetTradeData();
    if (!my_trade)
    {
        return;
    }

    TradeStatusInfo info;

    if (tradeSlot >= TRADE_SLOT_COUNT)
    {
        info.Status = TRADE_STATUS_TRADE_CANCELED;
        who.GetSession()->SendTradeStatus(info);
        return;
    }

    Item* item = who.GetItemByPos(bag, slot);
    if (!item || (tradeSlot != TRADE_SLOT_NONTRADED && !item->CanBeTraded()))
    {
        info.Status = TRADE_STATUS_TRADE_CANCELED;
        who.GetSession()->SendTradeStatus(info);
        return;
    }

    if (my_trade->HasItem(item->GetObjectGuid()))
    {

        info.Status = TRADE_STATUS_TRADE_CANCELED;
        who.GetSession()->SendTradeStatus(info);
        return;
    }

    my_trade->SetItem(TradeSlots(tradeSlot), item);
}

void trade::ClearTradeItem(Player& who, WorldPacket& recvPacket)
{
    uint8 tradeSlot;
    recvPacket >> tradeSlot;

    TradeData* my_trade = who.GetTradeData();
    if (!my_trade)
    {
        return;
    }

    if (tradeSlot >= TRADE_SLOT_COUNT)
    {
        return;
    }

    my_trade->SetItem(TradeSlots(tradeSlot), nullptr);
}
