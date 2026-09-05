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
 * @file Opcodes.cpp
 * @brief Network opcode handler registration
 *
 * This file registers all network packet handlers for the world server.
 * It maps each opcode to its corresponding handler function in WorldSession,
 * along with session status requirements and processing mode.
 *
 * Opcode processing modes:
 * - PROCESS_INPLACE: Process immediately in network thread
 * - PROCESS_THREADUNSAFE: Process in world update thread
 *
 * Session status requirements:
 * - STATUS_NEVER: Never process (deprecated/debug opcodes)
 * - STATUS_LOGGEDIN: Require player to be logged in
 * - STATUS_UNHANDLED: No handler assigned
 *
 * @see Opcodes.h for opcode definitions
 * @see WorldSession for packet handler implementations
 */

#include "OpcodeTable.h"
#include "PetAnswers.h"
#include "ProtocolAnswers.h"
#include "TradeAnswers.h"
#include "TaxiAnswers.h"
#include "SpellAnswers.h"
#include "SkillAnswers.h"
#include "QueryAnswers.h"
#include "PetitionAnswers.h"
#include "NPCAnswers.h"
#include "MovementAnswers.h"
#include "SocialAnswers.h"
#include "InstanceAnswers.h"
#include "MailAnswers.h"
#include "LootAnswers.h"
#include "MeetingstoneAnswers.h"
#include "ItemVendorAnswers.h"
#include "ItemEnchantAnswers.h"
#include "ItemAnswers.h"
#include "GuildAnswers.h"
#include "GroupAnswers.h"
#include "TicketAnswers.h"
#include "DuelAnswers.h"
#include "CombatAnswers.h"
#include "CharacterCustomizeAnswers.h"
#include "CharacterAnswers.h"
#include "ChatAnswers.h"
#include "ChannelAnswers.h"
#include "BattleGroundAnswers.h"
#include "AuctionAnswers.h"
#include "QuestHandler.h"
#include "Corpse.h"

/**
 * @brief Define opcode handler
 * @param opcode Opcode number
 * @param name Opcode name string
 * @param status Required session status
 * @param packetProcessing Processing mode
 * @param handler Handler function pointer
 *
 * Registers an opcode with its handler in the opcode table.
 */
static void DefineOpcode(uint16 opcode, const char* name, SessionStatus status, PacketProcessing packetProcessing, void (WorldSession::*handler)(WorldPacket& recvPacket))
{
    opcodeTable[opcode].name = name;
    opcodeTable[opcode].status = status;
    opcodeTable[opcode].packetProcessing = packetProcessing;
    opcodeTable[opcode].handler = handler;
    opcodeTable[opcode].answer = nullptr;
}

static void DefineOpcode(uint16 opcode, const char* name, SessionStatus status, PacketProcessing packetProcessing, void (*answer)(WorldSession& session, WorldPacket& packet))
{
    opcodeTable[opcode].name = name;
    opcodeTable[opcode].status = status;
    opcodeTable[opcode].packetProcessing = packetProcessing;
    opcodeTable[opcode].handler = nullptr;
    opcodeTable[opcode].answer = answer;
}

#define OPCODE( name, status, packetProcessing, handler ) DefineOpcode( name, #name, status, packetProcessing, handler )

/// Correspondence between opcodes and their names
OpcodeHandler opcodeTable[NUM_MSG_TYPES];

/**
 * @brief Initialize opcode table
 *
 * Registers all packet handlers for the world server.
 * Initializes all opcodes to STATUS_UNHANDLED, then registers
 * specific handlers for each supported opcode.
 */
