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
#include <sstream>
#include "Language.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "PetitionAnswers.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "GossipDef.h"
#include "SocialMgr.h"
#include "PlayerRegistry.h"

#define GUILD_CHARTER               5863
#define GUILD_CHARTER_COST          1000
#define CHARTER_DISPLAY_ID          16161

void petitions::PetitionBuy(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_BUY");
    recv_data.hexlike();

    ObjectGuid guidNPC = 0;
    uint32 unk2;
    std::string name;

    recv_data >> guidNPC;
    recv_data.read_skip<uint32>();
    recv_data.read_skip<uint64>();
    recv_data >> name;
    recv_data.read_skip<uint32>();
    recv_data.read_skip<uint32>();
    recv_data.read_skip<uint32>();
    recv_data.read_skip<uint32>();
    recv_data.read_skip<uint32>();
    recv_data.read_skip<uint32>();
    recv_data.read_skip<uint32>();
    recv_data.read_skip<uint32>();
    recv_data.read_skip<uint32>();
    recv_data.read_skip<uint32>();
    recv_data.read_skip<uint16>();
    recv_data.read_skip<uint8>();

    recv_data >> unk2;
    recv_data.read_skip<uint32>();

    DEBUG_LOG("Petitioner %s tried sell petition: name %s", GuidString(guidNPC).c_str(), name.c_str());

    Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(guidNPC, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionBuyOpcode - %s not found or you can't interact with him.", GuidString(guidNPC).c_str());
        return;
    }

    if (!pCreature->IsTabardDesigner())
    {
        return;
    }

    if (session.GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        session.GetPlayer()->RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    if (session.GetPlayer()->GetGuildId())
    {
        return;
    }

    uint32 charterid = GUILD_CHARTER;
    uint32 cost = GUILD_CHARTER_COST;

    if (sGuildMgr.GetGuildByName(name))
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_EXISTS_S);
        return;
    }
    if (sObjectMgr.IsReservedName(name) || !ObjectMgr::IsValidCharterName(name))
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_INVALID);
        return;
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(charterid);
    if (!pProto)
    {
        session.GetPlayer()->SendBuyError(BUY_ERR_CANT_FIND_ITEM, nullptr, charterid, 0);
        return;
    }

    if (session.GetPlayer()->GetMoney() < sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST))
    {

        session.GetPlayer()->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, charterid, 0);
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = session.GetPlayer()->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, charterid, pProto->BuyCount);
    if (msg != EQUIP_ERR_OK)
    {
        session.GetPlayer()->SendEquipError(msg, nullptr, nullptr, charterid);
        return;
    }

    session.GetPlayer()->ModifyMoney(-int64(sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST)));
    Item* charter = session.GetPlayer()->StoreNewItem(dest, GUILD_CHARTER, true);
    if (!charter)
    {
        return;
    }

    charter->SetUInt32Value(ITEM_FIELD_ENCHANTMENT, charter->GetGUIDLow());

    charter->SetState(ITEM_CHANGED, session.GetPlayer());
    session.GetPlayer()->SendNewItem(charter, 1, true, false);

    QueryResult* result = CharacterDatabase.PQuery("SELECT `petitionguid` FROM `petition` WHERE `ownerguid` = '%u'", session.GetPlayer()->GetGUIDLow());

    std::ostringstream ssInvalidPetitionGUIDs;

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            ssInvalidPetitionGUIDs << "'" << fields[0].GetUInt32() << "' , ";
        }
        while (result->NextRow());

        delete result;
    }

    ssInvalidPetitionGUIDs << "'" << charter->GetGUIDLow() << "'";

    DEBUG_LOG("Invalid petition GUIDs: %s", ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.escape_string(name);
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `petitionguid` IN ( %s )",  ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `petitionguid` IN ( %s )", ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.PExecute("INSERT INTO `petition` (`ownerguid`, `petitionguid`, `name`) VALUES ('%u', '%u', '%s')",
        session.GetPlayer()->GetGUIDLow(), charter->GetGUIDLow(), name.c_str());
    CharacterDatabase.CommitTransaction();
}

