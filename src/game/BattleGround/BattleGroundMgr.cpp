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

#include "Utilities/Util.h"
#include "Platform/Define.h"
#include <ctime>
#include <vector>
#include <map>
#include <set>
#include "SharedDefines.h"
#include "Player.h"
#include "BattleGroundMgr.h"
#include "BattleGroundAV.h"
#include "BattleGroundAB.h"
#include "BattleGroundWS.h"
#include "InstanceLedger.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "Chat.h"
#include "World.h"
#include "WorldPacket.h"
#include "Language.h"
#include "GameEventMgr.h"
#include "DisableMgr.h"
#include "GameTime.h"

#include "Policies/Singleton.h"

BattleGroundQueue::BattleGroundQueue()
{
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        for (uint8 j = 0; j < MAX_BATTLEGROUND_BRACKETS; ++j)
        {
            m_SumOfWaitTimes[i][j] = 0;
            m_WaitTimeLastPlayer[i][j] = 0;
            for (uint8 k = 0; k < COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME; ++k)
            {
                m_WaitTimes[i][j][k] = 0;
            }
        }
    }
}

BattleGroundQueue::~BattleGroundQueue()
{
    m_QueuedPlayers.clear();
    for (uint8 i = 0; i < MAX_BATTLEGROUND_BRACKETS; ++i)
    {
        for (uint8 j = 0; j < BG_QUEUE_GROUP_TYPES_COUNT; ++j)
        {
            for (GroupsQueueType::iterator itr = m_QueuedGroups[i][j].begin(); itr != m_QueuedGroups[i][j].end(); ++itr)
            {
                delete(*itr);
            }
            m_QueuedGroups[i][j].clear();
        }
    }
}

BattleGroundMgr::BattleGroundMgr()
{
    for (uint8 i = BATTLEGROUND_TYPE_NONE; i < MAX_BATTLEGROUND_TYPE_ID; ++i)
    {
        m_BattleGrounds[i].clear();
    }
    m_Testing = false;
}

BattleGroundMgr::~BattleGroundMgr()
{
    DeleteAllBattleGrounds();
}

void BattleGroundMgr::DeleteAllBattleGrounds()
{

    for (uint8 i = BATTLEGROUND_TYPE_NONE; i < MAX_BATTLEGROUND_TYPE_ID; ++i)
    {
        for (BattleGroundSet::iterator itr = m_BattleGrounds[i].begin(); itr != m_BattleGrounds[i].end();)
        {
            BattleGround* bg = itr->second;
            ++itr;
            delete bg;
        }
    }
}

void BattleGroundMgr::Update(uint32 )
{

    if (!m_QueueUpdateScheduler.empty())
    {
        std::vector<uint32> scheduled;
        {
            scheduled = std::vector<uint32>(m_QueueUpdateScheduler);
            m_QueueUpdateScheduler.clear();
        }

        for (uint8 i = 0; i < scheduled.size(); ++i)
        {
            BattleGroundQueueTypeId bgQueueTypeId = BattleGroundQueueTypeId(scheduled[i] >> 16 & 255);
            BattleGroundTypeId bgTypeId = BattleGroundTypeId((scheduled[i] >> 8) & 255);
            BattleGroundBracketId bracket_id = BattleGroundBracketId(scheduled[i] & 255);
            m_BattleGroundQueues[bgQueueTypeId].Update(bgTypeId, bracket_id);
        }
    }
}

void BattleGroundMgr::BuildBattleGroundStatusPacket(WorldPacket* data, BattleGround* bg, uint8 QueueSlot, uint8 StatusID, uint32 Time1, uint32 Time2)
{

    if (StatusID == 0 || !bg)
    {
        data->Initialize(SMSG_BATTLEFIELD_STATUS, 4 * 2);
        *data << uint32(QueueSlot);
        *data << uint32(0);
        return;
    }

    data->Initialize(SMSG_BATTLEFIELD_STATUS, (4 + 8 + 4 + 1 + 4 + 4 + 4));
    *data << uint32(QueueSlot);
    *data << uint32(bg->GetMapId());
    *data << uint8(0);
    *data << uint32(bg->GetClientInstanceID());
    *data << uint32(StatusID);
    switch (StatusID)
    {
        case STATUS_WAIT_QUEUE:
            *data << uint32(Time1);
            *data << uint32(Time2);
            break;
        case STATUS_WAIT_JOIN:
            *data << uint32(Time1);
            break;
        case STATUS_IN_PROGRESS:
            *data << uint32(Time1);
            *data << uint32(Time2);
            break;
        default:
            sLog.outError("Unknown BG status!");
            break;
    }
}

