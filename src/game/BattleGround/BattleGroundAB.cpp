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

#include "Utilities/Errors.h"
#include <vector>
#include "Object.h"
#include "Player.h"
#include "BattleGround.h"
#include "BattleGroundAB.h"
#include "GameObject.h"
#include "BattleGroundMgr.h"
#include "Language.h"
#include "WorldPacket.h"

#include "DBCStores.h"

BattleGroundAB::BattleGroundAB()
    : m_IsInformedNearVictory(false),
    m_honorTicks(0),
    m_ReputationTics(0)
{

    m_StartMessageIds[BG_STARTING_EVENT_FIRST] = 0;
    m_StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_AB_START_ONE_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_THIRD] = LANG_BG_AB_START_HALF_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_AB_HAS_BEGUN;

    for (uint8 i = 0; i < BG_AB_NODES_MAX; ++i)
    {

        m_BannerTimers[i].timer = 0;
        m_BannerTimers[i].type = 0;
        m_BannerTimers[i].teamIndex = 0;

        m_Nodes[i] = BG_AB_NODE_TYPE_NEUTRAL;

        m_NodeTimers[i] = 0;

        m_prevNodes[i] = 0;
    }

    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        m_ReputationScoreTics[i] = 0;
        m_honorScoreTicks[i] = 0;
        m_lastTick[i] = 0;
    }
}

BattleGroundAB::~BattleGroundAB()
{}

void BattleGroundAB::Update(uint32 diff)
{

    BattleGround::Update(diff);

    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        int team_points[PVP_TEAM_COUNT] = { 0, 0 };

        for (uint8 node = 0; node < BG_AB_NODES_MAX; ++node)
        {

            if (m_BannerTimers[node].timer)
            {
                if (m_BannerTimers[node].timer > diff)
                {
                    m_BannerTimers[node].timer -= diff;
                }
                else
                {
                    m_BannerTimers[node].timer = 0;
                    _CreateBanner(node, m_BannerTimers[node].type, m_BannerTimers[node].teamIndex, false);
                }
            }

            if (m_NodeTimers[node])
            {
                if (m_NodeTimers[node] > diff)
                {
                    m_NodeTimers[node] -= diff;
                }
                else
                {
                    m_NodeTimers[node] = 0;

                    uint8 teamIndex = m_Nodes[node] - 1;
                    m_prevNodes[node] = m_Nodes[node];
                    m_Nodes[node] += 2;

                    _CreateBanner(node, BG_AB_NODE_TYPE_OCCUPIED, teamIndex, true);
                    _SendNodeUpdate(node);
                    _NodeOccupied(node, (teamIndex == 0) ? ALLIANCE : HORDE);

                    if (teamIndex == 0)
                    {
                        SendMessage2ToAll(LANG_BG_AB_NODE_TAKEN, CHAT_MSG_BG_SYSTEM_ALLIANCE, nullptr, LANG_BG_ALLY, _GetNodeNameId(node));
                        PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_ALLIANCE);
                    }
                    else
                    {
                        SendMessage2ToAll(LANG_BG_AB_NODE_TAKEN, CHAT_MSG_BG_SYSTEM_HORDE, nullptr, LANG_BG_HORDE, _GetNodeNameId(node));
                        PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_HORDE);
                    }
                }
            }

            for (uint8 team = 0; team < PVP_TEAM_COUNT; ++team)
            {
                if (m_Nodes[node] == team + BG_AB_NODE_TYPE_OCCUPIED)
                {
                    ++team_points[team];
                }
            }
        }

        for (uint8 team = 0; team < PVP_TEAM_COUNT; ++team)
        {
            int points = team_points[team];
            if (!points)
            {
                continue;
            }
            m_lastTick[team] += diff;
            if (m_lastTick[team] > BG_AB_TickIntervals[points])
            {
                m_lastTick[team] -= BG_AB_TickIntervals[points];
                m_TeamScores[team] += BG_AB_TickPoints[points];
                m_honorScoreTicks[team] += BG_AB_TickPoints[points];
                m_ReputationScoreTics[team] += BG_AB_TickPoints[points];
                if (m_ReputationScoreTics[team] >= m_ReputationTics)
                {
                    (team == TEAM_INDEX_ALLIANCE) ? RewardReputationToTeam(509, 10, ALLIANCE) : RewardReputationToTeam(510, 10, HORDE);
                    m_ReputationScoreTics[team] -= m_ReputationTics;
                }
                if (m_honorScoreTicks[team] >= m_honorTicks)
                {
                    RewardHonorToTeam(BG_AB_PerTickHonor[GetBracketId()], (team == TEAM_INDEX_ALLIANCE) ? ALLIANCE : HORDE);
                    m_honorScoreTicks[team] -= m_honorTicks;
                }
                if (!m_IsInformedNearVictory && m_TeamScores[team] > BG_AB_WARNING_NEAR_VICTORY_SCORE)
                {
                    if (team == TEAM_INDEX_ALLIANCE)
                    {
                        SendMessageToAll(LANG_BG_AB_A_NEAR_VICTORY, CHAT_MSG_BG_SYSTEM_NEUTRAL);
                    }
                    else
                    {
                        SendMessageToAll(LANG_BG_AB_H_NEAR_VICTORY, CHAT_MSG_BG_SYSTEM_NEUTRAL);
                    }
                    PlaySoundToAll(BG_AB_SOUND_NEAR_VICTORY);
                    m_IsInformedNearVictory = true;
                }

                if (m_TeamScores[team] > BG_AB_MAX_TEAM_SCORE)
                {
                    m_TeamScores[team] = BG_AB_MAX_TEAM_SCORE;
                }
                if (team == TEAM_INDEX_ALLIANCE)
                {
                    UpdateWorldState(BG_AB_OP_RESOURCES_ALLY, m_TeamScores[team]);
                }
                if (team == TEAM_INDEX_HORDE)
                {
                    UpdateWorldState(BG_AB_OP_RESOURCES_HORDE, m_TeamScores[team]);
                }
            }
        }

        if (m_TeamScores[TEAM_INDEX_ALLIANCE] >= BG_AB_MAX_TEAM_SCORE)
        {
            EndBattleGround(ALLIANCE);
        }
        if (m_TeamScores[TEAM_INDEX_HORDE] >= BG_AB_MAX_TEAM_SCORE)
        {
            EndBattleGround(HORDE);
        }
    }
}

