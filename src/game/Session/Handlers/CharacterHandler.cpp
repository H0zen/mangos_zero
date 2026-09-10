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
#include "Database/SqlOperations.h"
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <string>
#include <memory>
#include "Common/Locales.h"
#include <ctime>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "WorldSession.h"
#include "CharacterAnswers.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Mint.h"
#include "Player.h"
#include "DBCStores.h"
#include "InitialWorldEntry.h"
#include "CinematicFlyover.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "CorpseManager.h"
#include "Group.h"
#include "PlayerDump.h"
#include "SocialMgr.h"
#include "Util.h"
#include "Language.h"
#include "Chat.h"
#include "Config/Config.h"
#include "SpellMgr.h"
#include "GameTime.h"
#include "Timer.h"

enum CinematicsSkipMode
{
    CINEMATICS_SKIP_NONE      = 0,
    CINEMATICS_SKIP_SAME_RACE = 1,
    CINEMATICS_SKIP_ALL       = 2
};

class LoginQueryHolder : public SqlQueryHolder
{
    private:
        uint32 m_accountId;
        ObjectGuid m_guid = 0;
    public:
        LoginQueryHolder(uint32 accountId, ObjectGuid guid)
            : m_accountId(accountId), m_guid(guid) {}
        ObjectGuid GetGuid() const { return m_guid; }
        uint32 GetAccountId() const { return m_accountId; }
        bool Initialize();
};

bool LoginQueryHolder::Initialize()
{
    SetSize(MAX_PLAYER_LOGIN_QUERY);

    bool res = true;

    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADFROM,            "SELECT `guid`, `account`, `name`, `race`, `class`, `gender`, `level`, `xp`, `money`, `playerBytes`, `playerBytes2`, `playerFlags`,"
        "`position_x`, `position_y`, `position_z`, `map`, `orientation`, `taximask`, `cinematic`, `totaltime`, `leveltime`, `rest_bonus`, `logout_time`, `is_logout_resting`, `resettalents_cost`,"
        "`resettalents_time`, `trans_x`, `trans_y`, `trans_z`, `trans_o`, `transguid`, `extra_flags`, `stable_slots`, `at_login`, `zone`, `online`, `death_expire_time`, `taxi_path`,"
        "`honor_highest_rank`, `honor_standing`, `stored_honor_rating`, `stored_dishonorable_kills`, `stored_honorable_kills`,"
        "`watchedFaction`, `drunk`,"
        "`health`, `power1`, `power2`, `power3`, `power4`, `power5`, `exploredZones`, `equipmentCache`, `ammoId`, `actionBars`, `createdDate` FROM `characters` WHERE `guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGROUP,           "SELECT `groupId` FROM group_member WHERE `memberGuid` ='%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES,  "SELECT `id`, `permanent`, `map`, `resettime` FROM `character_instance` LEFT JOIN `instance` ON `instance` = `id` WHERE `guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADAURAS,           "SELECT `caster_guid`,`item_guid`,`spell`,`stackcount`,`remaincharges`,`basepoints0`,`basepoints1`,`basepoints2`,`periodictime0`,`periodictime1`,`periodictime2`,`maxduration`,`remaintime`,`effIndexMask` FROM `character_aura` WHERE `guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLS,          "SELECT `spell`,`active`,`disabled` FROM `character_spell` WHERE `guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,     "SELECT `quest`,`status`,`rewarded`,`explored`,`timer`,`mobcount1`,`mobcount2`,`mobcount3`,`mobcount4`,`itemcount1`,`itemcount2`,`itemcount3`,`itemcount4` FROM `character_queststatus` WHERE `guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADHONORCP,         "SELECT `victim_type`,`victim`,`honor`,`date`,`type` FROM `character_honor_cp` WHERE `used`=0 AND `guid`='%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADREPUTATION,      "SELECT `faction`,`standing`,`flags` FROM `character_reputation` WHERE `guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADINVENTORY,       "SELECT `data`,`bag`,`slot`,`item`,`item_template` FROM `character_inventory` JOIN `item_instance` ON `character_inventory`.`item` = `item_instance`.`guid` WHERE `character_inventory`.`guid` = '%u' ORDER BY `bag`,`slot`", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADITEMLOOT,        "SELECT `guid`,`itemid`,`amount`,`property` FROM `item_loot` WHERE `owner_guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADACTIONS,         "SELECT `button`,`action`,`type` FROM `character_action` WHERE `guid` = '%u' ORDER BY `button`", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSOCIALLIST,      "SELECT `friend`,`flags` FROM `character_social` WHERE `guid` = '%u' LIMIT 255", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADHOMEBIND,        "SELECT `map`,`zone`,`position_x`,`position_y`,`position_z` FROM `character_homebind` WHERE `guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,  "SELECT `spell`,`item`,`time` FROM `character_spell_cooldown` WHERE `guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGUILD,           "SELECT `guildid`,`rank` FROM `guild_member` WHERE `guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADBGDATA,          "SELECT `instance_id`, `team`, `join_x`, `join_y`, `join_z`, `join_o`, `join_map` FROM `character_battleground_data` WHERE `guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS,          "SELECT `skill`, `value`, `max` FROM `character_skills` WHERE `guid` = '%u'", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILS,           "SELECT `id`,`messageType`,`sender`,`receiver`,`subject`,`body`,`expire_time`,`deliver_time`,`money`,`cod`,`checked`,`stationery`,`mailTemplateId`,`has_items` FROM `mail` WHERE `receiver` = '%u' ORDER BY `id` DESC", GuidCounter(m_guid));
    res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,     "SELECT `data`, `mail_id`, `item_guid`, `item_template` FROM `mail_items` JOIN `item_instance` ON `item_guid` = `guid` WHERE `receiver` = '%u'", GuidCounter(m_guid));

    return res;
}

