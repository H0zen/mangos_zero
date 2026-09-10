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
#include "World.h"
#include "Config.h"
#include "Version.h"
#include "UpdateTime.h"
#include "CorpseManager.h"

bool ChatHandler::HandleServerInfoCommand(char* )
{
    uint32 activeClientsNum = sWorld.GetActiveSessionCount();
    uint32 queuedClientsNum = sWorld.GetQueuedSessionCount();
    uint32 maxActiveClientsNum = sWorld.GetMaxActiveSessionCount();
    uint32 maxQueuedClientsNum = sWorld.GetMaxQueuedSessionCount();
    std::string str = secsToTimeString(sWorld.GetUptime());
    uint32 updateTime = sWorldUpdateTime.GetLastUpdateTime();

    char const* full;
    full = MangosVersion::ProductRevision();
    SendSysMessage(full);

    if (sScriptMgr.IsScriptLibraryLoaded())
    {
        char const* ver = sScriptMgr.GetScriptLibraryVersion();
        if (ver && *ver)
        {
            PSendSysMessage(LANG_USING_SCRIPT_LIB, ver);
        }
        else
        {
            SendSysMessage(LANG_USING_SCRIPT_LIB_UNKNOWN);
        }
    }
    else
    {
        SendSysMessage(LANG_USING_SCRIPT_LIB_NONE);
    }

    PSendSysMessage("%s", MangosVersion::FullRevision());
    PSendSysMessage("%s", MangosVersion::RunningSystem());

    PSendSysMessage(LANG_USING_WORLD_DB, sWorld.GetDBVersion());
    PSendSysMessage(LANG_CONNECTED_USERS, activeClientsNum, maxActiveClientsNum, queuedClientsNum, maxQueuedClientsNum);
    PSendSysMessage(LANG_UPTIME, str.c_str());
    PSendSysMessage("World Delay: %u", updateTime);

    return true;
}

bool ChatHandler::HandleServerMotdCommand(char* )
{
    PSendSysMessage(LANG_MOTD_CURRENT, sWorld.GetMotd());
    return true;
}

bool ChatHandler::HandleServerShutDownCancelCommand(char* )
{
    sWorld.ShutdownCancel();
    return true;
}

bool ChatHandler::HandleServerShutDownCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    char* timeStr = strtok((char*)args, " ");
    char* exitCodeStr = strtok(nullptr, "");

    int32 time = atoi(timeStr);

    if ((time == 0 && (timeStr[0] != '0' || timeStr[1] != '\0')) || time < 0)
    {
        return false;
    }

    if (exitCodeStr)
    {
        int32 exitCode = atoi(exitCodeStr);

        if (exitCode == 0 && (exitCodeStr[0] != '0' || exitCodeStr[1] != '\0'))
        {
            return false;
        }

        if (exitCode < 0 || exitCode > 125)
        {
            return false;
        }

        sWorld.ShutdownServ(time, SHUTDOWN_MASK_STOP, exitCode);
    }
    else
    {
        sWorld.ShutdownServ(time, SHUTDOWN_MASK_STOP, SHUTDOWN_EXIT_CODE);
    }

    return true;
}

bool ChatHandler::HandleServerRestartCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    char* timeStr = strtok((char*)args, " ");
    char* exitCodeStr = strtok(nullptr, "");

    int32 time = atoi(timeStr);

    if ((time == 0 && (timeStr[0] != '0' || timeStr[1] != '\0')) || time < 0)
    {
        return false;
    }

    if (exitCodeStr)
    {
        int32 exitCode = atoi(exitCodeStr);

        if (exitCode == 0 && (exitCodeStr[0] != '0' || exitCodeStr[1] != '\0'))
        {
            return false;
        }

        if (exitCode < 0 || exitCode > 125)
        {
            return false;
        }

        sWorld.ShutdownServ(time, SHUTDOWN_MASK_RESTART, exitCode);
    }
    else
    {
        sWorld.ShutdownServ(time, SHUTDOWN_MASK_RESTART, RESTART_EXIT_CODE);
    }

    return true;
}

bool ChatHandler::HandleServerIdleRestartCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    char* timeStr = strtok((char*)args, " ");
    char* exitCodeStr = strtok(nullptr, "");

    int32 time = atoi(timeStr);

    if ((time == 0 && (timeStr[0] != '0' || timeStr[1] != '\0')) || time < 0)
    {
        return false;
    }

    if (exitCodeStr)
    {
        int32 exitCode = atoi(exitCodeStr);

        if (exitCode == 0 && (exitCodeStr[0] != '0' || exitCodeStr[1] != '\0'))
        {
            return false;
        }

        if (exitCode < 0 || exitCode > 125)
        {
            return false;
        }

        sWorld.ShutdownServ(time, SHUTDOWN_MASK_IDLE, exitCode);
    }
    else
    {
        sWorld.ShutdownServ(time, SHUTDOWN_MASK_IDLE, SHUTDOWN_EXIT_CODE);
    }

    return true;
}

