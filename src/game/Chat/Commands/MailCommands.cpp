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
#include <list>
#include "Chat.h"
#include "ObjectMgr.h"
#include "Mail.h"
#include "MassMailMgr.h"

bool ChatHandler::HandleSendMailCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* target;
    ObjectGuid target_guid = 0;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
    {
        return false;
    }

    MailDraft draft;

    if (!*args)
    {
        return false;
    }
    else
    {

        if (!HandleSendMailHelper(draft, args))
        {
            return false;
        }
    }

    MailSender sender(MAIL_NORMAL, m_session ? m_session->GetPlayer()->GetGUIDLow() : (uint32)0, MAIL_STATIONERY_GM);

    draft.SendMailTo(MailReceiver(target, target_guid), sender);

    std::string nameLink = playerLink(target_name);
    PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
    return true;
}

bool ChatHandler::HandleSendMailHelper(MailDraft& draft, char* args)
{

    char* msgSubject = ExtractQuotedArg(&args);
    if (!msgSubject)
    {
        return false;
    }

    char* msgText = ExtractQuotedArg(&args);
    if (!msgText)
    {
        return false;
    }

    draft.SetSubjectAndBody(msgSubject, msgText);

    return true;
}

bool ChatHandler::HandleSendMassMailCommand(char* args)
{

    uint32 raceMask = 0;
    char const* name = nullptr;

    if (!ExtractRaceMask(&args, raceMask, &name))
    {
        return false;
    }

    MailDraft* draft = new MailDraft;

    if (!HandleSendMailHelper(*draft, args))
    {
        delete draft;
        return false;
    }

    MailSender sender(MAIL_NORMAL, (uint32)0, MAIL_STATIONERY_GM);

    sMassMailMgr.AddMassMailTask(draft, sender, raceMask);

    PSendSysMessage(LANG_MAIL_SENT, name);
    return true;
}

bool ChatHandler::HandleSendItemsHelper(MailDraft& draft, char* args)
{

    char* msgSubject = ExtractQuotedArg(&args);
    if (!msgSubject)
    {
        return false;
    }

    char* msgText = ExtractQuotedArg(&args);
    if (!msgText)
    {
        return false;
    }

    typedef std::tuple<uint32, uint32, uint32> ItemToSend;
    typedef std::list< ItemToSend > ItemsToSend;
    ItemsToSend items;

    while (char* itemStr = ExtractArg(&args))
    {

        uint32 item_id = 0;
        uint32 item_count = 1;
        uint32 item_enchant_id = 0;

        if (sscanf(itemStr, "%u:%u:%u", &item_id, &item_count, &item_enchant_id) == 0)
        {

            if (sscanf(itemStr, "%u:%u", &item_id, &item_count) == 0)
            {

                if (sscanf(itemStr, "%u", &item_id) == 0)
                {
                    return false;
                }
            }
        }

        if (!item_id)
        {
            PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
            SetSentErrorMessage(true);
            return false;
        }

        ItemPrototype const* item_proto = ObjectMgr::GetItemPrototype(item_id);
        if (!item_proto)
        {
            PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
            SetSentErrorMessage(true);
            return false;
        }

        if (item_count < 1 || (item_proto->MaxCount > 0 && item_count > uint32(item_proto->MaxCount)))
        {
            PSendSysMessage(LANG_COMMAND_INVALID_ITEM_COUNT, item_count, item_id);
            SetSentErrorMessage(true);
            return false;
        }

        uint32 max_items_count = item_proto->GetMaxStackSize();
        uint32 remaining_items_count = item_count;

        while (remaining_items_count > max_items_count)
        {
            items.push_back(ItemToSend(item_id, max_items_count, item_enchant_id));
            remaining_items_count -= max_items_count;
        }

        items.push_back(ItemToSend(item_id, remaining_items_count, item_enchant_id));

        if (items.size() > MAX_MAIL_ITEMS)
        {
            PSendSysMessage(LANG_COMMAND_MAIL_ITEMS_LIMIT, MAX_MAIL_ITEMS);
            SetSentErrorMessage(true);
            return false;
        }
    }

    draft.SetSubjectAndBody(msgSubject, msgText);

    for (ItemsToSend::iterator itr = items.begin(); itr != items.end(); ++itr)
    {
        uint32 item_id =  std::get<0>(*itr);
        uint32 item_count = std::get<1>(*itr);
        if (Item* item = Item::CreateItem(item_id, item_count, m_session ? m_session->GetPlayer() : 0))
        {
            uint32 item_enchant_id = std::get<2>(*itr);
            if (item_enchant_id)
            {
                item->SetEnchantment(PERM_ENCHANTMENT_SLOT, item_enchant_id, 0, 0);
            }
            item->SaveToDB();
            draft.AddItem(item);
        }
    }

    return true;
}

