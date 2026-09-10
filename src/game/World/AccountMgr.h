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

#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <string>

enum AccountOpResult
{
    AOR_OK,
    AOR_NAME_TOO_LONG,
    AOR_PASS_TOO_LONG,
    AOR_NAME_ALREADY_EXIST,
    AOR_NAME_NOT_EXIST,
    AOR_DB_INTERNAL_ERROR
};

#define MAX_ACCOUNT_STR 16
#define MAX_PASSWORD_STR 16

class AccountMgr
{
    public:

        AccountMgr();

        ~AccountMgr();

        AccountOpResult CreateAccount(std::string username, std::string password);

        AccountOpResult CreateAccount(std::string username, std::string password, uint32 expansion);

        AccountOpResult DeleteAccount(uint32 accid);

        AccountOpResult ChangeUsername(uint32 accid, std::string new_uname, std::string new_passwd);

        AccountOpResult ChangePassword(uint32 accid, std::string new_passwd);

        bool CheckPassword(uint32 accid, std::string passwd);

        uint32 GetId(std::string username);

        AccountTypes GetSecurity(uint32 acc_id);

        bool GetName(uint32 acc_id, std::string& name);

        uint32 GetCharactersCount(uint32 acc_id);

        std::string CalculateShaPassHash(std::string& name, std::string& password);
};

#define sAccountMgr MaNGOS::Singleton<AccountMgr>::Instance()
