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

#include <algorithm>
#include <string>
#include "Chat.h"
#include "ObjectMgr.h"
#include "World.h"
#include "GMTicketMgr.h"
#include "Mail.h"
#include "PlayerRegistry.h"

void ChatHandler::ShowTicket(GMTicket const* ticket)
{
    std::string lastupdated = TimeToTimestampStr(ticket->GetLastUpdate());

    std::string name;
    if (!sObjectMgr.GetPlayerNameByGUID(ticket->GetPlayerGuid(), name))
    {
        name = GetMangosString(LANG_UNKNOWN);
    }

    std::string nameLink = playerLink(name);

    char const* response = ticket->GetResponse();

    PSendSysMessage(LANG_COMMAND_TICKETVIEW, nameLink.c_str(), lastupdated.c_str(), ticket->GetText());
    if (strlen(response))
    {
        PSendSysMessage(LANG_COMMAND_TICKETRESPONSE, ticket->GetResponse());
    }
}

bool ChatHandler::HandleTicketAcceptCommand(char* args)
{
    char* px = ExtractLiteralArg(&args);

    if (!px)
    {
        return false;
    }

    if (strncmp(px, "on", 3) == 0)
    {
        sTicketMgr.SetAcceptTickets(true);
        SendSysMessage(LANG_COMMAND_TICKETS_SYSTEM_ON);
    }

    else if (strncmp(px, "off", 4) == 0)
    {
        sTicketMgr.SetAcceptTickets(false);
        SendSysMessage(LANG_COMMAND_TICKETS_SYSTEM_OFF);
    }
    else
    {
        return false;
    }

    return true;
}

bool ChatHandler::HandleTicketCloseCommand(char* args)
{
    GMTicket* ticket = nullptr;

    uint32 num;
    if (ExtractUInt32(&args, num))
    {
        if (num == 0)
        {
            return false;
        }

        ticket = sTicketMgr.GetGMTicket(num);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST, num);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        ObjectGuid target_guid = 0;
        std::string target_name;
        if (!ExtractPlayerTarget(&args, nullptr, &target_guid, &target_name))
        {
            return false;
        }

        ticket = sTicketMgr.GetGMTicket(target_guid);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST_NAME, target_name.c_str());
            SetSentErrorMessage(true);
            return false;
        }
    }

    ObjectGuid target_guid = ticket->GetPlayerGuid();

    Player* pPlayer = sObjectMgr.GetPlayer(target_guid);

    std::string target_name;
    sObjectMgr.GetPlayerNameByGUID(target_guid, target_name);

    if (!pPlayer && !sWorld.getConfig(CONFIG_BOOL_GM_TICKET_OFFLINE_CLOSING))
    {
        SendSysMessage(LANG_COMMAND_TICKET_CANT_CLOSE);
        return false;
    }

    if (*args)
    {
        ticket->SetResponseText(args);
    }

    if (!*ticket->GetResponse())
    {
        const uint32 responseBufferSize = 256;
        char response[responseBufferSize];

        if (m_session)
        {
            const char* format = "[System Message] This ticket was closed by <GM> %s without any written response, perhaps it was resolved by direct chat.";
            const char* buffer;
            snprintf(response, responseBufferSize, format, m_session->GetPlayer()->GetName());
        }
        else
        {
            strcpy(response, "[System Message] this ticket was closed using CLI console.");
        }

        ticket->SetResponseText(response);
    }

    ticket->Close();

    uint32 ticketId = ticket->GetId();

    sTicketMgr.Delete(ticket->GetPlayerGuid());

    const char* gmNameReplacementWhenUsingCLI = "ADMIN";

    sPlayerRegistry.ForEach([&](Player* player)
    {
        if (player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && player->isAcceptTickets())
        {
            ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETCLOSED_NAME, ticketId, target_name.c_str(), m_session ? m_session->GetPlayer()->GetName() : gmNameReplacementWhenUsingCLI);
        }
    });

    if (!m_session)
    {

        PSendSysMessage(LANG_COMMAND_TICKETCLOSED_NAME, ticketId, target_name.c_str(), gmNameReplacementWhenUsingCLI);
    }

    return true;
}

