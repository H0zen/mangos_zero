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

#include <unordered_map>
#include "LockedQueue/LockedQueue.h"
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include "Common/Locales.h"
#include <cstring>
#include <ctime>
#include <string>
#include <map>
#include "ScheduledExit.h"
#include "Utilities/Util.h"
#include "Timer.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include <set>
#include <list>
#include <vector>
#include <atomic>

class Object;
#include "ObjectGuid.h"
class WorldPacket;
class WorldSession;
class Player;
class SqlResultQueue;
class QueryResult;

enum ServerMessageType
{
    SERVER_MSG_SHUTDOWN_TIME          = 1,
    SERVER_MSG_RESTART_TIME           = 2,
    SERVER_MSG_CUSTOM                 = 3,
    SERVER_MSG_SHUTDOWN_CANCELLED     = 4,
    SERVER_MSG_RESTART_CANCELLED      = 5,
};

enum ShutdownMask
{
    SHUTDOWN_MASK_RESTART = 1,
    SHUTDOWN_MASK_STOP    = 2,
    SHUTDOWN_MASK_IDLE    = 4,
};

enum ShutdownExitCode
{
    SHUTDOWN_EXIT_CODE = 0,
    ERROR_EXIT_CODE    = 1,
    RESTART_EXIT_CODE  = 2,
};

enum WorldTimers
{
    WUPDATE_AUCTIONS = 0,
    WUPDATE_UPTIME,
    WUPDATE_CORPSES,
    WUPDATE_EVENTS,
    WUPDATE_DELETECHARS,
    WUPDATE_METRICS,
    WUPDATE_COUNT
};

