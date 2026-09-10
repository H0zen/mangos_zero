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

#include "BattleGroundMgr.h"
#include "Platform/Define.h"
#include <algorithm>
#include "SharedDefines.h"
#include "Player.h"
#include "BattleGroundAV.h"
#include "BattleGroundAB.h"
#include "BattleGroundWS.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "Chat.h"
#include "World.h"
#include "WorldPacket.h"
#include "GameEventMgr.h"
#include "Formulas.h"
#include "DisableMgr.h"
#include "GameTime.h"
#include "Policies/Singleton.h"
#include "Language.h"

void BattleGroundQueue::SelectionPool::Init()
{
    SelectedGroups.clear();
    PlayerCount = 0;
}

bool BattleGroundQueue::SelectionPool::KickGroup(uint32 size)
{

    bool found = false;
    GroupsQueueType::iterator groupToKick = SelectedGroups.begin();
    for (GroupsQueueType::iterator itr = groupToKick; itr != SelectedGroups.end(); ++itr)
    {
        if (abs((int32)((*itr)->Players.size() - size)) <= 1)
        {
            groupToKick = itr;
            found = true;
        }
        else if (!found && (*itr)->Players.size() >= (*groupToKick)->Players.size())
        {
            groupToKick = itr;
        }
    }

    if (GetPlayerCount())
    {

        GroupQueueInfo* ginfo = (*groupToKick);
        SelectedGroups.erase(groupToKick);
        PlayerCount -= ginfo->Players.size();

        if (ginfo->Players.size() <= size + 1)
        {
            return false;
        }
    }
    return true;
}

bool BattleGroundQueue::SelectionPool::AddGroup(GroupQueueInfo* ginfo, uint32 desiredCount)
{

    if (!ginfo->IsInvitedToBGInstanceGUID && desiredCount >= PlayerCount + ginfo->Players.size())
    {
        SelectedGroups.push_back(ginfo);

        PlayerCount += ginfo->Players.size();
        return true;
    }
    if (PlayerCount < desiredCount)
    {
        return true;
    }
    return false;
}

GroupQueueInfo* BattleGroundQueue::AddGroup(Player* leader, Group* grp, BattleGroundTypeId BgTypeId, BattleGroundBracketId bracketId, bool isPremade)
{

    GroupQueueInfo* ginfo = new GroupQueueInfo;
    ginfo->BgTypeId                  = BgTypeId;
    ginfo->IsInvitedToBGInstanceGUID = 0;
    ginfo->JoinTime                  = GameTime::GetGameTimeMS();
    ginfo->RemoveInviteTime          = 0;
    ginfo->GroupTeam                 = leader->GetTeam();

    ginfo->Players.clear();

    uint32 index = 0;
    if (!isPremade)
    {
        index += PVP_TEAM_COUNT;
    }

    if (ginfo->GroupTeam == HORDE)
    {
        ++index;
    }

    DEBUG_LOG("Adding Group to BattleGroundQueue bgTypeId : %u, bracket_id : %u, index : %u", BgTypeId, bracketId, index);

    uint32 lastOnlineTime = GameTime::GetGameTimeMS();

    {
        if (grp)
        {
            for (GroupReference* itr = grp->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->getSource();
                if (!member)
                {
                    continue;
                }
                PlayerQueueInfo& pl_info = m_QueuedPlayers[member->GetObjectGuid()];
                pl_info.LastOnlineTime   = lastOnlineTime;
                pl_info.GroupInfo        = ginfo;

                ginfo->Players[member->GetObjectGuid()]  = &pl_info;
            }
        }
        else
        {
            PlayerQueueInfo& pl_info = m_QueuedPlayers[leader->GetObjectGuid()];
            pl_info.LastOnlineTime   = lastOnlineTime;
            pl_info.GroupInfo        = ginfo;
            ginfo->Players[leader->GetObjectGuid()]  = &pl_info;
        }

        m_QueuedGroups[bracketId][index].push_back(ginfo);

        if (!isPremade && sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN))
        {
            if (BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(ginfo->BgTypeId))
            {
                char const* bgName = bg->GetName();
                uint32 MinPlayers = bg->GetMinPlayersPerTeam();
                uint32 qHorde = 0;
                uint32 qAlliance = 0;
                uint32 q_min_level = leader->GetMinLevelForBattleGroundBracketId(bracketId, BgTypeId);
                GroupsQueueType::const_iterator itr;
                for (itr = m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE].begin(); itr != m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE].end(); ++itr)
                {
                    if (!(*itr)->IsInvitedToBGInstanceGUID)
                    {
                        qAlliance += (*itr)->Players.size();
                    }
                }

                for (itr = m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_HORDE].begin(); itr != m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_HORDE].end(); ++itr)
                {
                    if (!(*itr)->IsInvitedToBGInstanceGUID)
                    {
                        qHorde += (*itr)->Players.size();
                    }
                }

                if (sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN) == 1)
                {
                    ChatHandler(leader).PSendSysMessage(LANG_BG_QUEUE_ANNOUNCE_SELF, bgName, q_min_level, q_min_level + 10,
                        qAlliance, (MinPlayers > qAlliance) ? MinPlayers - qAlliance : (uint32)0, qHorde, (MinPlayers > qHorde) ? MinPlayers - qHorde : (uint32)0);
                }

                else
                {
                    sWorld.SendWorldText(LANG_BG_QUEUE_ANNOUNCE_WORLD, bgName, q_min_level, q_min_level + 10,
                        qAlliance, (MinPlayers > qAlliance) ? MinPlayers - qAlliance : (uint32)0, qHorde, (MinPlayers > qHorde) ? MinPlayers - qHorde : (uint32)0);
                }
            }
        }

    }

    return ginfo;
}