bool ChatHandler::HandleSendItemsCommand(char* args)
{

    Player* receiver;
    ObjectGuid receiver_guid = 0;
    std::string receiver_name;
    if (!ExtractPlayerTarget(&args, &receiver, &receiver_guid, &receiver_name))
    {
        return false;
    }

    MailDraft draft;

    if (!HandleSendItemsHelper(draft, args))
    {
        return false;
    }

    MailSender sender(MAIL_NORMAL, m_session ? m_session->GetPlayer()->GetGUIDLow() : (uint32)0, MAIL_STATIONERY_GM);

    draft.SendMailTo(MailReceiver(receiver, receiver_guid), sender);

    std::string nameLink = playerLink(receiver_name);
    PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
    return true;
}

bool ChatHandler::HandleSendMassItemsCommand(char* args)
{

    uint32 raceMask = 0;
    char const* name = nullptr;

    if (!ExtractRaceMask(&args, raceMask, &name))
    {
        return false;
    }

    MailDraft* draft = new MailDraft;

    if (!HandleSendItemsHelper(*draft, args))
    {
        delete draft;
        return false;
    }

    MailSender sender(MAIL_NORMAL, (uint32)0, MAIL_STATIONERY_GM);

    sMassMailMgr.AddMassMailTask(draft, sender, raceMask);

    PSendSysMessage(LANG_MAIL_SENT, name);
    return true;
}

bool ChatHandler::HandleSendMoneyHelper(MailDraft& draft, char* args)
{

    char* msgSubject = ExtractQuotedArg(&args);
    if (!msgSubject)
    {
        return false;
    }

    char* msgText = ExtractQuotedArg(&args);
    if (!msgText)
    {
        return false;
    }

    uint32 money;
    if (!ExtractUInt32(&args, money))
    {
        return false;
    }

    if (money <= 0)
    {
        return false;
    }

    draft.SetSubjectAndBody(msgSubject, msgText).SetMoney(money);

    return true;
}

bool ChatHandler::HandleSendMoneyCommand(char* args)
{

    Player* receiver;
    ObjectGuid receiver_guid = 0;
    std::string receiver_name;
    if (!ExtractPlayerTarget(&args, &receiver, &receiver_guid, &receiver_name))
    {
        return false;
    }

    MailDraft draft;

    if (!HandleSendMoneyHelper(draft, args))
    {
        return false;
    }

    MailSender sender(MAIL_NORMAL, (uint32)0, MAIL_STATIONERY_GM);

    draft.SendMailTo(MailReceiver(receiver, receiver_guid), sender);

    std::string nameLink = playerLink(receiver_name);
    PSendSysMessage(LANG_MAIL_SENT, nameLink.c_str());
    return true;
}

bool ChatHandler::HandleSendMassMoneyCommand(char* args)
{

    uint32 raceMask = 0;
    char const* name = nullptr;

    if (!ExtractRaceMask(&args, raceMask, &name))
    {
        return false;
    }

    MailDraft* draft = new MailDraft;

    if (!HandleSendMoneyHelper(*draft, args))
    {
        delete draft;
        return false;
    }

    MailSender sender(MAIL_NORMAL, (uint32)0, MAIL_STATIONERY_GM);

    sMassMailMgr.AddMassMailTask(draft, sender, raceMask);

    PSendSysMessage(LANG_MAIL_SENT, name);
    return true;
}
