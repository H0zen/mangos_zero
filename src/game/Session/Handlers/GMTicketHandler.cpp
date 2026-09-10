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
#include "TicketAnswers.h"
#include "Platform/Define.h"
#include <string>
#include "Language.h"
#include "WorldPacket.h"
#include "Log.h"
#include "GMTicketMgr.h"
#include "PlayerRegistry.h"
#include "Player.h"
#include "Chat.h"

void WorldSession::SendGMTicketGetTicket(uint32 status, GMTicket* ticket )
{
    std::string text = ticket ? ticket->GetText() : "";

    int len = text.size() + 1;
    WorldPacket data(SMSG_GMTICKET_GETTICKET, (4 + len + 1 + 4 + 2 + 4 + 4));
    data << uint32(status);
    if (status == 6)
    {
        data << text;
        data << uint8(0x7);
        data << float(0);
        data << float(0);
        data << float(0);
        data << uint8(0);
        data << uint8(0);
    }
    SendPacket(&data);
}

void tickets::GMTicketGetTicket(WorldSession& session, WorldPacket& )
{
    session.SendQueryTimeResponse();

    GMTicket* ticket = sTicketMgr.GetGMTicket(session.GetPlayer()->GetObjectGuid());
    if (ticket)
    {
        session.SendGMTicketGetTicket(0x06, ticket);
    }
    else
    {
        session.SendGMTicketGetTicket(0x0A);
    }
}

void tickets::GMTicketUpdateText(Player& who, WorldPacket& recv_data)
{
    std::string ticketText;
    recv_data >> ticketText;

    stripLineInvisibleChars(ticketText);

    ltrim(ticketText);

    GMTicketResponse responce = GMTICKET_RESPONSE_UPDATE_SUCCESS;
    if (GMTicket* ticket = sTicketMgr.GetGMTicket(who.GetObjectGuid()))
    {
        ticket->SetText(ticketText.c_str());
    }
    else
    {
        sLog.outError("Ticket update: Player %s (GUID: %u) doesn't have active ticket", who.GetName(), who.GetGUIDLow());
        responce = GMTICKET_RESPONSE_UPDATE_ERROR;
    }

    WorldPacket data(SMSG_GMTICKET_UPDATETEXT, 4);
    data << uint32(responce);
    who.GetSession()->SendPacket(&data);

    GMTicket * ticket = sTicketMgr.GetGMTicket(who.GetObjectGuid());

    sPlayerRegistry.ForEach([ticket, &who](Player* player)
    {
        if (player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && player->isAcceptTickets())
        {
            ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETUPDATED, who.GetName(), ticket->GetId());

        }
    }
    );
}

void WorldSession::SendGMTicketStatusUpdate(GMTicketStatus statusCode)
{
    WorldPacket data(SMSG_GM_TICKET_STATUS_UPDATE, 4);
    data << uint32(statusCode);
    SendPacket(&data);
}

void tickets::GMTicketDeleteTicket(Player& who, WorldPacket& )
{

    GMTicket *ticket = sTicketMgr.GetGMTicket(who.GetObjectGuid());
    if (ticket)
    {
        ticket->CloseByClient();
    }
    sTicketMgr.Delete(who.GetObjectGuid());

    WorldPacket data(SMSG_GMTICKET_DELETETICKET, 4);
    data << uint32(GMTICKET_RESPONSE_TICKET_DELETED);
    who.GetSession()->SendPacket(&data);

    who.GetSession()->SendGMTicketGetTicket(0x0A);
}

void tickets::GMTicketCreate(WorldSession& session, WorldPacket& recv_data)
{
    uint32 mapId;
    uint8 category;
    float x, y, z;
    std::string ticketText = "";
    recv_data >> category;
    recv_data >> mapId >> x >> y >> z;
    recv_data >> ticketText;

    std::string reserved;
    recv_data >> reserved;

    if (category == 2)
    {
        uint32 chatDataLineCount;
        recv_data >> chatDataLineCount;

        uint32 chatDataSizeInflated;
        recv_data >> chatDataSizeInflated;

        if (size_t chatDataSizeDeflated = (recv_data.size() - recv_data.rpos()))
        {
            recv_data.read_skip(chatDataSizeDeflated);
        }
    }

    DEBUG_LOG("TicketCreate: map %u, x %f, y %f, z %f, text %s", mapId, x, y, z, ticketText.c_str());

    if (sTicketMgr.GetGMTicket(session.GetPlayer()->GetObjectGuid()))
    {
        WorldPacket data(SMSG_GMTICKET_CREATE, 4);
        data << uint32(GMTICKET_RESPONSE_ALREADY_EXIST);
        session.SendPacket(&data);
        return;
    }

    sTicketMgr.Create(session.GetPlayer()->GetObjectGuid(), ticketText.c_str());

    session.SendQueryTimeResponse();

    WorldPacket data(SMSG_GMTICKET_CREATE, 4);
    data << uint32(GMTICKET_RESPONSE_CREATE_SUCCESS);
    session.SendPacket(&data);

    GMTicket * ticket = sTicketMgr.GetGMTicket(session.GetPlayer()->GetObjectGuid());

    sPlayerRegistry.ForEach([ticket, &session](Player* player)
    {
        if (player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && player->isAcceptTickets())
        {
            ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETNEW, session.GetPlayer()->GetName(), ticket->GetId());
        }
    }
    );
}

void tickets::GMTicketSystemStatus(Player& who, WorldPacket& )
{
    WorldPacket data(SMSG_GMTICKET_SYSTEMSTATUS, 4);

    data << uint32(sTicketMgr.WillAcceptTickets() ? 1 : 0);
    who.GetSession()->SendPacket(&data);
}

void tickets::GMTicketSurveySubmit(Player& who, WorldPacket& recv_data)
{

    GMTicket* ticket = sTicketMgr.GetGMTicket(who.GetObjectGuid());
    if (!ticket)
    {
        return;
    }

    ticket->SaveSurveyData(recv_data);
}
