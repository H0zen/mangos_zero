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
#include "Common/Locales.h"
#include <string>
#include <vector>
#include "Chat.h"
#include "Language.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldSession.h"
#include "AccountMgr.h"
#include "DBCStores.h"
#include "Util.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectGuid.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "SpellMgr.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "CommandMgr.h"

void ChatHandler::SkipWhiteSpaces(char** args)
{
    if (!*args)
    {
        return;
    }

    while (isWhiteSpace(**args))
    {
        ++(*args);
    }
}

bool  ChatHandler::ExtractInt32(char** args, int32& val)
{
    if (!*args || !** args)
    {
        return false;
    }

    char* tail = *args;

    long valRaw = strtol(*args, &tail, 10);

    if (tail != *args && isWhiteSpace(*tail))
    {
        *(tail++) = '\0';
    }
    else if (*tail)
    {
        return false;
    }

    if (valRaw < std::numeric_limits<int32>::min() || valRaw > std::numeric_limits<int32>::max())
    {
        return false;
    }

    val = int32(valRaw);
    *args = tail;
    return true;
}

bool  ChatHandler::ExtractOptInt32(char** args, int32& val, int32 defVal)
{
    if (!*args || !** args)
    {
        val = defVal;
        return true;
    }

    return ExtractInt32(args, val);
}

bool  ChatHandler::ExtractUInt32Base(char** args, uint32& val, uint32 base)
{
    if (!*args || !** args)
    {
        return false;
    }

    char* tail = *args;

    unsigned long valRaw = strtoul(*args, &tail, base);

    if (tail != *args && isWhiteSpace(*tail))
    {
        *(tail++) = '\0';
    }
    else if (*tail)
    {
        return false;
    }

    if (valRaw > std::numeric_limits<uint32>::max())
    {
        return false;
    }

    val = uint32(valRaw);
    *args = tail;

    SkipWhiteSpaces(args);
    return true;
}

bool  ChatHandler::ExtractOptUInt32(char** args, uint32& val, uint32 defVal)
{
    if (!*args || !** args)
    {
        val = defVal;
        return true;
    }

    return ExtractUInt32(args, val);
}

bool  ChatHandler::ExtractFloat(char** args, float& val)
{
    if (!*args || !** args)
    {
        return false;
    }

    char* tail = *args;

    double valRaw = strtod(*args, &tail);

    if (tail != *args && isWhiteSpace(*tail))
    {
        *(tail++) = '\0';
    }
    else if (*tail)
    {
        return false;
    }

    val = float(valRaw);
    *args = tail;

    SkipWhiteSpaces(args);
    return true;
}

bool  ChatHandler::ExtractOptFloat(char** args, float& val, float defVal)
{
    if (!*args || !** args)
    {
        val = defVal;
        return true;
    }

    return ExtractFloat(args, val);
}

char* ChatHandler::ExtractLiteralArg(char** args, char const* lit )
{
    if (!*args || !** args)
    {
        return nullptr;
    }

    char* head = *args;

    switch (head[0])
    {

        case '[': case '\'': case '"':
            return nullptr;

        case '|':

            if (head[1] != '|')
            {
                return nullptr;
            }
            ++head;
            break;
        default: break;
    }

    if (lit)
    {
        int l = strlen(lit);

        int largs = 0;
        while (head[largs] && !isWhiteSpace(head[largs]))
        {
            ++largs;
        }

        if (largs < l)
        {
            l = largs;
        }

        int diff = strncmp(head, lit, l);

        if (diff != 0)
        {
            return nullptr;
        }

        if (head[l] && !isWhiteSpace(head[l]))
        {
            return nullptr;
        }

        char* arg = head;

        if (head[l])
        {
            head[l] = '\0';

            head += l + 1;

            *args = head;
        }
        else
        {
            *args = head + l;
        }

        SkipWhiteSpaces(args);
        return arg;
    }

    char* name = strtok(head, " ");

    char* tail = strtok(nullptr, "");

    *args = tail ? tail : (char*)"";

    SkipWhiteSpaces(args);

    return name;
}