class CharacterHandler
{
    public:
        void HandleCharEnumCallback(QueryResult* result, uint32 account)
        {
            WorldSession* session = sWorld.FindSession(account);
            if (!session)
            {
                delete result;
                return;
            }
            session->HandleCharEnum(result);
        }

        void HandlePlayerLoginCallback(QueryResult * , SqlQueryHolder* holder)
        {
            if (!holder)
            {
                return;
            }
            WorldSession* session = sWorld.FindSession(((LoginQueryHolder*)holder)->GetAccountId());
            if (!session)
            {
                delete holder;
                return;
            }
            session->HandlePlayerLogin((LoginQueryHolder*)holder);
        }
} chrHandler;

void WorldSession::HandleCharEnum(QueryResult* result)
{
    WorldPacket data(SMSG_CHAR_ENUM, 100);

    CharacterEnumMapSnapshot::MapByGuid advertisedMaps;

    uint8 num = 0;

    data << num;

    if (result)
    {
        do
        {
            uint32 guidlow = (*result)[0].GetUInt32();
            uint32 advertisedMap = (*result)[9].GetUInt32();
            DETAIL_LOG("Build enum data for char guid %u from account %u.", guidlow, GetAccountId());
            if (CharacterRows::WriteCharacterList(result, &data))
            {
                ++num;
                advertisedMaps.emplace(
                    MakeGuid(HIGHGUID_PLAYER, guidlow), advertisedMap);
            }
        }
        while (result->NextRow());

        delete result;
    }

    data.put<uint8>(0, num);

    SendPacket(&data);

    m_characterEnumMaps.Replace(std::move(advertisedMaps));
}

void WorldSession::HandleCharEnumOpcode(WorldPacket & )
{

    SendPendingAddonInfo();

    uint32 accountId = GetAccountId();
    CharacterDatabase.AsyncPQuery([accountId](QueryResult* result)
                                  {
                                      chrHandler.HandleCharEnumCallback(result, accountId);
                                  },

            "SELECT `characters`.`guid`, `characters`.`name`, `characters`.`race`, `characters`.`class`, `characters`.`gender`, `characters`.`playerBytes`, `characters`.`playerBytes2`, `characters`.`level`, "

            "`characters`.`zone`, `characters`.`map`, `characters`.`position_x`, `characters`.`position_y`, `characters`.`position_z`, `guild_member`.`guildid`, `characters`.`playerFlags`, "

            "`characters`.`at_login`, `character_pet`.`entry`, `character_pet`.`modelid`, `character_pet`.`level`, `characters`.`equipmentCache` "
            "FROM `characters` LEFT JOIN `character_pet` ON `characters`.`guid`=`character_pet`.`owner` AND `character_pet`.`slot`='%u' "
            "LEFT JOIN `guild_member` ON `characters`.`guid` = `guild_member`.`guid` "
            "WHERE `characters`.`account` = '%u' ORDER BY `characters`.`guid`",
        PET_SAVE_AS_CURRENT, GetAccountId());
}