void BattleGroundQueue::PlayerInvitedToBGUpdateAverageWaitTime(GroupQueueInfo* ginfo, BattleGroundBracketId bracket_id)
{
    uint32 timeInQueue = getMSTimeDiff(ginfo->JoinTime, GameTime::GetGameTimeMS());
    uint8 team_index = TEAM_INDEX_ALLIANCE;

    if (ginfo->GroupTeam == HORDE)
    {
        team_index = TEAM_INDEX_HORDE;
    }

    uint32* lastPlayerAddedPointer = &(m_WaitTimeLastPlayer[team_index][bracket_id]);

    m_SumOfWaitTimes[team_index][bracket_id] -= m_WaitTimes[team_index][bracket_id][(*lastPlayerAddedPointer)];

    m_WaitTimes[team_index][bracket_id][(*lastPlayerAddedPointer)] = timeInQueue;

    m_SumOfWaitTimes[team_index][bracket_id] += timeInQueue;

    (*lastPlayerAddedPointer)++;
    (*lastPlayerAddedPointer) %= COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME;
}

uint32 BattleGroundQueue::GetAverageQueueWaitTime(GroupQueueInfo* ginfo, BattleGroundBracketId bracket_id)
{
    uint8 team_index = TEAM_INDEX_ALLIANCE;
    if (ginfo->GroupTeam == HORDE)
    {
        team_index = TEAM_INDEX_HORDE;
    }

    if (m_WaitTimes[team_index][bracket_id][COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME - 1])
    {
        return (m_SumOfWaitTimes[team_index][bracket_id] / COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME);
    }
    else

    {
        return 0;
    }
}

void BattleGroundQueue::RemovePlayer(ObjectGuid guid, bool decreaseInvitedCount)
{
    int32 bracket_id = -1;
    QueuedPlayersMap::iterator itr;

    itr = m_QueuedPlayers.find(guid);
    if (itr == m_QueuedPlayers.end())
    {
        sLog.outError("BattleGroundQueue: couldn't find for remove: %s", GuidString(guid).c_str());
        return;
    }

    GroupQueueInfo* group = itr->second.GroupInfo;
    GroupsQueueType::iterator group_itr, group_itr_tmp;

    uint32 index = BattleGround::GetTeamIndexByTeamId(group->GroupTeam);

    for (int8 bracket_id_tmp = MAX_BATTLEGROUND_BRACKETS - 1; bracket_id_tmp >= 0 && bracket_id == -1; --bracket_id_tmp)
    {

        for (uint8 j = index; j < BG_QUEUE_GROUP_TYPES_COUNT; j += BG_QUEUE_NORMAL_ALLIANCE)
        {
            for (group_itr_tmp = m_QueuedGroups[bracket_id_tmp][j].begin(); group_itr_tmp != m_QueuedGroups[bracket_id_tmp][j].end(); ++group_itr_tmp)
            {
                if ((*group_itr_tmp) == group)
                {
                    bracket_id = bracket_id_tmp;
                    group_itr = group_itr_tmp;

                    index = j;
                    break;
                }
            }
        }
    }

    if (bracket_id == -1)
    {
        sLog.outError("BattleGroundQueue: ERROR Can not find groupinfo for %s", GuidString(guid).c_str());
        return;
    }
    DEBUG_LOG("BattleGroundQueue: Removing %s, from bracket_id %u", GuidString(guid).c_str(), (uint32)bracket_id);

    GroupQueueInfoPlayers::iterator pitr = group->Players.find(guid);
    if (pitr != group->Players.end())
    {
        group->Players.erase(pitr);
    }

    if (decreaseInvitedCount && group->IsInvitedToBGInstanceGUID)
    {
        BattleGround* bg = sBattleGroundMgr.GetBattleGround(group->IsInvitedToBGInstanceGUID, group->BgTypeId);
        if (bg)
        {
            bg->DecreaseInvitedCount(group->GroupTeam);
        }
    }

    m_QueuedPlayers.erase(itr);

    if (group->Players.empty())
    {
        m_QueuedGroups[bracket_id][index].erase(group_itr);
        delete group;
    }
}

