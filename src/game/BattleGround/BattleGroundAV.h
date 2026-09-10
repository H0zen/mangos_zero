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

#include "Platform/Define.h"
#include "BattleGround.h"

#define BG_AV_MAX_NODE_DISTANCE             25

#define BG_AV_BOSS_KILL_QUEST_SPELL         23658

#define BG_AV_CAPTIME                       240000
#define BG_AV_SNOWFALL_FIRSTCAP             300000

#define BG_AV_SCORE_INITIAL_POINTS          600
#define BG_AV_SCORE_NEAR_LOSE               120

#define BG_AV_KILL_BOSS                     4
#define BG_AV_REP_BOSS                      350
#define BG_AV_REP_BOSS_HOLIDAY              525

#define BG_AV_KILL_CAPTAIN                  3
#define BG_AV_REP_CAPTAIN                   125
#define BG_AV_REP_CAPTAIN_HOLIDAY           185
#define BG_AV_RES_CAPTAIN                   100

#define BG_AV_KILL_TOWER                    3
#define BG_AV_REP_TOWER                     12
#define BG_AV_REP_TOWER_HOLIDAY             18
#define BG_AV_RES_TOWER                     75

#define BG_AV_KILL_GET_COMMANDER            1

#define BG_AV_KILL_SURVIVING_TOWER          2
#define BG_AV_REP_SURVIVING_TOWER           12
#define BG_AV_REP_SURVIVING_TOWER_HOLIDAY   18

#define BG_AV_KILL_SURVIVING_CAPTAIN        2
#define BG_AV_REP_SURVIVING_CAPTAIN         125
#define BG_AV_REP_SURVIVING_CAPTAIN_HOLIDAY 175

#define BG_AV_KILL_MAP_COMPLETE             0
#define BG_AV_KILL_MAP_COMPLETE_HOLIDAY     4

#define BG_AV_REP_OWNED_GRAVE               12
#define BG_AV_REP_OWNED_GRAVE_HOLIDAY       18

#define BG_AV_REP_OWNED_MINE                24
#define BG_AV_REP_OWNED_MINE_HOLIDAY        36

enum BG_AV_Sounds
{
    BG_AV_SOUND_NEAR_LOSE               = 8456,
    BG_AV_SOUND_ALLIANCE_ASSAULTS       = 8212,
    BG_AV_SOUND_HORDE_ASSAULTS          = 8174,
    BG_AV_SOUND_ALLIANCE_GOOD           = 8173,
    BG_AV_SOUND_HORDE_GOOD              = 8213,
    BG_AV_SOUND_BOTH_TOWER_DEFEND       = 8192,
    BG_AV_SOUND_ALLIANCE_CAPTAIN        = 8232,
    BG_AV_SOUND_HORDE_CAPTAIN           = 8333
};

enum BG_AV_OTHER_VALUES
{
    BG_AV_NORTH_MINE            = 0,
    BG_AV_SOUTH_MINE            = 1,
    BG_AV_MINE_TICK_TIMER       = 45000,
    BG_AV_MINE_RECLAIM_TIMER    = 1200000,
    BG_AV_FACTION_A             = 730,
    BG_AV_FACTION_H             = 729
};
#define BG_AV_MAX_MINES 2

enum BG_AV_ObjectIds
{

    BG_AV_OBJECTID_MINE_N               = 178785,
    BG_AV_OBJECTID_MINE_S               = 178784
};