char* ChatHandler::ExtractQuotedArg(char** args, bool asis )
{
    if (!*args || !** args)
    {
        return nullptr;
    }

    if (**args != '\'' &&**  args != '"' &&**  args != '[')
    {
        return nullptr;
    }

    char guard = (*args)[0];

    if (guard == '[')
    {
        guard = ']';
    }

    char* tail = (*args) + 1;
    char* head = asis ? *args : tail;

    while (*tail && *tail != guard)
    {
        ++tail;
    }

    if (!*tail || (tail[1] && !isWhiteSpace(tail[1])))
    {
        return nullptr;
    }

    if (!tail[1])
    {
        if (!asis)
        {
            *tail = '\0';
        }
    }
    else
    {
        if (asis)
        {
            ++tail;
        }

        *tail = '\0';
    }

    *args = tail + 1;

    SkipWhiteSpaces(args);

    return head;
}

char* ChatHandler::ExtractQuotedOrLiteralArg(char** args, bool asis )
{
    char* arg = ExtractQuotedArg(args, asis);
    if (!arg)
    {
        arg = ExtractLiteralArg(args);
    }
    return arg;
}

bool  ChatHandler::ExtractOnOff(char** args, bool& value)
{
    char* arg = ExtractLiteralArg(args);
    if (!arg)
    {
        return false;
    }

    if (strncmp(arg, "on", 3) == 0)
    {
        value = true;
    }
    else if (strncmp(arg, "off", 4) == 0)
    {
        value = false;
    }
    else
    {
        return false;
    }

    return true;
}

char* ChatHandler::ExtractLinkArg(char** args, char const* const* linkTypes , int* foundIdx , char** keyPair , char** somethingPair )
{
    if (!*args || !** args)
    {
        return nullptr;
    }

    if ((*args)[0] != '|' || (*args)[1] == '|')
    {
        return nullptr;
    }

    char* head = *args;

    char* tail = (*args) + 1;

    if (*tail != 'H')
    {
        while (*tail && *tail != '|')
        {
            ++tail;
        }

        if (!*tail)
        {
            return nullptr;
        }

        ++tail;
    }

    if (*tail != 'H')
    {
        return nullptr;
    }

    int linktype_idx = 0;

    if (linkTypes)
    {

        for (; linkTypes[linktype_idx]; ++linktype_idx)
        {

            int l = strlen(linkTypes[linktype_idx]);
            if (strncmp(tail, linkTypes[linktype_idx], l) == 0 &&
                (tail[l] == ':' || tail[l] == '|'))
            {
                break;
            }
        }

        if (!linkTypes[linktype_idx])
        {
            return nullptr;
        }

        tail += strlen(linkTypes[linktype_idx]);

        if (*tail != ':')
        {
            return nullptr;
        }
    }
    else
    {
        while (*tail && *tail != ':')
        {
            ++tail;
        }

        if (!*tail)
        {
            return nullptr;
        }
    }

    ++tail;

    char* keyStart = tail;

    while (*tail && *tail != '|' && *tail != ':')
    {
        ++tail;
    }

    if (!*tail)
    {
        return nullptr;
    }

    char* keyEnd = tail;

    char* somethingStart = tail + 1;
    char* somethingEnd   = tail + 1;

    if (*tail == ':' && somethingPair)
    {

        ++tail;

        while (*tail && *tail != '|' && *tail != ':')
        {
            ++tail;
        }

        if (!*tail)
        {
            return nullptr;
        }

        somethingEnd = tail;
    }

    while (*tail && (*tail != '|' || *(tail + 1) != 'h'))
    {
        ++tail;
    }

    if (!*tail)
    {
        return nullptr;
    }

    tail += 2;

    if (*tail != '[')
    {
        return nullptr;
    }

    while (*tail && (*tail != ']' || *(tail + 1) != '|'))
    {
        ++tail;
    }

    tail += 2;

    if (*tail != 'h' || *(tail + 1) != '|')
    {
        return nullptr;
    }

    tail += 2;

    if (*tail != 'r' || (*(tail + 1) && !isWhiteSpace(*(tail + 1))))
    {
        return nullptr;
    }

    ++tail;

    if (*tail)
    {
        *(tail++) = '\0';
    }

    if (foundIdx)
    {
        *foundIdx = linktype_idx;
    }

    if (keyPair)
    {
        keyPair[0] = keyStart;
        keyPair[1] = keyEnd;
    }

    if (somethingPair)
    {
        somethingPair[0] = somethingStart;
        somethingPair[1] = somethingEnd;
    }

    *args = tail;

    SkipWhiteSpaces(args);

    return head;
}