bool BattleGroundQueue::IsPlayerInvited(ObjectGuid pl_guid, const uint32 bgInstanceGuid, const uint32 removeTime)
{
    QueuedPlayersMap::const_iterator qItr = m_QueuedPlayers.find(pl_guid);
    return (qItr != m_QueuedPlayers.end() &&
        qItr->second.GroupInfo->IsInvitedToBGInstanceGUID == bgInstanceGuid &&
        qItr->second.GroupInfo->RemoveInviteTime == removeTime);
}

bool BattleGroundQueue::GetPlayerGroupInfoData(ObjectGuid guid, GroupQueueInfo* ginfo)
{
    QueuedPlayersMap::const_iterator qItr = m_QueuedPlayers.find(guid);
    if (qItr == m_QueuedPlayers.end())
    {
        return false;
    }
    *ginfo = *(qItr->second.GroupInfo);
    return true;
}

bool BattleGroundQueue::InviteGroupToBG(GroupQueueInfo* ginfo, BattleGround* bg, Team side)
{

    if (side)
    {
        ginfo->GroupTeam = side;
    }

    if (!ginfo->IsInvitedToBGInstanceGUID)
    {

        ginfo->IsInvitedToBGInstanceGUID = bg->GetInstanceID();
        BattleGroundTypeId bgTypeId = bg->GetTypeID();
        BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(bgTypeId);
        BattleGroundBracketId bracket_id = bg->GetBracketId();

        ginfo->RemoveInviteTime = GameTime::GetGameTimeMS() + INVITE_ACCEPT_WAIT_TIME;

        for (GroupQueueInfoPlayers::iterator itr = ginfo->Players.begin(); itr != ginfo->Players.end(); ++itr)
        {

            Player* plr = sObjectMgr.GetPlayer(itr->first);

            if (!plr)
            {
                continue;
            }

            PlayerInvitedToBGUpdateAverageWaitTime(ginfo, bracket_id);

            bg->IncreaseInvitedCount(ginfo->GroupTeam);

            plr->Queues().CalledTo(bgQueueTypeId, ginfo->IsInvitedToBGInstanceGUID);

            BGQueueInviteEvent* inviteEvent = new BGQueueInviteEvent(plr->GetObjectGuid(), ginfo->IsInvitedToBGInstanceGUID, bgTypeId, ginfo->RemoveInviteTime);
            plr->m_Events.AddEvent(inviteEvent, plr->m_Events.CalculateTime(INVITATION_REMIND_TIME));

            BGQueueRemoveEvent* removeEvent = new BGQueueRemoveEvent(plr->GetObjectGuid(), ginfo->IsInvitedToBGInstanceGUID, bgTypeId, bgQueueTypeId, ginfo->RemoveInviteTime);
            plr->m_Events.AddEvent(removeEvent, plr->m_Events.CalculateTime(INVITE_ACCEPT_WAIT_TIME));

            WorldPacket data;

            uint32 queueSlot = plr->Queues().SlotOf(bgQueueTypeId);

            DEBUG_LOG("Battleground: invited %s to BG instance %u queueindex %u bgtype %u, I can't help it if they don't press the enter battle button.",
                plr->GetGuidStr().c_str(), bg->GetInstanceID(), queueSlot, bg->GetTypeID());

            sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_JOIN, INVITE_ACCEPT_WAIT_TIME, 0);
            plr->GetSession()->SendPacket(&data);
        }
        return true;
    }

    return false;
}

