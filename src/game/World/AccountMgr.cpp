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

#include "CharacterRows.h"
#include <string>
#include "AccountMgr.h"
#include "Database/DatabaseEnv.h"
#include "PlayerRegistry.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Policies/Singleton.h"
#include "Util.h"
#include "Auth/Sha1.h"

extern DatabaseType LoginDatabase;

AccountMgr::AccountMgr()
{}

AccountMgr::~AccountMgr()
{}

AccountOpResult AccountMgr::CreateAccount(std::string username, std::string password)
{
    if (utf8length(username) > MAX_ACCOUNT_STR)
    {
        return AOR_NAME_TOO_LONG;
    }

    if (utf8length(password) > MAX_PASSWORD_STR)
    {
        return AOR_PASS_TOO_LONG;
    }

    Utf8ToUpperOnlyLatin(username);
    Utf8ToUpperOnlyLatin(password);

    if (GetId(username))
    {
        {
            return AOR_NAME_ALREADY_EXIST;
        }
    }

    if (!LoginDatabase.PExecute("INSERT INTO `account` (`username`,`sha_pass_hash`,`joindate`) VALUES ('%s','%s',NOW())", username.c_str(), CalculateShaPassHash(username, password).c_str()))
    {
        return AOR_DB_INTERNAL_ERROR;
    }
    LoginDatabase.Execute("INSERT INTO `realmcharacters` (`realmid`, `acctid`, `numchars`) SELECT `realmlist`.`id`, `account`.`id`, 0 FROM `realmlist`,`account` LEFT JOIN `realmcharacters` ON `acctid`=`account`.`id` WHERE `acctid` IS NULL");

    return AOR_OK;
}

AccountOpResult AccountMgr::CreateAccount(std::string username, std::string password, uint32 expansion)
{
    if (utf8length(username) > MAX_ACCOUNT_STR)
    {
        return AOR_NAME_TOO_LONG;
    }

    Utf8ToUpperOnlyLatin(username);
    Utf8ToUpperOnlyLatin(password);

    if (GetId(username))
    {
        return AOR_NAME_ALREADY_EXIST;
    }

    if (!LoginDatabase.PExecute("INSERT INTO `account`(`username`,`sha_pass_hash`,`joindate`,`expansion`) VALUES('%s','%s',NOW(),'%u')", username.c_str(), CalculateShaPassHash(username, password).c_str(), expansion))
    {
        return AOR_DB_INTERNAL_ERROR;
    }
    LoginDatabase.Execute("INSERT INTO `realmcharacters` (`realmid`, `acctid`, `numchars`) SELECT `realmlist`.`id`, `account`.`id`, 0 FROM `realmlist`,`account` LEFT JOIN `realmcharacters` ON `acctid`=`account`.`id` WHERE `acctid` IS NULL");

    return AOR_OK;
}

AccountOpResult AccountMgr::DeleteAccount(uint32 accid)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT 1 FROM `account` WHERE `id`='%u'", accid);
    if (!result)
    {
        return AOR_NAME_NOT_EXIST;
    }
    delete result;

    result = CharacterDatabase.PQuery("SELECT `guid` FROM `characters` WHERE `account`='%u'", accid);
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 guidlo = fields[0].GetUInt32();
            ObjectGuid guid = MakeGuid(HIGHGUID_PLAYER, guidlo);

            sPlayerRegistry.Kick(guid);
            CharacterRows::Delete(guid, accid, false);
        }
        while (result->NextRow());

        delete result;
    }

    CharacterDatabase.PExecute("DELETE FROM `character_tutorial` WHERE `account` = '%u'", accid);

    LoginDatabase.BeginTransaction();

    bool res =
        LoginDatabase.PExecute("DELETE FROM `account` WHERE `id`='%u'", accid) &&
        LoginDatabase.PExecute("DELETE FROM `realmcharacters` WHERE `acctid`='%u'", accid);

    LoginDatabase.CommitTransaction();

    if (!res)
    {
        return AOR_DB_INTERNAL_ERROR;
    }

    return AOR_OK;
}