enum BG_AV_Nodes
{
    BG_AV_NODES_FIRSTAID_STATION        = 0,
    BG_AV_NODES_STORMPIKE_GRAVE         = 1,
    BG_AV_NODES_STONEHEART_GRAVE        = 2,
    BG_AV_NODES_SNOWFALL_GRAVE          = 3,
    BG_AV_NODES_ICEBLOOD_GRAVE          = 4,
    BG_AV_NODES_FROSTWOLF_GRAVE         = 5,
    BG_AV_NODES_FROSTWOLF_HUT           = 6,
    BG_AV_NODES_DUNBALDAR_SOUTH         = 7,
    BG_AV_NODES_DUNBALDAR_NORTH         = 8,
    BG_AV_NODES_ICEWING_BUNKER          = 9,
    BG_AV_NODES_STONEHEART_BUNKER       = 10,
    BG_AV_NODES_ICEBLOOD_TOWER          = 11,
    BG_AV_NODES_TOWER_POINT             = 12,
    BG_AV_NODES_FROSTWOLF_ETOWER        = 13,
    BG_AV_NODES_FROSTWOLF_WTOWER        = 14,
    BG_AV_NODES_ERROR                   = 255
};
#define BG_AV_NODES_MAX                 15

#define BG_AV_MINE_BOSSES            46
#define BG_AV_MINE_BOSSES_NORTH      46
#define BG_AV_MINE_BOSSES_SOUTH      47
#define BG_AV_CAPTAIN_A              48
#define BG_AV_CAPTAIN_H              49
#define BG_AV_MINE_EVENT             50
#define BG_AV_MINE_EVENT_NORTH       50
#define BG_AV_MINE_EVENT_SOUTH       51

#define BG_AV_MARSHAL_A_SOUTH        52
#define BG_AV_MARSHAL_A_NORTH        53
#define BG_AV_MARSHAL_A_ICE          54
#define BG_AV_MARSHAL_A_STONE        55
#define BG_AV_MARSHAL_H_ICE          56
#define BG_AV_MARSHAL_H_TOWER        57
#define BG_AV_MARSHAL_H_ETOWER       58
#define BG_AV_MARSHAL_H_WTOWER       59

#define BG_AV_HERALD                 60
#define BG_AV_BOSS_A                 61
#define BG_AV_BOSS_H                 62
#define BG_AV_NodeEventCaptainDead_A 63
#define BG_AV_NodeEventCaptainDead_H 64

enum BG_AV_Graveyards
{
    BG_AV_GRAVE_STORM_AID          = 751,
    BG_AV_GRAVE_STORM_GRAVE        = 689,
    BG_AV_GRAVE_STONE_GRAVE        = 729,
    BG_AV_GRAVE_SNOWFALL           = 169,
    BG_AV_GRAVE_ICE_GRAVE          = 749,
    BG_AV_GRAVE_FROSTWOLF          = 690,
    BG_AV_GRAVE_FROST_HUT          = 750,
    BG_AV_GRAVE_MAIN_ALLIANCE      = 611,
    BG_AV_GRAVE_MAIN_HORDE         = 610
};

const uint32 BG_AV_GraveyardIds[9] =
{
    BG_AV_GRAVE_STORM_AID,
    BG_AV_GRAVE_STORM_GRAVE,
    BG_AV_GRAVE_STONE_GRAVE,
    BG_AV_GRAVE_SNOWFALL,
    BG_AV_GRAVE_ICE_GRAVE,
    BG_AV_GRAVE_FROSTWOLF,
    BG_AV_GRAVE_FROST_HUT,
    BG_AV_GRAVE_MAIN_ALLIANCE,
    BG_AV_GRAVE_MAIN_HORDE
};

enum BG_AV_States
{
    POINT_ASSAULTED             = 0,
    POINT_CONTROLLED            = 1
};
#define BG_AV_MAX_STATES 2

enum BG_AV_WorldStates
{
    BG_AV_Alliance_Score        = 3127,
    BG_AV_Horde_Score           = 3128,
    BG_AV_SHOW_H_SCORE          = 3133,
    BG_AV_SHOW_A_SCORE          = 3134,
    AV_SNOWFALL_N               = 1966
};

enum BattleGroundAVTeamIndex
{
    BG_AV_TEAM_ALLIANCE        = TEAM_INDEX_ALLIANCE,
    BG_AV_TEAM_HORDE           = TEAM_INDEX_HORDE,
    BG_AV_TEAM_NEUTRAL         = TEAM_INDEX_NEUTRAL,
};