void BattleGroundQueue::FillPlayersToBG(BattleGround* bg, BattleGroundBracketId bracket_id)
{
    int32 hordeFree = bg->GetFreeSlotsForTeam(HORDE);
    int32 aliFree   = bg->GetFreeSlotsForTeam(ALLIANCE);

    GroupsQueueType::const_iterator Ali_itr = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE].begin();

    uint32 aliCount = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE].size();

    uint32 aliIndex = 0;
    for (; aliIndex < aliCount && m_SelectionPools[TEAM_INDEX_ALLIANCE].AddGroup((*Ali_itr), aliFree); ++aliIndex)
    {
        ++Ali_itr;
    }

    GroupsQueueType::const_iterator Horde_itr = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_HORDE].begin();
    uint32 hordeCount = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_HORDE].size();
    uint32 hordeIndex = 0;
    for (; hordeIndex < hordeCount && m_SelectionPools[TEAM_INDEX_HORDE].AddGroup((*Horde_itr), hordeFree); ++hordeIndex)
    {
        ++Horde_itr;
    }

    if (sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE) == 0)
    {
        return;
    }

    int32 diffAli   = aliFree   - int32(m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount());
    int32 diffHorde = hordeFree - int32(m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount());
    while (abs(diffAli - diffHorde) > 1 && (m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount() > 0 || m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() > 0))
    {

        if (diffAli < diffHorde)
        {

            if (m_SelectionPools[TEAM_INDEX_ALLIANCE].KickGroup(diffHorde - diffAli))
            {
                for (; aliIndex < aliCount && m_SelectionPools[TEAM_INDEX_ALLIANCE].AddGroup((*Ali_itr), (aliFree >= diffHorde) ? aliFree - diffHorde : 0); ++aliIndex)
                {
                    ++Ali_itr;
                }
            }

            if (!m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount())
            {
                if (aliFree <= diffHorde + 1)
                {
                    break;
                }
                m_SelectionPools[TEAM_INDEX_HORDE].KickGroup(diffHorde - diffAli);
            }
        }
        else
        {

            if (m_SelectionPools[TEAM_INDEX_HORDE].KickGroup(diffAli - diffHorde))
            {
                for (; hordeIndex < hordeCount && m_SelectionPools[TEAM_INDEX_HORDE].AddGroup((*Horde_itr), (hordeFree >= diffAli) ? hordeFree - diffAli : 0); ++hordeIndex)
                {
                    ++Horde_itr;
                }
            }
            if (!m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount())
            {
                if (hordeFree <= diffAli + 1)
                {
                    break;
                }
                m_SelectionPools[TEAM_INDEX_ALLIANCE].KickGroup(diffAli - diffHorde);
            }
        }

        diffAli   = aliFree   - int32(m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount());
        diffHorde = hordeFree - int32(m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount());
    }
}

bool BattleGroundQueue::CheckPremadeMatch(BattleGroundBracketId bracket_id, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam)
{

    if (!m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].empty() && !m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].empty())
    {

        GroupsQueueType::const_iterator ali_group, horde_group;
        for (ali_group = m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].begin(); ali_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].end(); ++ali_group)
        {
            if (!(*ali_group)->IsInvitedToBGInstanceGUID)
            {
                break;
            }
        }

        for (horde_group = m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].begin(); horde_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].end(); ++horde_group)
        {
            if (!(*horde_group)->IsInvitedToBGInstanceGUID)
            {
                break;
            }
        }

        if (ali_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].end() && horde_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].end())
        {
            m_SelectionPools[TEAM_INDEX_ALLIANCE].AddGroup((*ali_group), MaxPlayersPerTeam);
            m_SelectionPools[TEAM_INDEX_HORDE].AddGroup((*horde_group), MaxPlayersPerTeam);

            uint32 maxPlayers = std::max(m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount(), m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount());
            GroupsQueueType::const_iterator itr;
            for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
            {
                for (itr = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].begin(); itr != m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].end(); ++itr)
                {

                    if (!(*itr)->IsInvitedToBGInstanceGUID && !m_SelectionPools[i].AddGroup((*itr), maxPlayers))
                    {
                        break;
                    }
                }
            }

            return true;
        }
    }

    uint32 time_before = GameTime::GetGameTimeMS() - sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH);
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        if (!m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE + i].empty())
        {
            GroupsQueueType::iterator itr = m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE + i].begin();
            if (!(*itr)->IsInvitedToBGInstanceGUID && ((*itr)->JoinTime < time_before || (*itr)->Players.size() < MinPlayersPerTeam))
            {

                m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].push_front((*itr));
                m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE + i].erase(itr);
            }
        }
    }

    return false;
}