enum eConfigUInt32Values
{
    CONFIG_UINT32_COMPRESSION = 0,
    CONFIG_UINT32_INTERVAL_SAVE,
    CONFIG_UINT32_INTERVAL_GRIDCLEAN,
    CONFIG_UINT32_INTERVAL_MAPUPDATE,
    CONFIG_UINT32_INTERVAL_CHANGEWEATHER,
    CONFIG_UINT32_PORT_WORLD,
    CONFIG_UINT32_GAME_TYPE,
    CONFIG_UINT32_REALM_ZONE,
    CONFIG_UINT32_STRICT_PLAYER_NAMES,
    CONFIG_UINT32_STRICT_CHARTER_NAMES,
    CONFIG_UINT32_STRICT_PET_NAMES,
    CONFIG_UINT32_MIN_PLAYER_NAME,
    CONFIG_UINT32_MIN_CHARTER_NAME,
    CONFIG_UINT32_MIN_PET_NAME,
    CONFIG_UINT32_CHARACTERS_CREATING_DISABLED,
    CONFIG_UINT32_CHARACTERS_PER_ACCOUNT,
    CONFIG_UINT32_CHARACTERS_PER_REALM,
    CONFIG_UINT32_SKIP_CINEMATICS,
    CONFIG_UINT32_MAX_PLAYER_LEVEL,
    CONFIG_UINT32_START_PLAYER_LEVEL,
    CONFIG_UINT32_START_PLAYER_MONEY,
    CONFIG_UINT32_MAX_HONOR_POINTS,
    CONFIG_UINT32_START_HONOR_POINTS,
    CONFIG_UINT32_MIN_HONOR_KILLS,
    CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR,
    CONFIG_UINT32_INSTANCE_UNLOAD_DELAY,
    CONFIG_UINT32_MAX_SPELL_CASTS_IN_CHAIN,
    CONFIG_UINT32_MIN_TRAIN_MOUNT_LEVEL,
    CONFIG_UINT32_MOUNT_COST,
    CONFIG_UINT32_TRAIN_MOUNT_COST,
    CONFIG_UINT32_EPIC_MOUNT_COST,
    CONFIG_UINT32_MIN_TRAIN_EPIC_MOUNT_LEVEL,
    CONFIG_UINT32_TRAIN_EPIC_MOUNT_COST,
    CONFIG_UINT32_RABBIT_DAY,
    CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL,
    CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT,
    CONFIG_UINT32_TRADE_SKILL_GMIGNORE_LEVEL,
    CONFIG_UINT32_TRADE_SKILL_GMIGNORE_SKILL,
    CONFIG_UINT32_MIN_PETITION_SIGNS,
    CONFIG_UINT32_GM_LOGIN_STATE,
    CONFIG_UINT32_GM_VISIBLE_STATE,
    CONFIG_UINT32_GM_ACCEPT_TICKETS,
    CONFIG_UINT32_GM_TICKET_LIST_SIZE,
    CONFIG_UINT32_GM_CHAT,
    CONFIG_UINT32_GM_WISPERING_TO,
    CONFIG_UINT32_GM_LEVEL_IN_GM_LIST,
    CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST,
    CONFIG_UINT32_START_GM_LEVEL,
    CONFIG_UINT32_GM_INVISIBLE_AURA,
    CONFIG_UINT32_GM_MAX_SPEED_FACTOR,
    CONFIG_UINT32_GROUP_VISIBILITY,
    CONFIG_UINT32_MAIL_DELIVERY_DELAY,
    CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK,
    CONFIG_UINT32_UPTIME_UPDATE,
    CONFIG_UINT32_AUCTION_DEPOSIT_MIN,
    CONFIG_UINT32_RATE_MINING_LOWER,
    CONFIG_UINT32_RATE_MINING_RARE,
    CONFIG_UINT32_RATE_MINING_DARKIRON,
    CONFIG_UINT32_RATE_MINING_AUTOPOOLING,
    CONFIG_UINT32_SKILL_CHANCE_ORANGE,
    CONFIG_UINT32_SKILL_CHANCE_YELLOW,
    CONFIG_UINT32_SKILL_CHANCE_GREEN,
    CONFIG_UINT32_SKILL_CHANCE_GREY,
    CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS,
    CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS,
    CONFIG_UINT32_SKILL_GAIN_CRAFTING,
    CONFIG_UINT32_SKILL_GAIN_DEFENSE,
    CONFIG_UINT32_SKILL_GAIN_GATHERING,
    CONFIG_UINT32_SKILL_GAIN_WEAPON,
    CONFIG_UINT32_MAX_OVERSPEED_PINGS,
    CONFIG_UINT32_CHATFLOOD_MESSAGE_COUNT,
    CONFIG_UINT32_CHATFLOOD_MESSAGE_DELAY,
    CONFIG_UINT32_CHATFLOOD_MUTE_TIME,
    CONFIG_UINT32_CREATURE_FAMILY_ASSISTANCE_DELAY,
    CONFIG_UINT32_CREATURE_FAMILY_FLEE_DELAY,
    CONFIG_UINT32_WORLD_BOSS_LEVEL_DIFF,
    CONFIG_UINT32_LIVINGWORLD_ANCHOR_MASK,
    CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY,
    CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK,
    CONFIG_UINT32_CORPSE_DECAY_NORMAL,
    CONFIG_UINT32_CORPSE_DECAY_RARE,
    CONFIG_UINT32_CORPSE_DECAY_ELITE,
    CONFIG_UINT32_CORPSE_DECAY_RAREELITE,
    CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS,
    CONFIG_UINT32_INSTANT_LOGOUT,
    CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE,
    CONFIG_UINT32_BATTLEGROUND_PREMATURE_FINISH_TIMER,
    CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH,
    CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN,
    CONFIG_UNIT32_GUILD_PETITION_COST,
    CONFIG_UINT32_GUILD_EVENT_LOG_COUNT,
    CONFIG_UINT32_TIMERBAR_FATIGUE_GMLEVEL,
    CONFIG_UINT32_TIMERBAR_FATIGUE_MAX,
    CONFIG_UINT32_TIMERBAR_BREATH_GMLEVEL,
    CONFIG_UINT32_TIMERBAR_BREATH_MAX,
    CONFIG_UINT32_TIMERBAR_FIRE_GMLEVEL,
    CONFIG_UINT32_TIMERBAR_FIRE_MAX,
    CONFIG_UINT32_MIN_LEVEL_STAT_SAVE,
    CONFIG_UINT32_MAINTENANCE_DAY,
    CONFIG_UINT32_CHARDELETE_KEEP_DAYS,
    CONFIG_UINT32_CHARDELETE_METHOD,
    CONFIG_UINT32_CHARDELETE_MIN_LEVEL,
    CONFIG_UINT32_NUMTHREADS,
    CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE,
    CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT,
    CONFIG_UINT32_CREATURE_RESPAWN_AGGRO_DELAY,
    CONFIG_UINT32_MAX_WHOLIST_RETURNS,
    CONFIG_UINT32_LOG_WHISPERS,
    CONFIG_UINT32_AUTOBROADCAST_INTERVAL,
    CONFIG_UINT32_VALUE_COUNT
};