void characters::CharCreate(WorldSession& session, WorldPacket& recv_data)
{
    std::string name;
    uint8 race_, class_;

    recv_data >> name;

    recv_data >> race_;
    recv_data >> class_;

    uint8 gender, skin, face, hairStyle, hairColor, facialHair, outfitId;
    recv_data >> gender >> skin >> face;
    recv_data >> hairStyle >> hairColor >> facialHair >> outfitId;

    WorldPacket data(SMSG_CHAR_CREATE, 1);

    if (session.GetSecurity() == SEC_PLAYER)
    {
        if (uint32 mask = sWorld.getConfig(CONFIG_UINT32_CHARACTERS_CREATING_DISABLED))
        {
            bool disabled = false;

            Team team = Player::TeamForRace(race_);
            switch (team)
            {
                case ALLIANCE: disabled = mask & (1 << 0); break;
                case HORDE:    disabled = mask & (1 << 1); break;
                default: break;
            }

            if (disabled)
            {
                data << (uint8)CHAR_CREATE_DISABLED;
                session.SendPacket(&data);
                return;
            }
        }
    }

    ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(class_);
    ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race_);

    if (!classEntry || !raceEntry)
    {
        data << (uint8)CHAR_CREATE_FAILED;
        session.SendPacket(&data);
        sLog.outError("Class: %u or Race %u not found in DBC (Wrong DBC files?) or Cheater?", class_, race_);
        return;
    }

    if (!normalizePlayerName(name))
    {
        data << (uint8)CHAR_NAME_NO_NAME;
        session.SendPacket(&data);
        sLog.outError("Account:[%d] but tried to Create character with empty [name]", session.GetAccountId());
        return;
    }

    uint8 res = ObjectMgr::CheckPlayerName(name, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        data << uint8(res);
        session.SendPacket(&data);
        return;
    }

    if (session.GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(name))
    {
        data << (uint8)CHAR_NAME_RESERVED;
        session.SendPacket(&data);
        return;
    }

    if (sObjectMgr.GetPlayerGuidByName(name))
    {
        data << (uint8)CHAR_CREATE_NAME_IN_USE;
        session.SendPacket(&data);
        return;
    }

    QueryResult* resultacct = LoginDatabase.PQuery("SELECT SUM(`numchars`) FROM `realmcharacters` WHERE `acctid` = '%u'", session.GetAccountId());
    if (resultacct)
    {
        Field* fields = resultacct->Fetch();
        uint32 acctcharcount = fields[0].GetUInt32();
        delete resultacct;

        if (acctcharcount >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_ACCOUNT))
        {
            data << (uint8)CHAR_CREATE_ACCOUNT_LIMIT;
            session.SendPacket(&data);
            return;
        }
    }

    QueryResult* result = CharacterDatabase.PQuery("SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", session.GetAccountId());
    uint8 charcount = 0;
    if (result)
    {
        Field* fields = result->Fetch();
        charcount = fields[0].GetUInt8();
        delete result;

        if (charcount >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM))
        {
            data << (uint8)CHAR_CREATE_SERVER_LIMIT;
            session.SendPacket(&data);
            return;
        }
    }

    bool AllowTwoSideAccounts = !sWorld.IsPvPRealm() || sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS) || session.GetSecurity() > SEC_PLAYER;
    CinematicsSkipMode skipCinematics = CinematicsSkipMode(sWorld.getConfig(CONFIG_UINT32_SKIP_CINEMATICS));

    bool have_same_race = false;
    if (!AllowTwoSideAccounts || skipCinematics == CINEMATICS_SKIP_SAME_RACE)
    {
        QueryResult* result2 = CharacterDatabase.PQuery("SELECT `race` FROM `characters` WHERE `account` = '%u' %s",
            session.GetAccountId(), (skipCinematics == CINEMATICS_SKIP_SAME_RACE) ? "" : "LIMIT 1");
        if (result2)
        {
            Team team_ = Player::TeamForRace(race_);

            Field* field = result2->Fetch();
            uint8 acc_race  = field[0].GetUInt32();

            if (!AllowTwoSideAccounts)
            {
                if (acc_race == 0 || Player::TeamForRace(acc_race) != team_)
                {
                    data << (uint8)CHAR_CREATE_PVP_TEAMS_VIOLATION;
                    session.SendPacket(&data);
                    delete result2;
                    return;
                }
            }

            while (skipCinematics == CINEMATICS_SKIP_SAME_RACE && !have_same_race)
            {
                if (!result2->NextRow())
                {
                    break;
                }

                field = result2->Fetch();
                acc_race = field[0].GetUInt32();

                have_same_race = race_ == acc_race;
            }
            delete result2;
        }
    }

    Player* pNewChar = new Player(&session);

    uint32 createdDate = GetUnixTimeStamp();
    pNewChar->SetCreatedDate(createdDate);

    if (!pNewChar->Create(sMint.PlayerGuids().Next(), name, race_, class_, gender, skin, face, hairStyle, hairColor, facialHair, outfitId))
    {

        delete pNewChar;

        data << (uint8)CHAR_CREATE_ERROR;
        session.SendPacket(&data);

        return;
    }

    if ((have_same_race && skipCinematics == CINEMATICS_SKIP_SAME_RACE) || skipCinematics == CINEMATICS_SKIP_ALL)
    {
        pNewChar->setCinematic(1);
    }

    pNewChar->SetAtLoginFlag(AT_LOGIN_FIRST);

    pNewChar->SaveToDB();
    charcount += 1;

    LoginDatabase.PExecute("DELETE FROM `realmcharacters` WHERE `acctid`= '%u' AND `realmid`= '%u'", session.GetAccountId(), realmID);
    LoginDatabase.PExecute("INSERT INTO `realmcharacters` (`numchars`, `acctid`, `realmid`) VALUES (%u, %u, %u)",  charcount, session.GetAccountId(), realmID);

    data << (uint8)CHAR_CREATE_SUCCESS;
    session.SendPacket(&data);

    std::string IP_str = session.GetRemoteAddress();
    BASIC_LOG("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", session.GetAccountId(), IP_str.c_str(), name.c_str(), pNewChar->GetGUIDLow());
    sLog.outChar("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", session.GetAccountId(), IP_str.c_str(), name.c_str(), pNewChar->GetGUIDLow());

    delete pNewChar;
}