void InitializeOpcodes()
{
    for (uint16 i = 0; i < NUM_MSG_TYPES; ++i)
    {
        DefineOpcode(i, "UNKNOWN", STATUS_UNHANDLED, PROCESS_INPLACE, protocol::_NULL);
    }

    OPCODE(MSG_NULL_ACTION,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_BOOTME,                                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_DBLOOKUP,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_DBLOOKUP,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUERY_OBJECT_POSITION,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_QUERY_OBJECT_POSITION,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUERY_OBJECT_ROTATION,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_QUERY_OBJECT_ROTATION,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_WORLD_TELEPORT,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleWorldTeleportOpcode);
    OPCODE(CMSG_TELEPORT_TO_UNIT,                          STATUS_LOGGEDIN, PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_ZONE_MAP,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_ZONE_MAP,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_DEBUG_CHANGECELLZONE,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_EMBLAZON_TABARD_OBSOLETE,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_UNEMBLAZON_TABARD_OBSOLETE,                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_RECHARGE,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_LEARN_SPELL,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_CREATEMONSTER,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_DESTROYMONSTER,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_CREATEITEM,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_CREATEGAMEOBJECT,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_CHECK_FOR_BOTS,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_MAKEMONSTERATTACKGUID,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_BOT_DETECTED2,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_FORCEACTION,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_FORCEACTIONONOTHER,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_FORCEACTIONSHOW,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_FORCEACTIONSHOW,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PETGODMODE,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_PETGODMODE,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DEBUGINFOSPELLMISS_OBSOLETE,               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_WEATHER_SPEED_CHEAT,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_UNDRESSPLAYER,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_BEASTMASTER,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GODMODE,                                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_GODMODE,                                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_CHEAT_SETMONEY,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_LEVEL_CHEAT,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_PET_LEVEL_CHEAT,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SET_WORLDSTATE,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_COOLDOWN_CHEAT,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_USE_SKILL_CHEAT,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_FLAG_QUEST,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_FLAG_QUEST_FINISH,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_CLEAR_QUEST,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SEND_EVENT,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_DEBUG_AISTATE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_DEBUG_AISTATE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_DISABLE_PVP_CHEAT,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_ADVANCE_SPAWN_TIME,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_PVP_PORT_OBSOLETE,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_AUTH_SRP6_BEGIN,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_AUTH_SRP6_PROOF,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_AUTH_SRP6_RECODE,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_CHAR_CREATE,                               STATUS_AUTHED,   PROCESS_THREADUNSAFE, characters::CharCreate);
    OPCODE(CMSG_CHAR_ENUM,                                 STATUS_AUTHED,   PROCESS_THREADUNSAFE, &WorldSession::HandleCharEnumOpcode);
    OPCODE(CMSG_CHAR_DELETE,                               STATUS_AUTHED,   PROCESS_THREADUNSAFE, characters::CharDelete);
    OPCODE(SMSG_AUTH_SRP6_RESPONSE,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_CHAR_CREATE,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_CHAR_ENUM,                                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_CHAR_DELETE,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PLAYER_LOGIN,                              STATUS_AUTHED,   PROCESS_INPLACE,      characters::PlayerLogin);
    OPCODE(SMSG_NEW_WORLD,                                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_TRANSFER_PENDING,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_TRANSFER_ABORTED,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_CHARACTER_LOGIN_FAILED,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LOGIN_SETTIMESPEED,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_GAMETIME_UPDATE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GAMETIME_SET,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_GAMETIME_SET,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GAMESPEED_SET,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_GAMESPEED_SET,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SERVERTIME,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_SERVERTIME,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PLAYER_LOGOUT,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandlePlayerLogoutOpcode);
    OPCODE(CMSG_LOGOUT_REQUEST,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleLogoutRequestOpcode);
    OPCODE(SMSG_LOGOUT_RESPONSE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LOGOUT_COMPLETE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_LOGOUT_CANCEL,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleLogoutCancelOpcode);
    OPCODE(SMSG_LOGOUT_CANCEL_ACK,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_NAME_QUERY,                                STATUS_AUTHED,   PROCESS_THREADUNSAFE, queries::NameQuery);
    OPCODE(SMSG_NAME_QUERY_RESPONSE,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PET_NAME_QUERY,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<pets::PetNameQuery>);
    OPCODE(SMSG_PET_NAME_QUERY_RESPONSE,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GUILD_QUERY,                               STATUS_AUTHED,   PROCESS_THREADUNSAFE, guilds::GuildQuery);
    OPCODE(SMSG_GUILD_QUERY_RESPONSE,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_ITEM_QUERY_SINGLE,                         STATUS_LOGGEDIN, PROCESS_INPLACE,      items::ItemQuerySingle);
    OPCODE(CMSG_ITEM_QUERY_MULTIPLE,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_ITEM_QUERY_SINGLE_RESPONSE,                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ITEM_QUERY_MULTIPLE_RESPONSE,              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PAGE_TEXT_QUERY,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, queries::PageTextQuery);
    OPCODE(SMSG_PAGE_TEXT_QUERY_RESPONSE,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUEST_QUERY,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestQuery>);
    OPCODE(SMSG_QUEST_QUERY_RESPONSE,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GAMEOBJECT_QUERY,                          STATUS_LOGGEDIN, PROCESS_INPLACE,      queries::GameObjectQuery);
    OPCODE(SMSG_GAMEOBJECT_QUERY_RESPONSE,                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_CREATURE_QUERY,                            STATUS_LOGGEDIN, PROCESS_INPLACE,      queries::CreatureQuery);
    OPCODE(SMSG_CREATURE_QUERY_RESPONSE,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_WHO,                                       STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleWhoOpcode);
    OPCODE(SMSG_WHO,                                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_WHOIS,                                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleWhoisOpcode);
    OPCODE(SMSG_WHOIS,                                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_FRIEND_LIST,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleFriendListOpcode);
    OPCODE(SMSG_FRIEND_LIST,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_FRIEND_STATUS,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_ADD_FRIEND,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, social::AddFriend);
    OPCODE(CMSG_DEL_FRIEND,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<social::DelFriend>);
    OPCODE(SMSG_IGNORE_LIST,                               STATUS_NEVER,    PROCESS_THREADUNSAFE, protocol::_ServerSide);
    OPCODE(CMSG_ADD_IGNORE,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, social::AddIgnore);
    OPCODE(CMSG_DEL_IGNORE,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<social::DelIgnore>);
    OPCODE(CMSG_GROUP_INVITE,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::GroupInvite>);
    OPCODE(SMSG_GROUP_INVITE,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GROUP_CANCEL,                              STATUS_LOGGEDIN, PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_GROUP_CANCEL,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GROUP_ACCEPT,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::GroupAccept>);
    OPCODE(CMSG_GROUP_DECLINE,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::GroupDecline>);
    OPCODE(SMSG_GROUP_DECLINE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GROUP_UNINVITE,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::GroupUninvite>);
    OPCODE(CMSG_GROUP_UNINVITE_GUID,                       STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::GroupUninviteGuid>);
    OPCODE(SMSG_GROUP_UNINVITE,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GROUP_SET_LEADER,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::GroupSetLeader>);
    OPCODE(SMSG_GROUP_SET_LEADER,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_LOOT_METHOD,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::LootRules>);
    OPCODE(CMSG_GROUP_DISBAND,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::GroupDisband>);
    OPCODE(SMSG_GROUP_DESTROYED,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_GROUP_LIST,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PARTY_MEMBER_STATS,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PARTY_COMMAND_RESULT,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(UMSG_UPDATE_GROUP_MEMBERS,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GUILD_CREATE,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<guilds::GuildCreate>);
    OPCODE(CMSG_GUILD_INVITE,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildInvite);
    OPCODE(SMSG_GUILD_INVITE,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GUILD_ACCEPT,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<guilds::GuildAccept>);
    OPCODE(CMSG_GUILD_DECLINE,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<guilds::GuildDecline>);
    OPCODE(SMSG_GUILD_DECLINE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GUILD_INFO,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildInfo);
    OPCODE(SMSG_GUILD_INFO,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GUILD_ROSTER,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<guilds::GuildRoster>);
    OPCODE(SMSG_GUILD_ROSTER,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GUILD_PROMOTE,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildPromote);
    OPCODE(CMSG_GUILD_DEMOTE,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildDemote);
    OPCODE(CMSG_GUILD_LEAVE,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildLeave);
    OPCODE(CMSG_GUILD_REMOVE,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildRemove);
    OPCODE(CMSG_GUILD_DISBAND,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildDisband);
    OPCODE(CMSG_GUILD_LEADER,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildLeader);
    OPCODE(CMSG_GUILD_MOTD,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildMOTD);
    OPCODE(SMSG_GUILD_EVENT,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_GUILD_COMMAND_RESULT,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(UMSG_UPDATE_GUILD,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_MESSAGECHAT,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, chat::Messagechat);
    OPCODE(SMSG_MESSAGECHAT,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_JOIN_CHANNEL,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, channels::JoinChannel);
    OPCODE(CMSG_LEAVE_CHANNEL,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::LeaveChannel>);
    OPCODE(SMSG_CHANNEL_NOTIFY,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_CHANNEL_LIST,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelList>);
    OPCODE(SMSG_CHANNEL_LIST,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_CHANNEL_PASSWORD,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelPassword>);
    OPCODE(CMSG_CHANNEL_SET_OWNER,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelSetOwner>);
    OPCODE(CMSG_CHANNEL_OWNER,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelOwner>);
    OPCODE(CMSG_CHANNEL_MODERATOR,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelModerator>);
    OPCODE(CMSG_CHANNEL_UNMODERATOR,                       STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelUnmoderator>);
    OPCODE(CMSG_CHANNEL_MUTE,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelMute>);
    OPCODE(CMSG_CHANNEL_UNMUTE,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelUnmute>);
    OPCODE(CMSG_CHANNEL_INVITE,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelInvite>);
    OPCODE(CMSG_CHANNEL_KICK,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelKick>);
    OPCODE(CMSG_CHANNEL_BAN,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelBan>);
    OPCODE(CMSG_CHANNEL_UNBAN,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelUnban>);
    OPCODE(CMSG_CHANNEL_ANNOUNCEMENTS,                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelAnnouncements>);
    OPCODE(CMSG_CHANNEL_MODERATE,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelModerate>);
    OPCODE(SMSG_UPDATE_OBJECT,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DESTROY_OBJECT,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_USE_ITEM,                                  STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spells::UseItem>);
    OPCODE(CMSG_OPEN_ITEM,                                 STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spells::OpenItem>);
    OPCODE(CMSG_READ_ITEM,                                 STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::ReadItem>);
    OPCODE(SMSG_READ_ITEM_OK,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_READ_ITEM_FAILED,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ITEM_COOLDOWN,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GAMEOBJ_USE,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spells::GameObjectUse>);
    OPCODE(CMSG_GAMEOBJ_CHAIR_USE_OBSOLETE,                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_GAMEOBJECT_CUSTOM_ANIM,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_AREATRIGGER,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleAreaTriggerOpcode);
    OPCODE(MSG_MOVE_START_FORWARD,                         STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_START_BACKWARD,                        STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_STOP,                                  STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_START_STRAFE_LEFT,                     STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_START_STRAFE_RIGHT,                    STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_STOP_STRAFE,                           STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_JUMP,                                  STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_START_TURN_LEFT,                       STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_START_TURN_RIGHT,                      STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_STOP_TURN,                             STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_START_PITCH_UP,                        STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_START_PITCH_DOWN,                      STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_STOP_PITCH,                            STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_SET_RUN_MODE,                          STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_SET_WALK_MODE,                         STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_TOGGLE_LOGGING,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_TELEPORT,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_TELEPORT_CHEAT,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_TELEPORT_ACK,                          STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<movement::MoveTeleportAck>);
    OPCODE(MSG_MOVE_TOGGLE_FALL_LOGGING,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_FALL_LAND,                             STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_START_SWIM,                            STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_STOP_SWIM,                             STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_SET_RUN_SPEED_CHEAT,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_RUN_SPEED,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_RUN_BACK_SPEED_CHEAT,              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_RUN_BACK_SPEED,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_WALK_SPEED_CHEAT,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_WALK_SPEED,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_SWIM_SPEED_CHEAT,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_SWIM_SPEED,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_SWIM_BACK_SPEED_CHEAT,             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_SWIM_BACK_SPEED,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_ALL_SPEED_CHEAT,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_TURN_RATE_CHEAT,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_TURN_RATE,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_TOGGLE_COLLISION_CHEAT,                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_SET_FACING,                            STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_SET_PITCH,                             STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(MSG_MOVE_WORLDPORT_ACK,                         STATUS_TRANSFER, PROCESS_THREADUNSAFE, movement::MoveWorldportAck);
    OPCODE(SMSG_MONSTER_MOVE,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_MOVE_WATER_WALK,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_MOVE_LAND_WALK,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_MOVE_SET_RAW_POSITION_ACK,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_MOVE_SET_RAW_POSITION,                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMoveSetRawPosition);
    OPCODE(SMSG_FORCE_RUN_SPEED_CHANGE,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_FORCE_RUN_SPEED_CHANGE_ACK,                STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<movement::ForceSpeedChangeAckOpcodes>);
    OPCODE(SMSG_FORCE_RUN_BACK_SPEED_CHANGE,               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK,           STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<movement::ForceSpeedChangeAckOpcodes>);
    OPCODE(SMSG_FORCE_SWIM_SPEED_CHANGE,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_FORCE_SWIM_SPEED_CHANGE_ACK,               STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<movement::ForceSpeedChangeAckOpcodes>);
    OPCODE(SMSG_FORCE_MOVE_ROOT,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_FORCE_MOVE_ROOT_ACK,                       STATUS_LOGGEDIN, PROCESS_THREADSAFE,   &WorldSession::HandleMoveRootAck);
    OPCODE(SMSG_FORCE_MOVE_UNROOT,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_FORCE_MOVE_UNROOT_ACK,                     STATUS_LOGGEDIN, PROCESS_THREADSAFE,   &WorldSession::HandleMoveUnRootAck);
    OPCODE(MSG_MOVE_ROOT,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_UNROOT,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_HEARTBEAT,                             STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(SMSG_MOVE_KNOCK_BACK,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_MOVE_KNOCK_BACK_ACK,                       STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MoveKnockBackAck);
    OPCODE(MSG_MOVE_KNOCK_BACK,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_MOVE_FEATHER_FALL,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_MOVE_NORMAL_FALL,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_MOVE_SET_HOVER,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_MOVE_UNSET_HOVER,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_MOVE_HOVER_ACK,                            STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<movement::MoveHoverAck>);
    OPCODE(MSG_MOVE_HOVER,                                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_TRIGGER_CINEMATIC_CHEAT,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_OPENING_CINEMATIC,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_TRIGGER_CINEMATIC,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_NEXT_CINEMATIC_CAMERA,                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleNextCinematicCamera);
    OPCODE(CMSG_COMPLETE_CINEMATIC,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleCompleteCinematic);
    OPCODE(SMSG_TUTORIAL_FLAGS,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_TUTORIAL_FLAG,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, characters::TutorialFlag);
    OPCODE(CMSG_TUTORIAL_CLEAR,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTutorialClearOpcode);
    OPCODE(CMSG_TUTORIAL_RESET,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTutorialResetOpcode);
    OPCODE(CMSG_STANDSTATECHANGE,                          STATUS_LOGGEDIN, PROCESS_THREADSAFE,   &WorldSession::HandleStandStateChangeOpcode);
    OPCODE(CMSG_EMOTE,                                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<chat::Emote>);
    OPCODE(SMSG_EMOTE,                                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_TEXT_EMOTE,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, chat::TextEmote);
    OPCODE(SMSG_TEXT_EMOTE,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_AUTOEQUIP_GROUND_ITEM,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_AUTOSTORE_GROUND_ITEM,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_AUTOSTORE_LOOT_ITEM,                       STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spoils::AutostoreItem>);
    OPCODE(CMSG_STORE_LOOT_IN_SLOT,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_AUTOEQUIP_ITEM,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::AutoEquipItem>);
    OPCODE(CMSG_AUTOSTORE_BAG_ITEM,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::AutoStoreBagItem>);
    OPCODE(CMSG_SWAP_ITEM,                                 STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::SwapItem>);
    OPCODE(CMSG_SWAP_INV_ITEM,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::SwapInvItem>);
    OPCODE(CMSG_SPLIT_ITEM,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::SplitItem>);
    OPCODE(CMSG_AUTOEQUIP_ITEM_SLOT,                       STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::AutoEquipItemSlot>);
    OPCODE(OBSOLETE_DROP_ITEM,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_DESTROYITEM,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::DestroyItem>);
    OPCODE(SMSG_INVENTORY_CHANGE_FAILURE,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_OPEN_CONTAINER,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_INSPECT,                                   STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleInspectOpcode);
    OPCODE(SMSG_INSPECT,                                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_INITIATE_TRADE,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, trade::InitiateTrade);
    OPCODE(CMSG_BEGIN_TRADE,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<trade::BeginTrade>);
    OPCODE(CMSG_BUSY_TRADE,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<trade::BusyTrade>);
    OPCODE(CMSG_IGNORE_TRADE,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<trade::IgnoreTrade>);
    OPCODE(CMSG_ACCEPT_TRADE,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, trade::AcceptTrade);
    OPCODE(CMSG_UNACCEPT_TRADE,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<trade::UnacceptTrade>);
    OPCODE(CMSG_CANCEL_TRADE,                              STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT, PROCESS_THREADUNSAFE, trade::CancelTrade);
    OPCODE(CMSG_SET_TRADE_ITEM,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<trade::SetTradeItem>);
    OPCODE(CMSG_CLEAR_TRADE_ITEM,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<trade::ClearTradeItem>);
    OPCODE(CMSG_SET_TRADE_GOLD,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<trade::SetTradeGold>);
    OPCODE(SMSG_TRADE_STATUS,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_TRADE_STATUS_EXTENDED,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_INITIALIZE_FACTIONS,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SET_FACTION_VISIBLE,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SET_FACTION_STANDING,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SET_FACTION_ATWAR,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<characters::SetFactionAtWar>);
    OPCODE(CMSG_SET_FACTION_CHEAT,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_Deprecated);
    OPCODE(SMSG_SET_PROFICIENCY,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SET_ACTION_BUTTON,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSetActionButtonOpcode);
    OPCODE(SMSG_ACTION_BUTTONS,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_INITIAL_SPELLS,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LEARNED_SPELL,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SUPERCEDED_SPELL,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_NEW_SPELL_SLOT,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_CAST_SPELL,                                STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<spells::CastSpell>);
    OPCODE(CMSG_CANCEL_CAST,                               STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<spells::CancelCast>);
    OPCODE(SMSG_CAST_FAILED,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELL_START,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELL_GO,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELL_FAILURE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELL_COOLDOWN,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_COOLDOWN_EVENT,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_CANCEL_AURA,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spells::CancelAura>);
    OPCODE(SMSG_UPDATE_AURA_DURATION,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PET_CAST_FAILED,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_CHANNEL_START,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_CHANNEL_UPDATE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_CANCEL_CHANNELLING,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spells::CancelChanneling>);
    OPCODE(SMSG_AI_REACTION,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SET_SELECTION,                             STATUS_LOGGEDIN, PROCESS_INPLACE,      &WorldSession::HandleSetSelectionOpcode);
    OPCODE(CMSG_SET_TARGET_OBSOLETE,                       STATUS_LOGGEDIN, PROCESS_INPLACE,      &WorldSession::HandleSetTargetOpcode);
    OPCODE(CMSG_UNUSED,                                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_UNUSED2,                                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_ATTACKSWING,                               STATUS_LOGGEDIN, PROCESS_INPLACE,      PlayerAnswers<combat::AttackSwing>);
    OPCODE(CMSG_ATTACKSTOP,                                STATUS_LOGGEDIN, PROCESS_INPLACE,      PlayerAnswers<combat::AttackStop>);
    OPCODE(SMSG_ATTACKSTART,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ATTACKSTOP,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ATTACKSWING_NOTINRANGE,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ATTACKSWING_BADFACING,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ATTACKSWING_NOTSTANDING,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ATTACKSWING_DEADTARGET,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ATTACKSWING_CANT_ATTACK,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ATTACKERSTATEUPDATE,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_VICTIMSTATEUPDATE_OBSOLETE,                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DAMAGE_DONE_OBSOLETE,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DAMAGE_TAKEN_OBSOLETE,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_CANCEL_COMBAT,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PLAYER_COMBAT_XP_GAIN_OBSOLETE,            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELLHEALLOG,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELLENERGIZELOG,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SHEATHE_OBSOLETE,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SAVE_PLAYER,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SETDEATHBINDPOINT,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_BINDPOINTUPDATE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GETDEATHBINDZONE,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_BINDZONEREPLY,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PLAYERBOUND,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_CLIENT_CONTROL_UPDATE,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_REPOP_REQUEST,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRepopRequestOpcode);
    OPCODE(SMSG_RESURRECT_REQUEST,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_RESURRECT_RESPONSE,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleResurrectResponseOpcode);
    OPCODE(CMSG_LOOT,                                      STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spoils::Open>);
    OPCODE(CMSG_LOOT_MONEY,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spoils::Money>);
    OPCODE(CMSG_LOOT_RELEASE,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, spoils::Release);
    OPCODE(SMSG_LOOT_RESPONSE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LOOT_RELEASE_RESPONSE,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LOOT_REMOVED,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LOOT_MONEY_NOTIFY,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LOOT_ITEM_NOTIFY,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LOOT_CLEAR_MONEY,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ITEM_PUSH_RESULT,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DUEL_REQUESTED,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DUEL_OUTOFBOUNDS,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DUEL_INBOUNDS,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DUEL_COMPLETE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DUEL_WINNER,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_DUEL_ACCEPTED,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<duels::DuelAccepted>);
    OPCODE(CMSG_DUEL_CANCELLED,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<duels::DuelCancelled>);
    OPCODE(SMSG_MOUNTRESULT,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DISMOUNTRESULT,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PUREMOUNT_CANCELLED_OBSOLETE,              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_MOUNTSPECIAL_ANIM,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<movement::MountSpecialAnim>);
    OPCODE(SMSG_MOUNTSPECIAL_ANIM,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PET_TAME_FAILURE,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PET_SET_ACTION,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<pets::PetSetAction>);
    OPCODE(CMSG_PET_ACTION,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<pets::PetAction>);
    OPCODE(CMSG_PET_ABANDON,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<pets::PetAbandon>);
    OPCODE(CMSG_PET_RENAME,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<pets::PetRename>);
    OPCODE(SMSG_PET_NAME_INVALID,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PET_SPELLS,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PET_MODE,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GOSSIP_HELLO,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<npcs::GossipHello>);
    OPCODE(CMSG_GOSSIP_SELECT_OPTION,                      STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<npcs::GossipSelectOption>);
    OPCODE(SMSG_GOSSIP_MESSAGE,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_GOSSIP_COMPLETE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_NPC_TEXT_QUERY,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, queries::NpcTextQuery);
    OPCODE(SMSG_NPC_TEXT_UPDATE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_NPC_WONT_TALK,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUESTGIVER_STATUS_QUERY,                   STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestgiverStatusQuery>);
    OPCODE(SMSG_QUESTGIVER_STATUS,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUESTGIVER_HELLO,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestgiverHello>);
    OPCODE(SMSG_QUESTGIVER_QUEST_LIST,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUESTGIVER_QUERY_QUEST,                    STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestgiverQueryQuest>);
    OPCODE(CMSG_QUESTGIVER_QUEST_AUTOLAUNCH,               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestgiverQuestAutoLaunch>);
    OPCODE(SMSG_QUESTGIVER_QUEST_DETAILS,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUESTGIVER_ACCEPT_QUEST,                   STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestgiverAcceptQuest>);
    OPCODE(CMSG_QUESTGIVER_COMPLETE_QUEST,                 STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestgiverCompleteQuest>);
    OPCODE(SMSG_QUESTGIVER_REQUEST_ITEMS,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUESTGIVER_REQUEST_REWARD,                 STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestgiverRequestReward>);
    OPCODE(SMSG_QUESTGIVER_OFFER_REWARD,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUESTGIVER_CHOOSE_REWARD,                  STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestgiverChooseReward>);
    OPCODE(SMSG_QUESTGIVER_QUEST_INVALID,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUESTGIVER_CANCEL,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestgiverCancel>);
    OPCODE(SMSG_QUESTGIVER_QUEST_COMPLETE,                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_QUESTGIVER_QUEST_FAILED,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUESTLOG_SWAP_QUEST,                       STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestLogSwapQuest>);
    OPCODE(CMSG_QUESTLOG_REMOVE_QUEST,                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestLogRemoveQuest>);
    OPCODE(SMSG_QUESTLOG_FULL,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_QUESTUPDATE_FAILED,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_QUESTUPDATE_FAILEDTIMER,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_QUESTUPDATE_COMPLETE,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_QUESTUPDATE_ADD_KILL,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_QUESTUPDATE_ADD_ITEM,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUEST_CONFIRM_ACCEPT,                      STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestConfirmAccept>);
    OPCODE(SMSG_QUEST_CONFIRM_ACCEPT,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PUSHQUESTTOPARTY,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::PushQuestToParty>);
    OPCODE(CMSG_LIST_INVENTORY,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::ListInventory>);
    OPCODE(SMSG_LIST_INVENTORY,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SELL_ITEM,                                 STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::SellItem>);
    OPCODE(SMSG_SELL_ITEM,                                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_BUY_ITEM,                                  STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::BuyItem>);
    OPCODE(CMSG_BUY_ITEM_IN_SLOT,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::BuyItemInSlot>);
    OPCODE(SMSG_BUY_ITEM,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_BUY_FAILED,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_TAXICLEARALLNODES,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_TAXIENABLEALLNODES,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_TAXISHOWNODES,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_SHOWTAXINODES,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_TAXINODE_STATUS_QUERY,                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<taxi::TaxiNodeStatusQuery>);
    OPCODE(SMSG_TAXINODE_STATUS,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_TAXIQUERYAVAILABLENODES,                   STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<taxi::TaxiQueryAvailableNodes>);
    OPCODE(CMSG_ACTIVATETAXI,                              STATUS_LOGGEDIN, PROCESS_INPLACE,      PlayerAnswers<taxi::ActivateTaxi>);
    OPCODE(SMSG_ACTIVATETAXIREPLY,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_NEW_TAXI_PATH,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_TRAINER_LIST,                              STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<npcs::TrainerList>);
    OPCODE(SMSG_TRAINER_LIST,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_TRAINER_BUY_SPELL,                         STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<npcs::TrainerBuySpell>);
    OPCODE(SMSG_TRAINER_BUY_SUCCEEDED,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_TRAINER_BUY_FAILED,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_BINDER_ACTIVATE,                           STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<npcs::BinderActivate>);
    OPCODE(SMSG_PLAYERBINDERROR,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_BANKER_ACTIVATE,                           STATUS_LOGGEDIN, PROCESS_THREADSAFE,   npcs::BankerActivate);
    OPCODE(SMSG_SHOW_BANK,                                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_BUY_BANK_SLOT,                             STATUS_LOGGEDIN, PROCESS_THREADSAFE,   items::BuyBankSlot);
    OPCODE(SMSG_BUY_BANK_SLOT_RESULT,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PETITION_SHOWLIST,                         STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<petitions::PetitionShowList>);
    OPCODE(SMSG_PETITION_SHOWLIST,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PETITION_BUY,                              STATUS_LOGGEDIN, PROCESS_THREADSAFE,   petitions::PetitionBuy);
    OPCODE(CMSG_PETITION_SHOW_SIGNATURES,                  STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<petitions::PetitionShowSign>);
    OPCODE(SMSG_PETITION_SHOW_SIGNATURES,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PETITION_SIGN,                             STATUS_LOGGEDIN, PROCESS_THREADSAFE,   petitions::PetitionSign);
    OPCODE(SMSG_PETITION_SIGN_RESULTS,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_PETITION_DECLINE,                           STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<petitions::PetitionDecline>);
    OPCODE(CMSG_OFFER_PETITION,                            STATUS_LOGGEDIN, PROCESS_THREADSAFE,   petitions::OfferPetition);
    OPCODE(CMSG_TURN_IN_PETITION,                          STATUS_LOGGEDIN, PROCESS_THREADSAFE,   petitions::TurnInPetition);
    OPCODE(SMSG_TURN_IN_PETITION_RESULTS,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PETITION_QUERY,                            STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<petitions::PetitionQuery>);
    OPCODE(SMSG_PETITION_QUERY_RESPONSE,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_FISH_NOT_HOOKED,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_FISH_ESCAPED,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_BUG,                                       STATUS_LOGGEDIN, PROCESS_THREADSAFE,   &WorldSession::HandleBugOpcode);
    OPCODE(SMSG_NOTIFICATION,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PLAYED_TIME,                               STATUS_LOGGEDIN, PROCESS_THREADSAFE,   &WorldSession::HandlePlayedTime);
    OPCODE(SMSG_PLAYED_TIME,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_QUERY_TIME,                                STATUS_LOGGEDIN, PROCESS_THREADSAFE,   queries::QueryTime);
    OPCODE(SMSG_QUERY_TIME_RESPONSE,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LOG_XPGAIN,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_AURACASTLOG,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_RECLAIM_CORPSE,                            STATUS_LOGGEDIN, PROCESS_THREADSAFE,   &WorldSession::HandleReclaimCorpseOpcode);
    OPCODE(CMSG_WRAP_ITEM,                                 STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<items::WrapItem>);
    OPCODE(SMSG_LEVELUP_INFO,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_MINIMAP_PING,                               STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<groups::MinimapPing>);
    OPCODE(SMSG_RESISTLOG,                                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ENCHANTMENTLOG,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SET_SKILL_CHEAT,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_START_MIRROR_TIMER,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PAUSE_MIRROR_TIMER,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_STOP_MIRROR_TIMER,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PING,                                      STATUS_AUTHED,   PROCESS_THREADUNSAFE, protocol::Ping);
    OPCODE(SMSG_PONG,                                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_CLEAR_COOLDOWN,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_GAMEOBJECT_PAGETEXT,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SETSHEATHED,                               STATUS_LOGGEDIN, PROCESS_INPLACE,      PlayerAnswers<combat::SetSheathed>);
    OPCODE(SMSG_COOLDOWN_CHEAT,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELL_DELAYED,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PLAYER_MACRO_OBSOLETE,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_PLAYER_MACRO_OBSOLETE,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GHOST,                                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_INVIS,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_INVALID_PROMOTION_CODE,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_GM_BIND_OTHER,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_GM_SUMMON,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_ITEM_TIME_UPDATE,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ITEM_ENCHANT_TIME_UPDATE,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_AUTH_CHALLENGE,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_AUTH_SESSION,                              STATUS_NEVER,    PROCESS_THREADSAFE,   protocol::_EarlyProccess);
    OPCODE(SMSG_AUTH_RESPONSE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_GM_SHOWLABEL,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_PET_CAST_SPELL,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<pets::PetCastSpell>);
    OPCODE(MSG_SAVE_GUILD_EMBLEM,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<guilds::SaveGuildEmblem>);
    OPCODE(MSG_TABARDVENDOR_ACTIVATE,                      STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<npcs::TabardVendorActivate>);
    OPCODE(SMSG_PLAY_SPELL_VISUAL,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_ZONEUPDATE,                                STATUS_LOGGEDIN, PROCESS_THREADSAFE,   &WorldSession::HandleZoneUpdateOpcode);
    OPCODE(SMSG_PARTYKILLLOG,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_COMPRESSED_UPDATE_OBJECT,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PLAY_SPELL_IMPACT,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_EXPLORATION_EXPERIENCE,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GM_SET_SECURITY_GROUP,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_NUKE,                                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_RANDOM_ROLL,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::RandomRoll>);
    OPCODE(SMSG_ENVIRONMENTALDAMAGELOG,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_RWHOIS_OBSOLETE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_RWHOIS,                                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_UNLEARN_SPELL,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_UNLEARN_SKILL,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<skills::UnlearnSkill>);
    OPCODE(SMSG_REMOVED_SPELL,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_DECHARGE,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GMTICKET_CREATE,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, tickets::GMTicketCreate);
    OPCODE(SMSG_GMTICKET_CREATE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GMTICKET_UPDATETEXT,                       STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<tickets::GMTicketUpdateText>);
    OPCODE(SMSG_GMTICKET_UPDATETEXT,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_ACCOUNT_DATA_TIMES,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_REQUEST_ACCOUNT_DATA,                      STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestAccountData);
    OPCODE(CMSG_UPDATE_ACCOUNT_DATA,                       STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT, PROCESS_THREADUNSAFE, &WorldSession::HandleUpdateAccountData);
    OPCODE(SMSG_UPDATE_ACCOUNT_DATA,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_CLEAR_FAR_SIGHT_IMMEDIATE,                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_POWERGAINLOG_OBSOLETE,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GM_TEACH,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_CREATE_ITEM_TARGET,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GMTICKET_GETTICKET,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, tickets::GMTicketGetTicket);
    OPCODE(SMSG_GMTICKET_GETTICKET,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_UNLEARN_TALENTS,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE,            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_GAMEOBJECT_DESPAWN_ANIM,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_CORPSE_QUERY,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<queries::CorpseQuery>);
    OPCODE(CMSG_GMTICKET_DELETETICKET,                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<tickets::GMTicketDeleteTicket>);
    OPCODE(SMSG_GMTICKET_DELETETICKET,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_CHAT_WRONG_FACTION,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GMTICKET_SYSTEMSTATUS,                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<tickets::GMTicketSystemStatus>);
    OPCODE(SMSG_GMTICKET_SYSTEMSTATUS,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SPIRIT_HEALER_ACTIVATE,                    STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<npcs::SpiritHealerActivate>);
    OPCODE(CMSG_SET_STAT_CHEAT,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_SET_REST_START,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SKILL_BUY_STEP,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SKILL_BUY_RANK,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_XP_CHEAT,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_SPIRIT_HEALER_CONFIRM,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_CHARACTER_POINT_CHEAT,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_GOSSIP_POI,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_CHAT_IGNORED,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<chat::ChatIgnored>);
    OPCODE(CMSG_GM_VISION,                                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SERVER_COMMAND,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_SILENCE,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_REVEALTO,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_RESURRECT,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_SUMMONMOB,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_MOVECORPSE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_FREEZE,                                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_UBERINVIS,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_REQUEST_PLAYER_INFO,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_GM_PLAYER_INFO,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GUILD_RANK,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildRank);
    OPCODE(CMSG_GUILD_ADD_RANK,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildAddRank);
    OPCODE(CMSG_GUILD_DEL_RANK,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildDelRank);
    OPCODE(CMSG_GUILD_SET_PUBLIC_NOTE,                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildSetPublicNote);
    OPCODE(CMSG_GUILD_SET_OFFICER_NOTE,                    STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildSetOfficerNote);
    OPCODE(SMSG_LOGIN_VERIFY_WORLD,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_CLEAR_EXPLORATION,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SEND_MAIL,                                 STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, mail::SendMail);
    OPCODE(SMSG_SEND_MAIL_RESULT,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GET_MAIL_LIST,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<mail::GetMailList>);
    OPCODE(SMSG_MAIL_LIST_RESULT,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_BATTLEFIELD_LIST,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<battlegrounds::BattlefieldList>);
    OPCODE(SMSG_BATTLEFIELD_LIST,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_BATTLEFIELD_JOIN,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_BATTLEFIELD_WIN_OBSOLETE,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_BATTLEFIELD_LOSE_OBSOLETE,                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_TAXICLEARNODE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_TAXIENABLENODE,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_ITEM_TEXT_QUERY,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<mail::ItemTextQuery>);
    OPCODE(SMSG_ITEM_TEXT_QUERY_RESPONSE,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_MAIL_TAKE_MONEY,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<mail::MailTakeMoney>);
    OPCODE(CMSG_MAIL_TAKE_ITEM,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, mail::MailTakeItem);
    OPCODE(CMSG_MAIL_MARK_AS_READ,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<mail::MailMarkAsRead>);
    OPCODE(CMSG_MAIL_RETURN_TO_SENDER,                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, mail::MailReturnToSender);
    OPCODE(CMSG_MAIL_DELETE,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<mail::MailDelete>);
    OPCODE(CMSG_MAIL_CREATE_TEXT_ITEM,                     STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<mail::MailCreateTextItem>);
    OPCODE(SMSG_SPELLLOGMISS,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELLLOGEXECUTE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DEBUGAURAPROC,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PERIODICAURALOG,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELLDAMAGESHIELD,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELLNONMELEEDAMAGELOG,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_LEARN_TALENT,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<skills::LearnTalent>);
    OPCODE(SMSG_RESURRECT_FAILED,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_TOGGLE_PVP,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTogglePvP);
    OPCODE(SMSG_ZONE_UNDER_ATTACK,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_AUCTION_HELLO,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<auctions::AuctionHello>);
    OPCODE(CMSG_AUCTION_SELL_ITEM,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, auctions::AuctionSellItem);
    OPCODE(CMSG_AUCTION_REMOVE_ITEM,                       STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<auctions::AuctionRemoveItem>);
    OPCODE(CMSG_AUCTION_LIST_ITEMS,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<auctions::AuctionListItems>);
    OPCODE(CMSG_AUCTION_LIST_OWNER_ITEMS,                  STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<auctions::AuctionListOwnerItems>);
    OPCODE(CMSG_AUCTION_PLACE_BID,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<auctions::AuctionPlaceBid>);
    OPCODE(SMSG_AUCTION_COMMAND_RESULT,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_AUCTION_LIST_RESULT,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_AUCTION_OWNER_LIST_RESULT,                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_AUCTION_BIDDER_NOTIFICATION,               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_AUCTION_OWNER_NOTIFICATION,                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PROCRESIST,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_STANDSTATE_CHANGE_FAILURE_OBSOLETE,        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DISPEL_FAILED,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELLORDAMAGE_IMMUNE,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_AUCTION_LIST_BIDDER_ITEMS,                 STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<auctions::AuctionListBidderItems>);
    OPCODE(SMSG_AUCTION_BIDDER_LIST_RESULT,                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SET_FLAT_SPELL_MODIFIER,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SET_PCT_SPELL_MODIFIER,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SET_AMMO,                                  STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::SetAmmo>);
    OPCODE(SMSG_CORPSE_RECLAIM_DELAY,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SET_ACTIVE_MOVER,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<movement::SetActiveMover>);
    OPCODE(CMSG_PET_CANCEL_AURA,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spells::PetCancelAura>);
    OPCODE(CMSG_PLAYER_AI_CHEAT,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_CANCEL_AUTO_REPEAT_SPELL,                  STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spells::CancelAutoRepeatSpell>);
    OPCODE(MSG_GM_ACCOUNT_ONLINE,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_LIST_STABLED_PETS,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<npcs::ListStabledPets>);
    OPCODE(CMSG_STABLE_PET,                                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<npcs::StablePet>);
    OPCODE(CMSG_UNSTABLE_PET,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<npcs::UnstablePet>);
    OPCODE(CMSG_BUY_STABLE_SLOT,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<npcs::BuyStableSlot>);
    OPCODE(SMSG_STABLE_RESULT,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_STABLE_REVIVE_PET,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<npcs::StableRevivePet>);
    OPCODE(CMSG_STABLE_SWAP_PET,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<npcs::StableSwapPet>);
    OPCODE(MSG_QUEST_PUSH_RESULT,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestPushResult>);
    OPCODE(SMSG_PLAY_MUSIC,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PLAY_OBJECT_SOUND,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_REQUEST_PET_INFO,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestPetInfoOpcode);
    OPCODE(CMSG_FAR_SIGHT,                                 STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleFarSightOpcode);
    OPCODE(SMSG_SPELLDISPELLOG,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DAMAGE_CALC_LOG,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_ENABLE_DAMAGE_LOG,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GROUP_CHANGE_SUB_GROUP,                    STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::GroupChangeSubGroup>);
    OPCODE(CMSG_REQUEST_PARTY_MEMBER_STATS,                STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::RequestPartyMemberStats>);
    OPCODE(CMSG_GROUP_SWAP_SUB_GROUP,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_RESET_FACTION_CHEAT,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_AUTOSTORE_BANK_ITEM,                       STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::AutoStoreBankItem>);
    OPCODE(CMSG_AUTOBANK_ITEM,                             STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::AutoBankItem>);
    OPCODE(MSG_QUERY_NEXT_MAIL_TIME,                       STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<mail::QueryNextMailTime>);
    OPCODE(SMSG_RECEIVED_MAIL,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_RAID_GROUP_ONLY,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SET_DURABILITY_CHEAT,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SET_PVP_RANK_CHEAT,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_ADD_PVP_MEDAL_CHEAT,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_DEL_PVP_MEDAL_CHEAT,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SET_PVP_TITLE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_PVP_CREDIT,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_AUCTION_REMOVED_NOTIFICATION,              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GROUP_RAID_CONVERT,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::GroupRaidConvert>);
    OPCODE(CMSG_GROUP_ASSISTANT_LEADER,                    STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::GroupAssistantLeader>);
    OPCODE(CMSG_BUYBACK_ITEM,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::BuybackItem>);
    OPCODE(SMSG_SERVER_MESSAGE,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_MEETINGSTONE_JOIN,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<meetingstones::MeetingStoneJoin>);
    OPCODE(CMSG_MEETINGSTONE_LEAVE,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<meetingstones::MeetingStoneLeave>);
    OPCODE(CMSG_MEETINGSTONE_CHEAT,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_MEETINGSTONE_SETQUEUE,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_MEETINGSTONE_INFO,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMeetingStoneInfoOpcode);
    OPCODE(SMSG_MEETINGSTONE_COMPLETE,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_MEETINGSTONE_IN_PROGRESS,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_MEETINGSTONE_MEMBER_ADDED,                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GMTICKETSYSTEM_TOGGLE,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_CANCEL_GROWTH_AURA,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spells::CancelGrowthAura>);
    OPCODE(SMSG_CANCEL_AUTO_REPEAT,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_STANDSTATE_UPDATE,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LOOT_ALL_PASSED,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LOOT_ROLL_WON,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_LOOT_ROLL,                                 STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::LootRoll>);
    OPCODE(SMSG_LOOT_START_ROLL,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_LOOT_ROLL,                                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_LOOT_MASTER_GIVE,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spoils::MasterGive>);
    OPCODE(SMSG_LOOT_MASTER_LIST,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SET_FORCED_REACTIONS,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELL_FAILED_OTHER,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_GAMEOBJECT_RESET_STATE,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_REPAIR_ITEM,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<npcs::RepairItem>);
    OPCODE(SMSG_CHAT_PLAYER_NOT_FOUND,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_TALENT_WIPE_CONFIRM,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<skills::TalentWipeConfirm>);
    OPCODE(SMSG_SUMMON_REQUEST,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SUMMON_RESPONSE,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<movement::SummonResponse>);
    OPCODE(MSG_MOVE_TOGGLE_GRAVITY_CHEAT,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_MONSTER_MOVE_TRANSPORT,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PET_BROKEN,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_MOVE_FEATHER_FALL,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_WATER_WALK,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SERVER_BROADCAST,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SELF_RES,                                  STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spells::SelfRes>);
    OPCODE(SMSG_FEIGN_DEATH_RESISTED,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_RUN_SCRIPT,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_SCRIPT_MESSAGE,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DUEL_COUNTDOWN,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_AREA_TRIGGER_MESSAGE,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_TOGGLE_HELM,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleShowingHelmOpcode);
    OPCODE(CMSG_TOGGLE_CLOAK,                              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleShowingCloakOpcode);
    OPCODE(SMSG_MEETINGSTONE_JOINFAILED,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PLAYER_SKINNED,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DURABILITY_DAMAGE_DEATH,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SET_EXPLORATION,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SET_ACTIONBAR_TOGGLES,                     STATUS_AUTHED,   PROCESS_THREADUNSAFE, &WorldSession::HandleSetActionBarTogglesOpcode);
    OPCODE(UMSG_DELETE_GUILD_CHARTER,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_PETITION_RENAME,                            STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, petitions::PetitionRename);
    OPCODE(SMSG_INIT_WORLD_STATES,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_UPDATE_WORLD_STATE,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_ITEM_NAME_QUERY,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, items::ItemNameQuery);
    OPCODE(SMSG_ITEM_NAME_QUERY_RESPONSE,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PET_ACTION_FEEDBACK,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_CHAR_RENAME,                               STATUS_AUTHED,   PROCESS_THREADUNSAFE, characters::CharRename);
    OPCODE(SMSG_CHAR_RENAME,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_MOVE_SPLINE_DONE,                          STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<taxi::MoveSplineDone>);
    OPCODE(CMSG_MOVE_FALL_RESET,                           STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);
    OPCODE(SMSG_INSTANCE_SAVE_CREATED,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_RAID_INSTANCE_INFO,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_REQUEST_RAID_INFO,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::RequestRaidInfo>);
    OPCODE(CMSG_MOVE_TIME_SKIPPED,                         STATUS_LOGGEDIN, PROCESS_INPLACE,      PlayerAnswers<movement::MoveTimeSkipped>);
    OPCODE(CMSG_MOVE_FEATHER_FALL_ACK,                     STATUS_LOGGEDIN, PROCESS_THREADSAFE,   &WorldSession::HandleFeatherFallAck);
    OPCODE(CMSG_MOVE_WATER_WALK_ACK,                       STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<movement::MoveWaterWalkAck>);
    OPCODE(CMSG_MOVE_NOT_ACTIVE_MOVER,                     STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<movement::MoveNotActiveMover>);
    OPCODE(SMSG_PLAY_SOUND,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_BATTLEFIELD_STATUS,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleBattlefieldStatusOpcode);
    OPCODE(SMSG_BATTLEFIELD_STATUS,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_BATTLEFIELD_PORT,                          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<battlegrounds::BattleFieldPort>);
    OPCODE(MSG_INSPECT_HONOR_STATS,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleInspectHonorStatsOpcode);
    OPCODE(CMSG_BATTLEMASTER_HELLO,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, battlegrounds::BattlemasterHello);
    OPCODE(CMSG_MOVE_START_SWIM_CHEAT,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_MOVE_STOP_SWIM_CHEAT,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_FORCE_WALK_SPEED_CHANGE,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_FORCE_WALK_SPEED_CHANGE_ACK,               STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<movement::ForceSpeedChangeAckOpcodes>);
    OPCODE(SMSG_FORCE_SWIM_BACK_SPEED_CHANGE,              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK,          STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<movement::ForceSpeedChangeAckOpcodes>);
    OPCODE(SMSG_FORCE_TURN_RATE_CHANGE,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_FORCE_TURN_RATE_CHANGE_ACK,                STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<movement::ForceSpeedChangeAckOpcodes>);
    OPCODE(MSG_PVP_LOG_DATA,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandlePVPLogDataOpcode);
    OPCODE(CMSG_LEAVE_BATTLEFIELD,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<battlegrounds::LeaveBattlefield>);
    OPCODE(CMSG_AREA_SPIRIT_HEALER_QUERY,                  STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<battlegrounds::AreaSpiritHealerQuery>);
    OPCODE(CMSG_AREA_SPIRIT_HEALER_QUEUE,                  STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<battlegrounds::AreaSpiritHealerQueue>);
    OPCODE(SMSG_AREA_SPIRIT_HEALER_TIME,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GM_UNTEACH,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_WARDEN_DATA,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_WARDEN_DATA,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_GROUP_JOINED_BATTLEGROUND,                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_BATTLEGROUND_PLAYER_POSITIONS,              STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleBattleGroundPlayerPositionsOpcode);
    OPCODE(CMSG_PET_STOP_ATTACK,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<pets::PetStopAttack>);
    OPCODE(SMSG_BINDER_CONFIRM,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_BATTLEGROUND_PLAYER_JOINED,                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_BATTLEGROUND_PLAYER_LEFT,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_BATTLEMASTER_JOIN,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<battlegrounds::BattlemasterJoin>);
    OPCODE(SMSG_ADDON_INFO,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PET_UNLEARN,                               STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<pets::PetUnlearn>);
    OPCODE(SMSG_PET_UNLEARN_CONFIRM,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PARTY_MEMBER_STATS_FULL,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_PET_SPELL_AUTOCAST,                        STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<pets::PetSpellAutocast>);
    OPCODE(SMSG_WEATHER,                                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PLAY_TIME_WARNING,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_MINIGAME_SETUP,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_MINIGAME_STATE,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_MINIGAME_MOVE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_MINIGAME_MOVE_FAILED,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_RAID_INSTANCE_MESSAGE,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_COMPRESSED_MOVES,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GUILD_INFO_TEXT,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, guilds::GuildChangeInfoText);
    OPCODE(SMSG_CHAT_RESTRICTED,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_SET_RUN_SPEED,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_SET_RUN_BACK_SPEED,                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_SET_SWIM_SPEED,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_SET_WALK_SPEED,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_SET_SWIM_BACK_SPEED,                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_SET_TURN_RATE,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_MOVE_UNROOT,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_MOVE_FEATHER_FALL,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_MOVE_NORMAL_FALL,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_MOVE_SET_HOVER,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_MOVE_UNSET_HOVER,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_MOVE_WATER_WALK,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_MOVE_LAND_WALK,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_MOVE_START_SWIM,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_MOVE_STOP_SWIM,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_MOVE_SET_RUN_MODE,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPLINE_MOVE_SET_WALK_MODE,                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GM_NUKE_ACCOUNT,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_GM_DESTROY_CORPSE,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_GM_DESTROY_ONLINE_CORPSE,                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_ACTIVATETAXIEXPRESS,                       STATUS_LOGGEDIN, PROCESS_THREADSAFE,   PlayerAnswers<taxi::ActivateTaxiExpress>);
    OPCODE(SMSG_SET_FACTION_ATWAR,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_GAMETIMEBIAS_SET,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_DEBUG_ACTIONS_START,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_DEBUG_ACTIONS_STOP,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_SET_FACTION_INACTIVE,                      STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<characters::SetFactionInactive>);
    OPCODE(CMSG_SET_WATCHED_FACTION,                       STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<characters::SetWatchedFaction>);
    OPCODE(MSG_MOVE_TIME_SKIPPED,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_SPLINE_MOVE_ROOT,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_SET_EXPLORATION_ALL,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_INVALIDATE_PLAYER,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_RESET_INSTANCES,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<instances::ResetInstances>);
    OPCODE(SMSG_INSTANCE_RESET,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_INSTANCE_RESET_FAILED,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_UPDATE_LAST_INSTANCE,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_RAID_TARGET_UPDATE,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::RaidTargetUpdate>);
    OPCODE(MSG_RAID_READY_CHECK,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::RaidReadyCheck>);
    OPCODE(CMSG_LUA_USAGE,                                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_PET_ACTION_SOUND,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_PET_DISMISS_SOUND,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_GHOSTEE_GONE,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GM_UPDATE_TICKET_STATUS,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_GM_TICKET_STATUS_UPDATE,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_GMSURVEY_SUBMIT,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<tickets::GMTicketSurveySubmit>);
    OPCODE(SMSG_UPDATE_INSTANCE_OWNERSHIP,                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_IGNORE_KNOCKBACK_CHEAT,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_CHAT_PLAYER_AMBIGUOUS,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_DELAY_GHOST_TELEPORT,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_SPELLINSTAKILLLOG,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELL_UPDATE_CHAIN_TARGETS,                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_CHAT_FILTERED,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_EXPECTED_SPAM_RECORDS,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_SPELLSTEALLOG,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_LOTTERY_QUERY_OBSOLETE,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_LOTTERY_QUERY_RESULT_OBSOLETE,             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_BUY_LOTTERY_TICKET_OBSOLETE,               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_LOTTERY_RESULT_OBSOLETE,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_CHARACTER_PROFILE,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_CHARACTER_PROFILE_REALM_CONNECTED,         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_DEFENSE_MESSAGE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(MSG_GM_RESETINSTANCELIMIT,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(SMSG_MOVE_SET_FLIGHT,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(SMSG_MOVE_UNSET_FLIGHT,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);
    OPCODE(CMSG_MOVE_FLIGHT_ACK,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_START_SWIM_CHEAT,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(MSG_MOVE_STOP_SWIM_CHEAT,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);
    OPCODE(CMSG_CANCEL_MOUNT_AURA,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleCancelMountAuraOpcode);     /// 0x375: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_CANCEL_TEMP_ENCHANTMENT,                   STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<items::CancelTempEnchantment>);       /// 0x379: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_SET_TAXI_BENCHMARK_MODE,                   STATUS_AUTHED,   PROCESS_THREADUNSAFE, &WorldSession::HandleSetTaxiBenchmarkOpcode);        /// 0x389: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_MOVE_CHNG_TRANSPORT,                       STATUS_LOGGEDIN, PROCESS_THREADSAFE,   movement::MovementOpcodes);       /// 0x38D: @TODO need to check usage in vanilla WoW
    OPCODE(MSG_PARTY_ASSIGNMENT,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::PartyAssignment>);     /// 0x38E: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_OFFER_PETITION_ERROR,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x38F: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_RESET_FAILED_NOTIFY,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x396: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_REAL_GROUP_UPDATE,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x397: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_INIT_EXTRA_AURA_INFO,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3A3: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_SET_EXTRA_AURA_INFO,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3A4: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_SET_EXTRA_AURA_INFO_NEED_UPDATE,           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3A5: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_SPELL_CHANCE_PROC_LOG,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3AA: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_MOVE_SET_RUN_SPEED,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3AB: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_DISMOUNT,                                  STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3AC: @TODO need to check usage in vanilla WoW
    OPCODE(MSG_RAID_READY_CHECK_CONFIRM,                   STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3AE: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_CLEAR_TARGET,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3BE: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_BOT_DETECTED,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3BF: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_KICK_REASON,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3C4: @TODO need to check usage in vanilla WoW
    OPCODE(MSG_RAID_READY_CHECK_FINISHED,                  STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<groups::RaidReadyCheckFinished>);      /// 0x3C5: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_TARGET_CAST,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3CF: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_TARGET_SCRIPT_CAST,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3D0: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_CHANNEL_DISPLAY_LIST,                      STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::ChannelDisplayListQuery>);     /// 0x3D1: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_GET_CHANNEL_MEMBER_COUNT,                  STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::GetChannelMemberCount>);       /// 0x3D3: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_CHANNEL_MEMBER_COUNT,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3D4: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_DEBUG_LIST_TARGETS,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3D7: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_DEBUG_LIST_TARGETS,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3D8: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_PARTY_SILENCE,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3DC: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_PARTY_UNSILENCE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3DD: @TODO need to check usage in vanilla WoW
    OPCODE(MSG_NOTIFY_PARTY_SQUELCH,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3DE: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_COMSAT_RECONNECT_TRY,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3DF: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_COMSAT_DISCONNECT,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3E0: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_COMSAT_CONNECT_FAIL,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3E1: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_SET_CHANNEL_WATCH,                         STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<channels::SetChannelWatch>);     /// 0x3EE: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_USERLIST_ADD,                              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3EF: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_USERLIST_REMOVE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3F0: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_USERLIST_UPDATE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3F1: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_CLEAR_CHANNEL_WATCH,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3F2: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_GOGOGO_OBSOLETE,                           STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3F4: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_ECHO_PARTY_SQUELCH,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3F5: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_SPELLCLICK,                                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3F7: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_LOOT_LIST,                                 STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x3F8: @TODO need to check usage in vanilla WoW
    OPCODE(MSG_GUILD_EVENT_LOG_QUERY,                      STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<guilds::GuildEventLogQuery>);      /// 0x3FE: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_MAELSTROM_RENAME_GUILD,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x3FF: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_GET_MIRRORIMAGE_DATA,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x400: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_MIRRORIMAGE_DATA,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x401: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_FORCE_DISPLAY_UPDATE,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x402: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_SPELL_CHANCE_RESIST_PUSHBACK,              STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);      /// 0x403: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_IGNORE_DIMINISHING_RETURNS_CHEAT,          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x404: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_IGNORE_DIMINISHING_RETURNS_CHEAT,          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x405: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_KEEP_ALIVE,                                STATUS_AUTHED,   PROCESS_THREADUNSAFE, protocol::KeepAlive);       /// 0x406: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_RAID_READY_CHECK_ERROR,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x407: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_OPT_OUT_OF_LOOT,                           STATUS_AUTHED,   PROCESS_THREADUNSAFE, groups::OptOutOfLoot);        /// 0x408: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_SET_GRANTABLE_LEVELS,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x40B: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_GRANT_LEVEL,                               STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x40C: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_DECLINE_CHANNEL_INVITE,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x40F: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_GROUPACTION_THROTTLED,                     STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x410: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_OVERRIDE_LIGHT,                            STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x411: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_TOTEM_CREATED,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x412: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_TOTEM_DESTROYED,                           STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<spells::TotemDestroyed>);        /// 0x413: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_EXPIRE_RAID_INSTANCE,                      STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x414: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_NO_SPELL_VARIANCE,                         STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x415: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY,          STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, PlayerAnswers<quests::QuestgiverStatusMultipleQuery>);     /// 0x416: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_QUESTGIVER_STATUS_MULTIPLE,                STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x417: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_QUERY_SERVER_BUCK_DATA,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x41A: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_CLEAR_SERVER_BUCK_DATA,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x41B: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_SERVER_BUCK_DATA,                          STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x41C: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_SEND_UNLEARN_SPELLS,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x41D: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_PROPOSE_LEVEL_GRANT,                       STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x41E: @TODO need to check usage in vanilla WoW
    OPCODE(CMSG_ACCEPT_LEVEL_GRANT,                        STATUS_NEVER,    PROCESS_INPLACE,      protocol::_NULL);     /// 0x41F: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_REFER_A_FRIEND_FAILURE,                    STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x420: @TODO need to check usage in vanilla WoW
    OPCODE(SMSG_SUMMON_CANCEL,                             STATUS_NEVER,    PROCESS_INPLACE,      protocol::_ServerSide);       /// 0x423: @TODO need to check usage in vanilla WoW

    return;
};