bool ChatHandler::HandleTicketDeleteCommand(char* args)
{
    char* px = ExtractLiteralArg(&args);
    if (!px)
    {
        return false;
    }

    if (strncmp(px, "all", 4) == 0)
    {
        sTicketMgr.DeleteAll();
        SendSysMessage(LANG_COMMAND_ALLTICKETDELETED);
        return true;
    }

    uint32 num;

    if (ExtractUInt32(&px, num))
    {
        if (num == 0)
        {
            return false;
        }

        GMTicket* ticket = sTicketMgr.GetGMTicket(num);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST, num);
            SetSentErrorMessage(true);
            return false;
        }

        ObjectGuid guid = ticket->GetPlayerGuid();

        sTicketMgr.Delete(guid);

        if (Player* pl = sObjectMgr.GetPlayer(guid))
        {
            pl->GetSession()->SendGMTicketGetTicket(0x0A);
            PSendSysMessage(LANG_COMMAND_TICKETPLAYERDEL, GetNameLink(pl).c_str());
        }
        else
        {
            PSendSysMessage(LANG_COMMAND_TICKETDEL);
        }

        return true;
    }

    Player* target;
    ObjectGuid target_guid = 0;
    std::string target_name;
    if (!ExtractPlayerTarget(&px, &target, &target_guid, &target_name))
    {
        return false;
    }

    sTicketMgr.Delete(target_guid);

    if (target)
    {
        target->GetSession()->SendGMTicketGetTicket(0x0A);
    }

    std::string nameLink = playerLink(target_name);

    PSendSysMessage(LANG_COMMAND_TICKETPLAYERDEL, nameLink.c_str());
    return true;
}

bool ChatHandler::HandleTicketInfoCommand(char* args)
{
    size_t count = sTicketMgr.GetTicketCount();

    if (m_session)
    {
        PSendSysMessage(LANG_COMMAND_TICKETCOUNT, count, GetOnOffStr(m_session->GetPlayer()->isAcceptTickets()));
    }
    else
    {
        PSendSysMessage(LANG_COMMAND_TICKETCOUNT_CONSOLE, count);
    }

    return true;
}

bool ChatHandler::HandleTicketListCommand(char* args)
{
    uint16 numToShow = std::min(uint16(sTicketMgr.GetTicketCount()), uint16(sWorld.getConfig(CONFIG_UINT32_GM_TICKET_LIST_SIZE)));
    for (uint16 i = 0; i < numToShow; ++i)
    {
        GMTicket* ticket = sTicketMgr.GetGMTicketByOrderPos(i);
        time_t lastChanged = time_t(ticket->GetLastUpdate());
        PSendSysMessage(LANG_COMMAND_TICKET_OFFLINE_INFO, ticket->GetId(), GuidCounter(ticket->GetPlayerGuid()), ticket->HasResponse() ? "+" : "-", ctime(&lastChanged));
    }

    PSendSysMessage(LANG_COMMAND_TICKET_COUNT_ALL, numToShow, sTicketMgr.GetTicketCount());
    return true;
}

bool ChatHandler::HandleTicketOnlineListCommand(char* args)
{
    uint16 count = 0;
    for (uint16 i = 0; i < sTicketMgr.GetTicketCount(); ++i)
    {
        GMTicket* ticket = sTicketMgr.GetGMTicketByOrderPos(i);
        if (Player* player = sObjectMgr.GetPlayer(ticket->GetPlayerGuid(), true))
        {
            ++count;
            if (i < sWorld.getConfig(CONFIG_UINT32_GM_TICKET_LIST_SIZE))
            {
                time_t lastChanged = time_t(ticket->GetLastUpdate());
                PSendSysMessage(LANG_COMMAND_TICKET_BRIEF_INFO, ticket->GetId(), player->GetName(), ticket->HasResponse() ? "+" : "-", ctime(&lastChanged));
            }
        }
    }

    PSendSysMessage(LANG_COMMAND_TICKET_COUNT_ONLINE, std::min(count, uint16(sWorld.getConfig(CONFIG_UINT32_GM_TICKET_LIST_SIZE))), count);
    return true;
}

bool ChatHandler::HandleTicketMeAcceptCommand(char* args)
{
    char* px = ExtractLiteralArg(&args);
    if (!px)
    {
        PSendSysMessage(LANG_COMMAND_TICKET_ACCEPT_STATE, m_session->GetPlayer()->isAcceptTickets() ? "on" : "off");
        return true;
    }

    if (!m_session)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    if (strncmp(px, "on", 3) == 0)
    {
        m_session->GetPlayer()->SetAcceptTicket(true);
        SendSysMessage(LANG_COMMAND_TICKETON);
    }

    else if (strncmp(px, "off", 4) == 0)
    {
        m_session->GetPlayer()->SetAcceptTicket(false);
        SendSysMessage(LANG_COMMAND_TICKETOFF);
    }
    else
    {
        return false;
    }

    return true;
}

