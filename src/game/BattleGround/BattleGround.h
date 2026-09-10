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

#include <deque>
#include <queue>
#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include <ctime>
#include <map>
#include "SharedDefines.h"
#include "Map.h"
#include "ByteBuffer.h"
#include "ObjectGuid.h"

#define BG_EVENT_NONE 255

#define BG_EVENT_DOOR 254

class Creature;
class GameObject;
class Group;
class Player;
class WorldPacket;
class BattleGroundMap;

struct WorldSafeLocsEntry;

struct BattleGroundEventIdx
{
    uint8 event1;
    uint8 event2;
};

enum BattleGroundSounds
{
    SOUND_HORDE_WINS = 8454,
    SOUND_ALLIANCE_WINS = 8455,
    SOUND_BG_START = 3439
};

enum BattleGroundQuests
{
    SPELL_WS_QUEST_REWARD = 43483,
    SPELL_AB_QUEST_REWARD = 43484,
    SPELL_AV_QUEST_REWARD = 43475,
    SPELL_AV_QUEST_KILLED_BOSS = 23658,
    SPELL_AB_QUEST_REWARD_4_BASES = 24061,
    SPELL_AB_QUEST_REWARD_5_BASES = 24064
};

enum BattleGroundMarks
{
    SPELL_WS_MARK_LOSER = 24950,
    SPELL_WS_MARK_WINNER = 24951,
    SPELL_AB_MARK_LOSER = 24952,
    SPELL_AB_MARK_WINNER = 24953,
    SPELL_AV_MARK_LOSER = 24954,
    SPELL_AV_MARK_WINNER = 24955,
};

enum BattleGroundMarksCount
{
    ITEM_WINNER_COUNT = 3,
    ITEM_LOSER_COUNT = 1
};

enum BattleGroundTimeIntervals
{
    CHECK_PLAYER_POSITION_INVERVAL = 1000,
    RESURRECTION_INTERVAL = 30000,
    INVITATION_REMIND_TIME = 60000,
    INVITE_ACCEPT_WAIT_TIME = 80000,
    TIME_TO_AUTOREMOVE = 120000,
    MAX_OFFLINE_TIME = 300,
    RESPAWN_ONE_DAY = 86400,
    RESPAWN_IMMEDIATELY = 0,
    BUFF_RESPAWN_TIME = 180
};

enum BattleGroundStartTimeIntervals
{
    BG_START_DELAY_2M = 120000,
    BG_START_DELAY_1M = 60000,
    BG_START_DELAY_30S = 30000,
    BG_START_DELAY_NONE = 0,
};

enum BattleGroundBuffObjects
{
    BG_OBJECTID_SPEEDBUFF_ENTRY = 179871,
    BG_OBJECTID_REGENBUFF_ENTRY = 179904,
    BG_OBJECTID_BERSERKERBUFF_ENTRY = 179905
};

const uint32 Buff_Entries[3] = { BG_OBJECTID_SPEEDBUFF_ENTRY, BG_OBJECTID_REGENBUFF_ENTRY, BG_OBJECTID_BERSERKERBUFF_ENTRY };

enum BattleGroundStatus
{
    STATUS_NONE = 0,
    STATUS_WAIT_QUEUE = 1,
    STATUS_WAIT_JOIN = 2,
    STATUS_IN_PROGRESS = 3,
    STATUS_WAIT_LEAVE = 4
};

struct BattleGroundPlayer
{
    time_t  OfflineRemoveTime;
    Team    PlayerTeam;
};

struct BattleGroundObjectInfo
{
    BattleGroundObjectInfo() : object(nullptr), timer(0), spellid(0) {}

    GameObject* object;
    int32       timer;
    uint32      spellid;
};

enum BattleGroundQueueTypeId
{
    BATTLEGROUND_QUEUE_NONE = 0,
    BATTLEGROUND_QUEUE_AV = 1,
    BATTLEGROUND_QUEUE_WS = 2,
    BATTLEGROUND_QUEUE_AB = 3,
};
#define MAX_BATTLEGROUND_QUEUE_TYPES 4

enum BattleGroundBracketId
{
    BG_BRACKET_ID_TEMPLATE = -1,
    BG_BRACKET_ID_FIRST = 0,
    BG_BRACKET_ID_LAST = 5
};

