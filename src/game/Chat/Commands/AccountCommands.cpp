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
#include "World.h"
#include "Chat.h"
#include "AccountMgr.h"
#include "Database/DatabaseEnv.h"
#include "Player.h"

bool ChatHandler::HandleAccountCommand(char* args)
{

    if (*args)
    {
        return false;
    }

    AccountTypes gmlevel = GetAccessLevel();
    PSendSysMessage(LANG_ACCOUNT_LEVEL, uint32(gmlevel));
    return true;
}

bool ChatHandler::HandleAccountPasswordCommand(char* args)
{

    if (!GetAccountId())
    {
        SendSysMessage(LANG_RA_ONLY_COMMAND);
        SetSentErrorMessage(true);
        return false;
    }

    char* old_pass = ExtractQuotedOrLiteralArg(&args);
    char* new_pass = ExtractQuotedOrLiteralArg(&args);
    char* new_pass_c = ExtractQuotedOrLiteralArg(&args);

    if (!old_pass || !new_pass || !new_pass_c)
    {
        return false;
    }

    std::string password_old = old_pass;
    std::string password_new = new_pass;
    std::string password_new_c = new_pass_c;

    if (password_new != password_new_c)
    {
        SendSysMessage(LANG_NEW_PASSWORDS_NOT_MATCH);
        SetSentErrorMessage(true);
        return false;
    }

    if (!sAccountMgr.CheckPassword(GetAccountId(), password_old))
    {
        SendSysMessage(LANG_COMMAND_WRONGOLDPASSWORD);
        SetSentErrorMessage(true);
        return false;
    }

    AccountOpResult result = sAccountMgr.ChangePassword(GetAccountId(), password_new);

    switch (result)
    {
        case AOR_OK:
            SendSysMessage(LANG_COMMAND_PASSWORD);
            break;
        case AOR_PASS_TOO_LONG:
            SendSysMessage(LANG_PASSWORD_TOO_LONG);
            SetSentErrorMessage(true);
            return false;
        case AOR_NAME_NOT_EXIST:
        default:
            SendSysMessage(LANG_COMMAND_NOTCHANGEPASSWORD);
            SetSentErrorMessage(true);
            return false;
    }

    LogCommand(".account password *** *** ***");
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandleAccountLockCommand(char* args)
{

    if (!GetAccountId())
    {
        SendSysMessage(LANG_RA_ONLY_COMMAND);
        SetSentErrorMessage(true);
        return false;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (value)
    {
        LoginDatabase.PExecute("UPDATE `account` SET `locked` = '1' WHERE `id` = '%u'", GetAccountId());
        PSendSysMessage(LANG_COMMAND_ACCLOCKLOCKED);
    }
    else
    {
        LoginDatabase.PExecute("UPDATE `account` SET `locked` = '0' WHERE `id` = '%u'", GetAccountId());
        PSendSysMessage(LANG_COMMAND_ACCLOCKUNLOCKED);
    }

    return true;
}

bool ChatHandler::HandleAccountOnlineListCommand(char* args)
{
    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
    {
        return false;
    }

    QueryResult* result = LoginDatabase.PQuery("SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `active_realm_id` = %u", realmID);

    return ShowAccountListHelper(result, &limit);
}

bool ChatHandler::HandleAccountDeleteCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string account_name;
    uint32 account_id = ExtractAccountId(&args, &account_name);
    if (!account_id)
    {
        return false;
    }

    if (HasLowerSecurityAccount(nullptr, account_id, true))
    {
        return false;
    }

    AccountOpResult result = sAccountMgr.DeleteAccount(account_id);
    switch (result)
    {
        case AOR_OK:
            PSendSysMessage(LANG_ACCOUNT_DELETED, account_name.c_str());
            break;
        case AOR_NAME_NOT_EXIST:
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_name.c_str());
            SetSentErrorMessage(true);
            return false;
        case AOR_DB_INTERNAL_ERROR:
            PSendSysMessage(LANG_ACCOUNT_NOT_DELETED_SQL_ERROR, account_name.c_str());
            SetSentErrorMessage(true);
            return false;
        default:
            PSendSysMessage(LANG_ACCOUNT_NOT_DELETED, account_name.c_str());
            SetSentErrorMessage(true);
            return false;
    }

    return true;
}

bool ChatHandler::HandleAccountCreateCommand(char* args)
{

    char* szAcc = ExtractQuotedOrLiteralArg(&args);
    char* szPassword = ExtractQuotedOrLiteralArg(&args);
    if (!szAcc || !szPassword)
    {
        return false;
    }

    std::string account_name = szAcc;
    std::string password = szPassword;

    AccountOpResult result;
    uint32 expansion = 0;
    if (ExtractUInt32(&args, expansion))
    {

        sAccountMgr.CreateAccount(account_name, password, expansion);
    }
    else
    {
        result = sAccountMgr.CreateAccount(account_name, password);
        switch (result)
        {
            case AOR_OK:
                PSendSysMessage(LANG_ACCOUNT_CREATED, account_name.c_str());
                break;
            case AOR_NAME_TOO_LONG:
                SendSysMessage(LANG_ACCOUNT_TOO_LONG);
                SetSentErrorMessage(true);
                return false;
            case AOR_NAME_ALREADY_EXIST:
                SendSysMessage(LANG_ACCOUNT_ALREADY_EXIST);
                SetSentErrorMessage(true);
                return false;
            case AOR_DB_INTERNAL_ERROR:
                PSendSysMessage(LANG_ACCOUNT_NOT_CREATED_SQL_ERROR, account_name.c_str());
                SetSentErrorMessage(true);
                return false;
            default:
                PSendSysMessage(LANG_ACCOUNT_NOT_CREATED, account_name.c_str());
                SetSentErrorMessage(true);
                return false;
        }
    }
    return true;
}