void petitions::PetitionShowSign(Player& who, WorldPacket& recv_data)
{

    DEBUG_LOG("Received opcode CMSG_PETITION_SHOW_SIGNATURES");

    uint8 signs = 0;
    ObjectGuid petitionguid = 0;
    recv_data >> petitionguid;

    uint32 petitionguid_low = GuidCounter(petitionguid);

    if (who.GetGuildId())
    {
        return;
    }

    QueryResult* result = CharacterDatabase.PQuery("SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionguid_low);

    if (result)
    {
        signs = (uint8)result->GetRowCount();
    }

    DEBUG_LOG("CMSG_PETITION_SHOW_SIGNATURES petition: %s", GuidString(petitionguid).c_str());

    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + 1 + signs * 12));
    data << static_cast<ObjectGuid>(petitionguid);
    data << who.GetObjectGuid();
    data << uint32(petitionguid_low);
    data << uint8(signs);

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = result->Fetch();
        ObjectGuid signerGuid = MakeGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << static_cast<ObjectGuid>(signerGuid);
        data << uint32(0);

        result->NextRow();
    }
    delete result;
    who.GetSession()->SendPacket(&data);
}

void petitions::PetitionQuery(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_QUERY");

    uint32 guildguid;
    ObjectGuid petitionguid = 0;
    recv_data >> guildguid;
    recv_data >> petitionguid;
    DEBUG_LOG("CMSG_PETITION_QUERY Petition %s Guild GUID %u", GuidString(petitionguid).c_str(), guildguid);

    who.GetSession()->SendPetitionQueryOpcode(petitionguid);
}

void WorldSession::SendPetitionQueryOpcode(ObjectGuid petitionguid)
{
    uint32 petitionLowGuid = GuidCounter(petitionguid);

    ObjectGuid ownerGuid = 0;
    std::string name = "NO_NAME_FOR_GUID";
    uint8 signs = 0;

    QueryResult* result = CharacterDatabase.PQuery(
            "SELECT `ownerguid`, `name`, "
            "  (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '%u') AS `signs` "
            "FROM `petition` WHERE `petitionguid` = '%u'", petitionLowGuid, petitionLowGuid);

    if (result)
    {
        Field* fields = result->Fetch();
        ownerGuid = MakeGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        name      = fields[1].GetCppString();
        signs     = fields[2].GetUInt8();
        delete result;
    }
    else
    {
        DEBUG_LOG("CMSG_PETITION_QUERY failed for petition (GUID: %u)", petitionLowGuid);
        return;
    }

    WorldPacket data(SMSG_PETITION_QUERY_RESPONSE, (4 + 8 + name.size() + 1 + 2 + 4 * 11));
    data << uint32(petitionLowGuid);
    data << static_cast<ObjectGuid>(ownerGuid);
    data << name;
    data << uint8(0);
    data << uint32(1);
    data << uint32(9);
    data << uint32(9);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint16(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);

    data << uint32(0);
    SendPacket(&data);
}

void petitions::PetitionRename(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode MSG_PETITION_RENAME");

    ObjectGuid petitionGuid = 0;
    std::string newname;

    recv_data >> petitionGuid;
    recv_data >> newname;

    Item* item = session.GetPlayer()->GetItemByGuid(petitionGuid);
    if (!item)
    {
        return;
    }

    if (sGuildMgr.GetGuildByName(newname))
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, newname, ERR_GUILD_NAME_EXISTS_S);
        return;
    }
    if (sObjectMgr.IsReservedName(newname) || !ObjectMgr::IsValidCharterName(newname))
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, newname, ERR_GUILD_NAME_INVALID);
        return;
    }

    std::string db_newname = newname;
    CharacterDatabase.escape_string(db_newname);
    CharacterDatabase.PExecute("UPDATE `petition` SET `name` = '%s' WHERE `petitionguid` = '%u'",
        db_newname.c_str(), GuidCounter(petitionGuid));

    DEBUG_LOG("Petition %s renamed to '%s'", GuidString(petitionGuid).c_str(), newname.c_str());

    WorldPacket data(MSG_PETITION_RENAME, (8 + newname.size() + 1));
    data << static_cast<ObjectGuid>(petitionGuid);
    data << newname;
    session.SendPacket(&data);
}