enum eConfigInt32Values
{
    CONFIG_INT32_DEATH_SICKNESS_LEVEL = 0,
    CONFIG_INT32_QUEST_LOW_LEVEL_HIDE_DIFF,
    CONFIG_INT32_QUEST_HIGH_LEVEL_HIDE_DIFF,
    CONFIG_INT32_VALUE_COUNT
};

enum eConfigFloatValues
{
    CONFIG_FLOAT_RATE_HEALTH = 0,
    CONFIG_FLOAT_RATE_POWER_MANA,
    CONFIG_FLOAT_RATE_POWER_RAGE_INCOME,
    CONFIG_FLOAT_RATE_POWER_RAGE_LOSS,
    CONFIG_FLOAT_RATE_POWER_FOCUS,
    CONFIG_FLOAT_RATE_POWER_ENERGY,
    CONFIG_FLOAT_RATE_SKILL_DISCOVERY,
    CONFIG_FLOAT_RATE_DROP_ITEM_POOR,
    CONFIG_FLOAT_RATE_DROP_ITEM_NORMAL,
    CONFIG_FLOAT_RATE_DROP_ITEM_UNCOMMON,
    CONFIG_FLOAT_RATE_DROP_ITEM_RARE,
    CONFIG_FLOAT_RATE_DROP_ITEM_EPIC,
    CONFIG_FLOAT_RATE_DROP_ITEM_LEGENDARY,
    CONFIG_FLOAT_RATE_DROP_ITEM_ARTIFACT,
    CONFIG_FLOAT_RATE_DROP_ITEM_REFERENCED,
    CONFIG_FLOAT_RATE_DROP_MONEY,
    CONFIG_FLOAT_RATE_XP_KILL,
    CONFIG_FLOAT_RATE_XP_PETKILL,
    CONFIG_FLOAT_RATE_XP_QUEST,
    CONFIG_FLOAT_RATE_XP_EXPLORE,
    CONFIG_FLOAT_RATE_REPUTATION_GAIN,
    CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_KILL,
    CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_QUEST,
    CONFIG_FLOAT_RATE_CREATURE_NORMAL_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_HP,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_HP,
    CONFIG_FLOAT_RATE_CREATURE_NORMAL_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_DAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_NORMAL_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_SPELLDAMAGE,
    CONFIG_FLOAT_RATE_CREATURE_AGGRO,
    CONFIG_FLOAT_RATE_REST_INGAME,
    CONFIG_FLOAT_RATE_REST_OFFLINE_IN_TAVERN_OR_CITY,
    CONFIG_FLOAT_RATE_REST_OFFLINE_IN_WILDERNESS,
    CONFIG_FLOAT_RATE_DAMAGE_FALL,
    CONFIG_FLOAT_RATE_AUCTION_TIME,
    CONFIG_FLOAT_RATE_AUCTION_DEPOSIT,
    CONFIG_FLOAT_RATE_AUCTION_CUT,
    CONFIG_FLOAT_RATE_HONOR,
    CONFIG_FLOAT_RATE_MINING_AMOUNT,
    CONFIG_FLOAT_RATE_MINING_NEXT,
    CONFIG_FLOAT_RATE_TALENT,
    CONFIG_FLOAT_RATE_LOYALTY,
    CONFIG_FLOAT_RATE_CORPSE_DECAY_LOOTED,
    CONFIG_FLOAT_RATE_INSTANCE_RESET_TIME,
    CONFIG_FLOAT_RATE_TARGET_POS_RECALCULATION_RANGE,
    CONFIG_FLOAT_RATE_DURABILITY_LOSS_DAMAGE,
    CONFIG_FLOAT_RATE_DURABILITY_LOSS_PARRY,
    CONFIG_FLOAT_RATE_DURABILITY_LOSS_ABSORB,
    CONFIG_FLOAT_RATE_DURABILITY_LOSS_BLOCK,
    CONFIG_FLOAT_SIGHT_GUARDER,
    CONFIG_FLOAT_SIGHT_MONSTER,
    CONFIG_FLOAT_LISTEN_RANGE_SAY,
    CONFIG_FLOAT_LISTEN_RANGE_YELL,
    CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE,
    CONFIG_FLOAT_CREATURE_FAMILY_FLEE_ASSISTANCE_RADIUS,
    CONFIG_FLOAT_CREATURE_FAMILY_ASSISTANCE_RADIUS,
    CONFIG_FLOAT_GROUP_XP_DISTANCE,
    CONFIG_FLOAT_THREAT_RADIUS,
    CONFIG_FLOAT_GHOST_RUN_SPEED_WORLD,
    CONFIG_FLOAT_GHOST_RUN_SPEED_BG,
    CONFIG_FLOAT_VALUE_COUNT
};

