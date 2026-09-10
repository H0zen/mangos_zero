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
#include <string>
#include <vector>
#include <ctime>
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "QueryAnswers.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "CreatureRecord.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "NPCHandler.h"
#include "SQLStorages.h"
#include "Corpse.h"

void WorldSession::SendNameQueryOpcode(Player* p)
{
    if (!p)
    {
        return;
    }

    WorldPacket data(SMSG_NAME_QUERY_RESPONSE, (8 + 25 + 1 + 4 + 4 + 4));
    data << p->GetObjectGuid();
    data << p->GetName();
    data << uint8(0);
    data << uint32(p->getRace());
    data << uint32(p->getGender());
    data << uint32(p->getClass());

    SendPacket(&data);
}

void WorldSession::SendNameQueryOpcodeFromDB(ObjectGuid guid)
{
    uint32 accountId = GetAccountId();
    CharacterDatabase.AsyncPQuery([accountId](QueryResult* result)
                                  {
                                      WorldSession::SendNameQueryOpcodeFromDBCallBack(result, accountId);
                                  },

            "SELECT guid, name, race, gender, class "
            "FROM characters WHERE guid = '%u'",
        GuidCounter(guid));
}

void WorldSession::SendNameQueryOpcodeFromDBCallBack(QueryResult* result, uint32 accountId)
{
    if (!result)
    {
        return;
    }

    WorldSession* session = sWorld.FindSession(accountId);
    if (!session)
    {
        delete result;
        return;
    }

    Field* fields = result->Fetch();
    uint32 lowguid      = fields[0].GetUInt32();
    std::string name = fields[1].GetCppString();
    uint8 pRace = 0, pGender = 0, pClass = 0;
    if (!name.empty())
    {
        pRace        = fields[2].GetUInt8();
        pGender      = fields[3].GetUInt8();
        pClass       = fields[4].GetUInt8();
    }

    WorldPacket data(SMSG_NAME_QUERY_RESPONSE, (8 + (name.size()+1) + 1 + 4 + 4 + 4));
    data << MakeGuid(HIGHGUID_PLAYER, lowguid);
    data << name;
    data << uint8(0);
    data << uint32(pRace);
    data << uint32(pGender);
    data << uint32(pClass);

    session->SendPacket(&data);
    delete result;
}

void queries::NameQuery(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;

    recv_data >> guid;

    Player* pChar = sObjectMgr.GetPlayer(guid);

    if (pChar)
    {
        session.SendNameQueryOpcode(pChar);
    }
    else
    {
        session.SendNameQueryOpcodeFromDB(guid);
    }
}

void queries::QueryTime(WorldSession& session, WorldPacket& )
{
    session.SendQueryTimeResponse();
}

void queries::CreatureQuery(WorldSession& session, WorldPacket& recv_data)
{
    uint32 entry;
    ObjectGuid guid = 0;

    recv_data >> entry;
    recv_data >> guid;

    Creature* unit = session.GetPlayer()->GetMap()->GetAnyTypeCreature(guid);

    CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(entry);
    if (ci)
    {
        int loc_idx = session.GetSessionDbLocaleIndex();

        char const* name = ci->Name;
        char const* subName = ci->SubName;
        sObjectMgr.GetCreatureLocaleStrings(entry, loc_idx, &name, &subName);

        DETAIL_LOG("WORLD: CMSG_CREATURE_QUERY '%s' - Entry: %u.", ci->Name, entry);

        CreatureRecord const record(*ci);

        WorldPacket data(SMSG_CREATURE_QUERY_RESPONSE, 100);
        data << uint32(entry);
        data << name;
        data << uint8(0) << uint8(0) << uint8(0);
        data << subName;
        data << uint32(record.Flags());

        data << uint32(unit && unit->IsPet() ? 0 : record.Kind());
        data << uint32(record.Family());
        data << uint32(record.Rank());
        data << uint32(0);
        data << uint32(record.PetSpells());

        data << uint32(unit ? unit->GetUInt32Value(UNIT_FIELD_DISPLAYID)
                            : Creature::ChooseDisplayId(ci));

        data << uint8(record.IsCivilian() ? 1 : 0);
        data << uint8(record.IsRacialLeader() ? 1 : 0);
        session.SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_CREATURE_QUERY_RESPONSE");
    }
    else
    {
        DEBUG_LOG("WORLD: CMSG_CREATURE_QUERY - Guid: %s Entry: %u NO CREATURE INFO!",
            GuidString(guid).c_str(), entry);
        WorldPacket data(SMSG_CREATURE_QUERY_RESPONSE, 4);
        data << uint32(entry | 0x80000000);
        session.SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_CREATURE_QUERY_RESPONSE");
    }
}

void queries::GameObjectQuery(WorldSession& session, WorldPacket& recv_data)
{
    uint32 entryID;
    recv_data >> entryID;
    ObjectGuid guid = 0;
    recv_data >> guid;

    const GameObjectInfo* info = ObjectMgr::GetGameObjectInfo(entryID);
    if (info)
    {
        std::string Name = info->name;

        int loc_idx = session.GetSessionDbLocaleIndex();
        if (loc_idx >= 0)
        {
            GameObjectLocale const* gl = sObjectMgr.GetGameObjectLocale(entryID);
            if (gl)
            {
                if (gl->Name.size() > size_t(loc_idx) && !gl->Name[loc_idx].empty())
                {
                    Name = gl->Name[loc_idx];
                }
            }
        }
        DETAIL_LOG("WORLD: CMSG_GAMEOBJECT_QUERY '%s' - Entry: %u. ", info->name, entryID);
        WorldPacket data(SMSG_GAMEOBJECT_QUERY_RESPONSE, 150);
        data << uint32(entryID);
        data << uint32(info->type);
        data << uint32(info->displayId);
        data << Name;
        data << uint8(0) << uint8(0) << uint8(0);
        data << uint8(0);
        data.append(info->raw.data, 24);

        session.SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_GAMEOBJECT_QUERY_RESPONSE");
    }
    else
    {
        DEBUG_LOG("WORLD: CMSG_GAMEOBJECT_QUERY - Guid: %s Entry: %u Missing gameobject info!",
            GuidString(guid).c_str(), entryID);
        WorldPacket data(SMSG_GAMEOBJECT_QUERY_RESPONSE, 4);
        data << uint32(entryID | 0x80000000);
        session.SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_GAMEOBJECT_QUERY_RESPONSE");
    }
}