bool BattleGroundQueue::CheckNormalMatch(BattleGroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers)
{
    GroupsQueueType::const_iterator itr_team[PVP_TEAM_COUNT];
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        itr_team[i] = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].begin();
        for (; itr_team[i] != m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].end(); ++(itr_team[i]))
        {
            if (!(*(itr_team[i]))->IsInvitedToBGInstanceGUID)
            {
                m_SelectionPools[i].AddGroup(*(itr_team[i]), maxPlayers);
                if (m_SelectionPools[i].GetPlayerCount() >= minPlayers)
                {
                    break;
                }
            }
        }
    }

    uint32 j = TEAM_INDEX_ALLIANCE;
    if (m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount() < m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount())
    {
        j = TEAM_INDEX_HORDE;
    }

    if (sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE) != 0 &&
        m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount() >= minPlayers && m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() >= minPlayers)
    {

        ++(itr_team[j]);
        for (; itr_team[j] != m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + j].end(); ++(itr_team[j]))
        {
            if (!(*(itr_team[j]))->IsInvitedToBGInstanceGUID)
            {
                if (!m_SelectionPools[j].AddGroup(*(itr_team[j]), m_SelectionPools[(j + 1) % PVP_TEAM_COUNT].GetPlayerCount()))
                {
                    break;
                }
            }
        }

        if (abs((int32)(m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount() - m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount())) > 2)
        {
            return false;
        }
    }

    if (sBattleGroundMgr.isTesting() && (m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() || m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount()))
    {
        return true;
    }

    return m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() >= minPlayers && m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount() >= minPlayers;
}

void BattleGroundQueue::Update(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id)
{

    if (m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].empty() &&
        m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].empty() &&
        m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE].empty() &&
        m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_HORDE].empty())
    {
        return;
    }

    BGFreeSlotQueueType::iterator itr, next;
    for (itr = sBattleGroundMgr.BGFreeSlotQueue[bgTypeId].begin(); itr != sBattleGroundMgr.BGFreeSlotQueue[bgTypeId].end(); itr = next)
    {
        next = itr;
        ++next;

        if ((*itr)->GetTypeID() == bgTypeId && (*itr)->GetBracketId() == bracket_id &&
            (*itr)->GetStatus() > STATUS_WAIT_QUEUE && (*itr)->GetStatus() < STATUS_WAIT_LEAVE)
        {
            BattleGround* bg = *itr;

            m_SelectionPools[TEAM_INDEX_ALLIANCE].Init();
            m_SelectionPools[TEAM_INDEX_HORDE].Init();

            FillPlayersToBG(bg, bracket_id);

            for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_INDEX_ALLIANCE].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_INDEX_ALLIANCE].SelectedGroups.end(); ++citr)
            {
                InviteGroupToBG((*citr), bg, (*citr)->GroupTeam);
            }
            for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_INDEX_HORDE].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_INDEX_HORDE].SelectedGroups.end(); ++citr)
            {
                InviteGroupToBG((*citr), bg, (*citr)->GroupTeam);
            }

            if (!bg->HasFreeSlots())
            {

                bg->RemoveFromBGFreeSlotQueue();
            }
        }
    }

    BattleGround* bg_template = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
    if (!bg_template)
    {
        sLog.outError("Battleground: Update: bg template not found for %u", bgTypeId);
        return;
    }

    uint32 MinPlayersPerTeam = bg_template->GetMinPlayersPerTeam();
    uint32 MaxPlayersPerTeam = bg_template->GetMaxPlayersPerTeam();
    if (sBattleGroundMgr.isTesting())
    {
        MinPlayersPerTeam = 1;
    }

    m_SelectionPools[TEAM_INDEX_ALLIANCE].Init();
    m_SelectionPools[TEAM_INDEX_HORDE].Init();

    if (CheckPremadeMatch(bracket_id, MinPlayersPerTeam, MaxPlayersPerTeam))
    {

        BattleGround* bg2 = sBattleGroundMgr.CreateNewBattleGround(bgTypeId, bracket_id);
        if (!bg2)
        {
            sLog.outError("BattleGroundQueue::Update - Can not create battleground: %u", bgTypeId);
            return;
        }

        for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
        {
            for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_INDEX_ALLIANCE + i].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_INDEX_ALLIANCE + i].SelectedGroups.end(); ++citr)
            {
                InviteGroupToBG((*citr), bg2, (*citr)->GroupTeam);
            }
        }

        bg2->StartBattleGround();

        m_SelectionPools[TEAM_INDEX_ALLIANCE].Init();
        m_SelectionPools[TEAM_INDEX_HORDE].Init();
    }

    {

        if (CheckNormalMatch(bracket_id, MinPlayersPerTeam, MaxPlayersPerTeam))
        {

            BattleGround* bg2 = sBattleGroundMgr.CreateNewBattleGround(bgTypeId, bracket_id);
            if (!bg2)
            {
                sLog.outError("BattleGroundQueue::Update - Can not create battleground: %u", bgTypeId);
                return;
            }

            for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
            {
                for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_INDEX_ALLIANCE + i].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_INDEX_ALLIANCE + i].SelectedGroups.end(); ++citr)
                {
                    InviteGroupToBG((*citr), bg2, (*citr)->GroupTeam);
                }
            }

            bg2->StartBattleGround();
        }
    }
}