enum eConfigBoolValues
{
    CONFIG_BOOL_GRID_UNLOAD = 0,
    CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY,
    CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_TRADE,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION,
    CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_MAIL,
    CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST,
    CONFIG_BOOL_ALLOW_TWO_SIDE_ADD_FRIEND,
    CONFIG_BOOL_INSTANCE_IGNORE_LEVEL,
    CONFIG_BOOL_INSTANCE_IGNORE_RAID,
    CONFIG_BOOL_CAST_UNSTUCK,
    CONFIG_BOOL_GM_LOG_TRADE,
    CONFIG_BOOL_GM_LOWER_SECURITY,
    CONFIG_BOOL_ALWAYS_MAX_SKILL_FOR_LEVEL,
    CONFIG_BOOL_WEATHER,
    CONFIG_BOOL_EVENT_ANNOUNCE,
    CONFIG_BOOL_QUEST_IGNORE_RAID,
    CONFIG_BOOL_DETECT_POS_COLLISION,
    CONFIG_BOOL_RESTRICTED_LFG_CHANNEL,
    CONFIG_BOOL_SILENTLY_GM_JOIN_TO_CHANNEL,
    CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING,
    CONFIG_BOOL_CHAT_STRICT_LINK_CHECKING_SEVERITY,
    CONFIG_BOOL_CHAT_STRICT_LINK_CHECKING_KICK,
    CONFIG_BOOL_ADDON_CHANNEL,
    CONFIG_BOOL_CORPSE_EMPTY_LOOT_SHOW,
    CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP,
    CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE,
    CONFIG_BOOL_DEATH_BONES_WORLD,
    CONFIG_BOOL_DEATH_BONES_BG,
    CONFIG_BOOL_ALL_TAXI_PATHS,
    CONFIG_BOOL_INSTANT_TAXI,
    CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING,
    CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING,
    CONFIG_BOOL_SKILL_FAIL_POSSIBLE_FISHINGPOOL,
    CONFIG_BOOL_BATTLEGROUND_CAST_DESERTER,
    CONFIG_BOOL_BATTLEGROUND_QUEUE_ANNOUNCER_START,
    CONFIG_BOOL_BATTLEGROUND_SCORE_STATISTICS,
    CONFIG_BOOL_OUTDOORPVP_SI_ENABLED,
    CONFIG_BOOL_OUTDOORPVP_EP_ENABLED,
    CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET,
    CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT,
    CONFIG_BOOL_CLEAN_CHARACTER_DB,
    CONFIG_BOOL_VMAP_INDOOR_CHECK,
    CONFIG_BOOL_PET_UNSUMMON_AT_MOUNT,
    CONFIG_BOOL_MMAP_ENABLED,
    CONFIG_BOOL_PLAYER_COMMANDS,
    CONFIG_BOOL_AUTOPOOLING_MINING_ENABLE,
    CONFIG_BOOL_ENABLE_QUEST_TRACKER,
    CONFIG_BOOL_GM_TICKET_OFFLINE_CLOSING,

    CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW_ENABLED,
    CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW,

    CONFIG_BOOL_VALUE_COUNT
};

enum RealmType
{
    REALM_TYPE_NORMAL   = 0,
    REALM_TYPE_PVP      = 1,
    REALM_TYPE_NORMAL2  = 4,
    REALM_TYPE_RP       = 6,
    REALM_TYPE_RPPVP    = 8,
    REALM_TYPE_FFA_PVP  = 16

};

enum RealmZone
{
    REALM_ZONE_UNKNOWN       = 0,
    REALM_ZONE_DEVELOPMENT   = 1,
    REALM_ZONE_UNITED_STATES = 2,
    REALM_ZONE_OCEANIC       = 3,
    REALM_ZONE_LATIN_AMERICA = 4,
    REALM_ZONE_TOURNAMENT_5  = 5,
    REALM_ZONE_KOREA         = 6,
    REALM_ZONE_TOURNAMENT_7  = 7,
    REALM_ZONE_ENGLISH       = 8,
    REALM_ZONE_GERMAN        = 9,
    REALM_ZONE_FRENCH        = 10,
    REALM_ZONE_SPANISH       = 11,
    REALM_ZONE_RUSSIAN       = 12,
    REALM_ZONE_TOURNAMENT_13 = 13,
    REALM_ZONE_TAIWAN        = 14,
    REALM_ZONE_TOURNAMENT_15 = 15,
    REALM_ZONE_CHINA         = 16,
    REALM_ZONE_CN1           = 17,
    REALM_ZONE_CN2           = 18,
    REALM_ZONE_CN3           = 19,
    REALM_ZONE_CN4           = 20,
    REALM_ZONE_CN5           = 21,
    REALM_ZONE_CN6           = 22,
    REALM_ZONE_CN7           = 23,
    REALM_ZONE_CN8           = 24,
    REALM_ZONE_TOURNAMENT_25 = 25,
    REALM_ZONE_TEST_SERVER   = 26,
    REALM_ZONE_TOURNAMENT_27 = 27,
    REALM_ZONE_QA_SERVER     = 28,
    REALM_ZONE_CN9           = 29
};

struct CliCommandHolder
{
    typedef void Print(void*, const char*);
    typedef void CommandFinished(void*, bool success);

    uint32 m_cliAccountId;
    AccountTypes m_cliAccessLevel;
    void* m_callbackArg;
    char* m_command;
    Print* m_print;
    CommandFinished* m_commandFinished;

    CliCommandHolder(uint32 accountId, AccountTypes cliAccessLevel, void* callbackArg, const char* command, Print* zprint, CommandFinished* commandFinished)
        : m_cliAccountId(accountId), m_cliAccessLevel(cliAccessLevel), m_callbackArg(callbackArg), m_print(zprint), m_commandFinished(commandFinished)
    {
        size_t len = strlen(command) + 1;
        m_command = new char[len];
        memcpy(m_command, command, len);
    }

    ~CliCommandHolder()
    {
        delete[] m_command;
    }
};

typedef std::unordered_map<uint32, WorldSession*> SessionMap;

class World
{
    public:
        static std::atomic<uint32> m_worldLoopCounter;

        World();
        ~World();

        void CleanupsBeforeStop();

        WorldSession* FindSession(uint32 id) const;
        void AddSession(WorldSession* s);
        bool RemoveSession(uint32 id);

