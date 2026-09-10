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

#include "Platform/Define.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <set>
#include <sstream>
#include "PlayerDump.h"
#include "Database/DatabaseEnv.h"
#include "SQLStorages.h"
#include "UpdateFields.h"
#include "ObjectMgr.h"
#include "Mint.h"
#include "AccountMgr.h"

struct DumpTable
{
    char const* name;
    DumpTableType type;

    bool isValid() const { return name != nullptr; }
};

static DumpTable dumpTables[] =
{
    { "characters",                       DTT_CHARACTER  },
    { "character_action",                 DTT_CHAR_TABLE },
    { "character_aura",                   DTT_CHAR_TABLE },
    { "character_homebind",               DTT_CHAR_TABLE },
    { "character_honor_cp",               DTT_CHAR_TABLE },
    { "character_inventory",              DTT_INVENTORY  },
    { "character_queststatus",            DTT_CHAR_TABLE },
    { "character_pet",                    DTT_PET        },
    { "character_reputation",             DTT_CHAR_TABLE },
    { "character_skills",                 DTT_CHAR_TABLE },
    { "character_spell",                  DTT_CHAR_TABLE },
    { "character_spell_cooldown",         DTT_CHAR_TABLE },
    { "character_ticket",                 DTT_CHAR_TABLE },
    { "mail",                             DTT_MAIL       },
    { "mail_items",                       DTT_MAIL_ITEM  },
    { "pet_aura",                         DTT_PET_TABLE  },
    { "pet_spell",                        DTT_PET_TABLE  },
    { "pet_spell_cooldown",               DTT_PET_TABLE  },
    { "character_gifts",                  DTT_ITEM_GIFT  },
    { "item_instance",                    DTT_ITEM       },
    { "item_loot",                        DTT_ITEM_LOOT  },
    { nullptr,                               DTT_CHAR_TABLE },
};

static bool findtoknth(std::string& str, int n, std::string::size_type& s, std::string::size_type& e)
{
    int i; s = e = 0;
    std::string::size_type size = str.size();
    for (i = 1; s < size && i < n; ++s)
    {
        if (str[s] == ' ')
        {
            ++i;
        }
    }
    if (i < n)
    {
        return false;
    }

    e = str.find(' ', s);

    return e != std::string::npos;
}

std::string gettoknth(std::string& str, int n)
{
    std::string::size_type s = 0, e = 0;
    if (!findtoknth(str, n, s, e))
    {
        return "";
    }

    return str.substr(s, e - s);
}

bool findnth(std::string& str, int n, std::string::size_type& s, std::string::size_type& e)
{
    s = str.find("VALUES ('") + 9;
    if (s == std::string::npos)
    {
        return false;
    }

    do
    {
        e = str.find("'", s);
        if (e == std::string::npos)
        {
            return false;
        }
    }
    while (str[e - 1] == '\\');

    for (int i = 1; i < n; ++i)
    {
        do
        {
            s = e + 4;
            e = str.find("'", s);
            if (e == std::string::npos)
            {
                return false;
            }
        }
        while (str[e - 1] == '\\');
    }
    return true;
}

std::string gettablename(std::string& str)
{
    std::string::size_type s = 13;
    std::string::size_type e = str.find('`', s);
    if (e == std::string::npos)
    {
        return "";
    }

    return str.substr(s, e - s);
}

bool changenth(std::string& str, int n, const char* with, bool insert = false, bool nonzero = false)
{
    std::string::size_type s, e;
    if (!findnth(str, n, s, e))
    {
        return false;
    }

    if (nonzero && str.substr(s, e - s) == "0")
    {
        return true;
    }
    if (!insert)
    {
        str.replace(s, e - s, with);
    }
    else
    {
        str.insert(s, with);
    }

    return true;
}

std::string getnth(std::string& str, int n)
{
    std::string::size_type s, e;
    if (!findnth(str, n, s, e))
    {
        return "";
    }

    return str.substr(s, e - s);
}

bool changetoknth(std::string& str, int n, const char* with, bool insert = false, bool nonzero = false)
{
    std::string::size_type s = 0, e = 0;
    if (!findtoknth(str, n, s, e))
    {
        return false;
    }
    if (nonzero && str.substr(s, e - s) == "0")
    {
        return true;
    }
    if (!insert)
    {
        str.replace(s, e - s, with);
    }
    else
    {
        str.insert(s, with);
    }

    return true;
}