void BattleGroundMgr::BuildPvpLogDataPacket(WorldPacket* data, BattleGround* bg)
{
    data->Initialize(MSG_PVP_LOG_DATA, (1 + 4 + 40 * bg->GetPlayerScoresSize()));

    if (bg->GetStatus() != STATUS_WAIT_LEAVE)
    {
        *data << uint8(0);
    }
    else
    {
        *data << uint8(1);
        *data << uint8(bg->GetWinner());
    }

    *data << (uint32)(bg->GetPlayerScoresSize());

    for (BattleGround::BattleGroundScoreMap::const_iterator itr = bg->GetPlayerScoresBegin(); itr != bg->GetPlayerScoresEnd(); ++itr)
    {
        const BattleGroundScore* score = itr->second;

        *data << static_cast<ObjectGuid>(itr->first);

        Player* plr = sObjectMgr.GetPlayer(itr->first);

        *data << uint32(plr ? plr->GetHonorRankInfo().visualRank : 0);
        *data << uint32(itr->second->KillingBlows);
        *data << uint32(itr->second->HonorableKills);
        *data << uint32(itr->second->Deaths);
        *data << uint32(itr->second->BonusHonor);

        switch (bg->GetTypeID())
        {
            case BATTLEGROUND_AV:
                *data << (uint32)0x00000007;
                *data << (uint32)((BattleGroundAVScore*)score)->GraveyardsAssaulted;
                *data << (uint32)((BattleGroundAVScore*)score)->GraveyardsDefended;
                *data << (uint32)((BattleGroundAVScore*)score)->TowersAssaulted;
                *data << (uint32)((BattleGroundAVScore*)score)->TowersDefended;
                *data << (uint32)((BattleGroundAVScore*)score)->SecondaryObjectives;
                *data << (uint32)((BattleGroundAVScore*)score)->LieutnantCount;
                *data << (uint32)((BattleGroundAVScore*)score)->SecondaryNPC;
                break;
            case BATTLEGROUND_WS:
                *data << (uint32)0x00000002;
                *data << (uint32)((BattleGroundWGScore*)score)->FlagCaptures;
                *data << (uint32)((BattleGroundWGScore*)score)->FlagReturns;
                break;
            case BATTLEGROUND_AB:
                *data << (uint32)0x00000002;
                *data << (uint32)((BattleGroundABScore*)score)->BasesAssaulted;
                *data << (uint32)((BattleGroundABScore*)score)->BasesDefended;
                break;
            default:
                DEBUG_LOG("Unhandled MSG_PVP_LOG_DATA for BG id %u", bg->GetTypeID());
                *data << (uint32)0;
                break;
        }
    }
}

void BattleGroundMgr::BuildGroupJoinedBattlegroundPacket(WorldPacket* data, int32 status)
{
    data->Initialize(SMSG_GROUP_JOINED_BATTLEGROUND, 4);

    *data << int32(status);
}

void BattleGroundMgr::BuildUpdateWorldStatePacket(WorldPacket* data, uint32 field, uint32 value)
{
    data->Initialize(SMSG_UPDATE_WORLD_STATE, 4 + 4);
    *data << uint32(field);
    *data << uint32(value);
}

void BattleGroundMgr::BuildPlaySoundPacket(WorldPacket* data, uint32 soundid)
{
    data->Initialize(SMSG_PLAY_SOUND, 4);
    *data << uint32(soundid);
}

void BattleGroundMgr::BuildPlayerLeftBattleGroundPacket(WorldPacket* data, ObjectGuid guid)
{
    data->Initialize(SMSG_BATTLEGROUND_PLAYER_LEFT, 8);
    *data << static_cast<ObjectGuid>(guid);
}

void BattleGroundMgr::BuildPlayerJoinedBattleGroundPacket(WorldPacket* data, Player* plr)
{
    data->Initialize(SMSG_BATTLEGROUND_PLAYER_JOINED, 8);
    *data << plr->GetObjectGuid();
}

BattleGround* BattleGroundMgr::GetBattleGroundThroughClientInstance(uint32 instanceId, BattleGroundTypeId bgTypeId)
{

    BattleGround* bg = GetBattleGroundTemplate(bgTypeId);
    if (!bg)
    {
        return nullptr;
    }

    for (BattleGroundSet::iterator itr = m_BattleGrounds[bgTypeId].begin(); itr != m_BattleGrounds[bgTypeId].end(); ++itr)
    {
        if (itr->second->GetClientInstanceID() == instanceId)
        {
            return itr->second;
        }
    }
    return nullptr;
}

