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
#include "Utilities/Util.h"
#include "Chat.h"
#include "Language.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "World.h"
#include "Player.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "AccountMgr.h"
#include "SpellMgr.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "CommandMgr.h"

void ChatHandler::SendSysMessage(const char* str)
{
    WorldPacket data;

    char* buf = mangos_strdup(str);
    char* pos = buf;

    while (char* line = LineFromMessage(pos))
    {

        ObjectGuid senderGuid = 0;
        if (m_session)
        {
            senderGuid = m_session->GetPlayer()->GetObjectGuid();
        }

        ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, line, LANG_UNIVERSAL, CHAT_TAG_NONE, senderGuid);
        if (m_session)
        {
            m_session->SendPacket(&data);
        }
    }

    delete[] buf;
}

void ChatHandler::SendGlobalSysMessage(const char* str, AccountTypes minSec)
{

    WorldPacket data;

    char* buf = mangos_strdup(str);
    char* pos = buf;
    ObjectGuid senderGuid = m_session ? m_session->GetPlayer()->GetObjectGuid() : 0;

    while (char* line = LineFromMessage(pos))
    {
        ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, line, LANG_UNIVERSAL, CHAT_TAG_NONE, senderGuid);
        sWorld.SendGlobalMessage(&data, minSec);
    }

    delete[] buf;
}

void ChatHandler::SendSysMessage(int32 entry)
{
    SendSysMessage(GetMangosString(entry));
}

void ChatHandler::PSendSysMessage(int32 entry, ...)
{
    const char* format = GetMangosString(entry);
    va_list ap;
    char str [2048];
    va_start(ap, entry);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

void  ChatHandler::PSendSysMessageMultiline(int32 entry, ...)
{
    uint32 linecount = 0;

    const char* format = GetMangosString(entry);
    va_list ap;
    char str[2048];
    va_start(ap, entry);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);

    std::string mangosString(str);

    std::string::size_type pos = 0, nextpos;

    while ((nextpos = mangosString.find("@@", pos)) != std::string::npos)
    {

        if (nextpos != pos)
        {

            PSendSysMessage("%s", mangosString.substr(pos, nextpos - pos).c_str());
            ++linecount;
        }
        pos = nextpos + 2;
    }

    if (pos < mangosString.length())
    {
        PSendSysMessage("%s", mangosString.substr(pos).c_str());
    }
}

void ChatHandler::PSendSysMessage(const char* format, ...)
{
    va_list ap;
    char str [2048];
    va_start(ap, format);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

void ChatHandler::BuildChatPacket(WorldPacket& data, ChatMsg msgtype, char const* message, Language language , ChatTagFlags chatTag ,
    ObjectGuid const& senderGuid , char const* senderName ,
    ObjectGuid const& targetGuid , char const*  ,
    char const* channelName , uint8 playerRank )
{
    data.Initialize(SMSG_MESSAGECHAT);
    data << uint8(msgtype);
    data << uint32(language);

    switch (msgtype)
    {
        case CHAT_MSG_MONSTER_WHISPER:
        case CHAT_MSG_RAID_BOSS_WHISPER:
        case CHAT_MSG_RAID_BOSS_EMOTE:
        case CHAT_MSG_MONSTER_EMOTE:
            MANGOS_ASSERT(senderName);
            data << uint32(strlen(senderName) + 1);
            data << senderName;
            data << static_cast<ObjectGuid>(targetGuid);
            break;

        case CHAT_MSG_SAY:
        case CHAT_MSG_PARTY:
        case CHAT_MSG_YELL:
            data << static_cast<ObjectGuid>(senderGuid);
            data << static_cast<ObjectGuid>(senderGuid);
            break;

        case CHAT_MSG_MONSTER_SAY:
        case CHAT_MSG_MONSTER_YELL:
            MANGOS_ASSERT(senderName);
            data << static_cast<ObjectGuid>(senderGuid);
            data << uint32(strlen(senderName) + 1);
            data << senderName;
            data << static_cast<ObjectGuid>(targetGuid);
            break;

        case CHAT_MSG_CHANNEL:
            MANGOS_ASSERT(channelName);
            data << channelName;
            data << uint32(playerRank);
            data << static_cast<ObjectGuid>(senderGuid);
            break;

        default:
            data << static_cast<ObjectGuid>(senderGuid);
            break;
    }

    MANGOS_ASSERT(message);
    data << uint32(strlen(message) + 1);
    data << message;
    data << uint8(chatTag);
}
