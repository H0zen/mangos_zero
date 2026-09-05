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

/**
 * @file PetitionsHandler.cpp
 * @brief Guild and arena charter opcode handlers
 *
 * This file handles petition-related opcodes for guild and arena charters:
 * - CMSG_PETITION_BUY: Buy guild/arena charter
 * - CMSG_PETITION_SHOW_SIGNATURES: Show charter signatures
 * - CMSG_PETITION_SIGN: Sign charter
 * - CMSG_PETITION_OFFER: Offer charter to player
 * - CMSG_PETITION_TURN_IN: Turn in completed charter
 * - CMSG_QUERY_PETITION: Query charter info
 *
 * Charters require a certain number of signatures before they can be
 * turned in to create a guild or arena team.
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

// Charters ID in item_template
#define GUILD_CHARTER               5863
#define GUILD_CHARTER_COST          1000                    // 10 S
#define CHARTER_DISPLAY_ID          16161

/**
 * @brief Handles charter purchase and petition creation.
 *
 * @param recv_data The incoming petition-buy packet.
 */
void petitions::PetitionBuy(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_BUY");
    recv_data.hexlike();

    ObjectGuid guidNPC;
    uint32 unk2;
    std::string name;

    recv_data >> guidNPC;                                   // NPC GUID
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint64>();                          // 0
    recv_data >> name;                                      // name
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint16>();                          // 0
    recv_data.read_skip<uint8>();                           // 0

    recv_data >> unk2;                                      // index
    recv_data.read_skip<uint32>();                          // 0

    DEBUG_LOG("Petitioner %s tried sell petition: name %s", guidNPC.GetString().c_str(), name.c_str());

    // prevent cheating
    Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(guidNPC, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionBuyOpcode - %s not found or you can't interact with him.", guidNPC.GetString().c_str());
        return;
    }

    if (!pCreature->IsTabardDesigner())
    {
        return;
    }

    // remove fake death
    if (session.GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        session.GetPlayer()->RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    // if tabard designer, then trying to buy a guild charter.
    // do not let if already in guild.
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
        // player hasn't got enough money
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
    // ITEM_FIELD_ENCHANTMENT is guild
    // ITEM_FIELD_ENCHANTMENT+1 is current signatures count (showed on item)
    charter->SetState(ITEM_CHANGED, session.GetPlayer());
    session.GetPlayer()->SendNewItem(charter, 1, true, false);

    // a petition is invalid, if both the owner and the type matches
    // we checked above, if this player is in an arenateam, so this must be data corruption
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

    // delete petitions with the same guid as this one
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

/**
 * @brief Sends the current signature list for a petition.
 *
 * @param recv_data The incoming show-signatures packet.
 */
void petitions::PetitionShowSign(Player& who, WorldPacket& recv_data)
{
    // ok
    DEBUG_LOG("Received opcode CMSG_PETITION_SHOW_SIGNATURES");
    // recv_data.hexlike();

    uint8 signs = 0;
    ObjectGuid petitionguid;
    recv_data >> petitionguid;                              // petition guid

    // solve (possible) some strange compile problems with explicit use GUID_LOPART(petitionguid) at some GCC versions (wrong code optimization in compiler?)
    uint32 petitionguid_low = petitionguid.GetCounter();

    // if guild petition and has guild => error, return;
    if (who.GetGuildId())
    {
        return;
    }

    QueryResult* result = CharacterDatabase.PQuery("SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionguid_low);

    // result==nullptr also correct in case no sign yet
    if (result)
    {
        signs = (uint8)result->GetRowCount();
    }

    DEBUG_LOG("CMSG_PETITION_SHOW_SIGNATURES petition: %s", petitionguid.GetString().c_str());

    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + 1 + signs * 12));
    data << ObjectGuid(petitionguid);                       // petition guid
    data << who.GetObjectGuid();                       // owner guid
    data << uint32(petitionguid_low);                       // guild guid (in mangos always same as GUID_LOPART(petitionguid)
    data << uint8(signs);                                   // sign's count

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = result->Fetch();
        ObjectGuid signerGuid = ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << ObjectGuid(signerGuid);                     // Player GUID
        data << uint32(0);                                  // there 0 ...

        result->NextRow();
    }
    delete result;
    who.GetSession()->SendPacket(&data);
}

/**
 * @brief Handles a petition query request.
 *
 * @param recv_data The incoming petition query packet.
 */