bool ChatHandler::HandleServerIdleShutDownCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    char* timeStr = strtok((char*)args, " ");
    char* exitCodeStr = strtok(nullptr, "");

    int32 time = atoi(timeStr);

    if ((time == 0 && (timeStr[0] != '0' || timeStr[1] != '\0')) || time < 0)
    {
        return false;
    }

    if (exitCodeStr)
    {
        int32 exitCode = atoi(exitCodeStr);

        if (exitCode == 0 && (exitCodeStr[0] != '0' || exitCodeStr[1] != '\0'))
        {
            return false;
        }

        if (exitCode < 0 || exitCode > 125)
        {
            return false;
        }

        sWorld.ShutdownServ(time, SHUTDOWN_MASK_IDLE, exitCode);
    }
    else
    {
        sWorld.ShutdownServ(time, SHUTDOWN_MASK_IDLE, RESTART_EXIT_CODE);
    }

    return true;
}

bool ChatHandler::HandleServerExitCommand(char* )
{
    SendSysMessage(LANG_COMMAND_EXIT);
    World::StopNow(SHUTDOWN_EXIT_CODE);
    return true;
}

bool ChatHandler::HandleServerLogFilterCommand(char* args)
{
    if (!*args)
    {
        SendSysMessage(LANG_LOG_FILTERS_STATE_HEADER);
        for (int i = 0; i < LOG_FILTER_COUNT; ++i)
        {
            if (*logFilterData[i].name)
            {
                PSendSysMessage("  %-20s = %s", logFilterData[i].name, GetOnOffStr(sLog.HasLogFilter(1 << i)));
            }
        }
        return true;
    }

    char* filtername = ExtractLiteralArg(&args);
    if (!filtername)
    {
        return false;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (strncmp(filtername, "all", 4) == 0)
    {
        sLog.SetLogFilter(LogFilters(0xFFFFFFFF), value);
        PSendSysMessage(LANG_ALL_LOG_FILTERS_SET_TO_S, GetOnOffStr(value));
        return true;
    }

    for (int i = 0; i < LOG_FILTER_COUNT; ++i)
    {
        if (!*logFilterData[i].name)
        {
            continue;
        }

        if (!strncmp(filtername, logFilterData[i].name, strlen(filtername)))
        {
            sLog.SetLogFilter(LogFilters(1 << i), value);
            PSendSysMessage("  %-20s = %s", logFilterData[i].name, GetOnOffStr(value));
            return true;
        }
    }

    return false;
}

bool ChatHandler::HandleServerLogLevelCommand(char* args)
{
    if (!*args)
    {
        PSendSysMessage("Log level: %u", sLog.GetLogLevel());
        return true;
    }

    sLog.SetLogLevel(args);
    return true;
}

bool ChatHandler::HandleServerCorpsesCommand(char* )
{
    sCorpseManager.RemoveOldCorpses();
    return true;
}

bool ChatHandler::HandleServerResetAllRaidCommand(char* args)
{
    PSendSysMessage("Global raid instances reset, all players in raid instances will be teleported to homebind!");
    sMapPersistentStateMgr.GetScheduler().ResetAllRaid();
    return true;
}

bool ChatHandler::HandleServerSetMotdCommand(char* args)
{
    sWorld.SetMotd(args);
    PSendSysMessage(LANG_MOTD_NEW, args);
    return true;
}

bool ChatHandler::HandleServerPLimitCommand(char* args)
{
    if (*args)
    {
        char* param = ExtractLiteralArg(&args);
        if (!param)
        {
            return false;
        }

        int l = strlen(param);

        int val;
        if (strncmp(param, "player", l) == 0)
        {
            sWorld.SetPlayerLimit(-SEC_PLAYER);
        }
        else if (strncmp(param, "moderator", l) == 0)
        {
            sWorld.SetPlayerLimit(-SEC_MODERATOR);
        }
        else if (strncmp(param, "gamemaster", l) == 0)
        {
            sWorld.SetPlayerLimit(-SEC_GAMEMASTER);
        }
        else if (strncmp(param, "administrator", l) == 0)
        {
            sWorld.SetPlayerLimit(-SEC_ADMINISTRATOR);
        }
        else if (strncmp(param, "reset", l) == 0)
        {
            sWorld.SetPlayerLimit(sConfig.GetIntDefault("PlayerLimit", DEFAULT_PLAYER_LIMIT));
        }
        else if (ExtractInt32(&param, val))
        {
            if (val < -SEC_ADMINISTRATOR)
            {
                val = -SEC_ADMINISTRATOR;
            }

            sWorld.SetPlayerLimit(val);
        }
        else
        {
            return false;
        }

        if (sWorld.GetPlayerAmountLimit() > SEC_PLAYER)
        {
            sWorld.KickAllLess(sWorld.GetPlayerSecurityLimit());
        }
    }

    uint32 pLimit = sWorld.GetPlayerAmountLimit();
    AccountTypes allowedAccountType = sWorld.GetPlayerSecurityLimit();
    char const* secName;
    switch (allowedAccountType)
    {
        case SEC_PLAYER:        secName = "Player";        break;
        case SEC_MODERATOR:     secName = "Moderator";     break;
        case SEC_GAMEMASTER:    secName = "Gamemaster";    break;
        case SEC_ADMINISTRATOR: secName = "Administrator"; break;
        default:                secName = "<unknown>";     break;
    }

    PSendSysMessage("Player limits: amount %u, min. security level %s.", pLimit, secName);

    return true;
}