BattleGround* BattleGroundMgr::GetBattleGround(uint32 InstanceID, BattleGroundTypeId bgTypeId)
{

    BattleGroundSet::iterator itr;
    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
    {
        for (uint8 i = BATTLEGROUND_AV; i < MAX_BATTLEGROUND_TYPE_ID; ++i)
        {
            itr = m_BattleGrounds[i].find(InstanceID);
            if (itr != m_BattleGrounds[i].end())
            {
                return itr->second;
            }
        }
        return nullptr;
    }
    itr = m_BattleGrounds[bgTypeId].find(InstanceID);
    return ((itr != m_BattleGrounds[bgTypeId].end()) ? itr->second : nullptr);
}

BattleGround* BattleGroundMgr::GetBattleGroundTemplate(BattleGroundTypeId bgTypeId)
{

    return m_BattleGrounds[bgTypeId].empty() ? nullptr : m_BattleGrounds[bgTypeId].begin()->second;
}

uint32 BattleGroundMgr::CreateClientVisibleInstanceId(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id)
{

    uint32 lastId = 0;
    ClientBattleGroundIdSet& ids = m_ClientBattleGroundIds[bgTypeId][bracket_id];
    for (ClientBattleGroundIdSet::const_iterator itr = ids.begin(); itr != ids.end();)
    {
        if ((++lastId) != *itr)
        {
            break;
        }
        lastId = *itr;
    }
    ids.insert(lastId + 1);
    return lastId + 1;
}

BattleGround* BattleGroundMgr::CreateNewBattleGround(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id)
{

    BattleGround* bg_template = GetBattleGroundTemplate(bgTypeId);
    if (!bg_template)
    {
        sLog.outError("BattleGround: CreateNewBattleGround - bg template not found for %u", bgTypeId);
        return nullptr;
    }

    BattleGround* bg = nullptr;

    switch (bgTypeId)
    {
        case BATTLEGROUND_AV:
            bg = new BattleGroundAV(*(BattleGroundAV*)bg_template);
            break;
        case BATTLEGROUND_WS:
            bg = new BattleGroundWS(*(BattleGroundWS*)bg_template);
            break;
        case BATTLEGROUND_AB:
            bg = new BattleGroundAB(*(BattleGroundAB*)bg_template);
            break;
        default:

            return 0;
    }

    sInstanceLedger.OpenBattleGround(bg->GetMapId(), bg);

    bg->SetClientInstanceID(CreateClientVisibleInstanceId(bgTypeId, bracket_id));

    bg->Reset();

    bg->SetStatus(STATUS_WAIT_JOIN);
    bg->SetBracketId(bracket_id);

    return bg;
}

uint32 BattleGroundMgr::CreateBattleGround(BattleGroundTypeId bgTypeId, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam, uint32 LevelMin, uint32 LevelMax, char const* BattleGroundName, uint32 MapID, float Team1StartLocX, float Team1StartLocY, float Team1StartLocZ, float Team1StartLocO, float Team2StartLocX, float Team2StartLocY, float Team2StartLocZ, float Team2StartLocO, float StartMaxDist)
{

    BattleGround* bg;
    switch (bgTypeId)
    {
        case BATTLEGROUND_AV: bg = new BattleGroundAV; break;
        case BATTLEGROUND_WS: bg = new BattleGroundWS; break;
        case BATTLEGROUND_AB: bg = new BattleGroundAB; break;
        default:              bg = new BattleGround;   break;
    }

    bg->SetMapId(MapID);
    bg->SetTypeID(bgTypeId);
    bg->SetMinPlayersPerTeam(MinPlayersPerTeam);
    bg->SetMaxPlayersPerTeam(MaxPlayersPerTeam);
    bg->SetMinPlayers(MinPlayersPerTeam * 2);
    bg->SetMaxPlayers(MaxPlayersPerTeam * 2);
    bg->SetName(BattleGroundName);
    bg->SetTeamStartLoc(ALLIANCE, Team1StartLocX, Team1StartLocY, Team1StartLocZ, Team1StartLocO);
    bg->SetTeamStartLoc(HORDE,    Team2StartLocX, Team2StartLocY, Team2StartLocZ, Team2StartLocO);
    bg->SetStartMaxDist(StartMaxDist);
    bg->SetLevelRange(LevelMin, LevelMax);

    AddBattleGround(bg->GetInstanceID(), bg->GetTypeID(), bg);

    return bgTypeId;
}