        void UpdateMaxSessionCounters();
        const SessionMap& GetAllSessions() const { return m_sessions; }
        uint32 GetActiveAndQueuedSessionCount() const { return m_sessions.size(); }
        uint32 GetActiveSessionCount() const { return m_sessions.size() - m_QueuedSessions.size(); }
        uint32 GetQueuedSessionCount() const { return m_QueuedSessions.size(); }

        uint32 GetMaxQueuedSessionCount() const { return m_maxQueuedSessionCount; }
        uint32 GetMaxActiveSessionCount() const { return m_maxActiveSessionCount; }

        uint32 GetPlayerAmountLimit() const { return m_playerLimit >= 0 ? m_playerLimit : 0; }
        AccountTypes GetPlayerSecurityLimit() const { return m_playerLimit <= 0 ? AccountTypes(-m_playerLimit) : SEC_PLAYER; }

        void SetPlayerLimit(int32 limit, bool needUpdate = false);

        typedef std::list<WorldSession*> Queue;
        void AddQueuedSession(WorldSession*);
        bool RemoveQueuedSession(WorldSession* session);
        int32 GetQueuedSessionPos(WorldSession*);

        bool getAllowMovement() const { return m_allowMovement; }

        void SetAllowMovement(bool allow) { m_allowMovement = allow; }

        void SetMotd(const std::string& motd) { m_motd = motd; }

        const char* GetMotd() const { return m_motd.c_str(); }
        void showFooter(uint32 startupMs);

        LocaleConstant GetDefaultDbcLocale() const { return m_defaultDbcLocale; }

        std::string GetDataPath() const { return m_dataPath; }

        time_t const& GetStartTime() const { return m_startTime; }

        time_t const& GetGameTime() const { return m_gameTime; }

        uint32 GetUptime() const { return uint32(m_gameTime - m_startTime); }

        uint32 GetDateByLocalTime(const std::tm now) const { return uint32(now.tm_year*365 + (now.tm_year-1)/4 + now.tm_yday); }
        uint32 GetDateToday() const {   return GetDateByLocalTime(safe_localtime(m_gameTime)); }
        uint32 GetDateThisWeekBegin() const {   return GetDateToday() - safe_localtime(m_gameTime).tm_wday; }
        uint32 GetDateLastMaintenanceDay() const
        {
            uint32 today = GetDateToday();
            uint32 mDay  = getConfig(CONFIG_UINT32_MAINTENANCE_DAY);
            std::tm date     = safe_localtime(m_gameTime);

            return today - ((date.tm_wday - mDay  + 7) % 7);
        }

        uint16 GetConfigMaxSkillValue() const
        {
            uint32 lvl = getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
            return lvl > 60 ? 300 + ((lvl - 60) * 75) / 10 : lvl * 5;
        }

        void SetInitialWorldSettings();
        void LoadConfigSettings(bool reload = false);

        void SendWorldText(int32 string_id, ...);
        void SendGlobalMessage(WorldPacket* packet, AccountTypes minSec = SEC_PLAYER);
        void SendServerMessage(ServerMessageType type, const char* text = "", Player* player = nullptr);
        void SendZoneUnderAttackMessage(uint32 zoneId, Team team);
        void SendDefenseMessage(uint32 zoneId, int32 textId);

        bool IsShutdowning() const { return m_ShutdownTimer > 0; }
        void ShutdownServ(uint32 time, uint32 options, uint8 exitcode);
        void ShutdownCancel();
        void ShutdownMsg(bool show = false, Player* player = nullptr);
        static uint8 GetExitCode()
        {
            return m_ExitCode;
        }

        static void StopNow(uint8 exitcode) { m_stopEvent = true; m_ExitCode = exitcode; }
        static bool IsStopped()
        {
            return m_stopEvent;
        }

        void Update(uint32 diff);

        void UpdateSessions(uint32 diff);

        void AdvanceClocks(uint32 diff);

        void RunOffMapSystems(uint32 diff);

        void RunMaps(uint32 diff);

        void SettleTick(uint32 diff);

        void setConfig(eConfigFloatValues index, float value) { m_configFloatValues[index] = value; }