#define BG_AV_TEAMS_COUNT 3

const uint32 BG_AV_MineWorldStates[2][BG_AV_TEAMS_COUNT] =
{
    {1358, 1359, 1360},
    {1355, 1356, 1357}
};

const uint32 BG_AV_NodeWorldStates[BG_AV_NODES_MAX][4] =
{

    {1326, 1325, 1328, 1327},

    {1335, 1333, 1336, 1334},

    {1304, 1302, 1303, 1301},

    {1343, 1341, 1344, 1342},

    {1348, 1346, 1349, 1347},

    {1339, 1337, 1340, 1338},

    {1331, 1329, 1332, 1330},

    {1375, 1361, 1378, 1370},

    {1374, 1362, 1379, 1371},

    {1376, 1363, 1380, 1372},

    {1377, 1364, 1381, 1373},

    {1390, 1368, 1395, 1385},

    {1389, 1367, 1394, 1384},

    {1388, 1366, 1393, 1383},

    {1387, 1365, 1392, 1382},
};

#define BG_AV_MAX_GRAVETYPES 4

enum BG_AV_QuestIds
{
    BG_AV_QUEST_A_SCRAPS1       = 7223,
    BG_AV_QUEST_A_SCRAPS2       = 6781,
    BG_AV_QUEST_H_SCRAPS1       = 7224,
    BG_AV_QUEST_H_SCRAPS2       = 6741,
    BG_AV_QUEST_A_COMMANDER1    = 6942,
    BG_AV_QUEST_H_COMMANDER1    = 6825,
    BG_AV_QUEST_A_COMMANDER2    = 6941,
    BG_AV_QUEST_H_COMMANDER2    = 6826,
    BG_AV_QUEST_A_COMMANDER3    = 6943,
    BG_AV_QUEST_H_COMMANDER3    = 6827,
    BG_AV_QUEST_A_BOSS1         = 7386,
    BG_AV_QUEST_H_BOSS1         = 7385,
    BG_AV_QUEST_A_BOSS2         = 6881,
    BG_AV_QUEST_H_BOSS2         = 6801,
    BG_AV_QUEST_A_NEAR_MINE     = 5892,
    BG_AV_QUEST_H_NEAR_MINE     = 5893,
    BG_AV_QUEST_A_OTHER_MINE    = 6982,
    BG_AV_QUEST_H_OTHER_MINE    = 6985,
    BG_AV_QUEST_A_RIDER_HIDE    = 7026,
    BG_AV_QUEST_H_RIDER_HIDE    = 7002,
    BG_AV_QUEST_A_RIDER_TAME    = 7027,
    BG_AV_QUEST_H_RIDER_TAME    = 7001
};

struct BG_AV_NodeInfo
{
    BattleGroundAVTeamIndex TotalOwner;
    BattleGroundAVTeamIndex Owner;
    BattleGroundAVTeamIndex PrevOwner;
    BG_AV_States State;
    BG_AV_States PrevState;
    uint32       Timer;
    bool         Tower;
};

inline BG_AV_Nodes& operator++(BG_AV_Nodes& i)
{
    return i = BG_AV_Nodes(i + 1);
}

class BattleGroundAVScore : public BattleGroundScore
{
    public:

        BattleGroundAVScore() : GraveyardsAssaulted(0), GraveyardsDefended(0), TowersAssaulted(0), TowersDefended(0), SecondaryObjectives(0), LieutnantCount(0), SecondaryNPC(0) {};

        virtual ~BattleGroundAVScore() {};

        uint32 GetAttr1() const { return GraveyardsAssaulted; }
        uint32 GetAttr2() const { return GraveyardsDefended; }
        uint32 GetAttr3() const { return TowersAssaulted; }
        uint32 GetAttr4() const { return TowersDefended; }
        uint32 GetAttr5() const { return SecondaryObjectives; }