bool ChatHandler::HandleTicketRespondCommand(char* args)
{
    GMTicket* ticket = nullptr;

    uint32 num;
    if (ExtractUInt32(&args, num))
    {
        if (num == 0)
        {
            return false;
        }

        ticket = sTicketMgr.GetGMTicket(num);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST, num);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        ObjectGuid target_guid = 0;
        std::string target_name;
        if (!ExtractPlayerTarget(&args, nullptr, &target_guid, &target_name))
        {
            return false;
        }

        ticket = sTicketMgr.GetGMTicket(target_guid);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST_NAME, target_name.c_str());
            SetSentErrorMessage(true);
            return false;
        }
    }

    if (!*args)
    {
        return false;
    }

    ticket->SetResponseText(args);

    MailDraft draft;

    const char* signatureFormat = GetMangosString(LANG_COMMAND_TICKET_RESPOND_MAIL_SIGNATURE);
    const uint32 signatureBufferSize = 256;
    char signature[signatureBufferSize];

    if (m_session)
    {
        snprintf(signature, signatureBufferSize, signatureFormat, m_session->GetPlayer()->GetName());
    }
    else
    {

        strcpy(signature, "$B$BBest regards, $B$BThe Server Admin");
    }

    std::string  mailText = args;
    mailText = mailText + signature;

    draft.SetSubjectAndBody(GetMangosString(LANG_COMMAND_TICKET_RESPOND_MAIL_SUBJECT), mailText);

    uint32 senderGuidLow = 0;
    if (m_session)
    {
        senderGuidLow = m_session->GetPlayer()->GetGUIDLow();
    }

    MailSender sender(MAIL_NORMAL, senderGuidLow, MAIL_STATIONERY_GM);

    ObjectGuid target_guid = ticket->GetPlayerGuid();

    Player* target = sObjectMgr.GetPlayer(target_guid);

    std::string target_name;
    sObjectMgr.GetPlayerNameByGUID(target_guid, target_name);

    draft.SendMailTo(MailReceiver(target, target_guid), sender);

    const char* gmNameReplacementWhenUsingCLI = "ADMIN";

    if (target && target->IsInWorld())
    {
        ChatHandler(target).PSendSysMessageMultiline(LANG_COMMAND_TICKETCLOSED_PLAYER_NOTIF, m_session ? m_session->GetPlayer()->GetName() : gmNameReplacementWhenUsingCLI);
    }

    uint32 ticketId = ticket->GetId();

    ticket->Close();

    sTicketMgr.Delete(ticket->GetPlayerGuid());

    sPlayerRegistry.ForEach([&](Player* player)
    {
        if (player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && player->isAcceptTickets())
        {
            ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETCLOSED_NAME, ticketId, target_name.c_str(), m_session ? m_session->GetPlayer()->GetName() : gmNameReplacementWhenUsingCLI);
        }
    });

    if (!m_session)
    {

        PSendSysMessage(LANG_COMMAND_TICKETCLOSED_NAME, ticketId, target_name.c_str(), gmNameReplacementWhenUsingCLI);
    }

    return true;
}

bool ChatHandler::HandleTicketShowCommand(char* args)
{

    char* px = ExtractLiteralArg(&args);
    if (!px)
    {
        return false;
    }

    uint32 num;
    if (ExtractUInt32(&px, num))
    {
        if (num == 0)
        {
            return false;
        }

        GMTicket* ticket = sTicketMgr.GetGMTicket(num);
        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST, num);
            SetSentErrorMessage(true);
            return false;
        }

        ShowTicket(ticket);
        return true;
    }

    ObjectGuid target_guid = 0;
    std::string target_name;
    if (!ExtractPlayerTarget(&px, nullptr, &target_guid, &target_name))
    {
        return false;
    }

    GMTicket* ticket = sTicketMgr.GetGMTicket(target_guid);
    if (!ticket)
    {
        PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST_NAME, target_name.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    ShowTicket(ticket);

    return true;
}

bool ChatHandler::HandleTickerSurveyClose(char* args)
{
    GMTicket* ticket = nullptr;
    std::string target_name;
    ObjectGuid target_guid = 0;
    uint32 num;
    if (ExtractUInt32(&args, num))
    {
        if (num == 0)
        {
            return false;
        }

        ticket = sTicketMgr.GetGMTicket(num);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST, num);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {

        if (!ExtractPlayerTarget(&args, nullptr, &target_guid, &target_name))
        {
            return false;
        }

        ticket = sTicketMgr.GetGMTicket(target_guid);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST_NAME, target_name.c_str());
            SetSentErrorMessage(true);
            return false;
        }
    }

    uint32 ticketId = ticket->GetId();
    ticket->CloseWithSurvey();

    Player* pPlayer = sObjectMgr.GetPlayer(ticket->GetPlayerGuid());

    if (!pPlayer)
    {
        SendSysMessage(LANG_COMMAND_TICKET_CANT_CLOSE);
        return false;
    }

    sTicketMgr.Delete(ticket->GetPlayerGuid());
    ticket = nullptr;

    const char* gmNameReplacementWhenUsingCLI = "ADMIN";

    PSendSysMessage(LANG_COMMAND_TICKETCLOSED_NAME, ticketId, target_name.c_str(), m_session ? m_session->GetPlayer()->GetName() : gmNameReplacementWhenUsingCLI);

    return true;
}
