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

#include <iterator>
#include <string>
#include "Policies/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "Util.h"
#include "ObjectGuid.h"
#include "WorldSession.h"
#include "SharedDefines.h"
#include <map>

enum GMTicketResponse
{
    GMTICKET_RESPONSE_ALREADY_EXIST     = 1,
    GMTICKET_RESPONSE_CREATE_SUCCESS    = 2,
    GMTICKET_RESPONSE_CREATE_ERROR      = 3,
    GMTICKET_RESPONSE_UPDATE_SUCCESS    = 4,
    GMTICKET_RESPONSE_UPDATE_ERROR      = 5,
    GMTICKET_RESPONSE_TICKET_DELETED    = 9
};

class GMTicket
{
    public:
        explicit GMTicket() : m_guid(), m_ticketId(0), m_text(), m_responseText(), m_lastUpdate(0)
        {}

        void Init(ObjectGuid guid, const std::string& text, const std::string& responseText, time_t update, uint32 ticketId);

        ObjectGuid GetPlayerGuid() const
        {
            return m_guid;
        }

        const char* GetText() const
        {
            return m_text.c_str();
        }

        const char* GetResponse() const
        {
            return m_responseText.c_str();
        }

        uint64 GetLastUpdate() const
        {
            return m_lastUpdate;
        }

        uint32 GetId() const
        {
            return m_ticketId;
        }

        void SetText(const char* text);

        void SetResponseText(const char* text);

        bool HasResponse()
        {
            return !m_responseText.empty();
        }

        void SaveSurveyData(WorldPacket& recvData) const;

        void Close() const;

        void CloseByClient() const;

        void CloseWithSurvey() const;
    private:
        void _Close(GMTicketStatus statusCode) const;

        ObjectGuid m_guid = 0;
        uint32 m_ticketId;
        std::string m_text;
        std::string m_responseText;
        time_t m_lastUpdate;
};
typedef std::map<ObjectGuid, GMTicket> GMTicketMap;
typedef std::map<uint32, GMTicket*> GMTicketIdMap;

class GMTicketMgr
{
    public:

        GMTicketMgr() : m_TicketSystemOn(true), m_GMTicketMap(), m_GMTicketIdMap() {}
        ~GMTicketMgr() {}

        void LoadGMTickets();

        GMTicket* GetGMTicket(ObjectGuid guid)
        {
            GMTicketMap::iterator itr = m_GMTicketMap.find(guid);
            if (itr == m_GMTicketMap.end())
            {
                return nullptr;
            }
            return &(itr->second);
        }

        GMTicket* GetGMTicket(uint32 id)
        {
            GMTicketIdMap::iterator itr = m_GMTicketIdMap.find(id);
            if (itr == m_GMTicketIdMap.end())
            {
                return nullptr;
            }
            return itr->second;
        }

        size_t GetTicketCount() const
        {
            return m_GMTicketMap.size();
        }

        GMTicket* GetGMTicketByOrderPos(uint32 pos)
        {
            if (pos >= GetTicketCount())
            {
                return nullptr;
            }

            GMTicketMap::iterator itr = m_GMTicketMap.begin();
            std::advance(itr, pos);
            if (itr == m_GMTicketMap.end())
            {
                return nullptr;
            }
            return &(itr->second);
        }

        void Delete(ObjectGuid guid)
        {
            GMTicketMap::iterator itr = m_GMTicketMap.find(guid);
            if (itr == m_GMTicketMap.end())
            {
                return;
            }
            m_GMTicketIdMap.erase(itr->second.GetId());
            m_GMTicketMap.erase(itr);
        }

        void DeleteAll();

        void Create(ObjectGuid guid, const char* text);

        void SetAcceptTickets(bool accept) { m_TicketSystemOn = accept; }

        bool WillAcceptTickets() const { return m_TicketSystemOn; }
    private:
        bool m_TicketSystemOn;
        GMTicketMap m_GMTicketMap;
        GMTicketIdMap m_GMTicketIdMap;
};

#define sTicketMgr MaNGOS::Singleton<GMTicketMgr>::Instance()
