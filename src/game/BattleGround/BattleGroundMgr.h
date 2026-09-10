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
#include "Platform/Define.h"
#include <ctime>
#include <vector>
#include <map>
#include <set>
#include <list>
#include "Policies/Singleton.h"
#include "BattleGround.h"
#include "Utilities/EventProcessor.h"

typedef std::map<uint32, BattleGround*> BattleGroundSet;

typedef std::list<BattleGround*> BGFreeSlotQueueType;

typedef std::unordered_map<uint32, BattleGroundTypeId> BattleMastersMap;

typedef std::unordered_map<uint32, BattleGroundEventIdx> CreatureBattleEventIndexesMap;

typedef std::unordered_map<uint32, BattleGroundEventIdx> GameObjectBattleEventIndexesMap;

#define COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME 10

struct GroupQueueInfo;

struct PlayerQueueInfo
{
    uint32  LastOnlineTime;
    GroupQueueInfo* GroupInfo;
};

typedef std::map<ObjectGuid, PlayerQueueInfo*> GroupQueueInfoPlayers;

struct GroupQueueInfo
{
    GroupQueueInfoPlayers Players;
    Team GroupTeam;
    BattleGroundTypeId BgTypeId;
    uint32 JoinTime;
    uint32 RemoveInviteTime;
    uint32 IsInvitedToBGInstanceGUID;
};

enum BattleGroundQueueGroupTypes
{
    BG_QUEUE_PREMADE_ALLIANCE   = 0,
    BG_QUEUE_PREMADE_HORDE      = 1,
    BG_QUEUE_NORMAL_ALLIANCE    = 2,
    BG_QUEUE_NORMAL_HORDE       = 3
};
#define BG_QUEUE_GROUP_TYPES_COUNT 4

enum BattleGroundGroupJoinStatus
{
    BG_GROUPJOIN_DESERTERS      = -2,
    BG_GROUPJOIN_FAILED         = -1

};

class BattleGround;

class BattleGroundQueue
{
    public:

        BattleGroundQueue();

        ~BattleGroundQueue();

        void Update(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);

        void FillPlayersToBG(BattleGround* bg, BattleGroundBracketId bracket_id);

        bool CheckPremadeMatch(BattleGroundBracketId bracket_id, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam);

        bool CheckNormalMatch(BattleGroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers);

        GroupQueueInfo* AddGroup(Player* leader, Group* group, BattleGroundTypeId bgTypeId, BattleGroundBracketId bracketId, bool isPremade);

        void RemovePlayer(ObjectGuid guid, bool decreaseInvitedCount);

        bool IsPlayerInvited(ObjectGuid pl_guid, const uint32 bgInstanceGuid, const uint32 removeTime);

        bool GetPlayerGroupInfoData(ObjectGuid guid, GroupQueueInfo* ginfo);

        void PlayerInvitedToBGUpdateAverageWaitTime(GroupQueueInfo* ginfo, BattleGroundBracketId bracket_id);

        uint32 GetAverageQueueWaitTime(GroupQueueInfo* ginfo, BattleGroundBracketId bracket_id);

    private:

        typedef std::map<ObjectGuid, PlayerQueueInfo> QueuedPlayersMap;
        QueuedPlayersMap m_QueuedPlayers;

        typedef std::list<GroupQueueInfo*> GroupsQueueType;

        GroupsQueueType m_QueuedGroups[MAX_BATTLEGROUND_BRACKETS][BG_QUEUE_GROUP_TYPES_COUNT];

        class SelectionPool
        {
            public:

                SelectionPool() : PlayerCount(0) {}

                void Init();

                bool AddGroup(GroupQueueInfo* ginfo, uint32 desiredCount);

                bool KickGroup(uint32 size);

                uint32 GetPlayerCount() const {return PlayerCount;}

            public:
                GroupsQueueType SelectedGroups;

            private:
                uint32 PlayerCount;
        };