#define MAX_BATTLEGROUND_BRACKETS 6

enum ScoreType
{
    SCORE_KILLING_BLOWS = 1,
    SCORE_DEATHS = 2,
    SCORE_HONORABLE_KILLS = 3,
    SCORE_BONUS_HONOR = 4,

    SCORE_FLAG_CAPTURES = 7,
    SCORE_FLAG_RETURNS = 8,

    SCORE_BASES_ASSAULTED = 9,
    SCORE_BASES_DEFENDED = 10,

    SCORE_GRAVEYARDS_ASSAULTED = 11,
    SCORE_GRAVEYARDS_DEFENDED = 12,
    SCORE_TOWERS_ASSAULTED = 13,
    SCORE_TOWERS_DEFENDED = 14,
    SCORE_SECONDARY_OBJECTIVES = 15
};

enum BattleGroundStartingEvents
{
    BG_STARTING_EVENT_NONE = 0x00,
    BG_STARTING_EVENT_1 = 0x01,
    BG_STARTING_EVENT_2 = 0x02,
    BG_STARTING_EVENT_3 = 0x04,
    BG_STARTING_EVENT_4 = 0x08
};

enum BattleGroundStartingEventsIds
{
    BG_STARTING_EVENT_FIRST = 0,
    BG_STARTING_EVENT_SECOND = 1,
    BG_STARTING_EVENT_THIRD = 2,
    BG_STARTING_EVENT_FOURTH = 3
};
#define BG_STARTING_EVENT_COUNT 4

enum BattleGroundJoinError
{
    BG_JOIN_ERR_OK = 0,
    BG_JOIN_ERR_OFFLINE_MEMBER = 1,
    BG_JOIN_ERR_GROUP_TOO_MANY = 2,
    BG_JOIN_ERR_MIXED_FACTION = 3,
    BG_JOIN_ERR_MIXED_LEVELS = 4,

    BG_JOIN_ERR_GROUP_MEMBER_ALREADY_IN_QUEUE = 6,
    BG_JOIN_ERR_GROUP_DESERTER = 7,
    BG_JOIN_ERR_ALL_QUEUES_USED = 8,
    BG_JOIN_ERR_GROUP_NOT_ENOUGH = 9
};

class BattleGroundScore
{
    public:

        BattleGroundScore() : KillingBlows(0), Deaths(0), HonorableKills(0),
            DishonorableKills(0), BonusHonor(0)
        {}

        virtual ~BattleGroundScore() {}

        uint32 GetKillingBlows() const { return KillingBlows; }

        uint32 GetDeaths() const { return Deaths; }

        uint32 GetHonorableKills() const { return HonorableKills; }

        uint32 GetBonusHonor() const { return BonusHonor; }

        uint32 GetDamageDone() const { return 0; }

        uint32 GetHealingDone() const { return 0; }

        virtual uint32 GetAttr1() const { return 0; }

        virtual uint32 GetAttr2() const { return 0; }

        virtual uint32 GetAttr3() const { return 0; }

        virtual uint32 GetAttr4() const { return 0; }

        virtual uint32 GetAttr5() const { return 0; }

        uint32 KillingBlows;
        uint32 Deaths;
        uint32 HonorableKills;
        uint32 DishonorableKills;
        uint32 BonusHonor;
};

class BattleGround
{
    friend class BattleGroundMgr;

    public:

        BattleGround();

        virtual ~BattleGround();

        virtual void Update(uint32 diff);

        virtual void Reset();

        virtual void StartingEventCloseDoors() {}

        virtual void StartingEventOpenDoors() {}

        char const* GetName() const { return m_Name; }

        BattleGroundTypeId GetTypeID() const
        {
            return m_TypeID;
        }

        BattleGroundBracketId GetBracketId() const { return m_BracketId; }

        uint32 GetInstanceID()
        {
            return m_Map ? GetBgMap()->GetInstanceId() : 0;
        }

        BattleGroundStatus GetStatus() const { return m_Status; }

        uint32 GetClientInstanceID() const { return m_ClientInstanceID; }

        uint32 GetStartTime() const { return m_StartTime; }

        uint32 GetEndTime() const { return m_EndTime; }

        uint32 GetMaxPlayers() const { return m_MaxPlayers; }

        uint32 GetMinPlayers() const { return m_MinPlayers; }

