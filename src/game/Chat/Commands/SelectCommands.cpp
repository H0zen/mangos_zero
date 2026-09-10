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
#include "Chat.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Language.h"

bool ChatHandler::HandleSelectPlayerCommand(char* args)
{
    if (!*args)
    {
        SendSysMessage("Usage: .select player <player_name>");
        SetSentErrorMessage(true);
        return false;
    }

    std::string playerName = ExtractPlayerNameFromLink(&args);
    if (playerName.empty())
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    normalizePlayerName(playerName);

    Player* target = sObjectMgr.GetPlayer(playerName.c_str());
    ObjectGuid targetGuid = 0;

    if (target)
    {
        targetGuid = target->GetObjectGuid();
    }
    else
    {

        targetGuid = sObjectMgr.GetPlayerGuidByName(playerName);
        if (!targetGuid)
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }

    if (!m_session)
    {
        uint32 accountId = GetAccountId();
        m_consoleSelectedPlayers[accountId] = targetGuid;

        PSendSysMessage("Selected player: %s (GUID: %u)", playerName.c_str(), GuidCounter(targetGuid));
    }
    else
    {
        SendSysMessage("This command is intended for console use. In-game, use target selection instead.");
    }

    return true;
}

bool ChatHandler::HandleSelectClearCommand(char* )
{
    if (!m_session)
    {
        uint32 accountId = GetAccountId();
        auto itr = m_consoleSelectedPlayers.find(accountId);

        if (itr != m_consoleSelectedPlayers.end())
        {
            m_consoleSelectedPlayers.erase(itr);
            SendSysMessage("Console player selection cleared.");
        }
        else
        {
            SendSysMessage("No player currently selected.");
        }
    }
    else
    {
        SendSysMessage("This command is intended for console use.");
    }
    return true;

}