        SelectionPool m_SelectionPools[PVP_TEAM_COUNT];

        bool InviteGroupToBG(GroupQueueInfo* ginfo, BattleGround* bg, Team side);

        uint32 m_WaitTimes[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS][COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME];
        uint32 m_WaitTimeLastPlayer[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS];
        uint32 m_SumOfWaitTimes[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS];
};

class BGQueueInviteEvent : public BasicEvent
{
    public:

        BGQueueInviteEvent(ObjectGuid pl_guid, uint32 BgInstanceGUID, BattleGroundTypeId BgTypeId, uint32 removeTime)
            : m_PlayerGuid(pl_guid), m_BgInstanceGUID(BgInstanceGUID), m_BgTypeId(BgTypeId), m_RemoveTime(removeTime)
        {};

        virtual ~BGQueueInviteEvent() {};

        bool Execute(uint64 e_time, uint32 p_time) override;

        void Abort(uint64 e_time) override;

    private:
        ObjectGuid m_PlayerGuid = 0;
        uint32 m_BgInstanceGUID;
        BattleGroundTypeId m_BgTypeId;
        uint32 m_RemoveTime;
};

class BGQueueRemoveEvent : public BasicEvent
{
    public:

        BGQueueRemoveEvent(ObjectGuid plGuid, uint32 bgInstanceGUID, BattleGroundTypeId BgTypeId, BattleGroundQueueTypeId bgQueueTypeId, uint32 removeTime)
            : m_PlayerGuid(plGuid), m_BgInstanceGUID(bgInstanceGUID), m_RemoveTime(removeTime), m_BgTypeId(BgTypeId), m_BgQueueTypeId(bgQueueTypeId)
        {}

        virtual ~BGQueueRemoveEvent() {}

        bool Execute(uint64 e_time, uint32 p_time) override;

        void Abort(uint64 e_time) override;

    private:
        ObjectGuid m_PlayerGuid = 0;
        uint32 m_BgInstanceGUID;
        uint32 m_RemoveTime;
        BattleGroundTypeId m_BgTypeId;
        BattleGroundQueueTypeId m_BgQueueTypeId;
};

class BattleGroundMgr
{
    public:

        BattleGroundMgr();

        ~BattleGroundMgr();

        void Update(uint32 diff);

        void BuildPlayerJoinedBattleGroundPacket(WorldPacket* data, Player* plr);

        void BuildPlayerLeftBattleGroundPacket(WorldPacket* data, ObjectGuid guid);

        void BuildBattleGroundListPacket(WorldPacket* data, ObjectGuid guid, Player* plr, BattleGroundTypeId bgTypeId);

        void BuildGroupJoinedBattlegroundPacket(WorldPacket* data, int32 status);

        void BuildUpdateWorldStatePacket(WorldPacket* data, uint32 field, uint32 value);

        void BuildPvpLogDataPacket(WorldPacket* data, BattleGround* bg);

        void BuildBattleGroundStatusPacket(WorldPacket* data, BattleGround* bg, uint8 QueueSlot, uint8 StatusID, uint32 Time1, uint32 Time2);

        void BuildPlaySoundPacket(WorldPacket* data, uint32 soundid);

        BattleGround* GetBattleGroundThroughClientInstance(uint32 instanceId, BattleGroundTypeId bgTypeId);

        BattleGround* GetBattleGround(uint32 InstanceID, BattleGroundTypeId bgTypeId);

        BattleGround* GetBattleGroundTemplate(BattleGroundTypeId bgTypeId);

        BattleGround* CreateNewBattleGround(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);

        uint32 CreateBattleGround(BattleGroundTypeId bgTypeId, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam, uint32 LevelMin, uint32 LevelMax, char const* BattleGroundName, uint32 MapID, float Team1StartLocX, float Team1StartLocY, float Team1StartLocZ, float Team1StartLocO, float Team2StartLocX, float Team2StartLocY, float Team2StartLocZ, float Team2StartLocO, float StartMaxDist);