uint32 registerNewGuid(uint32 oldGuid, std::map<uint32, uint32>& guidMap, uint32 hiGuid)
{
    std::map<uint32, uint32>::const_iterator itr = guidMap.find(oldGuid);
    if (itr != guidMap.end())
    {
        return itr->second;
    }

    uint32 newguid = hiGuid + guidMap.size();
    guidMap[oldGuid] = newguid;
    return newguid;
}

bool changeGuid(std::string& str, int n, std::map<uint32, uint32>& guidMap, uint32 hiGuid, bool nonzero = false)
{
    char chritem[20];
    uint32 oldGuid = atoi(getnth(str, n).c_str());
    if (nonzero && oldGuid == 0)
    {
        return true;
    }

    uint32 newGuid = registerNewGuid(oldGuid, guidMap, hiGuid);
    snprintf(chritem, 20, "%u", newGuid);

    return changenth(str, n, chritem, false, nonzero);
}

bool changetokGuid(std::string& str, int n, std::map<uint32, uint32>& guidMap, uint32 hiGuid, bool nonzero = false)
{
    char chritem[20];
    uint32 oldGuid = atoi(gettoknth(str, n).c_str());
    if (nonzero && oldGuid == 0)
    {
        return true;
    }

    uint32 newGuid = registerNewGuid(oldGuid, guidMap, hiGuid);
    snprintf(chritem, 20, "%u", newGuid);

    return changetoknth(str, n, chritem, false, nonzero);
}

std::string CreateDumpString(char const* tableName, char const* tableColumnNamesAsChars, QueryResult* result)
{
    if (!tableName || !result)
    {
        return "";
    }

    std::ostringstream ss;
    ss << "INSERT INTO `" << tableName << "` ("<< tableColumnNamesAsChars  <<") VALUES (";
    Field* fields = result->Fetch();
    for (uint32 i = 0; i < result->GetFieldCount(); ++i)
    {
        if (i != 0)
        {
            ss << ", ";
        }

        if (fields[i].IsNULL())
        {
            ss << "NULL";
        }
        else
        {
            std::string s =  fields[i].GetCppString();
            CharacterDatabase.escape_string(s);

            ss << "'" << s << "'";
        }
    }
    ss << ");";
    return ss.str();
}

std::string PlayerDumpWriter::GenerateWhereStr(char const* field, uint32 guid)
{
    std::ostringstream wherestr;
    wherestr << field << " = '" << guid << "'";
    return wherestr.str();
}

std::string PlayerDumpWriter::GenerateWhereStr(char const* field, GUIDs const& guids, GUIDs::const_iterator& itr)
{
    std::ostringstream wherestr;
    wherestr << field << " IN ('";
    for (; itr != guids.end(); ++itr)
    {
        wherestr << *itr;

        if (wherestr.str().size() > MAX_QUERY_LEN - 50)
        {
            ++itr;
            break;
        }

        GUIDs::const_iterator itr2 = itr;
        if (++itr2 != guids.end())
        {
            wherestr << "','";
        }
    }
    wherestr << "')";
    return wherestr.str();
}

void StoreGUID(QueryResult* result, uint32 field, std::set<uint32>& guids)
{
    Field* fields = result->Fetch();
    uint32 guid = fields[field].GetUInt32();
    if (guid)
    {
        guids.insert(guid);
    }
}

void StoreGUID(QueryResult* result, uint32 data, uint32 field, std::set<uint32>& guids)
{
    Field* fields = result->Fetch();
    std::string dataStr = fields[data].GetCppString();
    uint32 guid = atoi(gettoknth(dataStr, field).c_str());
    if (guid)
    {
        guids.insert(guid);
    }
}

