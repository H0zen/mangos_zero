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

#include <string>
#include <set>

enum DumpTableType
{
    DTT_CHARACTER,

    DTT_CHAR_TABLE,

    DTT_INVENTORY,

    DTT_MAIL,

    DTT_MAIL_ITEM,

    DTT_ITEM,

    DTT_ITEM_GIFT,

    DTT_ITEM_LOOT,

    DTT_PET,
    DTT_PET_TABLE,
};

enum DumpReturn
{
    DUMP_SUCCESS,
    DUMP_FILE_OPEN_ERROR,
    DUMP_TOO_MANY_CHARS,
    DUMP_UNEXPECTED_END,
    DUMP_FILE_BROKEN,
    DUMP_DB_VERSION_MISMATCH
};

class PlayerDump
{
    protected:

        PlayerDump() {}
};

class PlayerDumpWriter : public PlayerDump
{
    public:

        PlayerDumpWriter() {}

        std::string GetDump(uint32 guid);

        DumpReturn WriteDump(const std::string& file, uint32 guid);

    private:
        typedef std::set<uint32> GUIDs;

        void DumpTableContent(std::string& dump, uint32 guid, char const* tableFrom, char const* tableTo, DumpTableType type);

        std::string GenerateWhereStr(char const* field, GUIDs const& guids, GUIDs::const_iterator& itr);

        std::string GenerateWhereStr(char const* field, uint32 guid);

        GUIDs pets;
        GUIDs mails;
        GUIDs items;
};

class PlayerDumpReader : public PlayerDump
{
    public:

        PlayerDumpReader() {}

        DumpReturn LoadDump(const std::string& file, uint32 account, std::string name, uint32 guid);
};