        uint32 GetMinLevel() const { return m_LevelMin; }

        uint32 GetMaxLevel() const { return m_LevelMax; }

        uint32 GetMaxPlayersPerTeam() const { return m_MaxPlayersPerTeam; }

        uint32 GetMinPlayersPerTeam() const { return m_MinPlayersPerTeam; }

        int32 GetStartDelayTime() const { return m_StartDelayTime; }

        Team GetWinner() const { return m_Winner; }

        virtual Team GetPrematureWinner();

        uint32 GetBattlemasterEntry() const;

        uint32 GetBonusHonorFromKill(uint32 kills) const;

        void SetName(char const* Name) { m_Name = Name; }

        void SetTypeID(BattleGroundTypeId TypeID) { m_TypeID = TypeID; }

        void SetBracketId(BattleGroundBracketId ID) { m_BracketId = ID; }

        void SetStatus(BattleGroundStatus Status) { m_Status = Status; }

        void SetClientInstanceID(uint32 InstanceID) { m_ClientInstanceID = InstanceID; }

        void SetStartTime(uint32 Time) { m_StartTime = Time; }

        void SetEndTime(uint32 Time) { m_EndTime = Time; }

        void SetMaxPlayers(uint32 MaxPlayers) { m_MaxPlayers = MaxPlayers; }

        void SetMinPlayers(uint32 MinPlayers) { m_MinPlayers = MinPlayers; }

        void SetLevelRange(uint32 min, uint32 max) { m_LevelMin = min; m_LevelMax = max; }

        void SetWinner(Team winner) { m_Winner = winner; }

        void ModifyStartDelayTime(int diff) { m_StartDelayTime -= diff; }

        void SetStartDelayTime(int Time) { m_StartDelayTime = Time; }

        void SetMaxPlayersPerTeam(uint32 MaxPlayers) { m_MaxPlayersPerTeam = MaxPlayers; }

        void SetMinPlayersPerTeam(uint32 MinPlayers) { m_MinPlayersPerTeam = MinPlayers; }

        void AddToBGFreeSlotQueue();

        void RemoveFromBGFreeSlotQueue();

        void DecreaseInvitedCount(Team team) { (team == ALLIANCE) ? --m_InvitedAlliance : --m_InvitedHorde; }

        void IncreaseInvitedCount(Team team) { (team == ALLIANCE) ? ++m_InvitedAlliance : ++m_InvitedHorde; }

        uint32 GetInvitedCount(Team team) const
        {
            if (team == ALLIANCE)
            {
                return m_InvitedAlliance;
            }
            else
            {
                return m_InvitedHorde;
            }
        }

        bool HasFreeSlots() const;

        uint32 GetFreeSlotsForTeam(Team team) const;

        typedef std::map<ObjectGuid, BattleGroundPlayer> BattleGroundPlayerMap;

        BattleGroundPlayerMap const& GetPlayers() const { return m_Players; }

        uint32 GetPlayersSize() const { return m_Players.size(); }

        typedef std::map<ObjectGuid, BattleGroundScore*> BattleGroundScoreMap;

        BattleGroundScoreMap::const_iterator GetPlayerScoresBegin() const { return m_PlayerScores.begin(); }

        BattleGroundScoreMap::const_iterator GetPlayerScoresEnd() const { return m_PlayerScores.end(); }

        uint32 GetPlayerScoresSize() const { return m_PlayerScores.size(); }

        void StartBattleGround();

        void SetMapId(uint32 MapID) { m_MapId = MapID; }

        uint32 GetMapId() const { return m_MapId; }

        void SetBgMap(BattleGroundMap* map) { m_Map = map; }

        BattleGroundMap* GetBgMap()
        {
            MANGOS_ASSERT(m_Map);
            return m_Map;
        }

        void SetTeamStartLoc(Team team, float X, float Y, float Z, float O);

        void GetTeamStartLoc(Team team, float& X, float& Y, float& Z, float& O) const
        {
            PvpTeamIndex idx = GetTeamIndexByTeamId(team);
            X = m_TeamStartLocX[idx];
            Y = m_TeamStartLocY[idx];
            Z = m_TeamStartLocZ[idx];
            O = m_TeamStartLocO[idx];
        }