void BattleGroundAB::StartingEventOpenDoors()
{
    OpenDoorEvent(BG_EVENT_DOOR);
}

void BattleGroundAB::AddPlayer(Player* plr)
{

    BattleGround::AddPlayer(plr);

    BattleGroundABScore* sc = new BattleGroundABScore;
    m_PlayerScores[plr->GetObjectGuid()] = sc;
}

void BattleGroundAB::RemovePlayer(Player* , ObjectGuid )
{

}

bool BattleGroundAB::HandleAreaTrigger(Player* source, uint32 trigger)
{
    switch (trigger)
    {
        case 3948:
            if (source->GetTeam() != ALLIANCE)
            {
                source->GetSession()->SendNotification(LANG_BATTLEGROUND_ONLY_ALLIANCE_USE);
            }
            else
            {
                source->Battle().Leave();
            }
            break;
        case 3949:
            if (source->GetTeam() != HORDE)
            {
                source->GetSession()->SendNotification(LANG_BATTLEGROUND_ONLY_HORDE_USE);
            }
            else
            {
                source->Battle().Leave();
            }
            break;
        default:
            return false;
    }
    return true;
}

void BattleGroundAB::_CreateBanner(uint8 node, uint8 type, uint8 teamIndex, bool delay)
{

    if (delay)
    {
        m_BannerTimers[node].timer = 2000;
        m_BannerTimers[node].type = type;
        m_BannerTimers[node].teamIndex = teamIndex;
        return;
    }

    if (type != BG_AB_NODE_TYPE_NEUTRAL)
    {
        type += teamIndex;
    }

    SpawnEvent(node, type, true);
}

int32 BattleGroundAB::_GetNodeNameId(uint8 node)
{
    switch (node)
    {
        case BG_AB_NODE_STABLES:    return LANG_BG_AB_NODE_STABLES;
        case BG_AB_NODE_BLACKSMITH: return LANG_BG_AB_NODE_BLACKSMITH;
        case BG_AB_NODE_FARM:       return LANG_BG_AB_NODE_FARM;
        case BG_AB_NODE_LUMBER_MILL: return LANG_BG_AB_NODE_LUMBER_MILL;
        case BG_AB_NODE_GOLD_MINE:  return LANG_BG_AB_NODE_GOLD_MINE;
        default:
            MANGOS_ASSERT(0);
    }
    return 0;
}

