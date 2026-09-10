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

#include "Common/Locales.h"
#include "Utilities/Errors.h"
#include <sstream>
#include <string>
#include <map>
#include "Chat.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "AccountMgr.h"
#include "SpellMgr.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "CommandMgr.h"
#include "ObjectLookup.h"
#include "Cast/Recipe/RecipeBook.h"

bool ChatHandler::load_command_table = true;
std::map<uint32, ObjectGuid> ChatHandler::m_consoleSelectedPlayers;

ChatCommand* ChatHandler::getCommandTable()
{
    static ChatCommand accountSetCommandTable[] =
    {
        { "addon",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleAccountSetAddonCommand,     "", nullptr },
        { "gmlevel",        SEC_CONSOLE,        true,  &ChatHandler::HandleAccountSetGmLevelCommand,   "", nullptr },
        { "password",       SEC_CONSOLE,        true,  &ChatHandler::HandleAccountSetPasswordCommand,  "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand accountCommandTable[] =
    {
        { "characters",     SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleAccountCharactersCommand,   "", nullptr },
        { "create",         SEC_CONSOLE,        true,  &ChatHandler::HandleAccountCreateCommand,       "", nullptr },
        { "delete",         SEC_CONSOLE,        true,  &ChatHandler::HandleAccountDeleteCommand,       "", nullptr },
        { "onlinelist",     SEC_CONSOLE,        true,  &ChatHandler::HandleAccountOnlineListCommand,   "", nullptr },
        { "lock",           SEC_PLAYER,         true,  &ChatHandler::HandleAccountLockCommand,         "", nullptr },
        { "set",            SEC_ADMINISTRATOR,  true,  nullptr,                                           "", accountSetCommandTable },
        { "password",       SEC_PLAYER,         true,  &ChatHandler::HandleAccountPasswordCommand,     "", nullptr },
        { "",               SEC_PLAYER,         true,  &ChatHandler::HandleAccountCommand,             "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand auctionCommandTable[] =
    {
        { "alliance",       SEC_ADMINISTRATOR,  false, &ChatHandler::HandleAuctionAllianceCommand,     "", nullptr },
        { "goblin",         SEC_ADMINISTRATOR,  false, &ChatHandler::HandleAuctionGoblinCommand,       "", nullptr },
        { "horde",          SEC_ADMINISTRATOR,  false, &ChatHandler::HandleAuctionHordeCommand,        "", nullptr },
        { "item",           SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleAuctionItemCommand,         "", nullptr },
        { "",               SEC_ADMINISTRATOR,  false, &ChatHandler::HandleAuctionCommand,             "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand banCommandTable[] =
    {
        { "account",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleBanAccountCommand,          "", nullptr },
        { "character",      SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleBanCharacterCommand,        "", nullptr },
        { "ip",             SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleBanIPCommand,               "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand baninfoCommandTable[] =
    {
        { "account",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleBanInfoAccountCommand,      "", nullptr },
        { "character",      SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleBanInfoCharacterCommand,    "", nullptr },
        { "ip",             SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleBanInfoIPCommand,           "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand banlistCommandTable[] =
    {
        { "account",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleBanListAccountCommand,      "", nullptr },
        { "character",      SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleBanListCharacterCommand,    "", nullptr },
        { "ip",             SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleBanListIPCommand,           "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand castCommandTable[] =
    {
        { "back",           SEC_ADMINISTRATOR,  false, &ChatHandler::HandleCastBackCommand,            "", nullptr },
        { "dist",           SEC_ADMINISTRATOR,  false, &ChatHandler::HandleCastDistCommand,            "", nullptr },
        { "self",           SEC_ADMINISTRATOR,  false, &ChatHandler::HandleCastSelfCommand,            "", nullptr },
        { "target",         SEC_ADMINISTRATOR,  false, &ChatHandler::HandleCastTargetCommand,          "", nullptr },
        { "",               SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleCastCommand,                "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand characterDeletedCommandTable[] =
    {
        { "delete",         SEC_CONSOLE,        true,  &ChatHandler::HandleCharacterDeletedDeleteCommand, "", nullptr },
        { "list",           SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleCharacterDeletedListCommand, "", nullptr },
        { "restore",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleCharacterDeletedRestoreCommand, "", nullptr },
        { "old",            SEC_CONSOLE,        true,  &ChatHandler::HandleCharacterDeletedOldCommand, "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand characterCommandTable[] =
    {
        { "deleted",        SEC_GAMEMASTER,     true,  nullptr,                                           "", characterDeletedCommandTable},
        { "erase",          SEC_CONSOLE,        true,  &ChatHandler::HandleCharacterEraseCommand,      "", nullptr },
        { "level",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleCharacterLevelCommand,      "", nullptr },
        { "rename",         SEC_GAMEMASTER,     true,  &ChatHandler::HandleCharacterRenameCommand,     "", nullptr },
        { "reputation",     SEC_GAMEMASTER,     true,  &ChatHandler::HandleCharacterReputationCommand, "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand debugPlayCommandTable[] =
    {
        { "cinematic",      SEC_MODERATOR,      false, &ChatHandler::HandleDebugPlayCinematicCommand,       "", nullptr },
        { "movie",          SEC_MODERATOR,      false, &ChatHandler::HandleDebugPlayMovieCommand,           "", nullptr },
        { "sound",          SEC_MODERATOR,      false, &ChatHandler::HandleDebugPlaySoundCommand,           "", nullptr },
        { nullptr,             0,                  false, nullptr,                                                "", nullptr }
    };

    static ChatCommand debugSendCommandTable[] =
    {
        { "buyerror",       SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSendBuyErrorCommand,        "", nullptr },
        { "channelnotify",  SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSendChannelNotifyCommand,   "", nullptr },
        { "chatmmessage",   SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSendChatMsgCommand,         "", nullptr },
        { "equiperror",     SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSendEquipErrorCommand,      "", nullptr },
        { "opcode",         SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSendOpcodeCommand,          "", nullptr },
        { "poi",            SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSendPoiCommand,             "", nullptr },
        { "qpartymsg",      SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSendQuestPartyMsgCommand,   "", nullptr },
        { "qinvalidmsg",    SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSendQuestInvalidMsgCommand, "", nullptr },
        { "sellerror",      SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSendSellErrorCommand,       "", nullptr },
        { "spellfail",      SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSendSpellFailCommand,       "", nullptr },
        { nullptr,             0,                  false, nullptr,                                                "", nullptr }
    };

    static ChatCommand debugCommandTable[] =
    {
        { "anim",           SEC_GAMEMASTER,     false, &ChatHandler::HandleDebugAnimCommand,                "", nullptr },
        { "crowd",          SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugCrowdSpawnCommand,          "", nullptr },
        { "uncrowd",        SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugCrowdDespawnCommand,        "", nullptr },
        { "bg",             SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugBattlegroundCommand,        "", nullptr },
        { "getitemstate",   SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugGetItemStateCommand,        "", nullptr },
        { "lootrecipient",  SEC_GAMEMASTER,     false, &ChatHandler::HandleDebugGetLootRecipientCommand,    "", nullptr },
        { "getitemvalue",   SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugGetItemValueCommand,        "", nullptr },
        { "getvalue",       SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugGetValueCommand,            "", nullptr },
        { "minion",         SEC_GAMEMASTER,     false, &ChatHandler::HandleDebugMinionCommand,              "", nullptr },
        { "vessel",         SEC_GAMEMASTER,     false, &ChatHandler::HandleDebugVesselCommand,              "", nullptr },
        { "moditemvalue",   SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugModItemValueCommand,        "", nullptr },
        { "modvalue",       SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugModValueCommand,            "", nullptr },
        { "play",           SEC_MODERATOR,      false, nullptr,                                                "", debugPlayCommandTable },
        { "recv",           SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugRecvOpcodeCommand,          "", nullptr },
        { "send",           SEC_ADMINISTRATOR,  false, nullptr,                                                "", debugSendCommandTable },
        { "setaurastate",   SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSetAuraStateCommand,        "", nullptr },
        { "setitemvalue",   SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSetItemValueCommand,        "", nullptr },
        { "setvalue",       SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSetValueCommand,            "", nullptr },
        { "spellcheck",     SEC_CONSOLE,        true,  &ChatHandler::HandleDebugSpellCheckCommand,          "", nullptr },
        { "spellcoefs",     SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleDebugSpellCoefsCommand,          "", nullptr },
        { "spellmods",      SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugSpellModsCommand,           "", nullptr },
        { "uws",            SEC_ADMINISTRATOR,  false, &ChatHandler::HandleDebugUpdateWorldStateCommand,    "", nullptr },
        { nullptr,             0,                  false, nullptr,                                                "", nullptr }
    };

    static ChatCommand eventCommandTable[] =
    {
        { "list",           SEC_GAMEMASTER,     true,  &ChatHandler::HandleEventListCommand,           "", nullptr },
        { "start",          SEC_GAMEMASTER,     true,  &ChatHandler::HandleEventStartCommand,          "", nullptr },
        { "stop",           SEC_GAMEMASTER,     true,  &ChatHandler::HandleEventStopCommand,           "", nullptr },
        { "",               SEC_GAMEMASTER,     true,  &ChatHandler::HandleEventInfoCommand,           "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand gmCommandTable[] =
    {
        { "chat",           SEC_MODERATOR,      false, &ChatHandler::HandleGMChatCommand,              "", nullptr },
        { "fly",            SEC_ADMINISTRATOR,  false, &ChatHandler::HandleGMFlyCommand,               "", nullptr },
        { "ingame",         SEC_PLAYER,         true,  &ChatHandler::HandleGMListIngameCommand,        "", nullptr },
        { "list",           SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleGMListFullCommand,          "", nullptr },
        { "visible",        SEC_MODERATOR,      false, &ChatHandler::HandleGMVisibleCommand,           "", nullptr },
        { "setview",        SEC_MODERATOR,      false, &ChatHandler::HandleSetViewCommand,             "", nullptr },
        { "",               SEC_MODERATOR,      false, &ChatHandler::HandleGMCommand,                  "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand goCommandTable[] =
    {
        { "creature",       SEC_MODERATOR,      false, &ChatHandler::HandleGoCreatureCommand,          "", nullptr },
        { "graveyard",      SEC_MODERATOR,      false, &ChatHandler::HandleGoGraveyardCommand,         "", nullptr },
        { "grid",           SEC_MODERATOR,      false, &ChatHandler::HandleGoGridCommand,              "", nullptr },
        { "object",         SEC_MODERATOR,      false, &ChatHandler::HandleGoObjectCommand,            "", nullptr },
        { "taxinode",       SEC_MODERATOR,      false, &ChatHandler::HandleGoTaxinodeCommand,          "", nullptr },
        { "trigger",        SEC_MODERATOR,      false, &ChatHandler::HandleGoTriggerCommand,           "", nullptr },
        { "zonexy",         SEC_MODERATOR,      false, &ChatHandler::HandleGoZoneXYCommand,            "", nullptr },
        { "xy",             SEC_MODERATOR,      false, &ChatHandler::HandleGoXYCommand,                "", nullptr },
        { "xyz",            SEC_MODERATOR,      false, &ChatHandler::HandleGoXYZCommand,               "", nullptr },
        { "",               SEC_MODERATOR,      false, &ChatHandler::HandleGoCommand,                  "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand gobjectCommandTable[] =
    {
        { "add",            SEC_GAMEMASTER,     false, &ChatHandler::HandleGameObjectAddCommand,       "", nullptr },
        { "anim",           SEC_GAMEMASTER,     false, &ChatHandler::HandleGameObjectAnimationCommand, "", nullptr },
        { "delete",         SEC_GAMEMASTER,     false, &ChatHandler::HandleGameObjectDeleteCommand,    "", nullptr },
        { "lootstate",      SEC_GAMEMASTER,     false, &ChatHandler::HandleGameObjectLootstateCommand, "", nullptr },
        { "move",           SEC_GAMEMASTER,     false, &ChatHandler::HandleGameObjectMoveCommand,      "", nullptr },
        { "near",           SEC_GAMEMASTER,     false, &ChatHandler::HandleGameObjectNearCommand,      "", nullptr },
        { "state",          SEC_GAMEMASTER,     false, &ChatHandler::HandleGameObjectStateCommand,     "", nullptr },
        { "target",         SEC_GAMEMASTER,     false, &ChatHandler::HandleGameObjectTargetCommand,    "", nullptr },
        { "turn",           SEC_GAMEMASTER,     false, &ChatHandler::HandleGameObjectTurnCommand,      "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand gridCommandTable[] =
    {
        { "info",           SEC_GAMEMASTER,     false, &ChatHandler::HandleGridInfoCommand,            "", nullptr },
        { "anchors",        SEC_GAMEMASTER,     false, &ChatHandler::HandleGridAnchorsCommand,         "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand guildCommandTable[] =
    {
        { "create",         SEC_GAMEMASTER,     true,  &ChatHandler::HandleGuildCreateCommand,         "", nullptr },
        { "delete",         SEC_GAMEMASTER,     true,  &ChatHandler::HandleGuildDeleteCommand,         "", nullptr },
        { "invite",         SEC_GAMEMASTER,     true,  &ChatHandler::HandleGuildInviteCommand,         "", nullptr },
        { "uninvite",       SEC_GAMEMASTER,     true,  &ChatHandler::HandleGuildUninviteCommand,       "", nullptr },
        { "rank",           SEC_GAMEMASTER,     true,  &ChatHandler::HandleGuildRankCommand,           "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand honorCommandTable[] =
    {
        { "add",            SEC_GAMEMASTER,     true,  &ChatHandler::HandleHonorAddCommand,            "", nullptr },
        { "addkill",        SEC_GAMEMASTER,     false, &ChatHandler::HandleHonorAddKillCommand,        "", nullptr },
        { "show",           SEC_GAMEMASTER,     false, &ChatHandler::HandleHonorShow,                  "", nullptr },
        { "update",         SEC_GAMEMASTER,     true,  &ChatHandler::HandleHonorUpdateCommand,         "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand instanceCommandTable[] =
    {
        { "listbinds",      SEC_ADMINISTRATOR,  false, &ChatHandler::HandleInstanceListBindsCommand,   "", nullptr },
        { "unbind",         SEC_ADMINISTRATOR,  false, &ChatHandler::HandleInstanceUnbindCommand,      "", nullptr },
        { "stats",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleInstanceStatsCommand,       "", nullptr },
        { "savedata",       SEC_ADMINISTRATOR,  false, &ChatHandler::HandleInstanceSaveDataCommand,    "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand learnCommandTable[] =
    {
        { "all",            SEC_ADMINISTRATOR,  false, &ChatHandler::HandleLearnAllCommand,            "", nullptr },
        { "all_gm",         SEC_GAMEMASTER,     false, &ChatHandler::HandleLearnAllGMCommand,          "", nullptr },
        { "all_crafts",     SEC_GAMEMASTER,     false, &ChatHandler::HandleLearnAllCraftsCommand,      "", nullptr },
        { "all_default",    SEC_MODERATOR,      false, &ChatHandler::HandleLearnAllDefaultCommand,     "", nullptr },
        { "all_lang",       SEC_MODERATOR,      false, &ChatHandler::HandleLearnAllLangCommand,        "", nullptr },
        { "all_myclass",    SEC_ADMINISTRATOR,  false, &ChatHandler::HandleLearnAllMyClassCommand,     "", nullptr },
        { "all_myspells",   SEC_ADMINISTRATOR,  false, &ChatHandler::HandleLearnAllMySpellsCommand,    "", nullptr },
        { "all_mytalents",  SEC_ADMINISTRATOR,  false, &ChatHandler::HandleLearnAllMyTalentsCommand,   "", nullptr },
        { "all_recipes",    SEC_GAMEMASTER,     false, &ChatHandler::HandleLearnAllRecipesCommand,     "", nullptr },
        { "",               SEC_ADMINISTRATOR,  false, &ChatHandler::HandleLearnCommand,               "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand listCommandTable[] =
    {
        { "auras",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleListAurasCommand,           "", nullptr },
        { "creature",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleListCreatureCommand,        "", nullptr },
        { "item",           SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleListItemCommand,            "", nullptr },
        { "object",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleListObjectCommand,          "", nullptr },
        { "talents",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleListTalentsCommand,         "", nullptr },
        { "players",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleListPlayersCommand,         "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand lookupAccountCommandTable[] =
    {
        { "email",          SEC_GAMEMASTER,     true,  &ChatHandler::HandleLookupAccountEmailCommand,  "", nullptr },
        { "ip",             SEC_GAMEMASTER,     true,  &ChatHandler::HandleLookupAccountIpCommand,     "", nullptr },
        { "name",           SEC_GAMEMASTER,     true,  &ChatHandler::HandleLookupAccountNameCommand,   "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand lookupPlayerCommandTable[] =
    {
        { "account",        SEC_GAMEMASTER,     true,  &ChatHandler::HandleLookupPlayerAccountCommand, "", nullptr },
        { "email",          SEC_GAMEMASTER,     true,  &ChatHandler::HandleLookupPlayerEmailCommand,   "", nullptr },
        { "ip",             SEC_GAMEMASTER,     true,  &ChatHandler::HandleLookupPlayerIpCommand,      "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand lookupCommandTable[] =
    {
        { "account",        SEC_GAMEMASTER,     true,  nullptr,                                           "", lookupAccountCommandTable },
        { "area",           SEC_MODERATOR,      true,  &ChatHandler::HandleLookupAreaCommand,          "", nullptr },
        { "creature",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleLookupCreatureCommand,      "", nullptr },
        { "event",          SEC_GAMEMASTER,     true,  &ChatHandler::HandleLookupEventCommand,         "", nullptr },
        { "faction",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleLookupFactionCommand,       "", nullptr },
        { "item",           SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleLookupItemCommand,          "", nullptr },
        { "itemset",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleLookupItemSetCommand,       "", nullptr },
        { "object",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleLookupObjectCommand,        "", nullptr },
        { "quest",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleLookupQuestCommand,         "", nullptr },
        { "player",         SEC_GAMEMASTER,     true,  nullptr,                                           "", lookupPlayerCommandTable },
        { "pool",           SEC_GAMEMASTER,     true,  &ChatHandler::HandleLookupPoolCommand,          "", nullptr },
        { "skill",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleLookupSkillCommand,         "", nullptr },
        { "spell",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleLookupSpellCommand,         "", nullptr },
        { "taxinode",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleLookupTaxiNodeCommand,      "", nullptr },
        { "tele",           SEC_MODERATOR,      true,  &ChatHandler::HandleLookupTeleCommand,          "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand mmapCommandTable[] =
    {
        { "path",           SEC_GAMEMASTER,     false, &ChatHandler::HandleMmapPathCommand,            "", nullptr },
        { "loc",            SEC_GAMEMASTER,     false, &ChatHandler::HandleMmapLocCommand,             "", nullptr },
        { "loadedtiles",    SEC_GAMEMASTER,     false, &ChatHandler::HandleMmapLoadedTilesCommand,     "", nullptr },
        { "stats",          SEC_GAMEMASTER,     false, &ChatHandler::HandleMmapStatsCommand,           "", nullptr },
        { "testarea",       SEC_GAMEMASTER,     false, &ChatHandler::HandleMmapTestArea,               "", nullptr },
        { "testheight",     SEC_GAMEMASTER,     false, &ChatHandler::HandleMmapTestHeight,             "", nullptr },
        { "",               SEC_ADMINISTRATOR,  false, &ChatHandler::HandleMmap,                       "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand modifyCommandTable[] =
    {
        { "hp",             SEC_MODERATOR,      true,  &ChatHandler::HandleModifyHPCommand,            "", nullptr },
        { "mana",           SEC_MODERATOR,      true,  &ChatHandler::HandleModifyManaCommand,          "", nullptr },
        { "rage",           SEC_MODERATOR,      true,  &ChatHandler::HandleModifyRageCommand,          "", nullptr },
        { "energy",         SEC_MODERATOR,      true,  &ChatHandler::HandleModifyEnergyCommand,        "", nullptr },
        { "money",          SEC_MODERATOR,      true,  &ChatHandler::HandleModifyMoneyCommand,         "", nullptr },
        { "speed",          SEC_MODERATOR,      true,  &ChatHandler::HandleModifySpeedCommand,         "", nullptr },
        { "swim",           SEC_MODERATOR,      true,  &ChatHandler::HandleModifySwimCommand,          "", nullptr },
        { "scale",          SEC_MODERATOR,      true,  &ChatHandler::HandleModifyScaleCommand,         "", nullptr },
        { "bwalk",          SEC_MODERATOR,      true,  &ChatHandler::HandleModifyBWalkCommand,         "", nullptr },
        { "aspeed",         SEC_MODERATOR,      true,  &ChatHandler::HandleModifyASpeedCommand,        "", nullptr },
        { "faction",        SEC_MODERATOR,      true,  &ChatHandler::HandleModifyFactionCommand,       "", nullptr },
        { "tp",             SEC_MODERATOR,      true,  &ChatHandler::HandleModifyTalentCommand,        "", nullptr },
        { "mount",          SEC_MODERATOR,      true,  &ChatHandler::HandleModifyMountCommand,         "", nullptr },
        { "honor",          SEC_MODERATOR,      true,  &ChatHandler::HandleModifyHonorCommand,         "", nullptr },
        { "rep",            SEC_GAMEMASTER,     true,  &ChatHandler::HandleModifyRepCommand,           "", nullptr },
        { "drunk",          SEC_MODERATOR,      false, &ChatHandler::HandleModifyDrunkCommand,         "", nullptr },
        { "standstate",     SEC_GAMEMASTER,     true,  &ChatHandler::HandleModifyStandStateCommand,    "", nullptr },
        { "morph",          SEC_GAMEMASTER,     true,  &ChatHandler::HandleModifyMorphCommand,         "", nullptr },
        { "gender",         SEC_GAMEMASTER,     true,  &ChatHandler::HandleModifyGenderCommand,        "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand npcCommandTable[] =
    {
        { "add",            SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcAddCommand,              "", nullptr },
        { "additem",        SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcAddVendorItemCommand,    "", nullptr },
        { "aiinfo",         SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcAIInfoCommand,           "", nullptr },
        { "allowmove",      SEC_ADMINISTRATOR,  false, &ChatHandler::HandleNpcAllowMovementCommand,    "", nullptr },
        { "changeentry",    SEC_ADMINISTRATOR,  false, &ChatHandler::HandleNpcChangeEntryCommand,      "", nullptr },
        { "changelevel",    SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcChangeLevelCommand,      "", nullptr },
        { "delete",         SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcDeleteCommand,           "", nullptr },
        { "delitem",        SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcDelVendorItemCommand,    "", nullptr },
        { "factionid",      SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcFactionIdCommand,        "", nullptr },
        { "flag",           SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcFlagCommand,             "", nullptr },
        { "follow",         SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcFollowCommand,           "", nullptr },
        { "info",           SEC_ADMINISTRATOR,  false, &ChatHandler::HandleNpcInfoCommand,             "", nullptr },
        { "move",           SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcMoveCommand,             "", nullptr },
        { "playemote",      SEC_ADMINISTRATOR,  false, &ChatHandler::HandleNpcPlayEmoteCommand,        "", nullptr },
        { "setmodel",       SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcSetModelCommand,         "", nullptr },
        { "setmovetype",    SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcSetMoveTypeCommand,      "", nullptr },
        { "spawndist",      SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcSpawnDistCommand,        "", nullptr },
        { "spawntime",      SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcSpawnTimeCommand,        "", nullptr },
        { "say",            SEC_MODERATOR,      false, &ChatHandler::HandleNpcSayCommand,              "", nullptr },
        { "textemote",      SEC_MODERATOR,      false, &ChatHandler::HandleNpcTextEmoteCommand,        "", nullptr },
        { "unfollow",       SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcUnFollowCommand,         "", nullptr },
        { "watch",          SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcWatchCommand,            "", nullptr },
        { "whisper",        SEC_MODERATOR,      false, &ChatHandler::HandleNpcWhisperCommand,          "", nullptr },
        { "yell",           SEC_MODERATOR,      false, &ChatHandler::HandleNpcYellCommand,             "", nullptr },
        { "tame",           SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcTameCommand,             "", nullptr },
        { "setdeathstate",  SEC_GAMEMASTER,     false, &ChatHandler::HandleNpcSetDeathStateCommand,    "", nullptr },

        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand pdumpCommandTable[] =
    {
        { "load",           SEC_ADMINISTRATOR,  true,  &ChatHandler::HandlePDumpLoadCommand,           "", nullptr },
        { "write",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandlePDumpWriteCommand,          "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand poolCommandTable[] =
    {
        { "list",           SEC_GAMEMASTER,     false, &ChatHandler::HandlePoolListCommand,            "", nullptr },
        { "spawns",         SEC_GAMEMASTER,     false, &ChatHandler::HandlePoolSpawnsCommand,          "", nullptr },
        { "",               SEC_GAMEMASTER,     true,  &ChatHandler::HandlePoolInfoCommand,            "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand questCommandTable[] =
    {
        { "add",            SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleQuestAddCommand,            "", nullptr },
        { "complete",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleQuestCompleteCommand,       "", nullptr },
        { "remove",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleQuestRemoveCommand,         "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand reloadCommandTable[] =
    {
        { "all",            SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadAllCommand,           "", nullptr },
        { "all_area",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadAllAreaCommand,       "", nullptr },
        { "all_eventai",    SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadAllEventAICommand,    "", nullptr },
        { "all_gossips",    SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadAllGossipsCommand,    "", nullptr },
        { "all_item",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadAllItemCommand,       "", nullptr },
        { "all_locales",    SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadAllLocalesCommand,    "", nullptr },
        { "all_loot",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadAllLootCommand,       "", nullptr },
        { "all_npc",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadAllNpcCommand,        "", nullptr },
        { "all_quest",      SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadAllQuestCommand,      "", nullptr },
        { "all_scripts",    SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadAllScriptsCommand,    "", nullptr },
        { "all_spell",      SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadAllSpellCommand,      "", nullptr },

        { "config",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadConfigCommand,        "", nullptr },

        { "areatrigger_quest_end",       SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadQuestAreaTriggersCommand,       "", nullptr },
        { "areatrigger_tavern",          SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadAreaTriggerTavernCommand,       "", nullptr },
        { "areatrigger_teleport",        SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadAreaTriggerTeleportCommand,     "", nullptr },
        { "autobroadcast",               SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadAutoBroadcastCommand,           "", nullptr },
        { "command",                     SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadCommandCommand,                 "", nullptr },
        { "conditions",                  SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadConditionsCommand,              "", nullptr },
        { "creature_ai_scripts",         SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadEventAIScriptsCommand,          "", nullptr },
        { "creature_ai_summons",         SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadEventAISummonsCommand,          "", nullptr },
        { "creature_ai_texts",           SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadEventAITextsCommand,            "", nullptr },
        { "creature_battleground",       SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadBattleEventCommand,             "", nullptr },
        { "creature_quest_end",          SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadCreatureQuestInvRelationsCommand, "", nullptr },
        { "creature_loot_template",      SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLootTemplatesCreatureCommand,   "", nullptr },
        { "creature_quest_start",        SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadCreatureQuestRelationsCommand,  "", nullptr },
        { "creature_template_classlevelstats", SEC_ADMINISTRATOR, true, &ChatHandler::HandleReloadCreaturesStatsCommand,     "", nullptr },
        { "creature_spells",             SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadCreatureSpellsCommand,          "", nullptr },
        { "db_script_string",            SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadDbScriptStringCommand,          "", nullptr },
        { "dbscripts_on_creature_death", SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadDBScriptsOnCreatureDeathCommand, "", nullptr },
        { "dbscripts_on_event",          SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadDBScriptsOnEventCommand,        "", nullptr },
        { "dbscripts_on_gossip",         SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadDBScriptsOnGossipCommand,       "", nullptr },
        { "dbscripts_on_go_use",         SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadDBScriptsOnGoUseCommand,        "", nullptr },
        { "dbscripts_on_quest_end",      SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadDBScriptsOnQuestEndCommand,     "", nullptr },
        { "dbscripts_on_quest_start",    SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadDBScriptsOnQuestStartCommand,   "", nullptr },
        { "dbscripts_on_spell",          SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadDBScriptsOnSpellCommand,        "", nullptr },
        { "dbscripts_on_creature_spell", SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadDBScriptsOnCreatureSpellCommand,"", nullptr },
        { "disables",                    SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadDisablesCommand,                "", nullptr },
        { "disenchant_loot_template",    SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLootTemplatesDisenchantCommand, "", nullptr },
        { "fishing_loot_template",       SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLootTemplatesFishingCommand,    "", nullptr },
        { "game_graveyard_zone",         SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadGameGraveyardZoneCommand,       "", nullptr },
        { "game_tele",                   SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadGameTeleCommand,                "", nullptr },
        { "gameobject_quest_end",        SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadGOQuestInvRelationsCommand,     "", nullptr },
        { "gameobject_loot_template",    SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLootTemplatesGameobjectCommand, "", nullptr },
        { "gameobject_quest_start",      SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadGOQuestRelationsCommand,        "", nullptr },
        { "gameobject_battleground",     SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadBattleEventCommand,             "", nullptr },
        { "gossip_menu",                 SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadGossipMenuCommand,              "", nullptr },
        { "gossip_menu_option",          SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadGossipMenuCommand,              "", nullptr },
        { "item_enchantment_template",   SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadItemEnchantementsCommand,       "", nullptr },
        { "item_loot_template",          SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLootTemplatesItemCommand,       "", nullptr },
        { "item_required_target",        SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadItemRequiredTragetCommand,      "", nullptr },
        { "locales_creature",            SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLocalesCreatureCommand,         "", nullptr },
        { "locales_gameobject",          SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLocalesGameobjectCommand,       "", nullptr },
        { "locales_gossip_menu_option",  SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLocalesGossipMenuOptionCommand, "", nullptr },
        { "locales_item",                SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLocalesItemCommand,             "", nullptr },
        { "locales_npc_text",            SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLocalesNpcTextCommand,          "", nullptr },
        { "locales_page_text",           SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLocalesPageTextCommand,         "", nullptr },
        { "locales_points_of_interest",  SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLocalesPointsOfInterestCommand, "", nullptr },
        { "locales_quest",               SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLocalesQuestCommand,            "", nullptr },
        { "locales_command",             SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLocalesCommandHelpCommand,            "", nullptr },
        { "mail_loot_template",          SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLootTemplatesMailCommand,       "", nullptr },
        { "mangos_string",               SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadMangosStringCommand,            "", nullptr },
        { "npc_text",                    SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadNpcTextCommand,                 "", nullptr },
        { "npc_trainer",                 SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadNpcTrainerCommand,              "", nullptr },
        { "npc_vendor",                  SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadNpcVendorCommand,               "", nullptr },
        { "page_text",                   SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadPageTextsCommand,               "", nullptr },
        { "pickpocketing_loot_template", SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLootTemplatesPickpocketingCommand, "", nullptr},
        { "points_of_interest",          SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadPointsOfInterestCommand,        "", nullptr },
        { "quest_template",              SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadQuestTemplateCommand,           "", nullptr },
        { "reference_loot_template",     SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLootTemplatesReferenceCommand,  "", nullptr },
        { "reserved_name",               SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadReservedNameCommand,            "", nullptr },
        { "reputation_reward_rate",      SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadReputationRewardRateCommand,    "", nullptr },
        { "reputation_spillover_template", SEC_ADMINISTRATOR, true, &ChatHandler::HandleReloadReputationSpilloverTemplateCommand, "", nullptr },
        { "script_binding",              SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadScriptBindingCommand,           "", nullptr },
        { "skill_fishing_base_level",    SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSkillFishingBaseLevelCommand,   "", nullptr },
        { "skinning_loot_template",      SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadLootTemplatesSkinningCommand,   "", nullptr },
        { "spell_affect",                SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellAffectCommand,             "", nullptr },
        { "spell_area",                  SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellAreaCommand,               "", nullptr },
        { "spell_bonus_data",            SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellBonusesCommand,            "", nullptr },
        { "spell_dummy",                 SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellDummyCommand,              "", nullptr },
        { "spell_chain",                 SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellChainCommand,              "", nullptr },
        { "spell_elixir",                SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellElixirCommand,             "", nullptr },
        { "spell_learn_spell",           SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellLearnSpellCommand,         "", nullptr },
        { "spell_pet_auras",             SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellPetAurasCommand,           "", nullptr },
        { "spell_proc_event",            SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellProcEventCommand,          "", nullptr },
        { "spell_proc_item_enchant",     SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellProcItemEnchantCommand,    "", nullptr },
        { "spell_script_target",         SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellScriptTargetCommand,       "", nullptr },
        { "spell_target_position",       SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellTargetPositionCommand,     "", nullptr },
        { "spell_threats",               SEC_ADMINISTRATOR, true,  &ChatHandler::HandleReloadSpellThreatsCommand,            "", nullptr },

        { nullptr,                          0,                 false, nullptr,                                                     "", nullptr }
    };

    static ChatCommand resetCommandTable[] =
    {
        { "honor",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleResetHonorCommand,          "", nullptr },
        { "level",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleResetLevelCommand,          "", nullptr },
        { "spells",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleResetSpellsCommand,         "", nullptr },
        { "stats",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleResetStatsCommand,          "", nullptr },
        { "talents",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleResetTalentsCommand,        "", nullptr },
        { "items",          SEC_ADMINISTRATOR,  false, &ChatHandler::HandleResetItemsCommand,         "", nullptr },
        { "mail",           SEC_ADMINISTRATOR,  false, &ChatHandler::HandleResetMailCommand,           "", nullptr },
        { "all",            SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleResetAllCommand,            "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand sendMassCommandTable[] =
    {
        { "items",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleSendMassItemsCommand,       "", nullptr },
        { "mail",           SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleSendMassMailCommand,        "", nullptr },
        { "money",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleSendMassMoneyCommand,       "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand sendCommandTable[] =
    {
        { "mass",           SEC_ADMINISTRATOR,  true,  nullptr,                                           "", sendMassCommandTable },

        { "items",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleSendItemsCommand,           "", nullptr },
        { "mail",           SEC_MODERATOR,      true,  &ChatHandler::HandleSendMailCommand,            "", nullptr },
        { "message",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleSendMessageCommand,         "", nullptr },
        { "money",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleSendMoneyCommand,           "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand serverIdleRestartCommandTable[] =
    {
        { "cancel",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleServerShutDownCancelCommand, "", nullptr },
        { ""   ,            SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleServerIdleRestartCommand,   "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand serverIdleShutdownCommandTable[] =
    {
        { "cancel",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleServerShutDownCancelCommand, "", nullptr },
        { ""   ,            SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleServerIdleShutDownCommand,  "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand serverRestartCommandTable[] =
    {
        { "cancel",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleServerShutDownCancelCommand, "", nullptr },
        { ""   ,            SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleServerRestartCommand,       "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand serverShutdownCommandTable[] =
    {
        { "cancel",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleServerShutDownCancelCommand, "", nullptr },
        { ""   ,            SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleServerShutDownCommand,      "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand serverLogCommandTable[] =
    {
        { "filter",         SEC_CONSOLE,        true,  &ChatHandler::HandleServerLogFilterCommand,     "", nullptr },
        { "level",          SEC_CONSOLE,        true,  &ChatHandler::HandleServerLogLevelCommand,      "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand serverSetCommandTable[] =
    {
        { "motd",           SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleServerSetMotdCommand,       "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand serverCommandTable[] =
    {
        { "corpses",        SEC_GAMEMASTER,     true,  &ChatHandler::HandleServerCorpsesCommand,       "", nullptr },
        { "exit",           SEC_CONSOLE,        true,  &ChatHandler::HandleServerExitCommand,          "", nullptr },
        { "idlerestart",    SEC_ADMINISTRATOR,  true,  nullptr,                                           "", serverIdleRestartCommandTable },
        { "idleshutdown",   SEC_ADMINISTRATOR,  true,  nullptr,                                           "", serverIdleShutdownCommandTable },
        { "info",           SEC_PLAYER,         true,  &ChatHandler::HandleServerInfoCommand,          "", nullptr },
        { "log",            SEC_CONSOLE,        true,  nullptr,                                           "", serverLogCommandTable },
        { "motd",           SEC_PLAYER,         true,  &ChatHandler::HandleServerMotdCommand,          "", nullptr },
        { "plimit",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleServerPLimitCommand,        "", nullptr },
        { "resetallraid",   SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleServerResetAllRaidCommand,  "", nullptr },
        { "restart",        SEC_ADMINISTRATOR,  true,  nullptr,                                           "", serverRestartCommandTable },
        { "shutdown",       SEC_ADMINISTRATOR,  true,  nullptr,                                           "", serverShutdownCommandTable },
        { "set",            SEC_ADMINISTRATOR,  true,  nullptr,                                           "", serverSetCommandTable },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand selectCommandTable[] =
    {
        { "player",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleSelectPlayerCommand,        "", nullptr },
        { "clear",          SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleSelectClearCommand,         "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand teleCommandTable[] =
    {
        { "add",            SEC_ADMINISTRATOR,  false, &ChatHandler::HandleTeleAddCommand,             "", nullptr },
        { "del",            SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleTeleDelCommand,             "", nullptr },
        { "name",           SEC_MODERATOR,      true,  &ChatHandler::HandleTeleNameCommand,            "", nullptr },
        { "group",          SEC_MODERATOR,      false, &ChatHandler::HandleTeleGroupCommand,           "", nullptr },
        { "",               SEC_MODERATOR,      false, &ChatHandler::HandleTeleCommand,                "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand ticketCommandTable[] =
    {
        { "accept",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleTicketAcceptCommand,         "", nullptr },
        { "close",          SEC_GAMEMASTER,     true,  &ChatHandler::HandleTicketCloseCommand,          "", nullptr },
        { "delete",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleTicketDeleteCommand,         "", nullptr },
        { "info",           SEC_GAMEMASTER,     true,  &ChatHandler::HandleTicketInfoCommand,           "", nullptr },
        { "list",           SEC_GAMEMASTER,     true,  &ChatHandler::HandleTicketListCommand,           "", nullptr },
        { "meaccept",       SEC_GAMEMASTER,     false, &ChatHandler::HandleTicketMeAcceptCommand,       "", nullptr },
        { "onlinelist",     SEC_GAMEMASTER,     true,  &ChatHandler::HandleTicketOnlineListCommand,     "", nullptr },
        { "respond",        SEC_GAMEMASTER,     true,  &ChatHandler::HandleTicketRespondCommand,        "", nullptr },
        { "show",           SEC_GAMEMASTER,     true,  &ChatHandler::HandleTicketShowCommand,           "", nullptr },
        { "surveyclose",    SEC_GAMEMASTER,     true,  &ChatHandler::HandleTickerSurveyClose,           "", nullptr },
        { nullptr,             0,                  false, nullptr,                                            "", nullptr }
    };

    static ChatCommand triggerCommandTable[] =
    {
        { "active",         SEC_GAMEMASTER,     false, &ChatHandler::HandleTriggerActiveCommand,       "", nullptr },
        { "near",           SEC_GAMEMASTER,     false, &ChatHandler::HandleTriggerNearCommand,         "", nullptr },
        { "",               SEC_GAMEMASTER,     true,  &ChatHandler::HandleTriggerCommand,             "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand unbanCommandTable[] =
    {
        { "account",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleUnBanAccountCommand,      "", nullptr },
        { "character",      SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleUnBanCharacterCommand,    "", nullptr },
        { "ip",             SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleUnBanIPCommand,           "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand wpCommandTable[] =
    {
        { "show",           SEC_GAMEMASTER,     false, &ChatHandler::HandleWpShowCommand,              "", nullptr },
        { "add",            SEC_GAMEMASTER,     false, &ChatHandler::HandleWpAddCommand,               "", nullptr },
        { "modify",         SEC_GAMEMASTER,     false, &ChatHandler::HandleWpModifyCommand,            "", nullptr },
        { "export",         SEC_ADMINISTRATOR,  false, &ChatHandler::HandleWpExportCommand,            "", nullptr },
        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    static ChatCommand commandTable[] =
    {
        { "account",        SEC_PLAYER,         true,  nullptr,                                           "", accountCommandTable  },
        { "auction",        SEC_ADMINISTRATOR,  false, nullptr,                                           "", auctionCommandTable  },
        { "cast",           SEC_ADMINISTRATOR,  true,  nullptr,                                           "", castCommandTable     },
        { "character",      SEC_GAMEMASTER,     true,  nullptr,                                           "", characterCommandTable},
        { "debug",          SEC_MODERATOR,      true,  nullptr,                                           "", debugCommandTable    },
        { "event",          SEC_GAMEMASTER,     false, nullptr,                                           "", eventCommandTable    },
        { "gm",             SEC_PLAYER,         true,  nullptr,                                           "", gmCommandTable       },
        { "honor",          SEC_GAMEMASTER,     false, nullptr,                                           "", honorCommandTable    },
        { "go",             SEC_MODERATOR,      false, nullptr,                                           "", goCommandTable       },
        { "gobject",        SEC_GAMEMASTER,     false, nullptr,                                           "", gobjectCommandTable  },
        { "grid",           SEC_GAMEMASTER,     false, nullptr,                                           "", gridCommandTable     },
        { "guild",          SEC_GAMEMASTER,     true,  nullptr,                                           "", guildCommandTable    },
        { "instance",       SEC_ADMINISTRATOR,  true,  nullptr,                                           "", instanceCommandTable },
        { "learn",          SEC_MODERATOR,      false, nullptr,                                           "", learnCommandTable    },
        { "list",           SEC_ADMINISTRATOR,  true,  nullptr,                                           "", listCommandTable     },
        { "lookup",         SEC_MODERATOR,      true,  nullptr,                                           "", lookupCommandTable   },
        { "modify",         SEC_MODERATOR,      false, nullptr,                                           "", modifyCommandTable   },
        { "npc",            SEC_MODERATOR,      false, nullptr,                                           "", npcCommandTable      },
        { "pool",           SEC_GAMEMASTER,     true,  nullptr,                                           "", poolCommandTable     },
        { "pdump",          SEC_ADMINISTRATOR,  true,  nullptr,                                           "", pdumpCommandTable    },
        { "quest",          SEC_ADMINISTRATOR,  true,  nullptr,                                           "", questCommandTable    },
        { "reload",         SEC_ADMINISTRATOR,  true,  nullptr,                                           "", reloadCommandTable   },
        { "reset",          SEC_ADMINISTRATOR,  true,  nullptr,                                           "", resetCommandTable    },
        { "select",         SEC_ADMINISTRATOR,  true,  nullptr,                                           "", selectCommandTable   },
        { "server",         SEC_PLAYER,         true,  nullptr,                                           "", serverCommandTable   },
        { "tele",           SEC_MODERATOR,      true,  nullptr,                                           "", teleCommandTable     },
        { "trigger",        SEC_GAMEMASTER,     false, nullptr,                                           "", triggerCommandTable  },
        { "wp",             SEC_GAMEMASTER,     false, nullptr,                                           "", wpCommandTable       },

        { "aura",           SEC_ADMINISTRATOR,  false, &ChatHandler::HandleAuraCommand,                "", nullptr },
        { "unaura",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleUnAuraCommand,              "", nullptr },
        { "announce",       SEC_MODERATOR,      true,  &ChatHandler::HandleAnnounceCommand,            "", nullptr },
        { "notify",         SEC_MODERATOR,      true,  &ChatHandler::HandleNotifyCommand,              "", nullptr },
        { "appear",         SEC_MODERATOR,      false, &ChatHandler::HandleAppearCommand,              "", nullptr },
        { "summon",         SEC_MODERATOR,      false, &ChatHandler::HandleSummonCommand,              "", nullptr },
        { "groupgo",        SEC_MODERATOR,      false, &ChatHandler::HandleGroupgoCommand,             "", nullptr },
        { "auragroup",      SEC_ADMINISTRATOR,  false, &ChatHandler::HandleAuraGroupCommand,             "", nullptr },
        { "unauragroup",    SEC_ADMINISTRATOR,  false, &ChatHandler::HandleUnAuraGroupCommand,             "", nullptr },
        { "commands",       SEC_PLAYER,         true,  &ChatHandler::HandleCommandsCommand,            "", nullptr },
        { "demorph",        SEC_GAMEMASTER,     true,  &ChatHandler::HandleDeMorphCommand,             "", nullptr },
        { "die",            SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleDieCommand,                 "", nullptr },
        { "revive",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReviveCommand,              "", nullptr },
        { "dismount",       SEC_PLAYER,         false, &ChatHandler::HandleDismountCommand,            "", nullptr },
        { "gps",            SEC_MODERATOR,      false, &ChatHandler::HandleGPSCommand,                 "", nullptr },
        { "guid",           SEC_GAMEMASTER,     false, &ChatHandler::HandleGUIDCommand,                "", nullptr },
        { "help",           SEC_PLAYER,         true,  &ChatHandler::HandleHelpCommand,                "", nullptr },
        { "itemmove",       SEC_GAMEMASTER,     false, &ChatHandler::HandleItemMoveCommand,            "", nullptr },
        { "cooldown",       SEC_ADMINISTRATOR,  false, &ChatHandler::HandleCooldownCommand,            "", nullptr },
        { "unlearn",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleUnLearnCommand,             "", nullptr },
        { "distance",       SEC_ADMINISTRATOR,  false, &ChatHandler::HandleGetDistanceCommand,         "", nullptr },
        { "recall",         SEC_MODERATOR,      true,  &ChatHandler::HandleRecallCommand,              "", nullptr },
        { "save",           SEC_PLAYER,         false, &ChatHandler::HandleSaveCommand,                "", nullptr },
        { "saveall",        SEC_MODERATOR,      true,  &ChatHandler::HandleSaveAllCommand,             "", nullptr },
        { "kick",           SEC_GAMEMASTER,     true,  &ChatHandler::HandleKickPlayerCommand,          "", nullptr },
        { "ban",            SEC_ADMINISTRATOR,  true,  nullptr,                                           "", banCommandTable      },
        { "unban",          SEC_ADMINISTRATOR,  true,  nullptr,                                           "", unbanCommandTable    },
        { "baninfo",        SEC_ADMINISTRATOR,  false, nullptr,                                           "", baninfoCommandTable  },
        { "banlist",        SEC_ADMINISTRATOR,  true,  nullptr,                                           "", banlistCommandTable  },
        { "start",          SEC_PLAYER,         false, &ChatHandler::HandleStartCommand,               "", nullptr },
        { "taxicheat",      SEC_MODERATOR,      false, &ChatHandler::HandleTaxiCheatCommand,           "", nullptr },
        { "linkgrave",      SEC_ADMINISTRATOR,  false, &ChatHandler::HandleLinkGraveCommand,           "", nullptr },
        { "neargrave",      SEC_ADMINISTRATOR,  false, &ChatHandler::HandleNearGraveCommand,           "", nullptr },
        { "explorecheat",   SEC_ADMINISTRATOR,  false, &ChatHandler::HandleExploreCheatCommand,        "", nullptr },
        { "levelup",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleLevelUpCommand,             "", nullptr },
        { "showarea",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleShowAreaCommand,            "", nullptr },
        { "hidearea",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleHideAreaCommand,            "", nullptr },
        { "additem",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleAddItemCommand,             "", nullptr },
        { "additemset",     SEC_ADMINISTRATOR,  false, &ChatHandler::HandleAddItemSetCommand,          "", nullptr },
        { "bank",           SEC_ADMINISTRATOR,  false, &ChatHandler::HandleBankCommand,                "", nullptr },
        { "wchange",        SEC_ADMINISTRATOR,  false, &ChatHandler::HandleChangeWeatherCommand,       "", nullptr },
        { "ticket",         SEC_GAMEMASTER,     false, nullptr,                                           "", ticketCommandTable   },
        { "maxskill",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleMaxSkillCommand,            "", nullptr },
        { "setskill",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleSetSkillCommand,            "", nullptr },
        { "whispers",       SEC_MODERATOR,      false, &ChatHandler::HandleWhispersCommand,            "", nullptr },
        { "pinfo",          SEC_GAMEMASTER,     true,  &ChatHandler::HandlePInfoCommand,               "", nullptr },
        { "respawn",        SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleRespawnCommand,             "", nullptr },
        { "send",           SEC_MODERATOR,      true,  nullptr,                                           "", sendCommandTable     },
        { "loadscripts",    SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleLoadScriptsCommand,         "", nullptr },
        { "mute",           SEC_MODERATOR,      true,  &ChatHandler::HandleMuteCommand,                "", nullptr },
        { "unmute",         SEC_MODERATOR,      true,  &ChatHandler::HandleUnmuteCommand,              "", nullptr },
        { "movegens",       SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleMovegensCommand,            "", nullptr },
        { "cometome",       SEC_ADMINISTRATOR,  false, &ChatHandler::HandleComeToMeCommand,            "", nullptr },
        { "damage",         SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleDamageCommand,              "", nullptr },
        { "combatstop",     SEC_GAMEMASTER,     true,  &ChatHandler::HandleCombatStopCommand,          "", nullptr },
        { "repairitems",    SEC_GAMEMASTER,     true,  &ChatHandler::HandleRepairitemsCommand,         "", nullptr },
        { "stable",         SEC_ADMINISTRATOR,  false, &ChatHandler::HandleStableCommand,              "", nullptr },
        { "waterwalk",      SEC_GAMEMASTER,     false, &ChatHandler::HandleWaterwalkCommand,           "", nullptr },
        { "freezeplayer",   SEC_GAMEMASTER,     true,  &ChatHandler::HandleFreezePlayerCommand,        "", nullptr },
        { "unfreezeplayer", SEC_GAMEMASTER,     true,  &ChatHandler::HandleUnfreezePlayerCommand,      "", nullptr },
        { "quit",           SEC_CONSOLE,        true,  &ChatHandler::HandleQuitCommand,                "", nullptr },
        { "mmap",           SEC_GAMEMASTER,     false, nullptr,                                           "", mmapCommandTable },
        { "spell_linked",   SEC_ADMINISTRATOR,  true,  &ChatHandler::HandleReloadSpellLinkedCommand,   "", nullptr },

        { nullptr,             0,                  false, nullptr,                                           "", nullptr }
    };

    if (load_command_table)
    {
        load_command_table = false;

        CheckIntegrity(commandTable, nullptr);

        QueryResult* result = WorldDatabase.Query("SELECT `id`, `command_text`,`security`,`help_text` FROM `command`");
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 id = fields[0].GetUInt32();
                std::string name = fields[1].GetCppString();
                SetDataForCommandInTable(commandTable, id, name.c_str(), fields[2].GetUInt16(), fields[3].GetCppString());
            }
            while (result->NextRow());
            delete result;
        }
    }

    return commandTable;
}

ChatHandler::ChatHandler(WorldSession* session) : m_session(session) {}

ChatHandler::ChatHandler(Player* player) : m_session(player->GetSession()) {}

ChatHandler::~ChatHandler() {}

const char* ChatHandler::GetMangosString(int32 entry) const
{
    return m_session->GetMangosString(entry);
}

const char* ChatHandler::GetOnOffStr(bool value) const
{
    return value ?  GetMangosString(LANG_ON) : GetMangosString(LANG_OFF);
}

uint32 ChatHandler::GetAccountId() const
{
    return m_session->GetAccountId();
}

AccountTypes ChatHandler::GetAccessLevel() const
{
    return m_session->GetSecurity();
}

bool ChatHandler::isAvailable(ChatCommand const& cmd) const
{

    return GetAccessLevel() >= (AccountTypes)cmd.SecurityLevel;
}

std::string ChatHandler::GetNameLink() const
{
    return GetNameLink(m_session->GetPlayer());
}

bool ChatHandler::HasLowerSecurity(Player* target, ObjectGuid guid, bool strong)
{
    WorldSession* target_session = nullptr;
    uint32 target_account = 0;

    if (target)
    {
        target_session = target->GetSession();
    }
    else
    {
        target_account = sObjectMgr.GetPlayerAccountIdByGUID(guid);
    }

    if (!target_session && !target_account)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return true;
    }

    return HasLowerSecurityAccount(target_session, target_account, strong);
}

bool ChatHandler::HasLowerSecurityAccount(WorldSession* target, uint32 target_account, bool strong)
{
    AccountTypes target_sec;

    if (GetAccessLevel() > SEC_PLAYER && !strong && !sWorld.getConfig(CONFIG_BOOL_GM_LOWER_SECURITY))
    {
        return false;
    }

    if (target)
    {
        target_sec = target->GetSecurity();
    }
    else if (target_account)
    {
        target_sec = sAccountMgr.GetSecurity(target_account);
    }
    else
    {
        return true;
    }

    if (GetAccessLevel() < target_sec || (strong && GetAccessLevel() <= target_sec))
    {
        SendSysMessage(LANG_YOURS_SECURITY_IS_LOW);
        SetSentErrorMessage(true);
        return true;
    }

    return false;
}

bool ChatHandler::hasStringAbbr(const char* name, const char* part)
{

    if (*name)
    {

        if (!*part)
        {
            return false;
        }

        for (;;)
        {
            if (!*part)
            {
                return true;
            }
            else if (!*name)
            {
                return false;
            }
            else if (tolower(*name) != tolower(*part))
            {
                return false;
            }
            ++name; ++part;
        }
    }

    return true;
}

void ChatHandler::CheckIntegrity(ChatCommand* table, ChatCommand* parentCommand)
{
    for (uint32 i = 0; table[i].Name != nullptr; ++i)
    {
        ChatCommand* command = &table[i];

        if (parentCommand && command->SecurityLevel < parentCommand->SecurityLevel)
        {
            sLog.outError("Subcommand '%s' of command '%s' have less access level (%u) that parent (%u)",
                command->Name, parentCommand->Name, command->SecurityLevel, parentCommand->SecurityLevel);
        }

        if (!parentCommand && strlen(command->Name) == 0)
        {
            sLog.outError("Subcommand '' at top level");
        }

        if (command->ChildCommands)
        {
            if (command->Handler)
            {
                if (parentCommand)
                {
                    sLog.outError("Subcommand '%s' of command '%s' have handler and subcommands in same time, must be used '' subcommand for handler instead.",
                        command->Name, parentCommand->Name);
                }
                else
                {
                    sLog.outError("First level command '%s' have handler and subcommands in same time, must be used '' subcommand for handler instead.",
                        command->Name);
                }
            }

            if (parentCommand && strlen(command->Name) == 0)
            {
                sLog.outError("Subcommand '' of command '%s' have subcommands", parentCommand->Name);
            }

            CheckIntegrity(command->ChildCommands, command);
        }
        else if (!command->Handler)
        {
            if (parentCommand)
            {
                sLog.outError("Subcommand '%s' of command '%s' not have handler and subcommands in same time. Must have some from its!",
                    command->Name, parentCommand->Name);
            }
            else
            {
                sLog.outError("First level command '%s' not have handler and subcommands in same time. Must have some from its!",
                    command->Name);
            }
        }
    }
}

ChatCommand const* ChatHandler::FindCommand(char const* text)
{
    ChatCommand* command = nullptr;
    char const* textPtr = text;

    return FindCommand(getCommandTable(), textPtr, command) == CHAT_COMMAND_OK ? command : nullptr;
}

ChatCommandSearchResult ChatHandler::FindCommand(ChatCommand* table, char const*& text, ChatCommand*& command, ChatCommand** parentCommand , std::string* cmdNamePtr , bool allAvailable , bool exactlyName )
{
    std::string cmd = "";

    while (*text != ' ' && *text != '\0')
    {
        cmd += *text;
        ++text;
    }

    while (*text == ' ')
    {
        ++text;
    }

    for (uint32 i = 0; table[i].Name != nullptr; ++i)
    {
        if (exactlyName)
        {
            size_t len = strlen(table[i].Name);
            if (strncmp(table[i].Name, cmd.c_str(), len + 1) != 0)
            {
                continue;
            }
        }
        else
        {
            if (!hasStringAbbr(table[i].Name, cmd.c_str()))
            {
                continue;
            }
        }

        if (table[i].ChildCommands != nullptr)
        {
            char const* oldchildtext = text;
            ChatCommand* parentSubcommand = nullptr;
            ChatCommandSearchResult res = FindCommand(table[i].ChildCommands, text, command, &parentSubcommand, cmdNamePtr, allAvailable, exactlyName);

            switch (res)
            {
                case CHAT_COMMAND_OK:
                {

                    if (parentCommand)
                    {
                        *parentCommand = parentSubcommand ? parentSubcommand : &table[i];
                    }

                    if (strlen(command->Name) == 0 && !parentSubcommand)
                    {
                        text = oldchildtext;
                    }

                    return CHAT_COMMAND_OK;
                }
                case CHAT_COMMAND_UNKNOWN:
                {

                    command = &table[i];
                    if (parentCommand)
                    {
                        *parentCommand = nullptr;
                    }

                    text = oldchildtext;
                    return CHAT_COMMAND_UNKNOWN_SUBCOMMAND;
                }
                case CHAT_COMMAND_UNKNOWN_SUBCOMMAND:
                default:
                {

                    if (parentCommand)
                    {
                        *parentCommand = parentSubcommand ? parentSubcommand : &table[i];
                    }
                    return res;
                }
            }
        }

        if (!allAvailable && !isAvailable(table[i]))
        {
            continue;
        }

        if (!table[i].Handler)
        {
            continue;
        }

        command = &table[i];

        if (parentCommand)
        {
            *parentCommand = nullptr;
        }

        if (cmdNamePtr)
        {
            *cmdNamePtr = cmd;
        }

        return CHAT_COMMAND_OK;
    }

    command = nullptr;

    if (parentCommand)
    {
        *parentCommand = nullptr;
    }

    if (cmdNamePtr)
    {
        *cmdNamePtr = cmd;
    }

    return CHAT_COMMAND_UNKNOWN;
}

void ChatHandler::ExecuteCommand(const char* text)
{
    std::string fullcmd = text;

    ChatCommand* command = nullptr;
    ChatCommand* parentCommand = nullptr;

    ChatCommandSearchResult res = FindCommand(getCommandTable(), text, command, &parentCommand);

    switch (res)
    {
        case CHAT_COMMAND_OK:
        {
            SetSentErrorMessage(false);
            if ((this->*(command->Handler))((char*)text))
            {
                if (command->SecurityLevel > SEC_PLAYER)
                {
                    LogCommand(fullcmd.c_str());
                }
            }

            else if (!HasSentErrorMessage())
            {
                if (!command->Help.empty())
                {
                    std::string helpText = command->Help;

                    if (m_session)
                    {
                        int loc_idx = m_session->GetSessionDbLocaleIndex();
                        sCommandMgr.GetCommandHelpLocaleString(command->Id, loc_idx, &helpText);
                    }

                    SendSysMessage(helpText.c_str());
                }
                else
                {
                    SendSysMessage(LANG_CMD_SYNTAX);
                }

                if (ChatCommand* showCommand = (strlen(command->Name) == 0 && parentCommand ? parentCommand : command))
                {
                    if (ChatCommand* childs = showCommand->ChildCommands)
                    {
                        ShowHelpForSubCommands(childs, showCommand->Name);
                    }
                }

                SetSentErrorMessage(true);
            }
            break;
        }
        case CHAT_COMMAND_UNKNOWN_SUBCOMMAND:
        {
            SendSysMessage(LANG_NO_SUBCMD);
            ShowHelpForCommand(command->ChildCommands, text);
            SetSentErrorMessage(true);
            break;
        }
        case CHAT_COMMAND_UNKNOWN:
        {
            SendSysMessage(LANG_NO_CMD);
            SetSentErrorMessage(true);
            break;
        }
    }
}

bool ChatHandler::SetDataForCommandInTable(ChatCommand* commandTable, uint32 id, const char* text, uint32 security, std::string const& help)
{
    std::string fullcommand = text;

    ChatCommand* command = nullptr;
    std::string cmdName;

    ChatCommandSearchResult res = FindCommand(commandTable, text, command, nullptr, &cmdName, true, true);

    switch (res)
    {
        case CHAT_COMMAND_OK:
        {
            if (command->SecurityLevel != security)
            {
                DETAIL_LOG("Table `command` overwrite for command '%s' default security (%u) by %u",
                    fullcommand.c_str(), command->SecurityLevel, security);
            }

            command->Id = id;
            command->SecurityLevel = security;
            command->Help          = help;
            return true;
        }
        case CHAT_COMMAND_UNKNOWN_SUBCOMMAND:
        {

            if (cmdName.empty())
            {
                sLog.outErrorDb("Table `command` have command '%s' that only used with some subcommand selection, it can't have help or overwritten access level, skip.", cmdName.c_str());
            }
            else
            {
                sLog.outErrorDb("Table `command` have unexpected subcommand '%s' in command '%s', skip.", cmdName.c_str(), fullcommand.c_str());
            }
            return false;
        }
        case CHAT_COMMAND_UNKNOWN:
        {
            sLog.outErrorDb("Table `command` have nonexistent command '%s', skip.", cmdName.c_str());
            return false;
        }
    }

    return false;
}

bool ChatHandler::ParseCommands(const char* text)
{
    MANGOS_ASSERT(text);
    MANGOS_ASSERT(*text);

    if (m_session)
    {
        if (m_session->GetSecurity() == SEC_PLAYER && !sWorld.getConfig(CONFIG_BOOL_PLAYER_COMMANDS))
        {
            return false;
        }

        if (text[0] != '!' && text[0] != '.')
        {
            return false;
        }

        if (strlen(text) < 2)
        {
            return false;
        }
    }

    if ((text[0] == '.' && text[1] == '.') || (text[0] == '!' && text[1] == '!'))
    {
        return false;
    }

    if (text[0] == '!' || text[0] == '.')
    {
        ++text;
    }

    ExecuteCommand(text);

    return true;
}

Player* ChatHandler::getSelectedPlayer()
{
    if (!m_session)
    {
        uint32 accountId = GetAccountId();
        auto itr = m_consoleSelectedPlayers.find(accountId);
        if (itr != m_consoleSelectedPlayers.end())
        {
            return sObjectMgr.GetPlayer(itr->second);
        }
        return nullptr;
    }

    ObjectGuid guid  = m_session->GetPlayer()->GetSelectionGuid();

    if (!guid)
    {
        return m_session->GetPlayer();
    }

    return sObjectMgr.GetPlayer(guid);
}

Unit* ChatHandler::getSelectedUnit()
{
    if (!m_session)
    {
        uint32 accountId = GetAccountId();
        auto itr = m_consoleSelectedPlayers.find(accountId);
        if (itr != m_consoleSelectedPlayers.end())
        {
            return sObjectMgr.GetPlayer(itr->second);
        }
        return nullptr;
    }

    ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();

    if (!guid)
    {
        return m_session->GetPlayer();
    }

    return ObjectLookup::GetUnit(*m_session->GetPlayer(), guid);
}

Creature* ChatHandler::getSelectedCreature()
{
    if (!m_session)
    {
        return nullptr;
    }

    return m_session->GetPlayer()->GetMap()->GetAnyTypeCreature(m_session->GetPlayer()->GetSelectionGuid());
}

std::string ChatHandler::GetNameLink(Player* chr) const
{
    return playerLink(chr->GetName());
}

bool ChatHandler::needReportToTarget(Player* chr) const
{
    Player* pl = m_session->GetPlayer();
    return pl != chr && pl->IsVisibleGloballyFor(chr);
}

LocaleConstant ChatHandler::GetSessionDbcLocale() const
{
    return m_session->GetSessionDbcLocale();
}

int ChatHandler::GetSessionDbLocaleIndex() const
{
    return m_session->GetSessionDbLocaleIndex();
}

const char* CliHandler::GetMangosString(int32 entry) const
{
    return sObjectMgr.GetMangosStringForDBCLocale(entry);
}

uint32 CliHandler::GetAccountId() const
{
    return m_accountId;
}

AccountTypes CliHandler::GetAccessLevel() const
{
    return m_loginAccessLevel;
}

bool CliHandler::isAvailable(ChatCommand const& cmd) const
{

    if (!cmd.AllowConsole)
    {
        return false;
    }

    return GetAccessLevel() >= (AccountTypes)cmd.SecurityLevel;
}

void CliHandler::SendSysMessage(const char* str)
{
    m_print(m_callbackArg, str);
    m_print(m_callbackArg, "\r\n");
}

std::string CliHandler::GetNameLink() const
{
    return GetMangosString(LANG_CONSOLE_COMMAND);
}

bool CliHandler::needReportToTarget(Player* ) const
{
    return true;
}

LocaleConstant CliHandler::GetSessionDbcLocale() const
{
    return sWorld.GetDefaultDbcLocale();
}

int CliHandler::GetSessionDbLocaleIndex() const
{
    return sObjectMgr.GetDBCLocaleIndex();
}

template <typename T>

void ChatHandler::ShowNpcOrGoSpawnInformation(uint32 guid)
{
    if (uint16 pool_id = sPoolMgr.IsPartOfAPool<T>(guid))
    {
        uint16 top_pool_id = sPoolMgr.IsPartOfTopPool<Pool>(pool_id);
        if (!top_pool_id || top_pool_id == pool_id)
        {
            PSendSysMessage(LANG_NPC_GO_INFO_POOL, pool_id);
        }
        else
        {
            PSendSysMessage(LANG_NPC_GO_INFO_TOP_POOL, pool_id, top_pool_id);
        }

        if (int16 event_id = sGameEventMgr.GetGameEventId<Pool>(top_pool_id))
        {
            GameEventMgr::GameEventDataMap const& events = sGameEventMgr.GetEventMap();
            GameEventData const& eventData = events[std::abs(event_id)];

            if (event_id > 0)
            {
                PSendSysMessage(LANG_NPC_GO_INFO_POOL_GAME_EVENT_S, top_pool_id, std::abs(event_id), eventData.description.c_str());
            }
            else
            {
                PSendSysMessage(LANG_NPC_GO_INFO_POOL_GAME_EVENT_D, top_pool_id, std::abs(event_id), eventData.description.c_str());
            }
        }
    }
    else if (int16 event_id = sGameEventMgr.GetGameEventId<T>(guid))
    {
        GameEventMgr::GameEventDataMap const& events = sGameEventMgr.GetEventMap();
        GameEventData const& eventData = events[std::abs(event_id)];

        if (event_id > 0)
        {
            PSendSysMessage(LANG_NPC_GO_INFO_GAME_EVENT_S, std::abs(event_id), eventData.description.c_str());
        }
        else
        {
            PSendSysMessage(LANG_NPC_GO_INFO_GAME_EVENT_D, std::abs(event_id), eventData.description.c_str());
        }
    }
}

template <typename T>

std::string ChatHandler::PrepareStringNpcOrGoSpawnInformation(uint32 guid)
{
    std::string str = "";
    if (uint16 pool_id = sPoolMgr.IsPartOfAPool<T>(guid))
    {
        uint16 top_pool_id = sPoolMgr.IsPartOfTopPool<T>(guid);
        if (int16 event_id = sGameEventMgr.GetGameEventId<Pool>(top_pool_id))
        {
            char buffer[100];
            const char* format = GetMangosString(LANG_NPC_GO_INFO_POOL_EVENT_STRING);
            sprintf(buffer, format, pool_id, event_id);
            str = buffer;
        }
        else
        {
            char buffer[100];
            const char* format = GetMangosString(LANG_NPC_GO_INFO_POOL_STRING);
            sprintf(buffer, format, pool_id);
            str = buffer;
        }
    }
    else if (int16 event_id = sGameEventMgr.GetGameEventId<T>(guid))
    {
        char buffer[100];
        const char* format = GetMangosString(LANG_NPC_GO_INFO_EVENT_STRING);
        sprintf(buffer, format, event_id);
        str = buffer;
    }

    return str;
}

void ChatHandler::LogCommand(char const* fullcmd)
{

    if (m_session)
    {
        Player* p = m_session->GetPlayer();
        ObjectGuid sel_guid = p->GetSelectionGuid();
        sLog.outCommand(GetAccountId(), "Command: %s [Player: %s (Account: %u) X: %f Y: %f Z: %f Map: %u Selected: %s]",
            fullcmd, p->GetName(), GetAccountId(), p->Where().X(), p->Where().Y(), p->Where().Z(), p->GetMapId(),
            GuidString(sel_guid).c_str());
    }
    else
    {
        sLog.outCommand(GetAccountId(), "Command: %s [Account: %u from %s]",
            fullcmd, GetAccountId(), GetAccountId() ? "RA-connection" : "Console");
    }
}

void ChatHandler::ShowFactionListHelper(FactionEntry const* factionEntry, LocaleConstant loc, FactionState const* repState , Player* target )
{
    std::string name = factionEntry->Name_lang[loc];

    std::ostringstream ss;
    if (m_session)
    {
        ss << factionEntry->ID << " - |cffffffff|Hfaction:" << factionEntry->ID << "|h[" << name << " " << localeNames[loc] << "]|h|r";
    }
    else
    {
        ss << factionEntry->ID << " - " << name << " " << localeNames[loc];
    }

    if (repState)
    {
        ReputationRank rank = target->GetReputationMgr().GetRank(factionEntry);
        std::string rankName = GetMangosString(ReputationRankStrIndex[rank]);

        ss << " " << rankName << "|h|r (" << target->GetReputationMgr().GetReputation(factionEntry) << ")";

        if (repState->Flags & FACTION_FLAG_VISIBLE)
        {
            ss << GetMangosString(LANG_FACTION_VISIBLE);
        }
        if (repState->Flags & FACTION_FLAG_AT_WAR)
        {
            ss << GetMangosString(LANG_FACTION_ATWAR);
        }
        if (repState->Flags & FACTION_FLAG_PEACE_FORCED)
        {
            ss << GetMangosString(LANG_FACTION_PEACE_FORCED);
        }
        if (repState->Flags & FACTION_FLAG_HIDDEN)
        {
            ss << GetMangosString(LANG_FACTION_HIDDEN);
        }
        if (repState->Flags & FACTION_FLAG_INVISIBLE_FORCED)
        {
            ss << GetMangosString(LANG_FACTION_INVISIBLE_FORCED);
        }
        if (repState->Flags & FACTION_FLAG_INACTIVE)
        {
            ss << GetMangosString(LANG_FACTION_INACTIVE);
        }
    }
    else if (target)
    {
        ss << GetMangosString(LANG_FACTION_NOREPUTATION);
    }

    SendSysMessage(ss.str().c_str());
}

void ChatHandler::ShowSpellListHelper(Player* target, SpellEntry const* spellInfo, LocaleConstant loc)
{
    uint32 id = spellInfo->ID;

    bool known = target && target->HasSpell(id);
    bool learn = (spellInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_LEARN_SPELL);

    uint32 talentCost = GetTalentSpellCost(id);

    bool talent = (talentCost > 0);
    bool passive = (cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive);
    bool active = target && target->HasAura(id);

    uint32 rank = talentCost ? talentCost : sSpellMgr.GetSpellRank(learn ? spellInfo->EffectTriggerSpell[EFFECT_INDEX_0] : id);

    std::ostringstream ss;
    if (m_session)
    {
        ss << id << " - |cffffffff|Hspell:" << id << "|h[" << spellInfo->Name_lang[loc];
    }
    else
    {
        ss << id << " - " << spellInfo->Name_lang[loc];
    }

    if (rank)
    {
        ss << GetMangosString(LANG_SPELL_RANK) << rank;
    }

    if (m_session)
    {
        ss << " " << localeNames[loc] << "]|h|r";
    }
    else
    {
        ss << " " << localeNames[loc];
    }

    if (talent)
    {
        ss << GetMangosString(LANG_TALENT);
    }
    if (passive)
    {
        ss << GetMangosString(LANG_PASSIVE);
    }
    if (learn)
    {
        ss << GetMangosString(LANG_LEARN);
    }
    if (known)
    {
        ss << GetMangosString(LANG_KNOWN);
    }
    if (active)
    {
        ss << GetMangosString(LANG_ACTIVE);
    }

    SendSysMessage(ss.str().c_str());
}

bool ChatHandler::ShowPlayerListHelper(QueryResult* result, uint32* limit, bool title, bool error)
{
    if (!result)
    {
        if (error)
        {
            PSendSysMessage(LANG_NO_PLAYERS_FOUND);
            SetSentErrorMessage(true);
        }
        return false;
    }

    if (!m_session && title)
    {
        SendSysMessage(LANG_CHARACTERS_LIST_BAR);
        SendSysMessage(LANG_CHARACTERS_LIST_HEADER);
        SendSysMessage(LANG_CHARACTERS_LIST_BAR);
    }

    if (result)
    {

        do
        {

            if (limit)
            {
                if (*limit == 0)
                {
                    break;
                }
                --* limit;
            }

            Field* fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            std::string name = fields[1].GetCppString();
            uint8 race = fields[2].GetUInt8();
            uint8 class_ = fields[3].GetUInt8();
            uint32 level = fields[4].GetUInt32();

            ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race);
            ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(class_);

            char const* race_name = raceEntry ? raceEntry->Name_lang[GetSessionDbcLocale()] : "<?>";
            char const* class_name = classEntry ? classEntry->Name_lang[GetSessionDbcLocale()] : "<?>";

            if (!m_session)
            {
                PSendSysMessage(LANG_CHARACTERS_LIST_LINE_CONSOLE, guid, name.c_str(), race_name, class_name, level);
            }
            else
            {
                PSendSysMessage(LANG_CHARACTERS_LIST_LINE_CHAT, guid, name.c_str(), name.c_str(), race_name, class_name, level);
            }
        } while (result->NextRow());

        delete result;
    }

    if (!m_session)
    {
        SendSysMessage(LANG_CHARACTERS_LIST_BAR);
    }

    return true;
}

template void ChatHandler::ShowNpcOrGoSpawnInformation<Creature>(uint32 guid);
template void ChatHandler::ShowNpcOrGoSpawnInformation<GameObject>(uint32 guid);

template std::string ChatHandler::PrepareStringNpcOrGoSpawnInformation<Creature>(uint32 guid);
template std::string ChatHandler::PrepareStringNpcOrGoSpawnInformation<GameObject>(uint32 guid);

bool AddAuraToPlayer(const SpellEntry* spellInfo, Unit* target, Occupant* caster)
{

    if (!spellInfo)
    {
        return false;
    }

    SpellAuraHolder* holder = CreateSpellAuraHolder(spellInfo, target, caster);

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        uint8 eff = spellInfo->Effect[i];
        if (eff >= TOTAL_SPELL_EFFECTS)
        {
            continue;
        }
        if (IsAreaAuraEffect(eff) ||
            eff == SPELL_EFFECT_APPLY_AURA ||
            eff == SPELL_EFFECT_PERSISTENT_AREA_AURA)
        {
            Aura* aur = CreateAura(spellInfo, SpellEffectIndex(i), nullptr, holder, target);
            holder->AddAura(aur, SpellEffectIndex(i));
        }
    }
    target->AddSpellAuraHolder(holder);

    return true;
}