char* ChatHandler::ExtractArg(char** args, bool asis )
{
    if (!*args || !** args)
    {
        return nullptr;
    }

    char* arg = ExtractQuotedOrLiteralArg(args, asis);
    if (!arg)
    {
        arg = ExtractLinkArg(args);
    }

    return arg;
}

char* ChatHandler::ExtractOptNotLastArg(char** args)
{
    char* arg = ExtractArg(args, true);

    if (*args &&**  args)
    {
        return arg;
    }

    *args = arg ? arg : (char*)"";

    return nullptr;
}

char* ChatHandler::ExtractKeyFromLink(char** text, char const* linkType, char** something1 )
{
    char const* linkTypes[2];
    linkTypes[0] = linkType;
    linkTypes[1] = nullptr;

    int foundIdx;

    return ExtractKeyFromLink(text, linkTypes, &foundIdx, something1);
}

char* ChatHandler::ExtractKeyFromLink(char** text, char const* const* linkTypes, int* found_idx, char** something1 )
{

    if (!*text || !** text)
    {
        return nullptr;
    }

    char* arg = ExtractQuotedOrLiteralArg(text);
    if (arg)
    {
        if (found_idx)
        {
            *found_idx = -1;
        }

        return arg;
    }

    char* keyPair[2];
    char* somethingPair[2];

    arg = ExtractLinkArg(text, linkTypes, found_idx, keyPair, something1 ? somethingPair : nullptr);
    if (!arg)
    {
        return nullptr;
    }

    *keyPair[1] = '\0';

    if (something1)
    {
        *somethingPair[1] = '\0';
        *something1 = somethingPair[0];
    }

    return keyPair[0];
}

bool ChatHandler::ExtractUint32KeyFromLink(char** text, char const* linkType, uint32& value)
{
    char* arg = ExtractKeyFromLink(text, linkType);
    if (!arg)
    {
        return false;
    }

    return ExtractUInt32(&arg, value);
}

GameObject* ChatHandler::GetGameObjectWithGuid(uint32 lowguid, uint32 entry)
{
    if (!m_session)
    {
        return nullptr;
    }

    Player* pl = m_session->GetPlayer();

    return pl->GetMap()->GetGameObject(MakeGuid(HIGHGUID_GAMEOBJECT, entry, lowguid));
}

enum SpellLinkType
{
    SPELL_LINK_RAW     = -1,
    SPELL_LINK_SPELL   = 0,
    SPELL_LINK_TALENT  = 1,
    SPELL_LINK_ENCHANT = 2,
};

static char const* const spellKeys[] =
{
        "Hspell",
        "Htalent",
        "Henchant",
    nullptr
};