void BattleGroundAB::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    const uint8 plusArray[] = { 0, 2, 3, 0, 1 };

    for (uint8 node = 0; node < BG_AB_NODES_MAX; ++node)
    {
        FillInitialWorldState(data, count, BG_AB_OP_NODEICONS[node], m_Nodes[node] == 0);
    }

    for (uint8 node = 0; node < BG_AB_NODES_MAX; ++node)
    {
        for (uint8 i = 1; i < BG_AB_NODES_MAX; ++i)
        {
            FillInitialWorldState(data, count, BG_AB_OP_NODESTATES[node] + plusArray[i], m_Nodes[node] == i);
        }
    }

    uint8 ally = 0, horde = 0;
    for (uint8 node = 0; node < BG_AB_NODES_MAX; ++node)
    {
        if (m_Nodes[node] == BG_AB_NODE_STATUS_ALLY_OCCUPIED)
        {
            ++ally;
        }
        else if (m_Nodes[node] == BG_AB_NODE_STATUS_HORDE_OCCUPIED)
        {
            ++horde;
        }
    }

    FillInitialWorldState(data, count, BG_AB_OP_OCCUPIED_BASES_ALLY, ally);
    FillInitialWorldState(data, count, BG_AB_OP_OCCUPIED_BASES_HORDE, horde);

    FillInitialWorldState(data, count, BG_AB_OP_RESOURCES_MAX, BG_AB_MAX_TEAM_SCORE);
    FillInitialWorldState(data, count, BG_AB_OP_RESOURCES_WARNING, BG_AB_WARNING_NEAR_VICTORY_SCORE);
    FillInitialWorldState(data, count, BG_AB_OP_RESOURCES_ALLY, m_TeamScores[TEAM_INDEX_ALLIANCE]);
    FillInitialWorldState(data, count, BG_AB_OP_RESOURCES_HORDE, m_TeamScores[TEAM_INDEX_HORDE]);

    FillInitialWorldState(data, count, 0x745, 0x2);
}

void BattleGroundAB::_SendNodeUpdate(uint8 node)
{

    const uint8 plusArray[] = { 0, 2, 3, 0, 1 };

    if (m_prevNodes[node])
    {
        UpdateWorldState(BG_AB_OP_NODESTATES[node] + plusArray[m_prevNodes[node]], WORLD_STATE_REMOVE);
    }
    else
    {
        UpdateWorldState(BG_AB_OP_NODEICONS[node], WORLD_STATE_REMOVE);
    }

    UpdateWorldState(BG_AB_OP_NODESTATES[node] + plusArray[m_Nodes[node]], WORLD_STATE_ADD);

    uint8 ally = 0, horde = 0;
    for (uint8 i = 0; i < BG_AB_NODES_MAX; ++i)
    {
        if (m_Nodes[i] == BG_AB_NODE_STATUS_ALLY_OCCUPIED)
        {
            ++ally;
        }
        else if (m_Nodes[i] == BG_AB_NODE_STATUS_HORDE_OCCUPIED)
        {
            ++horde;
        }
    }

    UpdateWorldState(BG_AB_OP_OCCUPIED_BASES_ALLY, ally);
    UpdateWorldState(BG_AB_OP_OCCUPIED_BASES_HORDE, horde);
}

void BattleGroundAB::_NodeOccupied(uint8 node, Team team)
{
    uint8 capturedNodes = 0;
    for (uint8 i = 0; i < BG_AB_NODES_MAX; ++i)
    {
        if (m_Nodes[node] == GetTeamIndexByTeamId(team) + BG_AB_NODE_TYPE_OCCUPIED && !m_NodeTimers[i])
        {
            ++capturedNodes;
        }
    }
    if (capturedNodes >= 5)
    {
        CastSpellOnTeam(SPELL_AB_QUEST_REWARD_5_BASES, team);
    }
    if (capturedNodes >= 4)
    {
        CastSpellOnTeam(SPELL_AB_QUEST_REWARD_4_BASES, team);
    }
}