        float getConfig(eConfigFloatValues rate) const { return m_configFloatValues[rate]; }

        void setConfig(eConfigUInt32Values index, uint32 value) { m_configUint32Values[index] = value; }

        uint32 getConfig(eConfigUInt32Values index) const { return m_configUint32Values[index]; }

        void setConfig(eConfigInt32Values index, int32 value) { m_configInt32Values[index] = value; }

        int32 getConfig(eConfigInt32Values index) const { return m_configInt32Values[index]; }

        void setConfig(eConfigBoolValues index, bool value) { m_configBoolValues[index] = value; }

        bool getConfig(eConfigBoolValues index) const { return m_configBoolValues[index]; }

        bool isForceLoadMap(uint32 id) const { return m_configForceLoadMapIds.find(id) != m_configForceLoadMapIds.end(); }

        bool IsPvPRealm()
        {
            return (getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_PVP || getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_RPPVP || getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_FFA_PVP);
        }

        bool IsFFAPvPRealm()
        {
            return getConfig(CONFIG_UINT32_GAME_TYPE) == REALM_TYPE_FFA_PVP;
        }

        void KickAll();
        void KickAllLess(AccountTypes sec);
        BanReturn BanAccount(BanMode mode, std::string nameOrIP, uint32 duration_secs, std::string reason, const std::string &author);
        bool RemoveBanAccount(BanMode mode, std::string nameOrIP);

        static float GetMaxVisibleDistanceOnContinents()    { return m_MaxVisibleDistanceOnContinents; }
        static float GetMaxVisibleDistanceInInstances()     { return m_MaxVisibleDistanceInInstances;  }
        static float GetMaxVisibleDistanceInBGArenas()      { return m_MaxVisibleDistanceInBGArenas;   }

        static float GetMaxVisibleDistanceInFlight()        { return m_MaxVisibleDistanceInFlight;    }
        static float GetVisibleUnitGreyDistance()           { return m_VisibleUnitGreyDistance;       }
        static float GetVisibleObjectGreyDistance()         { return m_VisibleObjectGreyDistance;     }

        static float GetRelocationLowerLimitSq()            { return m_relocation_lower_limit_sq; }
        static uint32 GetRelocationAINotifyDelay()          { return m_relocation_ai_notify_delay; }

        static bool   GetVisibilityObserverSweepEnabled()   { return m_visibility_observer_sweep_enabled; }
        static uint32 GetVisibilityObserverSweepInterval()  { return m_visibility_observer_sweep_interval; }

        void InitServerMaintenanceCheck();
        void ServerMaintenanceStart();

        void ProcessCliCommands();
        void QueueCliCommand(CliCommandHolder* commandHolder) { cliCmdQueue.add(commandHolder); }

        void UpdateResultQueue();
        void InitResultQueue();

        void UpdateRealmCharCount(uint32 accid);

        LocaleConstant GetAvailableDbcLocale(LocaleConstant locale) const { if (m_availableDbcLocaleMask & (1 << locale)) { return locale; } else { return m_defaultDbcLocale; } }

        void LoadDBVersion();
        char const* GetDBVersion()
        {
            return m_DBVersion.c_str();
        }

        void LoadBroadcastStrings();

        void InvalidatePlayerDataToAllClient(ObjectGuid guid);

    protected:
        void _UpdateGameTime();

        void _UpdateRealmCharCount(QueryResult* resultCharCount, uint32 accountId);