        void SetStartMaxDist(float startMaxDist) { m_startMaxDist = startMaxDist; }

        float GetStartMaxDist() const { return m_startMaxDist; }

        virtual void FillInitialWorldStates(WorldPacket& , uint32& ) {}

        void SendPacketToTeam(Team team, WorldPacket* packet, Player* sender = nullptr, bool self = true);

        void SendPacketToAll(WorldPacket* packet);

        template<class Do>

        void BroadcastWorker(Do& _do);

        void PlaySoundToTeam(uint32 SoundID, Team team);

        void PlaySoundToAll(uint32 SoundID);

        void CastSpellOnTeam(uint32 SpellID, Team team);

        void RewardHonorToTeam(uint32 Honor, Team team);

        void RewardReputationToTeam(uint32 faction_id, uint32 Reputation, Team team);

        void RewardMark(Player* plr, uint32 count);

        void SendRewardMarkByMail(Player* plr, uint32 mark, uint32 count) const;

        void RewardItem(Player* plr, uint32 item_id, uint32 count);

        void RewardQuestComplete(Player* plr);

        void RewardSpellCast(Player* plr, uint32 spell_id);

        void UpdateWorldState(uint32 Field, uint32 Value);

        void UpdateWorldStateForPlayer(uint32 Field, uint32 Value, Player* Source);

        virtual void EndBattleGround(Team winner);

        void BlockMovement(Player* plr);

        void SendMessageToAll(int32 entry, ChatMsg type, Player const* source = nullptr);

        void SendYellToAll(int32 entry, uint32 language, ObjectGuid guid);

        void PSendMessageToAll(int32 entry, ChatMsg type, Player const* source, ...);

        void SendMessage2ToAll(int32 entry, ChatMsg type, Player const* source, int32 strId1 = 0, int32 strId2 = 0);

        void SendYell2ToAll(int32 entry, uint32 language, ObjectGuid guid, int32 arg1, int32 arg2);

        Group* GetBgRaid(Team team) const { return m_BgRaids[GetTeamIndexByTeamId(team)]; }

        void SetBgRaid(Team team, Group* bg_raid);

        virtual void UpdatePlayerScore(Player* Source, uint32 type, uint32 value);

        static PvpTeamIndex GetTeamIndexByTeamId(Team team) { return team == ALLIANCE ? TEAM_INDEX_ALLIANCE : TEAM_INDEX_HORDE; }

        uint32 GetPlayersCountByTeam(Team team) const { return m_PlayersCount[GetTeamIndexByTeamId(team)]; }

        uint32 GetAlivePlayersCountByTeam(Team team) const;

        void UpdatePlayersCountByTeam(Team team, bool remove)
        {
            if (remove)
            {
                --m_PlayersCount[GetTeamIndexByTeamId(team)];
            }
            else
            {
                ++m_PlayersCount[GetTeamIndexByTeamId(team)];
            }
        }

        virtual bool HandleAreaTrigger(Player* , uint32 ) { return false; }

        virtual void HandleKillPlayer(Player* player, Player* killer);

        virtual void HandleKillUnit(Creature* , Player* ) {}

        virtual bool HandleEvent(uint32 , GameObject* ) { return false; }

        virtual void HandleGameObjectCreate(GameObject* ) {}

        virtual void EventPlayerDroppedFlag(Player* ) {}

        virtual void EventPlayerClickedOnFlag(Player* , GameObject* ) {}

        virtual void EventPlayerCapturedFlag(Player* ) {}

        virtual bool AllowsQuestObject(uint32 , Team ) const { return true; }

        void EventPlayerLoggedIn(Player* player);

        void EventPlayerLoggedOut(Player* player);

        virtual WorldSafeLocsEntry const* GetClosestGraveYard(Player* player);

        virtual void AddPlayer(Player* plr);

        void AddOrSetPlayerToCorrectBgGroup(Player* plr, ObjectGuid plr_guid, Team team);

        virtual void RemovePlayerAtLeave(ObjectGuid guid, bool Transport, bool SendPacket);

        void OnObjectDBLoad(Creature* );

        void OnObjectDBLoad(GameObject* );

        void SpawnEvent(uint8 event1, uint8 event2, bool spawn);

        bool IsActiveEvent(uint8 event1, uint8 event2)
        {
            if (m_ActiveEvents.find(event1) == m_ActiveEvents.end())
            {
                return false;
            }
            return m_ActiveEvents[event1] == event2;
        }