void characters::CharDelete(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    recv_data >> guid;

    if (sObjectMgr.GetPlayer(guid))
    {
        return;
    }

    uint32 accountId = 0;
    std::string name;

    if (sGuildMgr.GetGuildByLeader(guid))
    {
        WorldPacket data(SMSG_CHAR_DELETE, 1);
        data << uint8(CHAR_DELETE_FAILED);
        session.SendPacket(&data);
        return;
    }

    uint32 lowguid = GuidCounter(guid);

    QueryResult* result = CharacterDatabase.PQuery("SELECT `account`,`name` FROM `characters` WHERE `guid`='%u'", lowguid);
    if (result)
    {
        Field* fields = result->Fetch();
        accountId = fields[0].GetUInt32();
        name = fields[1].GetCppString();
        delete result;
    }

    if (accountId != session.GetAccountId())
    {
        return;
    }

    std::string IP_str = session.GetRemoteAddress();
    BASIC_LOG("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", session.GetAccountId(), IP_str.c_str(), name.c_str(), lowguid);
    sLog.outChar("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", session.GetAccountId(), IP_str.c_str(), name.c_str(), lowguid);

    if (sLog.IsOutCharDump())
    {
        std::string dump = PlayerDumpWriter().GetDump(lowguid);
        sLog.outCharDump(dump.c_str(), session.GetAccountId(), lowguid, name.c_str());
    }

    CharacterRows::Delete(guid, session.GetAccountId());

    WorldPacket data(SMSG_CHAR_DELETE, 1);
    data << (uint8)CHAR_DELETE_SUCCESS;
    session.SendPacket(&data);
}

void characters::PlayerLogin(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid playerGuid = 0;
    recv_data >> playerGuid;

    if (session.PlayerLoading() || session.GetPlayer() != nullptr)
    {
        sLog.outError("Player tryes to login again, AccountId = %d", session.GetAccountId());
        return;
    }

    session.SendPendingAddonInfo();

    session.SetPlayerLoading(true);

    DEBUG_LOG("WORLD: Received opcode Player Logon Message");

    LoginQueryHolder* holder = new LoginQueryHolder(session.GetAccountId(), playerGuid);
    if (!holder->Initialize())
    {
        delete holder;
        session.SetPlayerLoading(false);
        return;
    }

    CharacterDatabase.DelayQueryHolder([](QueryResult* result, SqlQueryHolder* h)
                                       {
                                           chrHandler.HandlePlayerLoginCallback(result, h);
                                       }, holder);
}

