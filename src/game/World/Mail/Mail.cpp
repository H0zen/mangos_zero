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
#include <string>
#include <vector>
#include "Mail.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Mint.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Unit.h"
#include "Language.h"
#include "DBCStores.h"
#include "BattleGround/BattleGroundMgr.h"
#include "Item.h"
#include "AuctionHouseMgr.h"

MailSender::MailSender(Object* sender, MailStationery stationery) : m_stationery(stationery)
{
    switch (sender->GetTypeId())
    {
        case TYPEID_UNIT:
            m_messageType = MAIL_CREATURE;
            m_senderId = sender->GetEntry();
            break;
        case TYPEID_GAMEOBJECT:
            m_messageType = MAIL_GAMEOBJECT;
            m_senderId = sender->GetEntry();
            break;
        case TYPEID_ITEM:
        case TYPEID_CONTAINER:
            m_messageType = MAIL_ITEM;
            m_senderId = sender->GetEntry();
            break;
        case TYPEID_PLAYER:
            m_messageType = MAIL_NORMAL;
            m_senderId = sender->GetGUIDLow();
            if (static_cast<Player *>(sender)->isGameMaster())
            {
                m_stationery = MAIL_STATIONERY_GM;
            }
            break;
        default:
            m_messageType = MAIL_NORMAL;
            m_senderId = 0;
            sLog.outError("MailSender::MailSender - Mail have unexpected sender typeid (%u)", sender->GetTypeId());
            break;
    }
}

MailSender::MailSender(AuctionEntry* sender)
    : m_messageType(MAIL_AUCTION), m_senderId(sender->GetHouseId()), m_stationery(MAIL_STATIONERY_AUCTION)
{
}

MailReceiver::MailReceiver(Player* receiver) : m_receiver(receiver), m_receiver_guid(receiver->GetObjectGuid())
{
}

MailReceiver::MailReceiver(Player* receiver, ObjectGuid receiver_guid) : m_receiver(receiver), m_receiver_guid(receiver_guid)
{
    MANGOS_ASSERT(!receiver || receiver->GetObjectGuid() == receiver_guid);
}

MailDraft& MailDraft::AddItem(Item* item)
{
    m_items[item->GetGUIDLow()] = item;
    return *this;
}

bool MailDraft::prepareItems(Player* receiver)
{
    if (!m_mailTemplateId || !m_mailTemplateItemsNeed)
    {
        return false;
    }

    m_mailTemplateItemsNeed = false;

    Loot mailLoot(nullptr);

    mailLoot.FillLoot(m_mailTemplateId, LootTemplates_Mail, receiver, true, true);

    uint32 max_slot = mailLoot.GetMaxSlotInLootFor(receiver);
    for (uint32 i = 0; m_items.size() < MAX_MAIL_ITEMS && i < max_slot; ++i)
    {
        if (LootItem* lootitem = mailLoot.LootItemInSlot(i, receiver))
        {
            if (Item* item = Item::CreateItem(lootitem->itemid, lootitem->count, receiver))
            {
                item->SaveToDB();
                AddItem(item);
            }
        }
    }

    return true;
}

void MailDraft::deleteIncludedItems(bool inDB )
{
    for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
    {
        Item* item = mailItemIter->second;

        if (inDB)
        {
            CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", item->GetGUIDLow());
        }

        delete item;
    }

    m_items.clear();
}

void MailDraft::CloneFrom(MailDraft const& draft)
{
    m_mailTemplateId = draft.GetMailTemplateId();
    m_mailTemplateItemsNeed = draft.m_mailTemplateItemsNeed;

    m_subject = draft.GetSubject();
    m_body = draft.GetBody();
    m_money = draft.GetMoney();
    m_COD = draft.GetCOD();

    for (MailItemMap::const_iterator mailItemIter = draft.m_items.begin(); mailItemIter != draft.m_items.end(); ++mailItemIter)
    {
        Item* item = mailItemIter->second;

        if (Item* newitem = item->CloneItem(item->GetCount()))
        {
            newitem->SaveToDB();
            AddItem(newitem);
        }
    }
}