uint32 ChatHandler::ExtractSpellIdFromLink(char** text)
{

    int type;
    char* param1_str = nullptr;
    char* idS = ExtractKeyFromLink(text, spellKeys, &type, &param1_str);
    if (idS)
    {
        uint32 id;
        if (ExtractUInt32(&idS, id))
        {
            switch (type)
            {
                case SPELL_LINK_RAW:
                case SPELL_LINK_SPELL:
                case SPELL_LINK_ENCHANT:
                    return id;
                case SPELL_LINK_TALENT:
                {

                    TalentEntry const* talentEntry = sTalentStore.LookupEntry(id);
                    int32 rank;
                    if (talentEntry && ExtractInt32(&param1_str, rank))
                    {
                        if (rank < 0)
                        {
                            rank = 0;
                        }
                        if (rank < MAX_TALENT_RANK)
                        {
                            return talentEntry->RankID[rank];
                        }
                    }
                    break;
                }
            }
        }
    }

    char const* lookupName = idS ? idS : *text;
    if (!lookupName || !*lookupName)
    {
        return 0;
    }

    LocaleConstant locale = GetSessionDbcLocale();

    std::wstring wname;
    if (!Utf8toWStr(lookupName, wname))
    {
        return 0;
    }
    wstrToLower(wname);

    uint32 exactId = 0;
    std::vector<std::pair<uint32, std::string>> candidates;
    std::vector<std::pair<uint32, std::string>> fuzzyCandidates;

    for (uint32 id = 0; id < sSpellStore.GetNumRows(); ++id)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(id);
        if (!spell)
        {
            continue;
        }
        std::string sName = spell->Name_lang[locale];

        if (spell->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_LEARN_SPELL ||
            spell->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_LEARN_SPELL ||
            spell->Effect[EFFECT_INDEX_2] == SPELL_EFFECT_LEARN_SPELL ||
            sName.empty() || sName.compare(0, 5, "Test ") == 0 ||
            sName.compare(0, 2, "ZZ") == 0 || sName.compare(0, 2, "zz") == 0)
        {
            continue;
        }

        std::wstring wSpellName;
        if (!Utf8toWStr(sName, wSpellName))
        {
            continue;
        }
        wstrToLower(wSpellName);

        if (wSpellName == wname)
        {
            if (exactId == 0)
            {
                exactId = id;
            }
            candidates.push_back({id, spell->Name_lang[locale]});
        }
        else if (exactId == 0)
        {
            int loc = locale;
            if (!Utf8FitTo(sName, wname))
            {
                loc = 0;
                for (; loc < MAX_LOCALE; ++loc)
                {
                    if (loc == locale)
                    {
                        continue;
                    }
                    sName = spell->Name_lang[loc];
                    if (sName.empty())
                    {
                        continue;
                    }
                    if (Utf8FitTo(sName, wname))
                    {
                        break;
                    }
                }
            }

            if (loc < MAX_LOCALE)
            {
                fuzzyCandidates.push_back({id, spell->Name_lang[locale]});
            }
        }
    }

    if (!candidates.empty())
    {
        if (candidates.size() == 1)
        {
            return exactId;
        }

        SendSysMessage("Multiple spells found with that name:");
        for (auto const& c : candidates)
        {
            PSendSysMessage("  %u - %s", c.first, c.second.c_str());
        }
        SetSentErrorMessage(true);
        return 0;
    }

    if (fuzzyCandidates.empty())
    {
        SendSysMessage(LANG_COMMAND_NOSPELLFOUND);
        SetSentErrorMessage(true);
        return 0;
    }

    SendSysMessage("No exact match. Close matches:");
    for (auto const& c : fuzzyCandidates)
    {
        PSendSysMessage("  %u - %s", c.first, c.second.c_str());
    }
    SetSentErrorMessage(true);
    return 0;
}

GameTele const* ChatHandler::ExtractGameTeleFromLink(char** text)
{

    char* cId = ExtractKeyFromLink(text, "Htele");
    if (!cId)
    {
        return nullptr;
    }

    uint32 id;
    if (ExtractUInt32(&cId, id))
    {
        return sObjectMgr.GetGameTele(id);
    }
    else
    {
        return sObjectMgr.GetGameTele(cId);
    }
}

enum GuidLinkType
{
    GUID_LINK_RAW        = -1,
    GUID_LINK_PLAYER     = 0,
    GUID_LINK_CREATURE   = 1,
    GUID_LINK_GAMEOBJECT = 2,
};