        void AddBattleGround(uint32 InstanceID, BattleGroundTypeId bgTypeId, BattleGround* BG) { m_BattleGrounds[bgTypeId][InstanceID] = BG; };

        void RemoveBattleGround(uint32 instanceID, BattleGroundTypeId bgTypeId) { m_BattleGrounds[bgTypeId].erase(instanceID); }

        uint32 CreateClientVisibleInstanceId(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);

        void DeleteClientVisibleInstanceId(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id, uint32 clientInstanceID)
        {
            m_ClientBattleGroundIds[bgTypeId][bracket_id].erase(clientInstanceID);
        }

        void CreateInitialBattleGrounds();

        void DeleteAllBattleGrounds();

        void SendToBattleGround(Player* pl, uint32 InstanceID, BattleGroundTypeId bgTypeId);

        BattleGroundQueue m_BattleGroundQueues[MAX_BATTLEGROUND_QUEUE_TYPES];

        BGFreeSlotQueueType BGFreeSlotQueue[MAX_BATTLEGROUND_TYPE_ID];

        void ScheduleQueueUpdate(BattleGroundQueueTypeId bgQueueTypeId, BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);

        uint32 GetPrematureFinishTime() const;

        void ToggleTesting();

        void LoadBattleMastersEntry();

        BattleGroundTypeId GetBattleMasterBG(uint32 entry) const
        {
            BattleMastersMap::const_iterator itr = mBattleMastersMap.find(entry);
            if (itr != mBattleMastersMap.end())
            {
                return itr->second;
            }
            return BATTLEGROUND_TYPE_NONE;
        }

        void LoadBattleEventIndexes();

        const BattleGroundEventIdx GetCreatureEventIndex(uint32 dbTableGuidLow) const
        {
            CreatureBattleEventIndexesMap::const_iterator itr = m_CreatureBattleEventIndexMap.find(dbTableGuidLow);
            if (itr != m_CreatureBattleEventIndexMap.end())
            {
                return itr->second;
            }
            return m_CreatureBattleEventIndexMap.find(-1)->second;
        }

        const BattleGroundEventIdx GetGameObjectEventIndex(uint32 dbTableGuidLow) const
        {
            GameObjectBattleEventIndexesMap::const_iterator itr = m_GameObjectBattleEventIndexMap.find(dbTableGuidLow);
            if (itr != m_GameObjectBattleEventIndexMap.end())
            {
                return itr->second;
            }
            return m_GameObjectBattleEventIndexMap.find(-1)->second;
        }

        bool isTesting() const { return m_Testing; }

        static BattleGroundQueueTypeId BGQueueTypeId(BattleGroundTypeId bgTypeId);

        static BattleGroundTypeId BGTemplateId(BattleGroundQueueTypeId bgQueueTypeId);

        static HolidayIds BGTypeToWeekendHolidayId(BattleGroundTypeId bgTypeId);

        static BattleGroundTypeId WeekendHolidayIdToBGType(HolidayIds holiday);

        static bool IsBGWeekend(BattleGroundTypeId bgTypeId);
    private:
        BattleMastersMap    mBattleMastersMap;
        CreatureBattleEventIndexesMap m_CreatureBattleEventIndexMap;
        GameObjectBattleEventIndexesMap m_GameObjectBattleEventIndexMap;

        BattleGroundSet m_BattleGrounds[MAX_BATTLEGROUND_TYPE_ID];
        std::vector<uint32> m_QueueUpdateScheduler;

        typedef std::set<uint32> ClientBattleGroundIdSet;
        ClientBattleGroundIdSet m_ClientBattleGroundIds[MAX_BATTLEGROUND_TYPE_ID][MAX_BATTLEGROUND_BRACKETS];
        bool   m_Testing;
};

#define sBattleGroundMgr MaNGOS::Singleton<BattleGroundMgr>::Instance()