void BattleGroundAB::EventPlayerClickedOnFlag(Player* source, GameObject* target_obj)
{

    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }

    uint8 event = (sBattleGroundMgr.GetGameObjectEventIndex(target_obj->GetGUIDLow())).event1;
    if (event >= BG_AB_NODES_MAX)
    {
        return;
    }
    BG_AB_Nodes node = BG_AB_Nodes(event);

    PvpTeamIndex teamIndex = GetTeamIndexByTeamId(source->GetTeam());

    if (!(m_Nodes[node] == 0 || teamIndex == m_Nodes[node] % 2))
    {
        return;
    }

    source->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
    uint32 sound;

    if (m_Nodes[node] == BG_AB_NODE_TYPE_NEUTRAL)
    {
        UpdatePlayerScore(source, SCORE_BASES_ASSAULTED, 1);
        m_prevNodes[node] = m_Nodes[node];
        m_Nodes[node] = teamIndex + 1;

        _CreateBanner(node, BG_AB_NODE_TYPE_CONTESTED, teamIndex, true);
        _SendNodeUpdate(node);
        m_NodeTimers[node] = BG_AB_FLAG_CAPTURING_TIME;

        if (teamIndex == 0)
        {
            SendMessage2ToAll(LANG_BG_AB_NODE_CLAIMED, CHAT_MSG_BG_SYSTEM_ALLIANCE, source, _GetNodeNameId(node), LANG_BG_ALLY);
        }
        else
        {
            SendMessage2ToAll(LANG_BG_AB_NODE_CLAIMED, CHAT_MSG_BG_SYSTEM_HORDE, source, _GetNodeNameId(node), LANG_BG_HORDE);
        }

        sound = BG_AB_SOUND_NODE_CLAIMED;
    }

    else if ((m_Nodes[node] == BG_AB_NODE_STATUS_ALLY_CONTESTED) || (m_Nodes[node] == BG_AB_NODE_STATUS_HORDE_CONTESTED))
    {

        if (m_prevNodes[node] < BG_AB_NODE_TYPE_OCCUPIED)
        {
            UpdatePlayerScore(source, SCORE_BASES_ASSAULTED, 1);
            m_prevNodes[node] = m_Nodes[node];
            m_Nodes[node] = teamIndex + BG_AB_NODE_TYPE_CONTESTED;

            _CreateBanner(node, BG_AB_NODE_TYPE_CONTESTED, teamIndex, true);
            _SendNodeUpdate(node);
            m_NodeTimers[node] = BG_AB_FLAG_CAPTURING_TIME;

            if (teamIndex == TEAM_INDEX_ALLIANCE)
            {
                SendMessage2ToAll(LANG_BG_AB_NODE_ASSAULTED, CHAT_MSG_BG_SYSTEM_ALLIANCE, source, _GetNodeNameId(node));
            }
            else
            {
                SendMessage2ToAll(LANG_BG_AB_NODE_ASSAULTED, CHAT_MSG_BG_SYSTEM_HORDE, source, _GetNodeNameId(node));
            }
        }

        else
        {
            UpdatePlayerScore(source, SCORE_BASES_DEFENDED, 1);
            m_prevNodes[node] = m_Nodes[node];
            m_Nodes[node] = teamIndex + BG_AB_NODE_TYPE_OCCUPIED;

            _CreateBanner(node, BG_AB_NODE_TYPE_OCCUPIED, teamIndex, true);
            _SendNodeUpdate(node);
            m_NodeTimers[node] = 0;
            _NodeOccupied(node, (teamIndex == TEAM_INDEX_ALLIANCE) ? ALLIANCE : HORDE);

            if (teamIndex == TEAM_INDEX_ALLIANCE)
            {
                SendMessage2ToAll(LANG_BG_AB_NODE_DEFENDED, CHAT_MSG_BG_SYSTEM_ALLIANCE, source, _GetNodeNameId(node));
            }
            else
            {
                SendMessage2ToAll(LANG_BG_AB_NODE_DEFENDED, CHAT_MSG_BG_SYSTEM_HORDE, source, _GetNodeNameId(node));
            }
        }
        sound = (teamIndex == TEAM_INDEX_ALLIANCE) ? BG_AB_SOUND_NODE_ASSAULTED_ALLIANCE : BG_AB_SOUND_NODE_ASSAULTED_HORDE;
    }

    else
    {
        UpdatePlayerScore(source, SCORE_BASES_ASSAULTED, 1);
        m_prevNodes[node] = m_Nodes[node];
        m_Nodes[node] = teamIndex + BG_AB_NODE_TYPE_CONTESTED;

        _CreateBanner(node, BG_AB_NODE_TYPE_CONTESTED, teamIndex, true);
        _SendNodeUpdate(node);
        m_NodeTimers[node] = BG_AB_FLAG_CAPTURING_TIME;

        if (teamIndex == TEAM_INDEX_ALLIANCE)
        {
            SendMessage2ToAll(LANG_BG_AB_NODE_ASSAULTED, CHAT_MSG_BG_SYSTEM_ALLIANCE, source, _GetNodeNameId(node));
        }
        else
        {
            SendMessage2ToAll(LANG_BG_AB_NODE_ASSAULTED, CHAT_MSG_BG_SYSTEM_HORDE, source, _GetNodeNameId(node));
        }

        sound = (teamIndex == TEAM_INDEX_ALLIANCE) ? BG_AB_SOUND_NODE_ASSAULTED_ALLIANCE : BG_AB_SOUND_NODE_ASSAULTED_HORDE;
    }

    if (m_Nodes[node] >= BG_AB_NODE_TYPE_OCCUPIED)
    {
        if (teamIndex == TEAM_INDEX_ALLIANCE)
        {
            SendMessage2ToAll(LANG_BG_AB_NODE_TAKEN, CHAT_MSG_BG_SYSTEM_ALLIANCE, nullptr, LANG_BG_ALLY, _GetNodeNameId(node));
        }
        else
        {
            SendMessage2ToAll(LANG_BG_AB_NODE_TAKEN, CHAT_MSG_BG_SYSTEM_HORDE, nullptr, LANG_BG_HORDE, _GetNodeNameId(node));
        }
    }
    PlaySoundToAll(sound);
}