void petitions::PetitionQuery(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_QUERY");
    // recv_data.hexlike();

    uint32 guildguid;
    ObjectGuid petitionguid;
    recv_data >> guildguid;                                 // in mangos always same as GUID_LOPART(petitionguid)
    recv_data >> petitionguid;                              // petition guid
    DEBUG_LOG("CMSG_PETITION_QUERY Petition %s Guild GUID %u", petitionguid.GetString().c_str(), guildguid);

    who.GetSession()->SendPetitionQueryOpcode(petitionguid);
}

/**
 * @brief Sends petition metadata for a specific petition item.
 *
 * @param petitionguid The petition guid.
 */
void WorldSession::SendPetitionQueryOpcode(ObjectGuid petitionguid)
{
    uint32 petitionLowGuid = petitionguid.GetCounter();

    ObjectGuid ownerGuid;
    std::string name = "NO_NAME_FOR_GUID";
    uint8 signs = 0;

    QueryResult* result = CharacterDatabase.PQuery(
            "SELECT `ownerguid`, `name`, "
            "  (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '%u') AS `signs` "
            "FROM `petition` WHERE `petitionguid` = '%u'", petitionLowGuid, petitionLowGuid);

    if (result)
    {
        Field* fields = result->Fetch();
        ownerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
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
    data << uint32(petitionLowGuid);                        // guild/team guid (in mangos always same as GUID_LOPART(petition guid)
    data << ObjectGuid(ownerGuid);                          // charter owner guid
    data << name;                                           // name (guild/arena team)
    data << uint8(0);                                       // CString
    data << uint32(1);
    data << uint32(9);
    data << uint32(9);                                      // bypass client - side limitation, a different value is needed here for each petition
    data << uint32(0);                                      // 5
    data << uint32(0);                                      // 6
    data << uint32(0);                                      // 7
    data << uint32(0);                                      // 8
    data << uint32(0);                                      // 9
    data << uint16(0);                                      // 10 2 bytes field
    data << uint32(0);                                      // 11
    data << uint32(0);                                      // 12
    data << uint32(0);                                      // 13 count of next strings; if 0, no data for strings, only 1 uint32 below
    // for (int i=0; i<field13; ++i) data << chartSignersName[i];   Probably, names of the petition signers
    data << uint32(0);
    SendPacket(&data);
}

/**
 * @brief Handles petition renaming and updates persistent storage.
 *
 * @param recv_data The incoming petition rename packet.
 */
void petitions::PetitionRename(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode MSG_PETITION_RENAME");   // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    std::string newname;

    recv_data >> petitionGuid;                              // guid
    recv_data >> newname;                                   // new name

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
        db_newname.c_str(), petitionGuid.GetCounter());

    DEBUG_LOG("Petition %s renamed to '%s'", petitionGuid.GetString().c_str(), newname.c_str());

    WorldPacket data(MSG_PETITION_RENAME, (8 + newname.size() + 1));
    data << ObjectGuid(petitionGuid);
    data << newname;
    session.SendPacket(&data);
}

/**
 * @brief Handles signing a guild petition.
 *
 * @param recv_data The incoming petition sign packet.
 */