static char const* const guidKeys[] =
{
        "Hplayer",
        "Hcreature",
        "Hgameobject",
    nullptr
};

ObjectGuid ChatHandler::ExtractGuidFromLink(char** text)
{
    int type = 0;

    char* idS = ExtractKeyFromLink(text, guidKeys, &type);
    if (!idS)
    {
        return 0;
    }

    switch (type)
    {
        case GUID_LINK_RAW:
        case GUID_LINK_PLAYER:
        {
            std::string name = idS;
            if (!normalizePlayerName(name))
            {
                return 0;
            }

            if (Player* player = sObjectMgr.GetPlayer(name.c_str()))
            {
                return player->GetObjectGuid();
            }

            return sObjectMgr.GetPlayerGuidByName(name);
        }
        case GUID_LINK_CREATURE:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&idS, lowguid))
            {
                return 0;
            }

            if (CreatureData const* data = sObjectMgr.GetCreatureData(lowguid))
            {
                return data->GetObjectGuid(lowguid);
            }
            else
            {
                return 0;
            }
        }
        case GUID_LINK_GAMEOBJECT:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&idS, lowguid))
            {
                return 0;
            }

            if (GameObjectData const* data = sObjectMgr.GetGOData(lowguid))
            {
                return MakeGuid(HIGHGUID_GAMEOBJECT, data->id, lowguid);
            }
            else
            {
                return 0;
            }
        }
    }

    return 0;
}

enum LocationLinkType
{
    LOCATION_LINK_RAW               = -1,
    LOCATION_LINK_PLAYER            = 0,
    LOCATION_LINK_TELE              = 1,
    LOCATION_LINK_TAXINODE          = 2,
    LOCATION_LINK_CREATURE          = 3,
    LOCATION_LINK_GAMEOBJECT        = 4,
    LOCATION_LINK_CREATURE_ENTRY    = 5,
    LOCATION_LINK_GAMEOBJECT_ENTRY  = 6,
    LOCATION_LINK_AREATRIGGER       = 7,
    LOCATION_LINK_AREATRIGGER_TARGET = 8,
};

static char const* const locationKeys[] =
{
        "Htele",
        "Htaxinode",
        "Hplayer",
        "Hcreature",
        "Hgameobject",
        "Hcreature_entry",
        "Hgameobject_entry",
        "Hareatrigger",
        "Hareatrigger_target",
    nullptr
};