void BattleGroundAB::Reset()
{

    BattleGround::Reset();

    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        m_TeamScores[i] = 0;
        m_lastTick[i] = 0;
        m_honorScoreTicks[i] = 0;
        m_ReputationScoreTics[i] = 0;
    }

    m_IsInformedNearVictory = false;
    bool isBGWeekend = BattleGroundMgr::IsBGWeekend(GetTypeID());
    m_honorTicks = isBGWeekend ? AB_WEEKEND_HONOR_INTERVAL : AB_NORMAL_HONOR_INTERVAL;
    m_ReputationTics = isBGWeekend ? AB_WEEKEND_REPUTATION_INTERVAL : AB_NORMAL_REPUTATION_INTERVAL;

    for (uint8 i = 0; i < BG_AB_NODES_MAX; ++i)
    {
        m_Nodes[i] = 0;
        m_prevNodes[i] = 0;
        m_NodeTimers[i] = 0;
        m_BannerTimers[i].timer = 0;

        m_ActiveEvents[i] = BG_AB_NODE_TYPE_NEUTRAL;
    }
}

void BattleGroundAB::EndBattleGround(Team winner)
{

    if (winner == ALLIANCE)
    {
        RewardHonorToTeam(BG_AB_WinMatchHonor[GetBracketId()], ALLIANCE);
    }
    if (winner == HORDE)
    {
        RewardHonorToTeam(BG_AB_WinMatchHonor[GetBracketId()], HORDE);
    }

    BattleGround::EndBattleGround(winner);
}

WorldSafeLocsEntry const* BattleGroundAB::GetClosestGraveYard(Player* player)
{

    PvpTeamIndex teamIndex = GetTeamIndexByTeamId(player->GetTeam());

    std::vector<uint8> nodes;
    for (uint8 i = 0; i < BG_AB_NODES_MAX; ++i)
    {
        if (m_Nodes[i] == teamIndex + 3)
        {
            nodes.push_back(i);
        }
    }

    WorldSafeLocsEntry const* good_entry = nullptr;

    if (!nodes.empty())
    {
        float plr_x = player->Where().X();
        float plr_y = player->Where().Y();

        float mindist = 999999.0f;
        for (uint8 i = 0; i < nodes.size(); ++i)
        {
            WorldSafeLocsEntry const* entry = sWorldSafeLocsStore.LookupEntry(BG_AB_GraveyardIds[nodes[i]]);
            if (!entry)
            {
                continue;
            }
            float dist = (entry->x - plr_x) * (entry->x - plr_x) + (entry->y - plr_y) * (entry->y - plr_y);
            if (mindist > dist)
            {
                mindist = dist;
                good_entry = entry;
            }
        }
        nodes.clear();
    }

    if (!good_entry)
    {
        good_entry = sWorldSafeLocsStore.LookupEntry(BG_AB_GraveyardIds[teamIndex + 5]);
    }

    return good_entry;
}

void BattleGroundAB::UpdatePlayerScore(Player* source, uint32 type, uint32 value)
{

    BattleGroundScoreMap::iterator itr = m_PlayerScores.find(source->GetObjectGuid());
    if (itr == m_PlayerScores.end())
    {
        return;
    }

    switch (type)
    {
        case SCORE_BASES_ASSAULTED:
            ((BattleGroundABScore*)itr->second)->BasesAssaulted += value;
            break;
        case SCORE_BASES_DEFENDED:
            ((BattleGroundABScore*)itr->second)->BasesDefended += value;
            break;
        default:
            BattleGround::UpdatePlayerScore(source, type, value);
            break;
    }
}

Team BattleGroundAB::GetPrematureWinner()
{

    int32 hordeScore = m_TeamScores[TEAM_INDEX_HORDE];
    int32 allianceScore = m_TeamScores[TEAM_INDEX_ALLIANCE];

    if (hordeScore > allianceScore)
    {
        return HORDE;
    }

    if (allianceScore > hordeScore)
    {
        return ALLIANCE;
    }

    return BattleGround::GetPrematureWinner();
}