void PlayerDumpWriter::DumpTableContent(std::string& dump, uint32 guid, char const* tableFrom, char const* tableTo, DumpTableType type)
{
    GUIDs const* guids = nullptr;
    char const* fieldname;

    switch (type)
    {
        case DTT_ITEM:      fieldname = "guid";      guids = &items; break;
        case DTT_ITEM_GIFT: fieldname = "item_guid"; guids = &items; break;
        case DTT_ITEM_LOOT: fieldname = "guid";      guids = &items; break;
        case DTT_PET:       fieldname = "owner";                     break;
        case DTT_PET_TABLE: fieldname = "guid";      guids = &pets;  break;
        case DTT_MAIL:      fieldname = "receiver";                  break;
        case DTT_MAIL_ITEM: fieldname = "mail_id";   guids = &mails; break;
        default:            fieldname = "guid";                      break;
    }

    if (guids && guids->empty())
    {
        return;
    }

    GUIDs::const_iterator guids_itr;
    if (guids)
    {
        guids_itr = guids->begin();
    }

    do
    {
        std::string wherestr;

        if (guids)
        {
            wherestr = GenerateWhereStr(fieldname, *guids, guids_itr);
        }
        else
        {
            wherestr = GenerateWhereStr(fieldname, guid);
        }

        std::string tableColumnNamesStr = "";
        QueryNamedResult* resNames = CharacterDatabase.PQueryNamed("SELECT * FROM `%s` LIMIT 1", tableFrom);
        if (!resNames)
        {
            return;
        }

        QueryFieldNames const& namesMap = resNames->GetFieldNames();

        for (QueryFieldNames::const_iterator itr = namesMap.begin(); itr != namesMap.end(); ++itr)
        {
            tableColumnNamesStr += "`" + *itr  +"`,";
        }

        tableColumnNamesStr.pop_back();

        QueryFieldNames nonConstNamesMap = namesMap;
        nonConstNamesMap.clear();

        QueryResult* result = CharacterDatabase.PQuery("SELECT %s FROM `%s` WHERE %s", tableColumnNamesStr.c_str(), tableFrom, wherestr.c_str());
        if (!result)
        {
            return;
        }

        do
        {

            switch (type)
            {
                case DTT_INVENTORY:
                    StoreGUID(result, 3, items); break;
                case DTT_PET:
                    StoreGUID(result, 0, pets);  break;
                case DTT_MAIL:
                    StoreGUID(result, 0, mails);
                case DTT_MAIL_ITEM:
                    StoreGUID(result, 1, items); break;
                default:                       break;
            }

            dump += CreateDumpString(tableTo, tableColumnNamesStr.c_str(), result);
            dump += "\n";
        }
        while (result->NextRow());

        delete result;
    }
    while (guids && guids_itr != guids->end());
}

std::string PlayerDumpWriter::GetDump(uint32 guid)
{
    std::string dump;

    dump += "IMPORTANT NOTE: This sql queries not created for apply directly, use '.pdump load' command in console or client chat instead.\n";
    dump += "IMPORTANT NOTE: NOT APPLY ITS DIRECTLY to character DB or you will DAMAGE and CORRUPT character DB\n";

    QueryResult* result = CharacterDatabase.Query("SELECT `version`, `structure`, `description`, `comment` FROM `db_version` ORDER BY `version` DESC, `structure` DESC, `content` ASC LIMIT 1");

    if (result)
    {
        Field* fields = result->Fetch();

        dump += "DUMPED_WITH:"+std::to_string(fields[0].GetInt16())
        + "." + std::to_string(fields[1].GetInt16()) + ".X "
        +" CHAR. DB VERSION ( "
        + fields[2].GetCppString() + " / "
        + fields[3].GetCppString()
        + ")\n\n"
        ;

        delete result;
    }
    else
    {
        sLog.outError("Character DB not have 'db_version' table");
    }

    for (DumpTable* itr = &dumpTables[0]; itr->isValid(); ++itr)
    {
        DumpTableContent(dump, guid, itr->name, itr->name, itr->type);
    }

    return dump;
}

DumpReturn PlayerDumpWriter::WriteDump(const std::string& file, uint32 guid)
{
    FILE* fout = fopen(file.c_str(), "w");
    if (!fout)
    {
        return DUMP_FILE_OPEN_ERROR;
    }

    std::string dump = GetDump(guid);

    fprintf(fout, "%s\n", dump.c_str());
    fclose(fout);
    return DUMP_SUCCESS;
}

#define ROLLBACK(DR) {CharacterDatabase.RollbackTransaction(); fclose(fin); return (DR);}