void WorldSession::HandlePlayerLogin(LoginQueryHolder* holder)
{

    ObjectGuid playerGuid = holder->GetGuid();

    Player* pCurrChar = new Player(this);

    pCurrChar->GetMotionMaster()->Initialize();

    if (!pCurrChar->LoadFromDB(playerGuid, holder))
    {

        KickPlayer();

        delete pCurrChar;
        delete holder;

        m_playerLoading = false;

        return;
    }

    SetPlayer(pCurrChar);

    WorldPacket data;
    uint32 anchorMapId = 0;
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    float anchorZ = 0.0f;
    pCurrChar->GetWorldAnchor(anchorMapId, anchorX, anchorY, anchorZ);

    LoginVerifyDeliveryState loginVerifyDelivery;
    auto sendLoginVerifyWorld = [&]()
    {
        data.Initialize(SMSG_LOGIN_VERIFY_WORLD, 20);
        data << pCurrChar->GetMapId();

        if (pCurrChar->GetTransport())
        {
            Position const* aboard = pCurrChar->m_movementInfo.GetTransportPos();
            data << aboard->x << aboard->y << aboard->z << aboard->o;
        }
        else
        {
            data << pCurrChar->Where().X();
            data << pCurrChar->Where().Y();
            data << pCurrChar->Where().Z();
            data << pCurrChar->Where().Facing();
        }

        SendPacket(&data);
    };

    if (loginVerifyDelivery.TakeInitial(
            HasMatchingCharacterEnumMap(playerGuid, anchorMapId)))
    {
        sendLoginVerifyWorld();
    }

    data.Initialize(SMSG_ACCOUNT_DATA_TIMES, 128);
    for (int i = 0; i < 32; ++i)
    {
        data << uint32(0);
    }
    SendPacket(&data);

    pCurrChar->GetSocial()->SendFriendList(pCurrChar);
    pCurrChar->GetSocial()->SendIgnoreList(pCurrChar);

    {
        uint32 linecount = 0;

        std::string str_motd = sWorld.GetMotd();

        std::string::size_type pos = 0, nextpos;

        while ((nextpos = str_motd.find('@', pos)) != std::string::npos)
        {

            if (nextpos != pos)
            {

                ChatHandler(pCurrChar).PSendSysMessage("%s", str_motd.substr(pos, nextpos - pos).c_str());
                ++linecount;
            }
            pos = nextpos + 1;
        }

        if (pos < str_motd.length())
        {
            ChatHandler(pCurrChar).PSendSysMessage("%s", str_motd.substr(pos).c_str());
        }
        DEBUG_LOG("WORLD: Sent motd (SMSG_MOTD)");
    }

    if (QueryResult *resultGuild = holder->GetResult(PLAYER_LOGIN_QUERY_LOADGUILD))
    {

        Field* fields = resultGuild->Fetch();
        pCurrChar->SetInGuild(fields[0].GetUInt32());
        pCurrChar->SetRank(fields[1].GetUInt32());

        delete resultGuild;
    }

    else if (pCurrChar->GetGuildId())
    {
        pCurrChar->SetInGuild(0);
        pCurrChar->SetRank(0);
    }

    if (pCurrChar->GetGuildId() != 0)
    {

        Guild* guild = sGuildMgr.GetGuildById(pCurrChar->GetGuildId());

        if (guild)
        {

            data.Initialize(SMSG_GUILD_EVENT, (1 + 1 + guild->GetMOTD().size() + 1));
            data << uint8(GE_MOTD);
            data << uint8(1);
            data << guild->GetMOTD();
            SendPacket(&data);
            DEBUG_LOG("WORLD: Sent guild-motd (SMSG_GUILD_EVENT)");

            guild->BroadcastEvent(GE_SIGNED_ON, pCurrChar->GetObjectGuid(), pCurrChar->GetName());
        }

        else
        {
            sLog.outError("Player %s (GUID: %u) marked as member of nonexistent guild (id: %u), removing guild membership for player.",
                pCurrChar->GetName(),
                pCurrChar->GetGUIDLow(),
                pCurrChar->GetGuildId());

            pCurrChar->SetInGuild(0);
        }
    }

    if (!pCurrChar->IsAlive())
    {
        pCurrChar->SendCorpseReclaimDelay(true);
    }

    uint32 cinematicSequenceId = 0;

    if (!pCurrChar->getCinematic())
    {
        if (ChrRacesEntry const* race =
                sChrRacesStore.LookupEntry(pCurrChar->getRace()))
        {
            if (race->CinematicSequence &&
                sCinematicSequencesStore.LookupEntry(race->CinematicSequence))
            {
                cinematicSequenceId = race->CinematicSequence;
            }
        }
    }

    InitialWorldEntryHook initialEntry(cinematicSequenceId);

    pCurrChar->SendInitialPacketsBeforeAddToMap(true);

    uint32 miscRequirement = 0;
    AreaLockStatus lockStatus = AREA_LOCKSTATUS_OK;
    if (AreaTrigger const* at = sObjectMgr.GetMapEntranceTrigger(pCurrChar->GetMapId()))
    {
        lockStatus = pCurrChar->GetAreaTriggerLockStatus(at, miscRequirement);
    }
    else
    {

        MapEntry const* mapEntry = sMapStore.LookupEntry(pCurrChar->GetMapId());
        if (!mapEntry)
        {
            lockStatus = AREA_LOCKSTATUS_UNKNOWN_ERROR;
        }
    }

    if (lockStatus != AREA_LOCKSTATUS_OK ||
        !pCurrChar->BoardingMap()->Add(pCurrChar, &initialEntry))
    {

        if (loginVerifyDelivery.TakeAdmissionFallback())
        {
            sendLoginVerifyWorld();
        }

        AreaTrigger const* at = sObjectMgr.GetGoBackTrigger(pCurrChar->GetMapId());
        if (at)
        {
            lockStatus = pCurrChar->GetAreaTriggerLockStatus(at, miscRequirement);
        }

        if (!at || lockStatus != AREA_LOCKSTATUS_OK || !pCurrChar->TeleportTo(at->target_mapId, at->target_X, at->target_Y, at->target_Z, pCurrChar->Where().Facing()))
        {
            pCurrChar->TeleportToHomebind();
        }
    }

    InitialWorldEntryContext const* entryContext = initialEntry.GetContext();

    if (!entryContext && !pCurrChar->IsBeingTeleportedFar())
    {
        pCurrChar->SendLoginTimeSpeed();
    }

    sPlayerRegistry.Add(pCurrChar);
    DEBUG_LOG("Player %s added to map %i", pCurrChar->GetName(), pCurrChar->GetMapId());

    pCurrChar->SendInitialPacketsAfterAddToMap(entryContext);

    if (entryContext && entryContext->cinematicStarted &&
        sConfig.GetBoolDefault("Cinematic.Flyover.Enable", false))
    {
        pCurrChar->SetCinematicFlyover(
            std::make_unique<CinematicFlyover>(pCurrChar,
                                               pCurrChar->getRace()));
    }

    static SqlStatementID updChars;
    static SqlStatementID updAccount;

    SqlStatement stmt = CharacterDatabase.CreateStatement(updChars, "UPDATE `characters` SET `online` = 1 WHERE `guid` = ?");
    stmt.PExecute(pCurrChar->GetGUIDLow());

        stmt = LoginDatabase.CreateStatement(updAccount, "UPDATE `account` SET `active_realm_id` = ? WHERE `id` = ?");
        stmt.PExecute(realmID, GetAccountId());

    pCurrChar->SetInGameTime(GameTime::GetGameTimeMS());

    if (Group* group = pCurrChar->GetGroup())
    {
        group->SendUpdate();
    }

    sSocialMgr.SendFriendStatus(pCurrChar, FRIEND_ONLINE, pCurrChar->GetObjectGuid(), true);

    pCurrChar->LoadCorpse();

    if (pCurrChar->m_deathState != ALIVE)
    {

        if (pCurrChar->getRace() == RACE_NIGHTELF)
        {
            pCurrChar->CastSpell(pCurrChar, 20584, true);
        }

        pCurrChar->CastSpell(pCurrChar, 8326, true);

        pCurrChar->SetWaterWalk(true);
    }

    pCurrChar->ContinueTaxiFlight();

    pCurrChar->LoadPet();

    if (sWorld.IsFFAPvPRealm() && !pCurrChar->isGameMaster() && !pCurrChar->HasPlayerFlag(PLAYER_FLAGS_RESTING))
    {
        pCurrChar->SetFFAPvP(true);
    }

    if (pCurrChar->HasPlayerFlag(PLAYER_FLAGS_CONTESTED_PVP))
    {
        pCurrChar->SetContestedPvP();
    }

    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_SPELLS))
    {
        pCurrChar->resetSpells();
        SendNotification(LANG_RESET_SPELLS);
    }

    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_RESET_TALENTS))
    {
        pCurrChar->resetTalents(true);
        SendNotification(LANG_RESET_TALENTS);
    }

    if (pCurrChar->HasAtLoginFlag(AT_LOGIN_FIRST))
    {
        pCurrChar->RemoveAtLoginFlag(AT_LOGIN_FIRST);
    }

    if (sWorld.IsShutdowning())
    {
        sWorld.ShutdownMsg(true, pCurrChar);
    }

    if (sWorld.getConfig(CONFIG_BOOL_ALL_TAXI_PATHS))
    {
        pCurrChar->SetTaxiCheater(true);
    }

    if (pCurrChar->isGameMaster())
    {
        SendNotification(LANG_GM_ON);
    }

    if (!pCurrChar->isGMVisible())
    {
        SendNotification(LANG_INVISIBLE_INVISIBLE);
        SpellEntry const* invisibleAuraInfo = sSpellStore.LookupEntry(sWorld.getConfig(CONFIG_UINT32_GM_INVISIBLE_AURA));
        if (invisibleAuraInfo && IsSpellAppliesAura(invisibleAuraInfo))
        {
            pCurrChar->CastSpell(pCurrChar, invisibleAuraInfo, true);
        }
    }

    std::string IP_str = GetRemoteAddress();
    sLog.outChar("Account: %d (IP: %s) Login Character:[%s] (guid: %u)",
        GetAccountId(), IP_str.c_str(), pCurrChar->GetName(), pCurrChar->GetGUIDLow());

    if (!pCurrChar->IsStandState() && !pCurrChar->hasUnitState(UNIT_STAT_STUNNED))
    {
        pCurrChar->SetStandState(UNIT_STAND_STATE_STAND);
    }

    m_playerLoading = false;

    m_clientTimeDelay = 0;

    pCurrChar->lastTimeLooted = time(nullptr);

    delete holder;
}