bool ChatHandler::HandleAccountCharactersCommand(char* args)
{

    std::string account_name;
    Player* target = nullptr;
    uint32 account_id = ExtractAccountId(&args, &account_name, &target);
    if (!account_id)
    {
        return false;
    }

    QueryResult* result = CharacterDatabase.PQuery("SELECT `guid`, `name`, `race`, `class`, `level` FROM `characters` WHERE `account` = %u", account_id);

    return ShowPlayerListHelper(result);
}

bool ChatHandler::HandleAccountSetAddonCommand(char* args)
{

    char* accountStr = ExtractOptNotLastArg(&args);

    std::string account_name;
    uint32 account_id = ExtractAccountId(&accountStr, &account_name);
    if (!account_id)
    {
        return false;
    }

    if (GetAccountId() && GetAccountId() != account_id && HasLowerSecurityAccount(nullptr, account_id, true))
    {
        return false;
    }

    uint32 lev;
    if (!ExtractUInt32(&args, lev))
    {
        return false;
    }

    LoginDatabase.PExecute("UPDATE `account` SET `expansion` = '%u' WHERE `id` = '%u'", lev, account_id);
    PSendSysMessage(LANG_ACCOUNT_SETADDON, account_name.c_str(), account_id, lev);
    return true;
}

bool ChatHandler::HandleAccountSetGmLevelCommand(char* args)
{
    char* accountStr = ExtractOptNotLastArg(&args);

    std::string targetAccountName;
    Player* targetPlayer = nullptr;
    uint32 targetAccountId = ExtractAccountId(&accountStr, &targetAccountName, &targetPlayer);
    if (!targetAccountId)
    {
        return false;
    }

    if (GetAccountId() == targetAccountId)
    {
        return false;
    }

    int32 gm;
    if (!ExtractInt32(&args, gm))
    {
        return false;
    }

    if (gm < SEC_PLAYER || gm > SEC_ADMINISTRATOR)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    if (HasLowerSecurityAccount(nullptr, targetAccountId, true))
    {
        return false;
    }

    AccountTypes plSecurity = GetAccessLevel();
    if (AccountTypes(gm) >= plSecurity)
    {
        SendSysMessage(LANG_YOURS_SECURITY_IS_LOW);
        SetSentErrorMessage(true);
        return false;
    }

    if (targetPlayer)
    {
        ChatHandler(targetPlayer).PSendSysMessage(LANG_YOURS_SECURITY_CHANGED, GetNameLink().c_str(), gm);
        targetPlayer->GetSession()->SetSecurity(AccountTypes(gm));
    }

    PSendSysMessage(LANG_YOU_CHANGE_SECURITY, targetAccountName.c_str(), gm);
    LoginDatabase.PExecute("UPDATE `account` SET `gmlevel` = '%i' WHERE `id` = '%u'", gm, targetAccountId);

    return true;
}

bool ChatHandler::HandleAccountSetPasswordCommand(char* args)
{

    std::string account_name;
    uint32 targetAccountId = ExtractAccountId(&args, &account_name);
    if (!targetAccountId)
    {
        return false;
    }

    char* szPassword1 = ExtractQuotedOrLiteralArg(&args);
    char* szPassword2 = ExtractQuotedOrLiteralArg(&args);
    if (!szPassword1 || !szPassword2)
    {
        return false;
    }

    if (HasLowerSecurityAccount(nullptr, targetAccountId, true))
    {
        return false;
    }

    if (strcmp(szPassword1, szPassword2))
    {
        SendSysMessage(LANG_NEW_PASSWORDS_NOT_MATCH);
        SetSentErrorMessage(true);
        return false;
    }

    AccountOpResult result = sAccountMgr.ChangePassword(targetAccountId, szPassword1);

    switch (result)
    {
        case AOR_OK:
            SendSysMessage(LANG_COMMAND_PASSWORD);
            break;
        case AOR_NAME_NOT_EXIST:
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_name.c_str());
            SetSentErrorMessage(true);
            return false;
        case AOR_PASS_TOO_LONG:
            SendSysMessage(LANG_PASSWORD_TOO_LONG);
            SetSentErrorMessage(true);
            return false;
        default:
            SendSysMessage(LANG_COMMAND_NOTCHANGEPASSWORD);
            SetSentErrorMessage(true);
            return false;
    }

    char msg[100];
    snprintf(msg, 100, ".account set password %s *** ***", account_name.c_str());
    LogCommand(msg);
    SetSentErrorMessage(true);
    return false;
}