void BattleGroundMgr::CreateInitialBattleGrounds()
{
    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query("SELECT `id`, `MinPlayersPerTeam`,`MaxPlayersPerTeam`,`MinLvl`,`MaxLvl`,`AllianceStartLoc`,`AllianceStartO`,`HordeStartLoc`,`HordeStartO`, `StartMaxDist` FROM `battleground_template`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 battlegrounds. DB table `battleground_template` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 bgTypeID_ = fields[0].GetUInt32();

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bgTypeID_))
        {
            continue;
        }

        BattleGroundTypeId bgTypeID = BattleGroundTypeId(bgTypeID_);

        uint32 MinPlayersPerTeam = fields[1].GetUInt32();
        uint32 MaxPlayersPerTeam = fields[2].GetUInt32();
        uint32 MinLvl = fields[3].GetUInt32();
        uint32 MaxLvl = fields[4].GetUInt32();

        float AStartLoc[4];
        float HStartLoc[4];

        uint32 start1 = fields[5].GetUInt32();

        WorldSafeLocsEntry const* start = sWorldSafeLocsStore.LookupEntry(start1);
        if (start)
        {
            AStartLoc[0] = start->x;
            AStartLoc[1] = start->y;
            AStartLoc[2] = start->z;
            AStartLoc[3] = fields[6].GetFloat();
        }
        else
        {
            sLog.outErrorDb("Table `battleground_template` for id %u have nonexistent WorldSafeLocs.dbc id %u in field `AllianceStartLoc`. BG not created.", bgTypeID, start1);
            continue;
        }

        uint32 start2 = fields[7].GetUInt32();

        start = sWorldSafeLocsStore.LookupEntry(start2);
        if (start)
        {
            HStartLoc[0] = start->x;
            HStartLoc[1] = start->y;
            HStartLoc[2] = start->z;
            HStartLoc[3] = fields[8].GetFloat();
        }
        else
        {
            sLog.outErrorDb("Table `battleground_template` for id %u refers to a non-existing WorldSafeLocs.dbc id %u in field `HordeStartLoc`. BG not created.", bgTypeID, start2);
            continue;
        }

        uint32 mapId = GetBattleGrounMapIdByTypeId(bgTypeID);
        char const* name;

        if (MapEntry const* mapEntry = sMapStore.LookupEntry(mapId))
        {
            name = mapEntry->MapName_lang[sWorld.GetDefaultDbcLocale()];
        }
        else
        {
            sLog.outErrorDb("Table `battleground_template` for id %u associated with nonexistent map id %u.", bgTypeID, mapId);
            continue;
        }

        float startMaxDist = fields[9].GetFloat();

        if (!CreateBattleGround(bgTypeID, MinPlayersPerTeam, MaxPlayersPerTeam, MinLvl, MaxLvl, name, mapId, AStartLoc[0], AStartLoc[1], AStartLoc[2], AStartLoc[3], HStartLoc[0], HStartLoc[1], HStartLoc[2], HStartLoc[3], startMaxDist))
        {
            continue;
        }

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u battlegrounds", count);
    sLog.outString();
}

void BattleGroundMgr::BuildBattleGroundListPacket(WorldPacket* data, ObjectGuid guid, Player* plr, BattleGroundTypeId bgTypeId)
{
    if (!plr)
    {
        return;
    }

    uint32 mapId = GetBattleGrounMapIdByTypeId(bgTypeId);

    data->Initialize(SMSG_BATTLEFIELD_LIST);
    *data << guid;
    *data << uint32(mapId);
    *data << uint8(0x00);

    {

        uint32 bracket_id = plr->GetBattleGroundBracketIdFromLevel(bgTypeId);
        ClientBattleGroundIdSet const& ids = m_ClientBattleGroundIds[bgTypeId][bracket_id];
        *data << uint32(ids.size());
        for (std::set<uint32>::const_iterator itr = ids.begin(); itr != ids.end(); ++itr)
        {
            *data << uint32(*itr);

        }

    }
}

void BattleGroundMgr::SendToBattleGround(Player* pl, uint32 instanceId, BattleGroundTypeId bgTypeId)
{
    BattleGround* bg = GetBattleGround(instanceId, bgTypeId);
    if (bg)
    {
        uint32 mapid = bg->GetMapId();
        float x, y, z, O;
        Team team = pl->Battle().Side();
        if (team == 0)
        {
            team = pl->GetTeam();
        }
        bg->GetTeamStartLoc(team, x, y, z, O);

        DETAIL_LOG("BATTLEGROUND: Sending %s to map %u, X %f, Y %f, Z %f, O %f", pl->GetName(), mapid, x, y, z, O);
        pl->TeleportTo(mapid, x, y, z, O);
    }
    else
    {
        sLog.outError("player %u trying to port to nonexistent bg instance %u", pl->GetGUIDLow(), instanceId);
    }
}