bool ChatHandler::ExtractLocationFromLink(char** text, uint32& mapid, float& x, float& y, float& z)
{
    int type = 0;

    char* idS = ExtractKeyFromLink(text, locationKeys, &type);
    if (!idS)
    {
        return false;
    }

    switch (type)
    {
        case LOCATION_LINK_RAW:
        case LOCATION_LINK_PLAYER:
        {
            std::string name = idS;
            if (!normalizePlayerName(name))
            {
                return false;
            }

            if (Player* player = sObjectMgr.GetPlayer(name.c_str()))
            {
                mapid = player->GetMapId();
                x = player->Where().X();
                y = player->Where().Y();
                z = player->Where().Z();
                return true;
            }

            if (ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(name))
            {

                float o;
                bool in_flight;
                return CharacterRows::PlaceOf(guid, mapid, x, y, z, o, in_flight);
            }

            return false;
        }
        case LOCATION_LINK_TELE:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            GameTele const* tele = sObjectMgr.GetGameTele(id);
            if (!tele)
            {
                return false;
            }
            mapid = tele->mapId;
            x = tele->position_x;
            y = tele->position_y;
            z = tele->position_z;
            return true;
        }
        case LOCATION_LINK_TAXINODE:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(id);
            if (!node)
            {
                return false;
            }
            mapid = node->map_id;
            x = node->x;
            y = node->y;
            z = node->z;
            return true;
        }
        case LOCATION_LINK_CREATURE:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&idS, lowguid))
            {
                return false;
            }

            if (CreatureData const* data = sObjectMgr.GetCreatureData(lowguid))
            {
                mapid = data->mapid;
                x = data->posX;
                y = data->posY;
                z = data->posZ;
                return true;
            }
            else
            {
                return false;
            }
        }
        case LOCATION_LINK_GAMEOBJECT:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&idS, lowguid))
            {
                return false;
            }

            if (GameObjectData const* data = sObjectMgr.GetGOData(lowguid))
            {
                mapid = data->mapid;
                x = data->posX;
                y = data->posY;
                z = data->posZ;
                return true;
            }
            else
            {
                return false;
            }
        }
        case LOCATION_LINK_CREATURE_ENTRY:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            if (ObjectMgr::GetCreatureTemplate(id))
            {
                FindCreatureData worker(id, m_session ? m_session->GetPlayer() : nullptr);

                sObjectMgr.DoCreatureData(worker);

                if (CreatureDataPair const* dataPair = worker.GetResult())
                {
                    mapid = dataPair->second.mapid;
                    x = dataPair->second.posX;
                    y = dataPair->second.posY;
                    z = dataPair->second.posZ;
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        case LOCATION_LINK_GAMEOBJECT_ENTRY:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            if (ObjectMgr::GetGameObjectInfo(id))
            {
                FindGOData worker(id, m_session ? m_session->GetPlayer() : nullptr);

                sObjectMgr.DoGOData(worker);

                if (GameObjectDataPair const* dataPair = worker.GetResult())
                {
                    mapid = dataPair->second.mapid;
                    x = dataPair->second.posX;
                    y = dataPair->second.posY;
                    z = dataPair->second.posZ;
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        case LOCATION_LINK_AREATRIGGER:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(id);
            if (!atEntry)
            {
                PSendSysMessage(LANG_COMMAND_GOAREATRNOTFOUND, id);
                SetSentErrorMessage(true);
                return false;
            }

            mapid = atEntry->mapid;
            x = atEntry->x;
            y = atEntry->y;
            z = atEntry->z;
            return true;
        }
        case LOCATION_LINK_AREATRIGGER_TARGET:
        {
            uint32 id;
            if (!ExtractUInt32(&idS, id))
            {
                return false;
            }

            if (!sAreaTriggerStore.LookupEntry(id))
            {
                PSendSysMessage(LANG_COMMAND_GOAREATRNOTFOUND, id);
                SetSentErrorMessage(true);
                return false;
            }

            AreaTrigger const* at = sObjectMgr.GetAreaTrigger(id);
            if (!at)
            {
                PSendSysMessage(LANG_AREATRIGER_NOT_HAS_TARGET, id);
                SetSentErrorMessage(true);
                return false;
            }

            mapid = at->target_mapId;
            x = at->target_X;
            y = at->target_Y;
            z = at->target_Z;
            return true;
        }
    }

    return false;
}

std::string ChatHandler::ExtractPlayerNameFromLink(char** text)
{

    char* name_str = ExtractKeyFromLink(text, "Hplayer");
    if (!name_str)
    {
        return "";
    }

    std::string name = name_str;
    if (!normalizePlayerName(name))
    {
        return "";
    }

    return name;
}

bool ChatHandler::ExtractPlayerTarget(char** args, Player** player , ObjectGuid* player_guid , std::string* player_name )
{
    if (*args &&**  args)
    {
        std::string name = ExtractPlayerNameFromLink(args);
        if (name.empty())
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }

        Player* pl = sObjectMgr.GetPlayer(name.c_str());

        if (player)
        {
            *player = pl;
        }

        ObjectGuid guid = !pl && (player_guid || player_name) ? sObjectMgr.GetPlayerGuidByName(name) : 0;

        if (player_guid)
        {
            *player_guid = pl ? pl->GetObjectGuid() : guid;
        }

        if (player_name)
        {
            *player_name = pl || guid ? name : "";
        }
    }
    else
    {
        Player* pl = getSelectedPlayer();

        if (player)
        {
            *player = pl;
        }

        ObjectGuid guid = pl ? pl->GetObjectGuid() : 0;

        if (!pl && !m_session)
        {
            uint32 accountId = GetAccountId();
            auto itr = m_consoleSelectedPlayers.find(accountId);
            if (itr != m_consoleSelectedPlayers.end())
            {
                guid = itr->second;
            }
        }

        if (player_guid)
        {
            *player_guid = guid;
        }

        if (player_name)
        {
            if (pl)
            {
                *player_name = pl->GetName();
            }
            else if (guid)
            {
                std::string name;
                if (!sObjectMgr.GetPlayerNameByGUID(guid, name) || name.empty())
                {
                    if (player_guid)
                    {
                        *player_guid = 0;
                    }
                    *player_name = "";
                }
                else
                {
                    *player_name = name;
                }
            }
            else
            {
                *player_name = "";
            }
        }
    }

    if ((!player || !*player) && (!player_guid || !*player_guid) && (!player_name || player_name->empty()))
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

uint32 ChatHandler::ExtractAccountId(char** args, std::string* accountName , Player** targetIfNullArg )
{
    uint32 account_id = 0;

    char* account_str = ExtractLiteralArg(args);

    if (!account_str)
    {
        if (!targetIfNullArg)
        {
            return 0;
        }

        Player* targetPlayer = getSelectedPlayer();
        if (!targetPlayer)
        {
            return 0;
        }

        account_id = targetPlayer->GetSession()->GetAccountId();

        if (accountName)
        {
            sAccountMgr.GetName(account_id, *accountName);
        }

        if (targetIfNullArg)
        {
            *targetIfNullArg = targetPlayer;
        }

        return account_id;
    }

    std::string account_name;

    if (ExtractUInt32(&account_str, account_id))
    {
        if (!sAccountMgr.GetName(account_id, account_name))
        {
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_str);
            SetSentErrorMessage(true);
            return 0;
        }
    }
    else
    {
        account_name = account_str;
        if (!Utf8ToUpperOnlyLatin(account_name))
        {
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_name.c_str());
            SetSentErrorMessage(true);
            return 0;
        }

        account_id = sAccountMgr.GetId(account_name);
        if (!account_id)
        {
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_name.c_str());
            SetSentErrorMessage(true);
            return 0;
        }
    }

    if (accountName)
    {
        *accountName = account_name;
    }

    if (targetIfNullArg)
    {
        *targetIfNullArg = nullptr;
    }

    return account_id;
}