DumpReturn PlayerDumpReader::LoadDump(const std::string& file, uint32 account, std::string name, uint32 guid)
{

    uint32 charcount = sAccountMgr.GetCharactersCount(account);
    if (charcount >= 10)
    {
        return DUMP_TOO_MANY_CHARS;
    }

    FILE* fin = fopen(file.c_str(), "r");
    if (!fin)
    {
        return DUMP_FILE_OPEN_ERROR;
    }

    QueryResult* result;
    char newguid[20], chraccount[20], newpetid[20], currpetid[20], lastpetid[20];

    bool incHighest = true;
    if (guid != 0 && guid < sMint.PlayerGuids().NextAfterMaxUsed())
    {
        result = CharacterDatabase.PQuery("SELECT * FROM `characters` WHERE `guid` = '%u'", guid);
        if (result)
        {
            guid = sMint.PlayerGuids().NextAfterMaxUsed();
            delete result;
        }
        else
        {
            incHighest = false;
        }
    }
    else
    {
        guid = sMint.PlayerGuids().NextAfterMaxUsed();
    }

    if (!normalizePlayerName(name))
    {
        name.clear();
    }

    if (ObjectMgr::CheckPlayerName(name, true) == CHAR_NAME_SUCCESS)
    {
        CharacterDatabase.escape_string(name);
        result = CharacterDatabase.PQuery("SELECT * FROM `characters` WHERE `name` = '%s'", name.c_str());
        if (result)
        {
            name.clear();
            delete result;
        }
    }
    else
    {
        name.clear();
    }

    snprintf(newguid, 20, "%u", guid);
    snprintf(chraccount, 20, "%u", account);
    snprintf(newpetid, 20, "%u", sMint.PetNumbers().Next());
    snprintf(lastpetid, 20, "%s", "");

    std::map<uint32, uint32> items;
    std::map<uint32, uint32> mails;
    std::map<uint32, uint32> itemTexts;
    char buf[32000] = "";

    typedef std::map<uint32, uint32> PetIds;
    typedef PetIds::value_type PetIdsPair;
    PetIds petids;

    CharacterDatabase.BeginTransaction();
    while (!feof(fin))
    {
        if (!fgets(buf, 32000, fin))
        {
            if (feof(fin))
            {
                break;
            }
            ROLLBACK(DUMP_FILE_BROKEN);
        }

        std::string line; line.assign(buf);

        size_t nw_pos = line.find_first_not_of(" \t\n\r\7");
        if (nw_pos == std::string::npos)
        {
            continue;
        }

        if (line.substr(nw_pos, 15) == "IMPORTANT NOTE:")
        {
            continue;
        }

        std::string dbVersionLinePrefix = line.substr(nw_pos, 12);

        if (dbVersionLinePrefix =="DUMPED_WITH:")
        {
            QueryResult* result = CharacterDatabase.Query("SELECT `version`, `structure`, `description`, `comment` FROM `db_version` ORDER BY `version` DESC, `structure` DESC, `content` ASC LIMIT 1");

            if (result)
            {
                Field* fields = result->Fetch();

                std::string dbversion = std::to_string(fields[0].GetInt16()) + "." + std::to_string(fields[1].GetInt16())+".X";
                size_t dbversionLen = dbversion.size();

                std::string dbversionInDumpFile = line.substr(nw_pos+12, dbversionLen);

                delete result;

                if (dbversionInDumpFile != dbversion)
                {
                    sLog.outError("LoadPlayerDump: Cannot load player dump - file version is %s, DB needs %s", dbversionInDumpFile.c_str(), dbversion.c_str());
                    ROLLBACK(DUMP_DB_VERSION_MISMATCH);
                }
                else
                {
                    continue;
                }
            }
        }

        std::string tn = gettablename(line);
        if (tn.empty())
        {
            sLog.outError("LoadPlayerDump: Can't extract table name from line: '%s'!", line.c_str());
            ROLLBACK(DUMP_FILE_BROKEN);
        }

        DumpTableType type = DTT_CHARACTER;
        DumpTable* dTable = &dumpTables[0];
        for (; dTable->isValid(); ++dTable)
        {
            if (tn == dTable->name)
            {
                type = dTable->type;
                break;
            }
        }

        if (!dTable->isValid())
        {
            sLog.outError("LoadPlayerDump: Unknown table: '%s'!", tn.c_str());
            ROLLBACK(DUMP_FILE_BROKEN);
        }

        switch (type)
        {
            case DTT_CHAR_TABLE:
                if (!changenth(line, 1, newguid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                break;

            case DTT_CHARACTER:
            {
                if (!changenth(line, 1, newguid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }

                if (!changenth(line, 2, chraccount))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }

                if (name.empty())
                {

                    name = getnth(line, 3);
                    CharacterDatabase.escape_string(name);

                    result = CharacterDatabase.PQuery("SELECT * FROM `characters` WHERE `name` = '%s'", name.c_str());
                    if (result)
                    {
                        delete result;

                        if (!changenth(line, 35, "1"))
                        {
                            ROLLBACK(DUMP_FILE_BROKEN);
                        }
                    }
                }
                else
                {
                    if (!changenth(line, 3, name.c_str()))
                    {
                        ROLLBACK(DUMP_FILE_BROKEN);
                    }
                }

                break;
            }
            case DTT_INVENTORY:
            {
                if (!changenth(line, 1, newguid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }

                if (!changeGuid(line, 2, items, sMint.ItemGuids().NextAfterMaxUsed(), true))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                if (!changeGuid(line, 4, items, sMint.ItemGuids().NextAfterMaxUsed()))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                break;
            }
            case DTT_ITEM:
            {

                if (!changeGuid(line, 1, items, sMint.ItemGuids().NextAfterMaxUsed()))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                if (!changenth(line, 2, newguid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                std::string vals = getnth(line, 3);
                if (!changetokGuid(vals, OBJECT_FIELD_GUID + 1, items, sMint.ItemGuids().NextAfterMaxUsed()))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                if (!changetoknth(vals, ITEM_FIELD_OWNER + 1, newguid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                if (!changenth(line, 3, vals.c_str()))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                break;
            }
            case DTT_ITEM_GIFT:
            {
                if (!changenth(line, 1, newguid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                if (!changeGuid(line, 2, items, sMint.ItemGuids().NextAfterMaxUsed()))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                break;
            }
            case DTT_ITEM_LOOT:
            {

                if (!changeGuid(line, 1, items, sMint.ItemGuids().NextAfterMaxUsed()))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                if (!changenth(line, 2, newguid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                break;
            }
            case DTT_PET:
            {

                snprintf(currpetid, 20, "%s", getnth(line, 1).c_str());
                if (strlen(lastpetid) == 0)
                {
                    snprintf(lastpetid, 20, "%s", currpetid);
                }

                if (strcmp(lastpetid, currpetid) != 0)
                {
                    snprintf(newpetid, 20, "%d", sMint.PetNumbers().Next());
                    snprintf(lastpetid, 20, "%s", currpetid);
                }

                std::map<uint32, uint32> :: const_iterator petids_iter = petids.find(atoi(currpetid));

                if (petids_iter == petids.end())
                {
                    petids.insert(PetIdsPair(atoi(currpetid), atoi(newpetid)));
                }

                if (!changenth(line, 1, newpetid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                if (!changenth(line, 3, newguid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }

                break;
            }
            case DTT_PET_TABLE:
            {
                snprintf(currpetid, 20, "%s", getnth(line, 1).c_str());

                std::map<uint32, uint32> :: const_iterator petids_iter = petids.find(atoi(currpetid));
                if (petids_iter == petids.end())
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }

                snprintf(newpetid, 20, "%d", petids_iter->second);

                if (!changenth(line, 1, newpetid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }

                break;
            }
            case DTT_MAIL:
            {
                if (!changeGuid(line, 1, mails, sMint.MailIds().NextAfterMaxUsed()))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                if (!changenth(line, 6, newguid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                break;
            }
            case DTT_MAIL_ITEM:
            {
                if (!changeGuid(line, 1, mails, sMint.MailIds().NextAfterMaxUsed()))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                if (!changeGuid(line, 2, items, sMint.ItemGuids().NextAfterMaxUsed()))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                if (!changenth(line, 4, newguid))
                {
                    ROLLBACK(DUMP_FILE_BROKEN);
                }
                break;
            }
            default:
                sLog.outError("Unknown dump table type: %u", type);
                break;
        }

        if (!CharacterDatabase.Execute(line.c_str()))
        {
            ROLLBACK(DUMP_FILE_BROKEN);
        }
    }

    CharacterDatabase.CommitTransaction();

    sMint.ItemGuids().Set(sMint.ItemGuids().NextAfterMaxUsed() + items.size());
    sMint.MailIds().Set(sMint.MailIds().NextAfterMaxUsed() +  mails.size());

    if (incHighest)
    {
        sMint.PlayerGuids().Set(sMint.PlayerGuids().NextAfterMaxUsed() + 1);
    }

    fclose(fin);

    return DUMP_SUCCESS;
}