    private:
        void setConfig(eConfigUInt32Values index, char const* fieldname, uint32 defvalue);
        void setConfig(eConfigInt32Values index, char const* fieldname, int32 defvalue);
        void setConfig(eConfigFloatValues index, char const* fieldname, float defvalue);
        void setConfig(eConfigBoolValues index, char const* fieldname, bool defvalue);
        void setConfigPos(eConfigFloatValues index, char const* fieldname, float defvalue);
        void setConfigMin(eConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 minvalue);
        void setConfigMin(eConfigInt32Values index, char const* fieldname, int32 defvalue, int32 minvalue);
        void setConfigMin(eConfigFloatValues index, char const* fieldname, float defvalue, float minvalue);
        void setConfigMinMax(eConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 minvalue, uint32 maxvalue);
        void setConfigMinMax(eConfigInt32Values index, char const* fieldname, int32 defvalue, int32 minvalue, int32 maxvalue);
        void setConfigMinMax(eConfigFloatValues index, char const* fieldname, float defvalue, float minvalue, float maxvalue);
        bool configNoReload(bool reload, eConfigUInt32Values index, char const* fieldname, uint32 defvalue);
        bool configNoReload(bool reload, eConfigInt32Values index, char const* fieldname, int32 defvalue);
        bool configNoReload(bool reload, eConfigFloatValues index, char const* fieldname, float defvalue);
        bool configNoReload(bool reload, eConfigBoolValues index, char const* fieldname, bool defvalue);

        void AutoBroadcast();
        struct BroadcastString
        {
            uint32 freq;
            std::string text;
        };
        std::vector<BroadcastString> m_broadcastList;
        uint32 m_broadcastWeight;
        bool m_broadcastEnable;
        IntervalTimer m_broadcastTimer;

        struct ScheduledExitWarning
        {
            uint32 remainingSeconds;
            int32 textId;
            bool sent;
        };

        void LoadScheduledExitConfig();
        void CheckScheduledExit();
        void StartScheduledExit();
        void ResetScheduledExitWarnings();
        void SendScheduledExitWarnings();
        void SendScheduledExitWarning(ScheduledExitWarning& warning);

        static volatile bool m_stopEvent;
        static uint8 m_ExitCode;
        uint32 m_ShutdownTimer;
        uint32 m_ShutdownMask;

        MaNGOS::ScheduledExitSchedule m_scheduledExit;
        MaNGOS::ScheduledExitState m_scheduledExitState;
        uint32 m_scheduledExitDelay;
        std::vector<ScheduledExitWarning> m_scheduledExitWarnings;
        bool m_scheduledExitCountdownActive;

        uint32 m_NextMaintenanceDate;
        uint32 m_MaintenanceTimeChecker;

        time_t m_startTime;
        time_t m_gameTime;
        IntervalTimer m_timers[WUPDATE_COUNT];
        uint32 mail_timer;
        uint32 mail_timer_expires;

        SessionMap m_sessions;
        uint32 m_maxActiveSessionCount;
        uint32 m_maxQueuedSessionCount;

        uint32 m_configUint32Values[CONFIG_UINT32_VALUE_COUNT];
        int32 m_configInt32Values[CONFIG_INT32_VALUE_COUNT];
        float m_configFloatValues[CONFIG_FLOAT_VALUE_COUNT];
        bool m_configBoolValues[CONFIG_BOOL_VALUE_COUNT];

        int32 m_playerLimit;
        LocaleConstant m_defaultDbcLocale;
        uint32 m_availableDbcLocaleMask;
        void DetectDBCLang();
        bool m_allowMovement;
        std::string m_motd;
        std::string m_dataPath;

        static float m_MaxVisibleDistanceOnContinents;
        static float m_MaxVisibleDistanceInInstances;
        static float m_MaxVisibleDistanceInBGArenas;

        static float m_MaxVisibleDistanceInFlight;
        static float m_VisibleUnitGreyDistance;
        static float m_VisibleObjectGreyDistance;

        static float  m_relocation_lower_limit_sq;
        static uint32 m_relocation_ai_notify_delay;

        static bool   m_visibility_observer_sweep_enabled;
        static uint32 m_visibility_observer_sweep_interval;

        MaNGOS::LockedQueue<CliCommandHolder*> cliCmdQueue;

        Queue m_QueuedSessions;

        void AddSession_(WorldSession* s);
        MaNGOS::LockedQueue<WorldSession*> addSessQueue;

        std::string m_DBVersion;

        std::set<uint32> m_configForceLoadMapIds;
};

extern uint32 realmID;

#define sWorld MaNGOS::Singleton<World>::Instance()