void characters::SetFactionAtWar(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_FACTION_ATWAR");

    uint32 repListID;
    uint8  flag;

    recv_data >> repListID;
    recv_data >> flag;

    who.GetReputationMgr().SetAtWar(repListID, flag);
}

void characters::TutorialFlag(WorldSession& session, WorldPacket& recv_data)
{
    uint32 iFlag;
    recv_data >> iFlag;

    uint32 wInt = (iFlag / 32);
    if (wInt >= 8)
    {

        return;
    }
    uint32 rInt = (iFlag % 32);

    uint32 tutflag = session.GetTutorialInt(wInt);
    tutflag |= (1 << rInt);
    session.SetTutorialInt(wInt, tutflag);

}

void WorldSession::HandleTutorialClearOpcode(WorldPacket & )
{
    for (int i = 0; i < 8; ++i)
    {
        SetTutorialInt(i, 0xFFFFFFFF);
    }
}

void WorldSession::HandleTutorialResetOpcode(WorldPacket & )
{
    for (int i = 0; i < 8; ++i)
    {
        SetTutorialInt(i, 0x00000000);
    }
}

void characters::SetWatchedFaction(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_WATCHED_FACTION");
    int32 repId;
    recv_data >> repId;
    who.SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, repId);
}

void characters::SetFactionInactive(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_FACTION_INACTIVE");
    uint32 replistid;
    uint8 inactive;
    recv_data >> replistid >> inactive;

    who.GetReputationMgr().SetInactive(replistid, inactive);
}

void WorldSession::HandleShowingHelmOpcode(WorldPacket & )
{
    DEBUG_LOG("CMSG_SHOWING_HELM for %s", _player->GetName());
    _player->TogglePlayerFlag(PLAYER_FLAGS_HIDE_HELM);
}

void WorldSession::HandleShowingCloakOpcode(WorldPacket & )
{
    DEBUG_LOG("CMSG_SHOWING_CLOAK for %s", _player->GetName());
    _player->TogglePlayerFlag(PLAYER_FLAGS_HIDE_CLOAK);
}