        ObjectGuid GetSingleCreatureGuid(uint8 event1, uint8 event2);

        void OpenDoorEvent(uint8 event1, uint8 event2 = 0);

        bool IsDoor(uint8 event1, uint8 event2);

        void HandleTriggerBuff(ObjectGuid go_guid);

        void SpawnBGObject(ObjectGuid guid, uint32 respawntime);

        void SpawnBGCreature(ObjectGuid guid, uint32 respawntime);

        void DoorOpen(ObjectGuid guid);

        void DoorClose(ObjectGuid guid);

        virtual bool HandlePlayerUnderMap(Player* ) { return false; }

        Team GetPlayerTeam(ObjectGuid guid);

        static Team GetOtherTeam(Team team) { return team ? ((team == ALLIANCE) ? HORDE : ALLIANCE) : TEAM_NONE; }

        static PvpTeamIndex GetOtherTeamIndex(PvpTeamIndex teamIdx) { return teamIdx == TEAM_INDEX_ALLIANCE ? TEAM_INDEX_HORDE : TEAM_INDEX_ALLIANCE; }

        bool IsPlayerInBattleGround(ObjectGuid guid);

        int32 m_TeamScores[PVP_TEAM_COUNT];

        struct EventObjects
        {
            GuidVector gameobjects;
            GuidVector creatures;
        };

        std::map<uint32, EventObjects> m_EventObjects;

        std::map<uint8, uint8> m_ActiveEvents;

    protected:

        void EndNow();

        void PlayerAddedToBGCheckIfBGIsRunning(Player* plr);

        BattleGroundScoreMap m_PlayerScores;

        virtual void RemovePlayer(Player* , ObjectGuid ) {}

        BattleGroundPlayerMap m_Players;

        uint8 m_Events;

        BattleGroundStartTimeIntervals m_StartDelayTimes[BG_STARTING_EVENT_COUNT];

        uint32 m_StartMessageIds[BG_STARTING_EVENT_COUNT];

        bool m_BuffChange;

    private:

        BattleGroundTypeId m_TypeID;

        BattleGroundStatus m_Status;

        uint32 m_ClientInstanceID;

        uint32 m_StartTime;

        uint32 m_validStartPositionTimer;

        int32 m_EndTime;

        BattleGroundBracketId m_BracketId;

        bool m_InBGFreeSlotQueue;

        Team m_Winner;

        int32 m_StartDelayTime;

        bool m_PrematureCountDown;

        uint32 m_PrematureCountDownTimer;

        char const* m_Name;

        typedef std::deque<ObjectGuid> OfflineQueue;
        OfflineQueue m_OfflineQueue;

        uint32 m_InvitedAlliance;

        uint32 m_InvitedHorde;

        Group* m_BgRaids[PVP_TEAM_COUNT];

        uint32 m_PlayersCount[PVP_TEAM_COUNT];

        uint32 m_LevelMin;

        uint32 m_LevelMax;

        uint32 m_MaxPlayersPerTeam;

        uint32 m_MaxPlayers;

        uint32 m_MinPlayersPerTeam;

        uint32 m_MinPlayers;

        uint32 m_MapId;

        BattleGroundMap* m_Map;

        float m_startMaxDist;

        float m_TeamStartLocX[PVP_TEAM_COUNT];

        float m_TeamStartLocY[PVP_TEAM_COUNT];

        float m_TeamStartLocZ[PVP_TEAM_COUNT];

        float m_TeamStartLocO[PVP_TEAM_COUNT];
};

inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, uint32 value)
{
    data << uint32(state);
    data << uint32(value);
    ++count;
}

inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, int32 value)
{
    data << uint32(state);
    data << int32(value);
    ++count;
}

inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, bool value)
{
    data << uint32(state);
    data << uint32(value ? 1 : 0);
    ++count;
}

struct WorldStatePair
{
    uint32 state;
    uint32 value;
};

inline void FillInitialWorldState(ByteBuffer& data, uint32& count, WorldStatePair const* array)
{
    for (WorldStatePair const* itr = array; itr->state; ++itr)
    {
        data << uint32(itr->state);
        data << uint32(itr->value);
        ++count;
    }
}