void petitions::PetitionSign(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_SIGN");

    Field* fields;
    ObjectGuid petitionGuid = 0;
    uint8 unk;
    recv_data >> petitionGuid;
    recv_data >> unk;

    uint32 petitionLowGuid = GuidCounter(petitionGuid);

    QueryResult* result = CharacterDatabase.PQuery(
            "SELECT `ownerguid`, "
            "  (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '%u') AS `signs` "
            "FROM `petition` WHERE `petitionguid` = '%u'", petitionLowGuid, petitionLowGuid);

    if (!result)
    {
        sLog.outError("any petition on server...");
        return;
    }

    fields = result->Fetch();
    uint32 ownerLowGuid = fields[0].GetUInt32();
    ObjectGuid ownerGuid = MakeGuid(HIGHGUID_PLAYER, ownerLowGuid);
    uint8 signs = fields[1].GetUInt8();

    delete result;

    if (ownerGuid == session.GetPlayer()->GetObjectGuid())
    {
        return;
    }

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
        session.GetPlayer()->GetTeam() != sObjectMgr.GetPlayerTeamByGUID(ownerGuid))
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (session.GetPlayer()->GetGuildId())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, session.GetPlayer()->GetName(), ERR_ALREADY_IN_GUILD_S);
        return;
    }
    if (session.GetPlayer()->Invites().ToGuild())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, session.GetPlayer()->GetName(), ERR_ALREADY_INVITED_TO_GUILD_S);
        return;
    }

    if (++signs > 9)
    {
        return;
    }

    result = CharacterDatabase.PQuery("SELECT `playerguid` FROM `petition_sign` WHERE `player_account` = '%u' AND `petitionguid` = '%u'", session.GetAccountId(), petitionLowGuid);

    if (result)
    {
        delete result;
        WorldPacket data(SMSG_PETITION_SIGN_RESULTS, (8 + 8 + 4));
        data << static_cast<ObjectGuid>(petitionGuid);
        data << static_cast<ObjectGuid>(session.GetPlayer()->GetObjectGuid());
        data << uint32(PETITION_SIGN_ALREADY_SIGNED);

        session.SendPacket(&data);

        if (Player* owner = sObjectMgr.GetPlayer(ownerGuid))
        {
            owner->GetSession()->SendPacket(&data);
        }
        return;
    }

    CharacterDatabase.PExecute("INSERT INTO `petition_sign` (`ownerguid`,`petitionguid`, `playerguid`, `player_account`) VALUES ('%u', '%u', '%u','%u')",
        ownerLowGuid, petitionLowGuid, session.GetPlayer()->GetGUIDLow(), session.GetAccountId());

    DEBUG_LOG("PETITION SIGN: %s by %s", GuidString(petitionGuid).c_str(), session.GetPlayer()->GetGuidStr().c_str());

    WorldPacket data(SMSG_PETITION_SIGN_RESULTS, (8 + 8 + 4));
    data << static_cast<ObjectGuid>(petitionGuid);
    data << static_cast<ObjectGuid>(session.GetPlayer()->GetObjectGuid());
    data << uint32(PETITION_SIGN_OK);

    session.SendPacket(&data);

    if (Player* owner = sObjectMgr.GetPlayer(ownerGuid))
    {
        owner->GetSession()->SendPacket(&data);
    }
}

void petitions::PetitionDecline(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode MSG_PETITION_DECLINE");

    ObjectGuid petitionGuid = 0;
    recv_data >> petitionGuid;

    DEBUG_LOG("Petition %s declined by %s", GuidString(petitionGuid).c_str(), who.GetGuidStr().c_str());

    uint32 petitionLowGuid = GuidCounter(petitionGuid);

    QueryResult* result = CharacterDatabase.PQuery("SELECT `ownerguid` FROM `petition` WHERE `petitionguid` = '%u'", petitionLowGuid);
    if (!result)
    {
        return;
    }

    Field* fields = result->Fetch();
    ObjectGuid ownerguid = MakeGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
    delete result;

    Player* owner = sObjectMgr.GetPlayer(ownerguid);
    if (owner)
    {
        WorldPacket data(MSG_PETITION_DECLINE, 8);
        data << who.GetObjectGuid();
        owner->GetSession()->SendPacket(&data);
    }
}

void petitions::OfferPetition(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_OFFER_PETITION");

    ObjectGuid petitionGuid = 0;
    ObjectGuid playerGuid = 0;
    recv_data >> petitionGuid;
    recv_data >> playerGuid;

    Player* player = sPlayerRegistry.Find(playerGuid);
    if (!player)
    {
        return;
    }

    DEBUG_LOG("OFFER PETITION: petition %s to %s", GuidString(petitionGuid).c_str(), GuidString(playerGuid).c_str());

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && session.GetPlayer()->GetTeam() != player->GetTeam())
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (player->GetGuildId())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, session.GetPlayer()->GetName(), ERR_ALREADY_IN_GUILD_S);
        return;
    }

    if (player->Invites().ToGuild())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, session.GetPlayer()->GetName(), ERR_ALREADY_INVITED_TO_GUILD_S);
        return;
    }

    uint8 signs = 0;
    QueryResult* result = CharacterDatabase.PQuery("SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", GuidCounter(petitionGuid));

    if (result)
    {
        signs = (uint8)result->GetRowCount();
    }

    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + 1 + signs * 12));
    data << static_cast<ObjectGuid>(petitionGuid);
    data << static_cast<ObjectGuid>(session.GetPlayer()->GetObjectGuid());
    data << uint32(GuidCounter(petitionGuid));
    data << uint8(signs);

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = result->Fetch();
        ObjectGuid signerGuid = MakeGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << static_cast<ObjectGuid>(signerGuid);
        data << uint32(0);

        result->NextRow();
    }

    delete result;
    player->GetSession()->SendPacket(&data);
}