void petitions::PetitionSign(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_SIGN");    // ok
    // recv_data.hexlike();

    Field* fields;
    ObjectGuid petitionGuid;
    uint8 unk;
    recv_data >> petitionGuid;                              // petition guid
    recv_data >> unk;

    uint32 petitionLowGuid = petitionGuid.GetCounter();

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
    ObjectGuid ownerGuid = ObjectGuid(HIGHGUID_PLAYER, ownerLowGuid);
    uint8 signs = fields[1].GetUInt8();

    delete result;

    if (ownerGuid == session.GetPlayer()->GetObjectGuid())  // TODO here send SMSG_PETITION_SIGN_RESULTS with PETITION_SIGN_CANT_SIGN_OWN
    {
        return;
    }

    // not let enemies sign guild charter
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
        session.GetPlayer()->GetTeam() != sObjectMgr.GetPlayerTeamByGUID(ownerGuid))
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (session.GetPlayer()->GetGuildId())  // TODO here, and maybe below, send SMSG_PETITION_SIGN_RESULTS with PETITION_SIGN_ALREADY_IN_GUILD
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, session.GetPlayer()->GetName(), ERR_ALREADY_IN_GUILD_S);
        return;
    }
    if (session.GetPlayer()->Invites().ToGuild())
    {
        session.SendGuildCommandResult(GUILD_INVITE_S, session.GetPlayer()->GetName(), ERR_ALREADY_INVITED_TO_GUILD_S);
        return;
    }

    if (++signs > 9)                                        // client signs maximum
    {
        return;
    }

    // client doesn't allow to sign petition two times by one character, but not check sign by another character from same account
    // not allow sign another player from already sign player account
    result = CharacterDatabase.PQuery("SELECT `playerguid` FROM `petition_sign` WHERE `player_account` = '%u' AND `petitionguid` = '%u'", session.GetAccountId(), petitionLowGuid);

    if (result)
    {
        delete result;
        WorldPacket data(SMSG_PETITION_SIGN_RESULTS, (8 + 8 + 4));
        data << ObjectGuid(petitionGuid);
        data << ObjectGuid(session.GetPlayer()->GetObjectGuid());
        data << uint32(PETITION_SIGN_ALREADY_SIGNED);

        // close at signer side
        session.SendPacket(&data);

        // update for owner if online
        if (Player* owner = sObjectMgr.GetPlayer(ownerGuid))
        {
            owner->GetSession()->SendPacket(&data);
        }
        return;
    }

    CharacterDatabase.PExecute("INSERT INTO `petition_sign` (`ownerguid`,`petitionguid`, `playerguid`, `player_account`) VALUES ('%u', '%u', '%u','%u')",
        ownerLowGuid, petitionLowGuid, session.GetPlayer()->GetGUIDLow(), session.GetAccountId());

    DEBUG_LOG("PETITION SIGN: %s by %s", petitionGuid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());

    WorldPacket data(SMSG_PETITION_SIGN_RESULTS, (8 + 8 + 4));
    data << ObjectGuid(petitionGuid);
    data << ObjectGuid(session.GetPlayer()->GetObjectGuid());
    data << uint32(PETITION_SIGN_OK);

    // close at signer side
    session.SendPacket(&data);

    // update signs count on charter, required testing...
    // Item *item = session.GetPlayer()->GetItemByGuid(petitionguid));
    // if (item)
    //    item->SetUInt32Value(ITEM_FIELD_ENCHANTMENT+1, signs);

    // update for owner if online
    if (Player* owner = sObjectMgr.GetPlayer(ownerGuid))
    {
        owner->GetSession()->SendPacket(&data);
    }
}

/**
 * @brief Handles declining a petition offer and notifies the owner.
 *
 * @param recv_data The incoming petition decline packet.
 */
void petitions::PetitionDecline(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode MSG_PETITION_DECLINE");  // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    recv_data >> petitionGuid;                              // petition guid

    DEBUG_LOG("Petition %s declined by %s", petitionGuid.GetString().c_str(), who.GetGuidStr().c_str());

    uint32 petitionLowGuid = petitionGuid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery("SELECT `ownerguid` FROM `petition` WHERE `petitionguid` = '%u'", petitionLowGuid);
    if (!result)
    {
        return;
    }

    Field* fields = result->Fetch();
    ObjectGuid ownerguid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
    delete result;

    Player* owner = sObjectMgr.GetPlayer(ownerguid);
    if (owner)                                              // petition owner online
    {
        WorldPacket data(MSG_PETITION_DECLINE, 8);
        data << who.GetObjectGuid();
        owner->GetSession()->SendPacket(&data);
    }
}

/**
 * @brief Offers a petition to another player for signature.
 *
 * @param recv_data The incoming offer-petition packet.
 */
void petitions::OfferPetition(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_OFFER_PETITION");   // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    ObjectGuid playerGuid;
    recv_data >> petitionGuid;                              // petition guid
    recv_data >> playerGuid;                                // player guid

    Player* player = sPlayerRegistry.Find(playerGuid);
    if (!player)
    {
        return;
    }

    DEBUG_LOG("OFFER PETITION: petition %s to %s", petitionGuid.GetString().c_str(), playerGuid.GetString().c_str());

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

    /// Get petition signs count
    uint8 signs = 0;
    QueryResult* result = CharacterDatabase.PQuery("SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    // result==nullptr also correct charter without signs
    if (result)
    {
        signs = (uint8)result->GetRowCount();
    }

    /// Send response
    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + 1 + signs * 12));
    data << ObjectGuid(petitionGuid);                       // petition guid
    data << ObjectGuid(session.GetPlayer()->GetObjectGuid());           // owner guid
    data << uint32(petitionGuid.GetCounter());              // guild guid (in mangos always same as low part of petition guid)
    data << uint8(signs);                                   // sign's count

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = result->Fetch();
        ObjectGuid signerGuid = ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << ObjectGuid(signerGuid);                     // Player GUID
        data << uint32(0);                                  // there 0 ...

        result->NextRow();
    }

    delete result;
    player->GetSession()->SendPacket(&data);
}