        uint32 GraveyardsAssaulted;
        uint32 GraveyardsDefended;
        uint32 TowersAssaulted;
        uint32 TowersDefended;
        uint32 SecondaryObjectives;
        uint32 LieutnantCount;
        uint32 SecondaryNPC;
};

class BattleGroundAV : public BattleGround
{
    friend class BattleGroundMgr;

    public:

        BattleGroundAV();

        void Update(uint32 diff) override;

        void AddPlayer(Player* plr) override;

        void StartingEventOpenDoors() override;

        void FillInitialWorldStates(WorldPacket& data, uint32& count) override;

        bool HandleAreaTrigger(Player* source, uint32 trigger) override;

        void Reset() override;

        void UpdateScore(PvpTeamIndex teamIdx, int32 points);

        void UpdatePlayerScore(Player* source, uint32 type, uint32 value) override;

        void EventPlayerClickedOnFlag(Player* source, GameObject* target_obj) override;

        void HandleKillPlayer(Player* player, Player* killer) override;

        void HandleKillUnit(Creature* creature, Player* killer) override;

        void HandleQuestComplete(uint32 questid, Player* player);

        bool AllowsQuestObject(uint32 entry, Team team) const override;

        void EndBattleGround(Team winner) override;

        WorldSafeLocsEntry const* GetClosestGraveYard(Player* plr) override;

        Team GetPrematureWinner() override;

        static BattleGroundAVTeamIndex GetAVTeamIndexByTeamId(Team team) { return BattleGroundAVTeamIndex(GetTeamIndexByTeamId(team)); }

    private:

        void EventPlayerAssaultsPoint(Player* player, BG_AV_Nodes node);

        void EventPlayerDefendsPoint(Player* player, BG_AV_Nodes node);

        void EventPlayerDestroyedPoint(BG_AV_Nodes node);

        void AssaultNode(BG_AV_Nodes node, PvpTeamIndex teamIdx);

        void DestroyNode(BG_AV_Nodes node);

        void InitNode(BG_AV_Nodes node, BattleGroundAVTeamIndex teamIdx, bool tower);

        void DefendNode(BG_AV_Nodes node, PvpTeamIndex teamIdx);

        void PopulateNode(BG_AV_Nodes node);

        uint32 GetNodeName(BG_AV_Nodes node) const;

        bool IsTower(BG_AV_Nodes node) const { return (node == BG_AV_NODES_ERROR) ? false : m_Nodes[node].Tower; }

        bool IsGrave(BG_AV_Nodes node) const { return (node == BG_AV_NODES_ERROR) ? false : !m_Nodes[node].Tower; }

        void ChangeMineOwner(uint8 mine, BattleGroundAVTeamIndex teamIdx);

        uint8 GetWorldStateType(uint8 state, BattleGroundAVTeamIndex teamIdx) const { return teamIdx * BG_AV_MAX_STATES + state; }

        void SendMineWorldStates(uint32 mine);

        void UpdateNodeWorldState(BG_AV_Nodes node);

        uint32 m_Team_QuestStatus[PVP_TEAM_COUNT][9];

        BG_AV_NodeInfo m_Nodes[BG_AV_NODES_MAX];

        BattleGroundAVTeamIndex m_Mine_Owner[BG_AV_MAX_MINES];
        BattleGroundAVTeamIndex m_Mine_PrevOwner[BG_AV_MAX_MINES];
        int32 m_Mine_Timer[BG_AV_MAX_MINES];
        uint32 m_Mine_Reclaim_Timer[BG_AV_MAX_MINES];

        bool m_IsInformedNearLose[PVP_TEAM_COUNT];

        uint32 m_HonorMapComplete;
        uint32 m_RepTowerDestruction;
        uint32 m_RepCaptain;
        uint32 m_RepBoss;
        uint32 m_RepOwnedGrave;
        uint32 m_RepOwnedMine;
        uint32 m_RepSurviveCaptain;
        uint32 m_RepSurviveTower;
};