void petitions::TurnInPetition(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_TURN_IN_PETITION");

    ObjectGuid petitionGuid = 0;

    recv_data >> petitionGuid;

    DEBUG_LOG("Petition %s turned in by %s", GuidString(petitionGuid).c_str(), session.GetPlayer()->GetGuidStr().c_str());

    ObjectGuid ownerGuid = 0;
    std::string name;

    QueryResult* result = CharacterDatabase.PQuery("SELECT `ownerguid`, `name` FROM `petition` WHERE `petitionguid` = '%u'", GuidCounter(petitionGuid));
    if (result)
    {
        Field* fields = result->Fetch();
        ownerGuid = MakeGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        name = fields[1].GetCppString();
        delete result;
    }
    else
    {
        sLog.outError("CMSG_TURN_IN_PETITION: petition table not have data for guid %u!", GuidCounter(petitionGuid));
        return;
    }

    if (session.GetPlayer()->GetGuildId())
    {
        WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
        data << uint32(PETITION_SIGN_ALREADY_IN_GUILD);
        session.GetPlayer()->GetSession()->SendPacket(&data);
        return;
    }

    if (session.GetPlayer()->GetObjectGuid() != ownerGuid)
    {
        return;
    }

    result = CharacterDatabase.PQuery("SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", GuidCounter(petitionGuid));
    uint8 signs = result ? (uint8)result->GetRowCount() : 0;

    uint32 count = sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS);
    if (signs < count)
    {
        WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
        data << uint32(PETITION_SIGN_NEED_MORE);
        session.SendPacket(&data);
        delete result;
        return;
    }

    if (sGuildMgr.GetGuildByName(name))
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_EXISTS_S);
        delete result;
        return;
    }

    Item* item = session.GetPlayer()->GetItemByGuid(petitionGuid);
    if (!item)
    {
        delete result;
        return;
    }

    session.GetPlayer()->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);

    Guild* guild = new Guild;
    if (!guild->Create(session.GetPlayer(), name))
    {
        delete guild;
        delete result;
        return;
    }

    sGuildMgr.AddGuild(guild);

    for (uint8 i = 0; i < signs; ++i)
    {
        Field* fields = result->Fetch();

        ObjectGuid signGuid = MakeGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        if ((signGuid == 0))
        {
            continue;
        }

        guild->AddMember(signGuid, guild->GetLowestRank());
        result->NextRow();
    }

    delete result;

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `petitionguid` = '%u'", GuidCounter(petitionGuid));
    CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `petitionguid` = '%u'", GuidCounter(petitionGuid));
    CharacterDatabase.CommitTransaction();

    DEBUG_LOG("TURN IN PETITION %s", GuidString(petitionGuid).c_str());

    WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
    data << uint32(PETITION_SIGN_OK);
    session.SendPacket(&data);
}

void petitions::PetitionShowList(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("Received CMSG_PETITION_SHOWLIST");

    ObjectGuid guid = 0;
    recv_data >> guid;

    who.GetSession()->SendPetitionShowList(guid);
}

void WorldSession::SendPetitionShowList(ObjectGuid guid)
{
    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionShowListOpcode - %s not found or you can't interact with him.", GuidString(guid).c_str());
        return;
    }

    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    WorldPacket data(SMSG_PETITION_SHOWLIST, 8 + 1 + 4 * 5);
    data << guid;

    if (pCreature->IsTabardDesigner())
    {
        data << uint8(1);
        data << uint32(1);
        data << uint32(GUILD_CHARTER);
        data << uint32(CHARTER_DISPLAY_ID);
        data << uint32(sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST));
        data << uint32(1);
    }

    SendPacket(&data);
    DEBUG_LOG("Sent SMSG_PETITION_SHOWLIST");
}