void MailDraft::SendReturnToSender(uint32 sender_acc, ObjectGuid sender_guid, ObjectGuid receiver_guid)
{
    Player* receiver = sObjectMgr.GetPlayer(receiver_guid);

    uint32 rc_account = 0;
    if (!receiver)
    {
        rc_account = sObjectMgr.GetPlayerAccountIdByGUID(receiver_guid);
    }

    if (!receiver && !rc_account)
    {
        deleteIncludedItems(true);
        return;
    }

    bool needItemDelay = false;

    if (!m_items.empty())
    {

        needItemDelay = sender_acc != rc_account;

        CharacterDatabase.BeginTransaction();
        for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
        {
            Item* item = mailItemIter->second;
            item->SaveToDB();

            CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid`='%u'", GuidCounter(receiver_guid), item->GetGUIDLow());
        }
        CharacterDatabase.CommitTransaction();
    }

    uint32 deliver_delay = needItemDelay ? sWorld.getConfig(CONFIG_UINT32_MAIL_DELIVERY_DELAY) : 0;

    SendMailTo(MailReceiver(receiver, receiver_guid), MailSender(MAIL_NORMAL, GuidCounter(sender_guid)), MAIL_CHECK_MASK_RETURNED, deliver_delay);
}

void MailDraft::SendMailTo(MailReceiver const& receiver, MailSender const& sender, MailCheckMask checked, uint32 deliver_delay)
{
    Player* pReceiver = receiver.GetPlayer();

    uint32 pReceiverAccount = 0;
    if (!pReceiver)
    {
        pReceiverAccount = sObjectMgr.GetPlayerAccountIdByGUID(receiver.GetPlayerGuid());
    }

    if (!pReceiver && !pReceiverAccount)
    {
        deleteIncludedItems(true);
        return;
    }

    bool has_items = !m_items.empty();

    if (pReceiver)
    {
        if (prepareItems(pReceiver))
        {
            has_items = true;
        }
    }

    uint32 mailId = sMint.MailIds().Next();

    time_t deliver_time = time(nullptr) + deliver_delay;

    uint32 expire_delay;

    expire_delay = 30 * DAY;

    if (sender.GetMailMessageType() == MAIL_AUCTION && m_items.empty() && !m_money)
    {
        expire_delay = HOUR;
    }

    else if (sender.GetMailMessageType() == MAIL_CREATURE && sBattleGroundMgr.GetBattleMasterBG(sender.GetSenderId()) != BATTLEGROUND_TYPE_NONE)
    {
        expire_delay = DAY;
    }
    else if (m_COD)
    {

        expire_delay = 3 * DAY;
    }

    else if (sender.GetStationery() == MAIL_STATIONERY_GM)
    {
        expire_delay = 90 * DAY;
    }

    time_t expire_time = deliver_time + expire_delay;

    CharacterDatabase.BeginTransaction();
    writeMailRows(mailId, receiver, sender, checked, deliver_time, expire_time, has_items);
    CharacterDatabase.CommitTransaction();

    if (pReceiver)
    {
        pReceiver->Post().Expecting(deliver_time);

        Mail* m = new Mail;
        m->messageID = mailId;
        m->mailTemplateId = GetMailTemplateId();
        m->subject = GetSubject();
        m->body = GetBody();
        m->money = GetMoney();
        m->COD = GetCOD();

        for (MailItemMap::const_iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
        {
            Item* item = mailItemIter->second;
            m->AddItem(item->GetGUIDLow(), item->GetEntry());
        }

        m->messageType = sender.GetMailMessageType();
        m->stationery = sender.GetStationery();
        m->sender = sender.GetSenderId();
        m->receiverGuid = receiver.GetPlayerGuid();
        m->expire_time = expire_time;
        m->deliver_time = deliver_time;
        m->checked = checked;
        m->state = MAIL_STATE_UNCHANGED;

        pReceiver->Post().Add(m);

        if (!m_items.empty())
        {
            for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
            {
                pReceiver->Post().Keep(mailItemIter->second);
            }
        }
    }
    else if (!m_items.empty())
    {
        deleteIncludedItems();
    }
}

void MailDraft::writeMailRows(uint32 mailId, MailReceiver const& receiver, MailSender const& sender,
                              MailCheckMask checked, time_t deliver_time, time_t expire_time,
                              bool has_items)
{
    std::string safe_subject = GetSubject();
    CharacterDatabase.escape_string(safe_subject);

    std::string safe_body = GetBody();
    CharacterDatabase.escape_string(safe_body);

    CharacterDatabase.PExecute("INSERT INTO `mail` (`id`,`messageType`,`stationery`,`mailTemplateId`,`sender`,`receiver`,`subject`,`body`,`has_items`,`expire_time`,`deliver_time`,`money`,`cod`,`checked`) "
        "VALUES ('%u', '%u', '%u', '%u', '%u', '%u', '%s', '%s', '%u', '" UI64FMTD "','" UI64FMTD "', '%u', '%u', '%u')",
        mailId, sender.GetMailMessageType(), sender.GetStationery(), GetMailTemplateId(), sender.GetSenderId(), GuidCounter(receiver.GetPlayerGuid()), safe_subject.c_str(), safe_body.c_str(), (has_items ? 1 : 0), (uint64)expire_time, (uint64)deliver_time, m_money, m_COD, checked);

    for (MailItemMap::const_iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
    {
        Item* item = mailItemIter->second;
        CharacterDatabase.PExecute("INSERT INTO `mail_items` (`mail_id`,`item_guid`,`item_template`,`receiver`) VALUES ('%u', '%u', '%u','%u')",
            mailId, item->GetGUIDLow(), item->GetEntry(), GuidCounter(receiver.GetPlayerGuid()));
    }
}

void Mail::prepareTemplateItems(Player* receiver)
{
    if (!mailTemplateId || !items.empty())
    {
        return;
    }

    has_items = true;

    Loot mailLoot(nullptr);

    mailLoot.FillLoot(mailTemplateId, LootTemplates_Mail, receiver, true, true);

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("UPDATE `mail` SET `has_items` = 1 WHERE `id` = %u", messageID);

    uint32 max_slot = mailLoot.GetMaxSlotInLootFor(receiver);
    for (uint32 i = 0; items.size() < MAX_MAIL_ITEMS && i < max_slot; ++i)
    {
        if (LootItem* lootitem = mailLoot.LootItemInSlot(i, receiver))
        {
            if (Item* item = Item::CreateItem(lootitem->itemid, lootitem->count, receiver))
            {
                item->SaveToDB();

                AddItem(item->GetGUIDLow(), item->GetEntry());

                receiver->Post().Keep(item);

                CharacterDatabase.PExecute("INSERT INTO `mail_items` (`mail_id`,`item_guid`,`item_template`,`receiver`) VALUES ('%u', '%u', '%u','%u')",
                    messageID, item->GetGUIDLow(), item->GetEntry(), receiver->GetGUIDLow());
            }
        }
    }

    CharacterDatabase.CommitTransaction();
}
