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
#include "Mail.h"
#include "Language.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Mint.h"
#include "Item.h"
#include "Player.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "MailAnswers.h"
#include "Opcodes.h"
#include "Chat.h"

bool WorldSession::CheckMailBox(ObjectGuid guid)
{

    if (guid == GetPlayer()->GetObjectGuid())
    {

        if (!ChatHandler(GetPlayer()).FindCommand("mailbox"))
        {
            DEBUG_LOG("%s attempt open mailbox in cheating way.", GuidString(guid).c_str());
            return false;
        }
    }

    else if ((GuidHigh(guid) == HIGHGUID_GAMEOBJECT))
    {
        if (!GetPlayer()->GetGameObjectIfCanInteractWith(guid, GAMEOBJECT_TYPE_MAILBOX))
        {
            DEBUG_LOG("Mailbox %s not found or %s can't interact with him.", GuidString(guid).c_str(), GetPlayer()->GetGuidStr().c_str());
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

void mail::SendMail(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid = 0;
    ObjectGuid itemGuid = 0;
    uint64 unk3;
    std::string receiver, subject, body;
    uint32 unk1, unk2, money, COD;
    uint8 unk4;
    recv_data >> mailboxGuid;
    recv_data >> receiver;

    recv_data >> subject;

    recv_data >> body;

    recv_data >> unk1;
    recv_data >> unk2;

    recv_data >> itemGuid;

    recv_data >> money >> COD;
    recv_data >> unk3;
    recv_data >> unk4;

    if (!session.CheckMailBox(mailboxGuid))
    {
        return;
    }

    if (receiver.empty())
    {
        return;
    }

    Player* pl = session.GetPlayer();

    ObjectGuid rc = 0;
    if (normalizePlayerName(receiver))
    {
        rc = sObjectMgr.GetPlayerGuidByName(receiver);
    }

    if (!rc)
    {
        DETAIL_LOG("%s is sending mail to %s (GUID: nonexistent!) with subject %s and body %s includes %u items, %u copper and %u COD copper with unk1 = %u, unk2 = %u",
            pl->GetGuidStr().c_str(), receiver.c_str(), subject.c_str(), body.c_str(), itemGuid ? 1 : 0, money, COD, unk1, unk2);
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_RECIPIENT_NOT_FOUND);
        return;
    }

    DETAIL_LOG("%s is sending mail to %s with subject %s and body %s includes %u items, %u copper and %u COD copper with unk1 = %u, unk2 = %u",
        pl->GetGuidStr().c_str(), GuidString(rc).c_str(), subject.c_str(), body.c_str(), itemGuid ? 1 : 0, money, COD, unk1, unk2);

    if (pl->GetObjectGuid() == rc)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_CANNOT_SEND_TO_SELF);
        return;
    }

    if (money && COD)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    uint32 reqmoney = money + 30;

    if (pl->GetMoney() < reqmoney)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_NOT_ENOUGH_MONEY);
        return;
    }

    Player* receive = sObjectMgr.GetPlayer(rc);

    Team rc_team;
    uint8 mails_count = 0;

    if (receive)
    {
        rc_team = receive->GetTeam();
        mails_count = receive->Post().Count();
    }
    else
    {
        rc_team = sObjectMgr.GetPlayerTeamByGUID(rc);
        if (QueryResult* result = CharacterDatabase.PQuery("SELECT COUNT(*) FROM `mail` WHERE `receiver` = '%u'", GuidCounter(rc)))
        {
            Field* fields = result->Fetch();
            mails_count = fields[0].GetUInt32();
            delete result;
        }
    }

    if (mails_count > 100)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_RECIPIENT_CAP_REACHED);
        return;
    }

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_MAIL) && pl->GetTeam() != rc_team && session.GetSecurity() == SEC_PLAYER)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_NOT_YOUR_TEAM);
        return;
    }

    Item* item = nullptr;

    if (itemGuid)
    {
        item = pl->GetItemByGuid(itemGuid);

        if (!item)
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_MAIL_ATTACHMENT_INVALID);
            return;
        }

        if (!item->CanBeTraded())
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_MAIL_ATTACHMENT_INVALID);
            return;
        }

        if ((item->GetProto()->Flags & ITEM_FLAG_CONJURED) || item->GetUInt32Value(ITEM_FIELD_DURATION))
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_MAIL_ATTACHMENT_INVALID);
            return;
        }

        if (COD && item->HasItemFlag(ITEM_DYNFLAG_WRAPPED))
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_CANT_SEND_WRAPPED_COD);
            return;
        }
    }

    pl->SendMailResult(0, MAIL_SEND, MAIL_OK);

    pl->ModifyMoney(-int32(reqmoney));

    bool needItemDelay = false;

    MailDraft draft(subject, body);

    if (itemGuid || money > 0)
    {
        uint32 rc_account = 0;
        if (receive)
        {
            rc_account = receive->GetSession()->GetAccountId();
        }
        else
        {
            rc_account = sObjectMgr.GetPlayerAccountIdByGUID(rc);
        }

        if (item)
        {
            if (session.GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
            {
                sLog.outCommand(session.GetAccountId(), "GM %s (Account: %u) mail item: %s (Entry: %u Count: %u) to player: %s (Account: %u)",
                    session.GetPlayerName(), session.GetAccountId(), item->GetProto()->Name1, item->GetEntry(), item->GetCount(), receiver.c_str(), rc_account);
            }

            pl->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
            CharacterDatabase.BeginTransaction();
            item->DeleteFromInventoryDB();
            item->SaveToDB();

            CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid`='%u'", GuidCounter(rc), item->GetGUIDLow());
            CharacterDatabase.CommitTransaction();

            draft.AddItem(item);

            needItemDelay = pl->GetSession()->GetAccountId() != rc_account;
        }

        if (money > 0 &&  session.GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
        {
            sLog.outCommand(session.GetAccountId(), "GM %s (Account: %u) mail money: %u to player: %s (Account: %u)",
                session.GetPlayerName(), session.GetAccountId(), money, receiver.c_str(), rc_account);
        }
    }

    uint32 deliver_delay = needItemDelay ? sWorld.getConfig(CONFIG_UINT32_MAIL_DELIVERY_DELAY) : 0;

    draft
        .SetMoney(money)
        .SetCOD(COD)
        .SendMailTo(MailReceiver(receive, rc), pl, body.empty() ? MAIL_CHECK_MASK_COPIED : MAIL_CHECK_MASK_HAS_BODY, deliver_delay);

    CharacterDatabase.BeginTransaction();
    pl->SaveInventoryAndGoldToDB();
    CharacterDatabase.CommitTransaction();
}

void mail::MailMarkAsRead(Player& who, WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid = 0;
    uint32 mailId;
    recv_data >> mailboxGuid;
    recv_data >> mailId;

    if (!who.GetSession()->CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = &who;

    if (Mail* m = pl->Post().Find(mailId))
    {
        pl->Post().Read();
        m->checked = m->checked | MAIL_CHECK_MASK_READ;
        pl->Post().Changed(true);
        m->state = MAIL_STATE_CHANGED;
    }
}

void mail::MailDelete(Player& who, WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid = 0;
    uint32 mailId;
    recv_data >> mailboxGuid;
    recv_data >> mailId;

    if (!who.GetSession()->CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = &who;
    pl->Post().Changed(true);

    if (Mail* m = pl->Post().Find(mailId))
    {

        if (m->COD)
        {
            pl->SendMailResult(mailId, MAIL_DELETED, MAIL_ERR_INTERNAL_ERROR);
            return;
        }

        m->state = MAIL_STATE_DELETED;
    }
    pl->SendMailResult(mailId, MAIL_DELETED, MAIL_OK);
}

void mail::MailReturnToSender(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid = 0;
    uint32 mailId;
    recv_data >> mailboxGuid;
    recv_data >> mailId;

    if (!session.CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = session.GetPlayer();
    Mail* m = pl->Post().Find(mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(nullptr))
    {
        pl->SendMailResult(mailId, MAIL_RETURNED_TO_SENDER, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `id` = '%u'", mailId);

    CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `mail_id` = '%u'", mailId);
    CharacterDatabase.CommitTransaction();
    pl->Post().Remove(mailId);

    if (m->messageType == MAIL_NORMAL && m->sender)
    {
        MailDraft draft;
        if (m->mailTemplateId)
        {
            draft.SetMailTemplate(m->mailTemplateId, false);
        }
        else
        {
            draft.SetSubjectAndBody(m->subject, m->body);
        }

        if (m->HasItems())
        {
            for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
            {
                if (Item* item = pl->Post().Attachment(itr2->item_guid))
                {
                    draft.AddItem(item);
                }

                pl->Post().Drop(itr2->item_guid);
            }
        }

        draft.SetMoney(m->money).SendReturnToSender(session.GetAccountId(), m->receiverGuid, MakeGuid(HIGHGUID_PLAYER, m->sender));
    }

    delete m;
    pl->SendMailResult(mailId, MAIL_RETURNED_TO_SENDER, MAIL_OK);
}

void mail::MailTakeItem(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid = 0;
    uint32 mailId;
    recv_data >> mailboxGuid;
    recv_data >> mailId;

    if (!session.CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = session.GetPlayer();

    Mail* m = pl->Post().Find(mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(nullptr))
    {
        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    if (pl->GetMoney() < m->COD)
    {
        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_ERR_NOT_ENOUGH_MONEY);
        return;
    }

    uint32 itemId = m->items[0].item_template;
    uint32 itemGuid = m->items[0].item_guid;

    Item* it = pl->Post().Attachment(itemGuid);

    ItemPosCountVec dest;
    InventoryResult msg = session.GetPlayer()->CanStoreItem(NULL_BAG, NULL_SLOT, dest, it, false);
    if (msg == EQUIP_ERR_OK)
    {
        m->RemoveItem(itemGuid);
        m->removedItems.push_back(itemGuid);

        if (m->COD > 0)
        {
            ObjectGuid sender_guid = MakeGuid(HIGHGUID_PLAYER, m->sender);
            Player* sender = sObjectMgr.GetPlayer(sender_guid);

            uint32 sender_accId = 0;

            if (session.GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
            {
                std::string sender_name;
                if (sender)
                {
                    sender_accId = sender->GetSession()->GetAccountId();
                    sender_name = sender->GetName();
                }
                else if (sender_guid)
                {

                    sender_accId = sObjectMgr.GetPlayerAccountIdByGUID(sender_guid);

                    if (!sObjectMgr.GetPlayerNameByGUID(sender_guid, sender_name))
                    {
                        sender_name = sObjectMgr.GetMangosStringForDBCLocale(LANG_UNKNOWN);
                    }
                }
                sLog.outCommand(session.GetAccountId(), "GM %s (Account: %u) receive mail item: %s (Entry: %u Count: %u) and send COD money: %u to player: %s (Account: %u)",
                    session.GetPlayerName(), session.GetAccountId(), it->GetProto()->Name1, it->GetEntry(), it->GetCount(), m->COD, sender_name.c_str(), sender_accId);
            }
            else if (!sender)
            {
                sender_accId = sObjectMgr.GetPlayerAccountIdByGUID(sender_guid);
            }

            if (sender || sender_accId)
            {
                MailDraft(m->subject, "")
                    .SetMoney(m->COD)
                    .SendMailTo(MailReceiver(sender, sender_guid), session.GetPlayer(), MAIL_CHECK_MASK_COD_PAYMENT);
            }

            pl->ModifyMoney(-int32(m->COD));
        }
        m->COD = 0;
        m->state = MAIL_STATE_CHANGED;
        pl->Post().Changed(true);
        pl->Post().Drop(it->GetGUIDLow());

        uint32 count = it->GetCount();
        pl->MoveItemToInventory(dest, it, true);

        CharacterDatabase.BeginTransaction();
        pl->SaveInventoryAndGoldToDB();
        pl->SaveMail();
        CharacterDatabase.CommitTransaction();

        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_OK, 0, itemId, count);
    }
    else
    {
        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_ERR_EQUIP_ERROR, msg);
    }
}

void mail::MailTakeMoney(Player& who, WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid = 0;
    uint32 mailId;
    recv_data >> mailboxGuid;
    recv_data >> mailId;

    if (!who.GetSession()->CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = &who;

    Mail* m = pl->Post().Find(mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(nullptr))
    {
        pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_OK);

    pl->ModifyMoney(m->money);
    m->money = 0;
    m->state = MAIL_STATE_CHANGED;
    pl->Post().Changed(true);

    CharacterDatabase.BeginTransaction();
    pl->SaveGoldToDB();
    pl->SaveMail();
    CharacterDatabase.CommitTransaction();
}

void mail::GetMailList(Player& who, WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid = 0;
    recv_data >> mailboxGuid;

    if (!who.GetSession()->CheckMailBox(mailboxGuid))
    {
        return;
    }

    uint32 mailsCount = 0;

    WorldPacket data(SMSG_MAIL_LIST_RESULT, (200));
    data << uint8(0);
    time_t cur_time = time(nullptr);

    for (PlayerMails::iterator itr = who.Post().begin(); itr != who.Post().end(); ++itr)
    {

        if (mailsCount >= 254)
        {
            break;
        }

        if ((*itr)->state == MAIL_STATE_DELETED || cur_time < (*itr)->deliver_time)
        {
            continue;
        }

        data << uint32((*itr)->messageID);
        data << uint8((*itr)->messageType);

        switch ((*itr)->messageType)
        {
            case MAIL_NORMAL:
                data << MakeGuid(HIGHGUID_PLAYER, (*itr)->sender);
                break;
            case MAIL_CREATURE:
            case MAIL_GAMEOBJECT:
            case MAIL_AUCTION:
                data << (uint32)(*itr)->sender;
                break;
            case MAIL_ITEM:
                break;
        }

        data << (*itr)->subject;
        data << uint32((*itr)->messageID);
        data << uint32(0);
        data << uint32((*itr)->stationery);

        Item* item = (*itr)->items.size() > 0 ? who.Post().Attachment((*itr)->items[0].item_guid) : nullptr;
        if (item)
        {
            data << uint32(item->GetEntry());
            data << uint32(item->GetEnchantmentId((EnchantmentSlot)PERM_ENCHANTMENT_SLOT));
            data << uint32(item->GetItemRandomPropertyId());
            data << uint32(item->GetItemSuffixFactor());
            data << uint8(item->GetCount());
            data << uint32(item->GetSpellCharges());
            data << uint32(item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY));
            data << uint32(item->GetUInt32Value(ITEM_FIELD_DURABILITY));
        }
        else
        {
            data << uint32(0) << uint32(0) << uint32(0) << uint32(0) << uint8(0) << uint32(0) << uint32(0) << uint32(0);
        }

        data << uint32((*itr)->money);
        data << uint32((*itr)->COD);
        data << uint32((*itr)->checked);
        data << float(float((*itr)->expire_time - time(nullptr)) / float(DAY));
        data << uint32((*itr)->mailTemplateId);

        mailsCount += 1;
    }

    data.put<uint8>(0, mailsCount);
    who.GetSession()->SendPacket(&data);

    who.Post().Recount();
}

void mail::ItemTextQuery(Player& who, WorldPacket& recv_data)
{
    uint32 itemId;
    uint32 mailId;
    uint32 unk;

    recv_data >> itemId >> mailId >> unk;

    DEBUG_LOG("CMSG_ITEM_TEXT_QUERY itemguid: %u, mailId: %u, unk: %u", itemId, mailId, unk);

    WorldPacket data(SMSG_ITEM_TEXT_QUERY_RESPONSE, (4 + 10));
    data << itemId;
    data << sObjectMgr.GetItemText(itemId);
    who.GetSession()->SendPacket(&data);
}

void mail::MailCreateTextItem(Player& who, WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid = 0;
    uint32 mailId;

    recv_data >> mailboxGuid;
    recv_data >> mailId;
    recv_data.read_skip<uint32>();

    if (!who.GetSession()->CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = &who;

    Mail* m = pl->Post().Find(mailId);
    if (!m || (m->body.empty() && !m->mailTemplateId) || m->state == MAIL_STATE_DELETED || m->deliver_time > time(nullptr))
    {
        pl->SendMailResult(mailId, MAIL_MADE_PERMANENT, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    Item* bodyItem = new Item;
    if (!bodyItem->Create(sMint.ItemGuids().Next(), MAIL_BODY_ITEM_TEMPLATE, pl))
    {
        delete bodyItem;
        return;
    }

    bodyItem->SetUInt32Value(ITEM_FIELD_ITEM_TEXT_ID, mailId);
    bodyItem->SetCreatorGuid(MakeGuid(HIGHGUID_PLAYER, m->sender));

    DETAIL_LOG("HandleMailCreateTextItem mailid=%u", mailId);

    ItemPosCountVec dest;
    InventoryResult msg = who.CanStoreItem(NULL_BAG, NULL_SLOT, dest, bodyItem, false);
    if (msg == EQUIP_ERR_OK)
    {
        m->checked = m->checked | MAIL_CHECK_MASK_COPIED;
        m->state = MAIL_STATE_CHANGED;
        pl->Post().Changed(true);

        pl->StoreItem(dest, bodyItem, true);
        pl->SendMailResult(mailId, MAIL_MADE_PERMANENT, MAIL_OK);
    }
    else
    {
        pl->SendMailResult(mailId, MAIL_MADE_PERMANENT, MAIL_ERR_EQUIP_ERROR, msg);
        delete bodyItem;
    }
}

void mail::QueryNextMailTime(Player& who, WorldPacket& )
{
    WorldPacket data(MSG_QUERY_NEXT_MAIL_TIME, 4);
    data << (who.Post().Unread() > 0 ? float(0) : float(-1));
    who.GetSession()->SendPacket(&data);
}
