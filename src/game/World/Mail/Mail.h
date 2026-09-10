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
#include <ctime>
#include <string>
#include <vector>
#include "ObjectGuid.h"
#include <map>

struct AuctionEntry;
class Item;
class Object;
class Player;

#define MAIL_BODY_ITEM_TEMPLATE 8383

#define MAX_MAIL_ITEMS 1

enum MailMessageType
{
    MAIL_NORMAL         = 0,
    MAIL_AUCTION        = 2,
    MAIL_CREATURE       = 3,
    MAIL_GAMEOBJECT     = 4,
    MAIL_ITEM           = 5,
};

enum MailCheckMask
{
    MAIL_CHECK_MASK_NONE        = 0x00,
    MAIL_CHECK_MASK_READ        = 0x01,
    MAIL_CHECK_MASK_RETURNED    = 0x02,
    MAIL_CHECK_MASK_COPIED      = 0x04,
    MAIL_CHECK_MASK_COD_PAYMENT = 0x08,
    MAIL_CHECK_MASK_HAS_BODY    = 0x10,
};

enum MailStationery
{
    MAIL_STATIONERY_UNKNOWN =  1,
    MAIL_STATIONERY_DEFAULT = 41,
    MAIL_STATIONERY_GM      = 61,
    MAIL_STATIONERY_AUCTION = 62,
    MAIL_STATIONERY_VAL     = 64,
    MAIL_STATIONERY_CHR     = 65,
};

enum MailState
{
    MAIL_STATE_UNCHANGED = 1,
    MAIL_STATE_CHANGED   = 2,
    MAIL_STATE_DELETED   = 3
};

enum MailAuctionAnswers
{
    AUCTION_OUTBIDDED           = 0,
    AUCTION_WON                 = 1,
    AUCTION_SUCCESSFUL          = 2,
    AUCTION_EXPIRED             = 3,
    AUCTION_CANCELLED_TO_BIDDER = 4,
    AUCTION_CANCELED            = 5,
    AUCTION_SALE_PENDING        = 6
};

class MailSender
{
    public:
        MailSender() : m_messageType(MAIL_NORMAL), m_senderId(0), m_stationery(MAIL_STATIONERY_DEFAULT) {}

        MailSender(MailMessageType messageType, uint32 sender_guidlow_or_entry, MailStationery stationery = MAIL_STATIONERY_DEFAULT)
            : m_messageType(messageType), m_senderId(sender_guidlow_or_entry), m_stationery(stationery)
        {
        }
        MailSender(Object* sender, MailStationery stationery = MAIL_STATIONERY_DEFAULT);
        MailSender(AuctionEntry* sender);
    public:

        MailMessageType GetMailMessageType() const { return m_messageType; }

        uint32 GetSenderId() const { return m_senderId; }

        MailStationery GetStationery() const { return m_stationery; }
    private:

        MailSender(MailMessageType messageType, uint64 wrong_guid, MailStationery stationery = MAIL_STATIONERY_DEFAULT);

        MailMessageType m_messageType;
        uint32 m_senderId;
        MailStationery m_stationery;
};

class MailReceiver
{
    public:
        explicit MailReceiver(ObjectGuid receiver_guid) : m_receiver(nullptr), m_receiver_guid(receiver_guid) {}
        MailReceiver(Player* receiver);
        MailReceiver(Player* receiver, ObjectGuid receiver_guid);
    public:

        Player* GetPlayer() const { return m_receiver; }

        ObjectGuid const& GetPlayerGuid() const { return m_receiver_guid; }
    private:
        Player* m_receiver;
        ObjectGuid m_receiver_guid = 0;
};

class MailDraft
{

    typedef std::map<uint32, Item*> MailItemMap;

    public:

        MailDraft()
            : m_mailTemplateId(0), m_mailTemplateItemsNeed(false), m_money(0), m_COD(0) {}

        explicit MailDraft(uint16 mailTemplateId, bool need_items = true)
            : m_mailTemplateId(mailTemplateId), m_mailTemplateItemsNeed(need_items), m_money(0), m_COD(0)
        {}

        MailDraft(std::string subject, std::string body)
            : m_mailTemplateId(0), m_mailTemplateItemsNeed(false), m_subject(subject), m_body(body), m_money(0), m_COD(0) {}

    public:

        uint16 GetMailTemplateId() const { return m_mailTemplateId; }

        std::string const& GetSubject() const { return m_subject; }

        std::string const& GetBody() const { return m_body; }

        uint32 GetMoney() const { return m_money; }

        uint32 GetCOD() const { return m_COD; }

    public:

        MailDraft& SetSubjectAndBody(std::string subject, std::string body) { m_subject = subject; m_body = body; return *this; }
        MailDraft& SetMailTemplate(uint16 mailTemplateId, bool need_items = true) { m_mailTemplateId = mailTemplateId, m_mailTemplateItemsNeed = need_items; return *this; }

        MailDraft& AddItem(Item* item);

        MailDraft& SetMoney(uint32 money) { m_money = money; return *this; }

        MailDraft& SetCOD(uint32 COD) { m_COD = COD; return *this; }

        void CloneFrom(MailDraft const& draft);
    public:
        void SendReturnToSender(uint32 sender_acc, ObjectGuid sender_guid, ObjectGuid receiver_guid);
        void SendMailTo(MailReceiver const& receiver, MailSender const& sender, MailCheckMask checked = MAIL_CHECK_MASK_NONE, uint32 deliver_delay = 0);

    private:
        MailDraft(MailDraft const&);
        MailDraft& operator=(MailDraft const&);

        void writeMailRows(uint32 mailId, MailReceiver const& receiver, MailSender const& sender,
                           MailCheckMask checked, time_t deliver_time, time_t expire_time,
                           bool has_items);

        void deleteIncludedItems(bool inDB = false);
        bool prepareItems(Player* receiver);

        uint16      m_mailTemplateId;

        bool        m_mailTemplateItemsNeed;

        std::string m_subject;

        std::string m_body;

        MailItemMap m_items;

        uint32 m_money;

        uint32 m_COD;
};

struct MailItemInfo
{
    uint32 item_guid = 0;
    uint32 item_template = 0;
};

typedef std::vector<MailItemInfo> MailItemInfoVec;

struct Mail
{

    uint32 messageID = 0;

    uint8 messageType = 0;

    uint8 stationery = 0;

    uint16 mailTemplateId = 0;

    uint32 sender = 0;

    ObjectGuid receiverGuid = 0;

    std::string subject;

    std::string body;

    bool has_items = false;

    MailItemInfoVec items;

    std::vector<uint32> removedItems;

    time_t expire_time = 0;

    time_t deliver_time = 0;

    uint32 money = 0;

    uint32 COD = 0;

    uint32 checked = 0;

    MailState state = MAIL_STATE_UNCHANGED;

    void AddItem(uint32 itemGuidLow, uint32 item_template)
    {
        MailItemInfo mii;
        mii.item_guid = itemGuidLow;
        mii.item_template = item_template;
        items.push_back(mii);
        has_items = true;
    }

    bool RemoveItem(uint32 item_guid)
    {
        for (MailItemInfoVec::iterator itr = items.begin(); itr != items.end(); ++itr)
        {
            if (itr->item_guid == item_guid)
            {
                items.erase(itr);
                return true;
            }
        }
        return false;
    }

    bool HasItems() const { return has_items; }

    void prepareTemplateItems(Player* receiver);
};