bool BGQueueInviteEvent::Execute(uint64 , uint32 )
{
    Player* plr = sObjectMgr.GetPlayer(m_PlayerGuid);

    if (!plr)
    {
        return true;
    }

    BattleGround* bg = sBattleGroundMgr.GetBattleGround(m_BgInstanceGUID, m_BgTypeId);

    if (!bg)
    {
        return true;
    }

    BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(bg->GetTypeID());
    uint32 queueSlot = plr->Queues().SlotOf(bgQueueTypeId);
    if (queueSlot < PLAYER_MAX_BATTLEGROUND_QUEUES)
    {

        BattleGroundQueue& bgQueue = sBattleGroundMgr.m_BattleGroundQueues[bgQueueTypeId];
        if (bgQueue.IsPlayerInvited(m_PlayerGuid, m_BgInstanceGUID, m_RemoveTime))
        {
            WorldPacket data;

            sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_JOIN, INVITE_ACCEPT_WAIT_TIME - INVITATION_REMIND_TIME, 0);
            plr->GetSession()->SendPacket(&data);
        }
    }
    return true;
}

void BGQueueInviteEvent::Abort(uint64 )
{

}

bool BGQueueRemoveEvent::Execute(uint64 , uint32 )
{
    Player* plr = sObjectMgr.GetPlayer(m_PlayerGuid);
    if (!plr)
    {

        return true;
    }

    BattleGround* bg = sBattleGroundMgr.GetBattleGround(m_BgInstanceGUID, m_BgTypeId);

    uint32 queueSlot = plr->Queues().SlotOf(m_BgQueueTypeId);
    if (queueSlot < PLAYER_MAX_BATTLEGROUND_QUEUES)
    {

        BattleGroundQueue& bgQueue = sBattleGroundMgr.m_BattleGroundQueues[m_BgQueueTypeId];
        if (bgQueue.IsPlayerInvited(m_PlayerGuid, m_BgInstanceGUID, m_RemoveTime))
        {
            DEBUG_LOG("Battleground: removing player %u from bg queue for instance %u because of not pressing enter battle in time.", plr->GetGUIDLow(), m_BgInstanceGUID);

            plr->Queues().Give(m_BgQueueTypeId);
            bgQueue.RemovePlayer(m_PlayerGuid, true);

            if (bg && bg->GetStatus() != STATUS_WAIT_LEAVE)
            {
                sBattleGroundMgr.ScheduleQueueUpdate(m_BgQueueTypeId, m_BgTypeId, bg->GetBracketId());
            }

            WorldPacket data;
            sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_NONE, 0, 0);
            plr->GetSession()->SendPacket(&data);
        }
    }

    return true;
}

void BGQueueRemoveEvent::Abort(uint64 )
{

}