struct RaceMaskName
{
    char const* literal;
    uint32 raceMask;
};

static RaceMaskName const raceMaskNames[] =
{

    { "human", (1 << (RACE_HUMAN - 1))   },
    { "orc", (1 << (RACE_ORC - 1))     },
    { "dwarf", (1 << (RACE_DWARF - 1))   },
    { "nightelf", (1 << (RACE_NIGHTELF - 1))},
    { "undead", (1 << (RACE_UNDEAD - 1))  },
    { "tauren", (1 << (RACE_TAUREN - 1))  },
    { "gnome", (1 << (RACE_GNOME - 1))   },
    { "troll", (1 << (RACE_TROLL - 1))   },

    { "alliance", RACEMASK_ALLIANCE },
    { "horde",    RACEMASK_HORDE },
    { "all", RACEMASK_ALL_PLAYABLE },

    { nullptr, 0 }
};

bool ChatHandler::ExtractRaceMask(char** text, uint32& raceMask, char const** maskName )
{
    if (ExtractUInt32(text, raceMask))
    {
        if (maskName)
        {
            *maskName = "custom mask";
        }
    }
    else
    {
        for (RaceMaskName const* itr = raceMaskNames; itr->literal; ++itr)
        {
            if (ExtractLiteralArg(text, itr->literal))
            {
                raceMask = itr->raceMask;

                if (maskName)
                {
                    *maskName = itr->literal;
                }
                break;
            }
        }

        if (!raceMask)
        {
            return false;
        }
    }

    return true;
}