/**
 * @brief Handles turning in a completed petition to create a guild.
 *
 * @param recv_data The incoming turn-in-petition packet.
 */
void petitions::TurnInPetition(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_TURN_IN_PETITION"); // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;

    recv_data >> petitionGuid;

    DEBUG_LOG("Petition %s turned in by %s", petitionGuid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());

    /// Collect petition info data
    ObjectGuid ownerGuid;
    std::string name;

    // data
    QueryResult* result = CharacterDatabase.PQuery("SELECT `ownerguid`, `name` FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    if (result)
    {
        Field* fields = result->Fetch();
        ownerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        name = fields[1].GetCppString();
        delete result;
    }
    else
    {
        sLog.outError("CMSG_TURN_IN_PETITION: petition table not have data for guid %u!", petitionGuid.GetCounter());
        return;
    }

    if (session.GetPlayer()->GetGuildId())
    {
        WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
        data << uint32(PETITION_SIGN_ALREADY_IN_GUILD); // already in guild
        session.GetPlayer()->GetSession()->SendPacket(&data);
        return;
    }

    if (session.GetPlayer()->GetObjectGuid() != ownerGuid)
    {
        return;
    }

    // signs
    result = CharacterDatabase.PQuery("SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    uint8 signs = result ? (uint8)result->GetRowCount() : 0;

    uint32 count = sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS);
    if (signs < count)
    {
        WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
        data << uint32(PETITION_SIGN_NEED_MORE); // need more signatures...
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

    // and at last charter item check
    Item* item = session.GetPlayer()->GetItemByGuid(petitionGuid);
    if (!item)
    {
        delete result;
        return;
    }

    // OK!

    // delete charter item
    session.GetPlayer()->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);

    Guild* guild = new Guild;
    if (!guild->Create(session.GetPlayer(), name))
    {
        delete guild;
        delete result;
        return;
    }

    // register guild and add guildmaster
    sGuildMgr.AddGuild(guild);

    // add members
    for (uint8 i = 0; i < signs; ++i)
    {
        Field* fields = result->Fetch();

        ObjectGuid signGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        if (signGuid.IsEmpty())
        {
            continue;
        }

        guild->AddMember(signGuid, guild->GetLowestRank());
        result->NextRow();
    }

    delete result;

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    CharacterDatabase.CommitTransaction();

    // created
    DEBUG_LOG("TURN IN PETITION %s", petitionGuid.GetString().c_str());

    WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
    data << uint32(PETITION_SIGN_OK);
    session.SendPacket(&data);
}

/**
 * @brief Handles a request to show the petitioner vendor list.
 *
 * @param recv_data The incoming show-list packet.
 */
void petitions::PetitionShowList(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("Received CMSG_PETITION_SHOWLIST");
    // recv_data.hexlike();

    ObjectGuid guid;
    recv_data >> guid;

    who.GetSession()->SendPetitionShowList(guid);
}

/**
 * @brief Sends the available petition list from a petitioner NPC.
 *
 * @param guid The petitioner NPC guid.
 */
void WorldSession::SendPetitionShowList(ObjectGuid guid)
{
    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionShowListOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    WorldPacket data(SMSG_PETITION_SHOWLIST, 8 + 1 + 4 * 5);
    data << guid;                           // npc guid

    if (pCreature->IsTabardDesigner())
    {
        data << uint8(1);                   // count
        data << uint32(1);                  // index
        data << uint32(GUILD_CHARTER);      // charter entry
        data << uint32(CHARTER_DISPLAY_ID); // charter display id
        data << uint32(sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST)); // charter cost
        data << uint32(1);                  // [-ZERO] unknown
    }

    SendPacket(&data);
    DEBUG_LOG("Sent SMSG_PETITION_SHOWLIST");
}