AccountOpResult AccountMgr::ChangeUsername(uint32 accid, std::string new_uname, std::string new_passwd)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT 1 FROM `account` WHERE `id`='%u'", accid);
    if (!result)
    {
        return AOR_NAME_NOT_EXIST;
    }
    delete result;

    if (utf8length(new_uname) > MAX_ACCOUNT_STR)
    {
        return AOR_NAME_TOO_LONG;
    }

    if (utf8length(new_passwd) > MAX_PASSWORD_STR)
    {
        return AOR_PASS_TOO_LONG;
    }

    Utf8ToUpperOnlyLatin(new_uname);
    Utf8ToUpperOnlyLatin(new_passwd);

    std::string safe_new_uname = new_uname;
    LoginDatabase.escape_string(safe_new_uname);

    if (!LoginDatabase.PExecute("UPDATE `account` SET `v`='0',`s`='0',`username`='%s',`sha_pass_hash`='%s' WHERE `id`='%u'", safe_new_uname.c_str(),
        CalculateShaPassHash(new_uname, new_passwd).c_str(), accid))
    {
        return AOR_DB_INTERNAL_ERROR;
    }

    return AOR_OK;
}

AccountOpResult AccountMgr::ChangePassword(uint32 accid, std::string new_passwd)
{
    std::string username;

    if (!GetName(accid, username))
    {
        return AOR_NAME_NOT_EXIST;
    }

    if (utf8length(new_passwd) > MAX_PASSWORD_STR)
    {
        return AOR_PASS_TOO_LONG;
    }

    Utf8ToUpperOnlyLatin(username);
    Utf8ToUpperOnlyLatin(new_passwd);

    if (!LoginDatabase.PExecute("UPDATE `account` SET `v`='0', `s`='0', `sha_pass_hash`='%s' WHERE `id`='%u'",
        CalculateShaPassHash(username, new_passwd).c_str(), accid))
    {
        return AOR_DB_INTERNAL_ERROR;
    }

    return AOR_OK;
}

uint32 AccountMgr::GetId(std::string username)
{
    LoginDatabase.escape_string(username);
    QueryResult* result = LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` = '%s'", username.c_str());
    if (!result)
    {
        return 0;
    }
    else
    {
        uint32 id = (*result)[0].GetUInt32();
        delete result;
        return id;
    }
}

AccountTypes AccountMgr::GetSecurity(uint32 acc_id)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT `gmlevel` FROM `account` WHERE `id` = '%u'", acc_id);
    if (result)
    {
        AccountTypes sec = AccountTypes((*result)[0].GetInt32());
        delete result;
        return sec;
    }

    return SEC_PLAYER;
}

bool AccountMgr::GetName(uint32 acc_id, std::string& name)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT `username` FROM `account` WHERE `id` = '%u'", acc_id);
    if (result)
    {
        name = (*result)[0].GetCppString();
        delete result;
        return true;
    }

    return false;
}

uint32 AccountMgr::GetCharactersCount(uint32 acc_id)
{

    QueryResult* result = CharacterDatabase.PQuery("SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", acc_id);
    if (result)
    {
        Field* fields = result->Fetch();
        uint32 charcount = fields[0].GetUInt32();
        delete result;
        return charcount;
    }
    else
    {
        return 0;
    }
}

bool AccountMgr::CheckPassword(uint32 accid, std::string passwd)
{
    std::string username;
    if (!GetName(accid, username))
    {
        return false;
    }

    Utf8ToUpperOnlyLatin(passwd);
    Utf8ToUpperOnlyLatin(username);

    QueryResult* result = LoginDatabase.PQuery("SELECT 1 FROM `account` WHERE `id`='%u' AND `sha_pass_hash`='%s'", accid, CalculateShaPassHash(username, passwd).c_str());
    if (result)
    {
        delete result;
        return true;
    }

    return false;
}

std::string AccountMgr::CalculateShaPassHash(std::string& name, std::string& password)
{
    Sha1Hash sha;
    sha.Initialize();
    sha.UpdateData(name);
    sha.UpdateData(":");
    sha.UpdateData(password);
    sha.Finalize();

    std::string encoded;
    hexEncodeByteArray(sha.GetDigest(), sha.GetLength(), encoded);

    return encoded;
}