void queries::CorpseQuery(Player& who, WorldPacket& )
{
    DETAIL_LOG("WORLD: Received opcode MSG_CORPSE_QUERY");

    Corpse* corpse = who.GetCorpse();

    if (!corpse)
    {
        WorldPacket data(MSG_CORPSE_QUERY, 1);
        data << uint8(0);
        who.GetSession()->SendPacket(&data);
        return;
    }

    uint32 corpsemapid = corpse->GetMapId();
    float x = corpse->Where().X();
    float y = corpse->Where().Y();
    float z = corpse->Where().Z();
    int32 mapid = corpsemapid;

    if (corpsemapid != who.GetMapId())
    {

        if (InstanceTemplate const* corpseMapEntry = sObjectMgr.GetInstanceTemplate(mapid))
        {
            if (corpseMapEntry->ghostEntranceMap >= 0)
            {

                if (TerrainInfo const* entranceMap = sTerrainMgr.LoadTerrain(corpseMapEntry->ghostEntranceMap))
                {
                    mapid = corpseMapEntry->ghostEntranceMap;
                    x = corpseMapEntry->ghostEntranceX;
                    y = corpseMapEntry->ghostEntranceY;
                    const auto entranceFloor = entranceMap->StaticFloor(x, y, MAX_HEIGHT);
                    z = entranceFloor ? *entranceFloor : INVALID_HEIGHT;
                }
            }
        }
    }

    WorldPacket data(MSG_CORPSE_QUERY, 1 + (5 * 4));
    data << uint8(1);
    data << int32(mapid);
    data << float(x);
    data << float(y);
    data << float(z);
    data << uint32(corpsemapid);
    who.GetSession()->SendPacket(&data);
}

void queries::NpcTextQuery(WorldSession& session, WorldPacket& recv_data)
{
    uint32 textID;
    ObjectGuid guid = 0;

    recv_data >> textID;
    recv_data >> guid;

    DETAIL_LOG("WORLD: CMSG_NPC_TEXT_QUERY ID '%u'", textID);

    session.GetPlayer()->SetTargetGuid(guid);

    GossipText const* pGossip = sObjectMgr.GetGossipText(textID);

    WorldPacket data(SMSG_NPC_TEXT_UPDATE, 100);
    data << textID;

    if (!pGossip)
    {
        for (uint32 i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            data << float(0);
            data << "Greetings $N";
            data << "Greetings $N";
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
        }
    }
    else
    {
        std::string Text_0[MAX_GOSSIP_TEXT_OPTIONS], Text_1[MAX_GOSSIP_TEXT_OPTIONS];
        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            Text_0[i] = pGossip->Options[i].Text_0;
            Text_1[i] = pGossip->Options[i].Text_1;
        }

        int loc_idx = session.GetSessionDbLocaleIndex();

        sObjectMgr.GetNpcTextLocaleStringsAll(textID, loc_idx, &Text_0, &Text_1);

        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            data << pGossip->Options[i].Probability;

            if (Text_0[i].empty())
            {
                data << Text_1[i];
            }
            else
            {
                data << Text_0[i];
            }

            if (Text_1[i].empty())
            {
                data << Text_0[i];
            }
            else
            {
                data << Text_1[i];
            }

            data << pGossip->Options[i].Language;

            for (int j = 0; j < 3; ++j)
            {
                data << pGossip->Options[i].Emotes[j]._Delay;
                data << pGossip->Options[i].Emotes[j]._Emote;
            }
        }
    }

    session.SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_NPC_TEXT_UPDATE");
}

void queries::PageTextQuery(WorldSession& session, WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_PAGE_TEXT_QUERY");

    uint32 pageID;
    recv_data >> pageID;
    recv_data.read_skip<uint64>();

    while (pageID)
    {
        PageText const* pPage = sPageTextStore.LookupEntry<PageText>(pageID);

        WorldPacket data(SMSG_PAGE_TEXT_QUERY_RESPONSE, 50);
        data << pageID;

        if (!pPage)
        {
            data << "Item page missing.";
            data << uint32(0);
            pageID = 0;
        }
        else
        {
            std::string Text = pPage->Text;

            int loc_idx = session.GetSessionDbLocaleIndex();
            if (loc_idx >= 0)
            {
                PageTextLocale const* pl = sObjectMgr.GetPageTextLocale(pageID);
                if (pl)
                {
                    if (pl->Text.size() > size_t(loc_idx) && !pl->Text[loc_idx].empty())
                    {
                        Text = pl->Text[loc_idx];
                    }
                }
            }

            data << Text;
            data << uint32(pPage->Next_Page);
            pageID = pPage->Next_Page;
        }
        session.SendPacket(&data);

        DEBUG_LOG("WORLD: Sent SMSG_PAGE_TEXT_QUERY_RESPONSE");
    }
}

void WorldSession::SendQueryTimeResponse()
{
    WorldPacket data(SMSG_QUERY_TIME_RESPONSE, 4);
    data << uint32(time(nullptr));
    SendPacket(&data);
}
