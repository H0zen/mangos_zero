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

#include <string>
#include "Object.h"
#include "Player.h"
#include "BattleGround.h"
#include "BattleGroundMgr.h"
#include "Creature.h"
#include "Language.h"
#include "SpellAuras.h"
#include "World.h"
#include "Group.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Mail.h"
#include "WorldPacket.h"
#include "Formulas.h"
#include "GridNotifiersImpl.h"
#include "Chat.h"
#include "PlayerRegistry.h"

namespace MaNGOS
{
    class BattleGroundChatBuilder
    {
        public:

            BattleGroundChatBuilder(ChatMsg msgtype, int32 textId, Player const* source, va_list* args = nullptr)
                : i_msgtype(msgtype), i_textId(textId), i_source(source), i_args(args) {}
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);

                ObjectGuid sourceGuid = i_source ? i_source->GetObjectGuid() : 0;
                std::string sourceName = i_source ? i_source->GetName() : "";

                if (i_args)
                {

                    va_list ap;
                    va_copy(ap, *i_args);

                    char str[2048];
                    vsnprintf(str, 2048, text, ap);
                    va_end(ap);

                    ChatHandler::BuildChatPacket(data, i_msgtype, &str[0], LANG_UNIVERSAL, CHAT_TAG_NONE, sourceGuid, sourceName.c_str());
                }
                else
                {
                    ChatHandler::BuildChatPacket(data, i_msgtype, text, LANG_UNIVERSAL, CHAT_TAG_NONE, sourceGuid, sourceName.c_str(), sourceGuid, sourceName.c_str());
                }
            }
        private:
            ChatMsg i_msgtype;
            int32 i_textId;
            Player const* i_source;
            va_list* i_args;
    };

    class BattleGroundYellBuilder
    {
        public:

            BattleGroundYellBuilder(Language language, int32 textId, Creature const* source, va_list* args = nullptr)
                : i_language(language), i_textId(textId), i_source(source), i_args(args) {}
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);

                if (i_args)
                {

                    va_list ap;
                    va_copy(ap, *i_args);

                    char str[2048];
                    vsnprintf(str, 2048, text, ap);
                    va_end(ap);

                    ChatHandler::BuildChatPacket(data, CHAT_MSG_MONSTER_YELL, &str[0], i_language, CHAT_TAG_NONE, i_source->GetObjectGuid(), i_source->GetName());
                }
                else
                {
                    ChatHandler::BuildChatPacket(data, CHAT_MSG_MONSTER_YELL, text, i_language, CHAT_TAG_NONE, i_source->GetObjectGuid(), i_source->GetName());
                }
            }
        private:
            Language i_language;
            int32 i_textId;
            Creature const* i_source;
            va_list* i_args;
    };

    class BattleGround2ChatBuilder
    {
        public:

            BattleGround2ChatBuilder(ChatMsg msgtype, int32 textId, Player const* source, int32 arg1, int32 arg2)
                : i_msgtype(msgtype), i_textId(textId), i_source(source), i_arg1(arg1), i_arg2(arg2) {}
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);
                char const* arg1str = i_arg1 ? sObjectMgr.GetMangosString(i_arg1, loc_idx) : "";
                char const* arg2str = i_arg2 ? sObjectMgr.GetMangosString(i_arg2, loc_idx) : "";

                char str[2048];
                snprintf(str, 2048, text, arg1str, arg2str);

                ObjectGuid guid = 0;
                if (i_source)
                {
                    guid = i_source->GetObjectGuid();
                }
                ChatHandler::BuildChatPacket(data, i_msgtype, str, LANG_UNIVERSAL, CHAT_TAG_NONE, guid);
            }
        private:
            ChatMsg i_msgtype;
            int32 i_textId;
            Player const* i_source;
            int32 i_arg1;
            int32 i_arg2;
    };

    class BattleGround2YellBuilder
    {
        public:

            BattleGround2YellBuilder(uint32 language, int32 textId, Creature const* source, int32 arg1, int32 arg2)
                : i_language(language), i_textId(textId), i_source(source), i_arg1(arg1), i_arg2(arg2) {}
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);
                char const* arg1str = i_arg1 ? sObjectMgr.GetMangosString(i_arg1, loc_idx) : "";
                char const* arg2str = i_arg2 ? sObjectMgr.GetMangosString(i_arg2, loc_idx) : "";

                char str[2048];
                snprintf(str, 2048, text, arg1str, arg2str);

                ChatHandler::BuildChatPacket(data, CHAT_MSG_MONSTER_YELL, str, LANG_UNIVERSAL, CHAT_TAG_NONE, i_source ? i_source->GetObjectGuid() : 0, i_source ? i_source->GetName() : "");
            }
        private:

            uint32 i_language;
            int32 i_textId;
            Creature const* i_source;
            int32 i_arg1;
            int32 i_arg2;
    };
}

template<class Do>

void BattleGround::BroadcastWorker(Do& _do)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (Player* plr = sPlayerRegistry.Find(itr->first))
        {
            _do(plr);
        }
    }
}

BattleGround::BattleGround()
{
    m_TypeID = BattleGroundTypeId(0);
    m_Status = STATUS_NONE;
    m_ClientInstanceID = 0;
    m_EndTime = 0;
    m_BracketId = BG_BRACKET_ID_TEMPLATE;
    m_InvitedAlliance = 0;
    m_InvitedHorde = 0;
    m_Winner = TEAM_NONE;
    m_StartTime = 0;
    m_Events = 0;
    m_Name = "";
    m_LevelMin = 0;
    m_LevelMax = 0;
    m_InBGFreeSlotQueue = false;
    m_BuffChange = false;
    m_MaxPlayersPerTeam = 0;
    m_MaxPlayers = 0;
    m_MinPlayersPerTeam = 0;
    m_MinPlayers = 0;
    m_StartDelayTime = 0;
    m_MapId = 0;
    m_Map = nullptr;
    m_startMaxDist = 0;
    m_validStartPositionTimer = 0;

    m_TeamStartLocX[TEAM_INDEX_ALLIANCE]   = 0;
    m_TeamStartLocX[TEAM_INDEX_HORDE] = 0;

    m_TeamStartLocY[TEAM_INDEX_ALLIANCE] = 0;
    m_TeamStartLocY[TEAM_INDEX_HORDE] = 0;

    m_TeamStartLocZ[TEAM_INDEX_ALLIANCE] = 0;
    m_TeamStartLocZ[TEAM_INDEX_HORDE] = 0;

    m_TeamStartLocO[TEAM_INDEX_ALLIANCE] = 0;
    m_TeamStartLocO[TEAM_INDEX_HORDE] = 0;

    m_BgRaids[TEAM_INDEX_ALLIANCE] = nullptr;
    m_BgRaids[TEAM_INDEX_HORDE] = nullptr;

    m_PlayersCount[TEAM_INDEX_ALLIANCE] = 0;
    m_PlayersCount[TEAM_INDEX_HORDE] = 0;

    m_TeamScores[TEAM_INDEX_ALLIANCE] = 0;
    m_TeamScores[TEAM_INDEX_HORDE] = 0;

    m_PrematureCountDown = false;
    m_PrematureCountDownTimer = 0;

    m_StartDelayTimes[BG_STARTING_EVENT_FIRST] = BG_START_DELAY_2M;
    m_StartDelayTimes[BG_STARTING_EVENT_SECOND] = BG_START_DELAY_1M;
    m_StartDelayTimes[BG_STARTING_EVENT_THIRD] = BG_START_DELAY_30S;
    m_StartDelayTimes[BG_STARTING_EVENT_FOURTH] = BG_START_DELAY_NONE;

    m_StartMessageIds[BG_STARTING_EVENT_FIRST] = 0;
    m_StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_WS_START_ONE_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_THIRD] = LANG_BG_WS_START_HALF_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_WS_HAS_BEGUN;
}

BattleGround::~BattleGround()
{

    sBattleGroundMgr.RemoveBattleGround(GetInstanceID(), GetTypeID());

    BattleGroundBracketId bracketId = GetBracketId();
    if (bracketId != BG_BRACKET_ID_TEMPLATE)
    {
        sBattleGroundMgr.DeleteClientVisibleInstanceId(GetTypeID(), bracketId, GetClientInstanceID());
    }

    if (m_Map)
    {
        m_Map->SetUnload();
    }

    this->RemoveFromBGFreeSlotQueue();

    for (BattleGroundScoreMap::const_iterator itr = m_PlayerScores.begin(); itr != m_PlayerScores.end(); ++itr)
    {
        delete itr->second;
    }
}

void BattleGround::Update(uint32 diff)
{
    if (!GetPlayersSize())
    {

        if (!GetInvitedCount(HORDE) && !GetInvitedCount(ALLIANCE))
        {
            delete this;
        }
        return;
    }

    if (!m_OfflineQueue.empty())
    {
        BattleGroundPlayerMap::iterator itr = m_Players.find(*(m_OfflineQueue.begin()));
        if (itr != m_Players.end())
        {
            if (itr->second.OfflineRemoveTime <= sWorld.GetGameTime())
            {
                RemovePlayerAtLeave(itr->first, true, true);
                m_OfflineQueue.pop_front();

            }
        }
    }

    if (GetStatus() == STATUS_IN_PROGRESS && sBattleGroundMgr.GetPrematureFinishTime() && (GetPlayersCountByTeam(ALLIANCE) < GetMinPlayersPerTeam() || GetPlayersCountByTeam(HORDE) < GetMinPlayersPerTeam()))
    {
        if (!m_PrematureCountDown)
        {
            m_PrematureCountDown = true;
            m_PrematureCountDownTimer = sBattleGroundMgr.GetPrematureFinishTime();
        }
        else if (m_PrematureCountDownTimer < diff)
        {
            EndBattleGround(GetPrematureWinner());
            m_PrematureCountDown = false;
        }
        else if (!sBattleGroundMgr.isTesting())
        {
            uint32 newtime = m_PrematureCountDownTimer - diff;

            if (newtime > (MINUTE * IN_MILLISECONDS))
            {
                if (newtime / (MINUTE * IN_MILLISECONDS) != m_PrematureCountDownTimer / (MINUTE * IN_MILLISECONDS))
                {
                    PSendMessageToAll(LANG_BATTLEGROUND_PREMATURE_FINISH_WARNING, CHAT_MSG_SYSTEM, nullptr, (uint32)(m_PrematureCountDownTimer / (MINUTE * IN_MILLISECONDS)));
                }
            }
            else
            {

                if (newtime / (15 * IN_MILLISECONDS) != m_PrematureCountDownTimer / (15 * IN_MILLISECONDS))
                {
                    PSendMessageToAll(LANG_BATTLEGROUND_PREMATURE_FINISH_WARNING_SECS, CHAT_MSG_SYSTEM, nullptr, (uint32)(m_PrematureCountDownTimer / IN_MILLISECONDS));
                }
            }
            m_PrematureCountDownTimer = newtime;
        }
    }
    else if (m_PrematureCountDown)
    {
        m_PrematureCountDown = false;
    }

    if (GetStatus() == STATUS_WAIT_JOIN && GetPlayersSize())
    {
        float maxDist = GetStartMaxDist();
        if (maxDist > 0.0f)
        {
            if (m_validStartPositionTimer < diff)
            {
                for (BattleGroundPlayerMap::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
                {
                    if (Player* player = sObjectMgr.GetPlayer(itr->first))
                    {
                        float x, y, z, o;
                        GetTeamStartLoc(player->GetTeam(), x, y, z, o);
                        if (!player->Where().WithinDist(Geometry::Vector3(x, y, z), maxDist))
                        {
                            player->TeleportTo(GetMapId(), x, y, z, o);
                        }
                    }
                }
                m_validStartPositionTimer = CHECK_PLAYER_POSITION_INVERVAL;
            }
            else
            {
                m_validStartPositionTimer -= diff;
            }
        }

        ModifyStartDelayTime(diff);

        if (!(m_Events & BG_STARTING_EVENT_1))
        {
            m_Events |= BG_STARTING_EVENT_1;

            StartingEventCloseDoors();
            SetStartDelayTime(m_StartDelayTimes[BG_STARTING_EVENT_FIRST]);

            if (m_StartMessageIds[BG_STARTING_EVENT_FIRST])
            {
                SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_FIRST], CHAT_MSG_BG_SYSTEM_NEUTRAL);
            }
        }

        else if (GetStartDelayTime() <= m_StartDelayTimes[BG_STARTING_EVENT_SECOND] && !(m_Events & BG_STARTING_EVENT_2))
        {
            m_Events |= BG_STARTING_EVENT_2;
            SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_SECOND], CHAT_MSG_BG_SYSTEM_NEUTRAL);
        }

        else if (GetStartDelayTime() <= m_StartDelayTimes[BG_STARTING_EVENT_THIRD] && !(m_Events & BG_STARTING_EVENT_3))
        {
            m_Events |= BG_STARTING_EVENT_3;
            SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_THIRD], CHAT_MSG_BG_SYSTEM_NEUTRAL);
        }

        else if (GetStartDelayTime() <= 0 && !(m_Events & BG_STARTING_EVENT_4))
        {
            m_Events |= BG_STARTING_EVENT_4;

            StartingEventOpenDoors();

            SendMessageToAll(m_StartMessageIds[BG_STARTING_EVENT_FOURTH], CHAT_MSG_BG_SYSTEM_NEUTRAL);
            SetStatus(STATUS_IN_PROGRESS);
            SetStartDelayTime(m_StartDelayTimes[BG_STARTING_EVENT_FOURTH]);

            {
                PlaySoundToAll(SOUND_BG_START);

                if (sWorld.getConfig(CONFIG_BOOL_BATTLEGROUND_QUEUE_ANNOUNCER_START))
                {
                    sWorld.SendWorldText(LANG_BG_STARTED_ANNOUNCE_WORLD, GetName(), GetMinLevel(), GetMaxLevel());
                }
            }
        }
    }

    if (GetStatus() == STATUS_WAIT_LEAVE)
    {

        m_EndTime -= diff;
        if (m_EndTime <= 0)
        {
            m_EndTime = 0;
            BattleGroundPlayerMap::iterator itr, next;
            for (itr = m_Players.begin(); itr != m_Players.end(); itr = next)
            {
                next = itr;
                ++next;

                RemovePlayerAtLeave(itr->first, true, true);

            }
        }
    }

    m_StartTime += diff;
}

void BattleGround::SetTeamStartLoc(Team team, float X, float Y, float Z, float O)
{
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    m_TeamStartLocX[teamIdx] = X;
    m_TeamStartLocY[teamIdx] = Y;
    m_TeamStartLocZ[teamIdx] = Z;
    m_TeamStartLocO[teamIdx] = O;
}

void BattleGround::SendPacketToAll(WorldPacket* packet)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
        {
            continue;
        }

        if (Player* plr = sObjectMgr.GetPlayer(itr->first))
        {
            plr->GetSession()->SendPacket(packet);
        }
        else
        {
            sLog.outError("BattleGround:SendPacketToAll: %s not found!", GuidString(itr->first).c_str());
        }
    }
}

void BattleGround::SendPacketToTeam(Team teamId, WorldPacket* packet, Player* sender, bool self)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
        {
            continue;
        }

        Player* plr = sObjectMgr.GetPlayer(itr->first);
        if (!plr)
        {
            sLog.outError("BattleGround:SendPacketToTeam: %s not found!", GuidString(itr->first).c_str());
            continue;
        }

        if (!self && sender == plr)
        {
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
        {
            team = plr->GetTeam();
        }

        if (team == teamId)
        {
            plr->GetSession()->SendPacket(packet);
        }
    }
}

void BattleGround::PlaySoundToAll(uint32 SoundID)
{
    WorldPacket data;
    sBattleGroundMgr.BuildPlaySoundPacket(&data, SoundID);
    SendPacketToAll(&data);
}

void BattleGround::PlaySoundToTeam(uint32 SoundID, Team teamId)
{
    WorldPacket data;

    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
        {
            continue;
        }

        Player* plr = sObjectMgr.GetPlayer(itr->first);
        if (!plr)
        {
            sLog.outError("BattleGround:PlaySoundToTeam: %s not found!", GuidString(itr->first).c_str());
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
        {
            team = plr->GetTeam();
        }

        if (team == teamId)
        {
            sBattleGroundMgr.BuildPlaySoundPacket(&data, SoundID);
            plr->GetSession()->SendPacket(&data);
        }
    }
}

void BattleGround::CastSpellOnTeam(uint32 SpellID, Team teamId)
{
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.OfflineRemoveTime)
        {
            continue;
        }

        Player* plr = sObjectMgr.GetPlayer(itr->first);

        if (!plr)
        {
            sLog.outError("BattleGround:CastSpellOnTeam: %s not found!", GuidString(itr->first).c_str());
            continue;
        }

        Team team = itr->second.PlayerTeam;
        if (!team)
        {
            team = plr->GetTeam();
        }

        if (team == teamId)
        {
            plr->CastSpell(plr, SpellID, true);
        }
    }
}

void BattleGround::BlockMovement(Player* plr)
{
    plr->SetClientControl(plr, 0);
}

void BattleGround::RemovePlayerAtLeave(ObjectGuid guid, bool Transport, bool SendPacket)
{
    Team team = GetPlayerTeam(guid);
    bool participant = false;

    BattleGroundPlayerMap::iterator itr = m_Players.find(guid);
    if (itr != m_Players.end())
    {
        UpdatePlayersCountByTeam(team, true);
        m_Players.erase(itr);

        participant = true;
    }

    BattleGroundScoreMap::iterator itr2 = m_PlayerScores.find(guid);
    if (itr2 != m_PlayerScores.end())
    {
        delete itr2->second;
        m_PlayerScores.erase(itr2);
    }

    Player* plr = sObjectMgr.GetPlayer(guid);

    if (plr)
    {

        if (plr->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        {
            plr->RemoveAurasOfType(SPELL_AURA_MOD_SHAPESHIFT);
        }

        plr->RemoveAurasOfType(SPELL_AURA_MOUNTED);

        if (!plr->IsAlive())
        {
            plr->ResurrectPlayer(1.0f);
            plr->SpawnCorpseBones();
        }
    }

    RemovePlayer(plr, guid);

    if (participant)
    {
        BattleGroundTypeId bgTypeId = GetTypeID();
        BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(GetTypeID());
        if (plr)
        {
            if (!team)
            {
                team = plr->GetTeam();
            }

            if (SendPacket)
            {
                WorldPacket data;
                sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, this, plr->Queues().SlotOf(bgQueueTypeId), STATUS_NONE, 0, 0);
                plr->GetSession()->SendPacket(&data);
            }

            plr->Queues().Give(bgQueueTypeId);
        }

        if (Group* group = GetBgRaid(team))
        {
            if (!group->RemoveMember(guid, 0))
            {
                SetBgRaid(team, nullptr);
                delete group;
            }
        }
        DecreaseInvitedCount(team);

        if (GetStatus() < STATUS_WAIT_LEAVE)
        {

            AddToBGFreeSlotQueue();
            sBattleGroundMgr.ScheduleQueueUpdate(bgQueueTypeId, bgTypeId, GetBracketId());
        }

        WorldPacket data;
        sBattleGroundMgr.BuildPlayerLeftBattleGroundPacket(&data, guid);
        SendPacketToTeam(team, &data, plr, false);
    }

    if (plr)
    {

        plr->Battle().In(0, BATTLEGROUND_TYPE_NONE);

        plr->Battle().Side(TEAM_NONE);

        if (Transport)
        {
            plr->Battle().TeleportBack();
        }

        DETAIL_LOG("BATTLEGROUND: Removed player %s from BattleGround.", plr->GetName());
    }

}

void BattleGround::Reset()
{
    SetWinner(TEAM_NONE);
    SetStatus(STATUS_WAIT_QUEUE);
    SetStartTime(0);
    SetEndTime(0);

    m_Events = 0;

    m_ActiveEvents[BG_EVENT_DOOR] = 0;

    if (m_InvitedAlliance > 0 || m_InvitedHorde > 0)
    {
        sLog.outError("BattleGround system: bad counter, m_InvitedAlliance: %d, m_InvitedHorde: %d", m_InvitedAlliance, m_InvitedHorde);
    }

    m_InvitedAlliance = 0;
    m_InvitedHorde = 0;
    m_InBGFreeSlotQueue = false;

    m_Players.clear();

    for (BattleGroundScoreMap::const_iterator itr = m_PlayerScores.begin(); itr != m_PlayerScores.end(); ++itr)
    {
        delete itr->second;
    }
    m_PlayerScores.clear();
}

void BattleGround::StartBattleGround()
{
    SetStartTime(0);

    AddToBGFreeSlotQueue();

    sBattleGroundMgr.AddBattleGround(GetInstanceID(), GetTypeID(), this);

}

void BattleGround::AddPlayer(Player* plr)
{

    if (plr->HasPlayerFlag(PLAYER_FLAGS_AFK))
    {
        plr->ToggleAFK();
    }

    ObjectGuid guid = plr->GetObjectGuid();
    Team team = plr->Battle().Side();

    BattleGroundPlayer bp;
    bp.OfflineRemoveTime = 0;
    bp.PlayerTeam = team;

    m_Players[guid] = bp;

    bool const isInBattleground = IsPlayerInBattleGround(guid);
    if (!isInBattleground)
    {
        UpdatePlayersCountByTeam(team, false);
    }

    WorldPacket data;
    sBattleGroundMgr.BuildPlayerJoinedBattleGroundPacket(&data, plr);
    SendPacketToTeam(team, &data, plr, false);

    PlayerAddedToBGCheckIfBGIsRunning(plr);
    AddOrSetPlayerToCorrectBgGroup(plr, guid, team);

    DETAIL_LOG("BATTLEGROUND: Player %s joined the battle.", plr->GetName());
}

void BattleGround::AddOrSetPlayerToCorrectBgGroup(Player* plr, ObjectGuid plr_guid, Team team)
{
    if (Group* group = GetBgRaid(team))
    {
        if (group->IsMember(plr_guid))
        {
            uint8 subgroup = group->GetMemberGroup(plr_guid);
            plr->SetBattleGroundRaid(group, subgroup);
        }
        else
        {
            group->AddMember(plr_guid, plr->GetName());
            if (Group* originalGroup = plr->GetOriginalGroup())
            {
                if (originalGroup->IsLeader(plr_guid))
                {
                    group->ChangeLeader(plr_guid);
                }
            }
        }
    }
    else
    {
        group = new Group;
        SetBgRaid(team, group);
        group->Create(plr_guid, plr->GetName());
    }
}

void BattleGround::EventPlayerLoggedIn(Player* player)
{
    ObjectGuid playerGuid = player->GetObjectGuid();

    for (OfflineQueue::iterator itr = m_OfflineQueue.begin(); itr != m_OfflineQueue.end(); ++itr)
    {
        if (*itr == playerGuid)
        {
            m_OfflineQueue.erase(itr);
            break;
        }
    }
    m_Players[playerGuid].OfflineRemoveTime = 0;
    PlayerAddedToBGCheckIfBGIsRunning(player);

}

void BattleGround::EventPlayerLoggedOut(Player* player)
{

    m_OfflineQueue.push_back(player->GetObjectGuid());
    m_Players[player->GetObjectGuid()].OfflineRemoveTime = sWorld.GetGameTime() + MAX_OFFLINE_TIME;
    if (GetStatus() == STATUS_IN_PROGRESS)
    {

        RemovePlayer(player, player->GetObjectGuid());
    }
}

void BattleGround::AddToBGFreeSlotQueue()
{

    if (!m_InBGFreeSlotQueue)
    {
        sBattleGroundMgr.BGFreeSlotQueue[m_TypeID].push_front(this);
        m_InBGFreeSlotQueue = true;
    }
}

void BattleGround::RemoveFromBGFreeSlotQueue()
{

    m_InBGFreeSlotQueue = false;
    BGFreeSlotQueueType& bgFreeSlot = sBattleGroundMgr.BGFreeSlotQueue[m_TypeID];
    for (BGFreeSlotQueueType::iterator itr = bgFreeSlot.begin(); itr != bgFreeSlot.end(); ++itr)
    {
        if ((*itr)->GetInstanceID() == GetInstanceID())
        {
            bgFreeSlot.erase(itr);
            return;
        }
    }
}

uint32 BattleGround::GetFreeSlotsForTeam(Team team) const
{

    if (GetStatus() == STATUS_WAIT_JOIN || GetStatus() == STATUS_IN_PROGRESS)
    {
        return (GetInvitedCount(team) < GetMaxPlayersPerTeam()) ? GetMaxPlayersPerTeam() - GetInvitedCount(team) : 0;
    }

    return 0;
}

bool BattleGround::HasFreeSlots() const
{
    return GetPlayersSize() < GetMaxPlayers();
}

void BattleGround::UpdatePlayerScore(Player* Source, uint32 type, uint32 value)
{

    BattleGroundScoreMap::const_iterator itr = m_PlayerScores.find(Source->GetObjectGuid());

    if (itr == m_PlayerScores.end())
    {
        return;
    }

    switch (type)
    {
        case SCORE_KILLING_BLOWS:
            itr->second->KillingBlows += value;
            break;
        case SCORE_DEATHS:
            itr->second->Deaths += value;
            break;
        case SCORE_HONORABLE_KILLS:
            itr->second->HonorableKills += value;
            break;
        case SCORE_BONUS_HONOR:

            if (Source->AddHonorCP(value, HONORABLE, 0, 0))
            {
                itr->second->BonusHonor += value;
            }
            break;
        default:
            sLog.outError("BattleGround: Unknown player score type %u", type);
            break;
    }
}

void BattleGround::SendMessageToAll(int32 entry, ChatMsg type, Player const* source)
{
    MaNGOS::BattleGroundChatBuilder bg_builder(type, entry, source);
    MaNGOS::LocalizedPacketDo<MaNGOS::BattleGroundChatBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);
}

void BattleGround::SendYellToAll(int32 entry, uint32 language, ObjectGuid guid)
{
    Creature* source = GetBgMap()->GetCreature(guid);
    if (!source)
    {
        return;
    }
    MaNGOS::BattleGroundYellBuilder bg_builder(Language(language), entry, source);
    MaNGOS::LocalizedPacketDo<MaNGOS::BattleGroundYellBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);
}

void BattleGround::PSendMessageToAll(int32 entry, ChatMsg type, Player const* source, ...)
{
    va_list ap;
    va_start(ap, source);

    MaNGOS::BattleGroundChatBuilder bg_builder(type, entry, source, &ap);
    MaNGOS::LocalizedPacketDo<MaNGOS::BattleGroundChatBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);

    va_end(ap);
}

void BattleGround::SendMessage2ToAll(int32 entry, ChatMsg type, Player const* source, int32 arg1, int32 arg2)
{
    MaNGOS::BattleGround2ChatBuilder bg_builder(type, entry, source, arg1, arg2);
    MaNGOS::LocalizedPacketDo<MaNGOS::BattleGround2ChatBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);
}

void BattleGround::SendYell2ToAll(int32 entry, uint32 language, ObjectGuid guid, int32 arg1, int32 arg2)
{
    Creature* source = GetBgMap()->GetCreature(guid);
    if (!source)
    {
        return;
    }
    MaNGOS::BattleGround2YellBuilder bg_builder(language, entry, source, arg1, arg2);
    MaNGOS::LocalizedPacketDo<MaNGOS::BattleGround2YellBuilder> bg_do(bg_builder);
    BroadcastWorker(bg_do);
}

void BattleGround::EndNow()
{
    RemoveFromBGFreeSlotQueue();
    SetStatus(STATUS_WAIT_LEAVE);
    SetEndTime(0);
}

void BattleGround::HandleTriggerBuff(ObjectGuid go_guid)
{
    GameObject* obj = GetBgMap()->GetGameObject(go_guid);
    if (!obj || obj->GetGoType() != GAMEOBJECT_TYPE_TRAP || !obj->isSpawned())
    {
        return;
    }

    obj->SetLootState(GO_JUST_DEACTIVATED);
    return;
}

void BattleGround::HandleKillPlayer(Player* player, Player* killer)
{

    UpdatePlayerScore(player, SCORE_DEATHS, 1);

    if (killer)
    {
        UpdatePlayerScore(killer, SCORE_HONORABLE_KILLS, 1);
        UpdatePlayerScore(killer, SCORE_KILLING_BLOWS, 1);

        for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
        {
            Player* plr = sObjectMgr.GetPlayer(itr->first);

            if (!plr || plr == killer)
            {
                continue;
            }

            if (plr->GetTeam() == killer->GetTeam() && plr->IsAtGroupRewardDistance(player))
            {
                UpdatePlayerScore(plr, SCORE_HONORABLE_KILLS, 1);
            }
        }
    }

    player->SetUnitFlag(UNIT_FLAG_SKINNABLE);
}

Team BattleGround::GetPlayerTeam(ObjectGuid guid)
{
    BattleGroundPlayerMap::const_iterator itr = m_Players.find(guid);
    if (itr != m_Players.end())
    {
        return itr->second.PlayerTeam;
    }
    return TEAM_NONE;
}

bool BattleGround::IsPlayerInBattleGround(ObjectGuid guid)
{
    BattleGroundPlayerMap::const_iterator itr = m_Players.find(guid);
    if (itr != m_Players.end())
    {
        return true;
    }
    return false;
}

void BattleGround::PlayerAddedToBGCheckIfBGIsRunning(Player* plr)
{
    if (GetStatus() != STATUS_WAIT_LEAVE)
    {
        return;
    }

    WorldPacket data;
    BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(GetTypeID());

    BlockMovement(plr);

    sBattleGroundMgr.BuildPvpLogDataPacket(&data, this);
    plr->GetSession()->SendPacket(&data);

    sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, this, plr->Queues().SlotOf(bgQueueTypeId), STATUS_IN_PROGRESS, GetEndTime(), GetStartTime());
    plr->GetSession()->SendPacket(&data);
}

uint32 BattleGround::GetAlivePlayersCountByTeam(Team team) const
{
    int count = 0;
    for (BattleGroundPlayerMap::const_iterator itr = m_Players.begin(); itr != m_Players.end(); ++itr)
    {
        if (itr->second.PlayerTeam == team)
        {
            Player* pl = sObjectMgr.GetPlayer(itr->first);
            if (pl && pl->IsAlive())
            {
                ++count;
            }
        }
    }
    return count;
}

void BattleGround::SetBgRaid(Team team, Group* bg_raid)
{
    Group*& old_raid = m_BgRaids[GetTeamIndexByTeamId(team)];

    if (old_raid)
    {
        old_raid->SetBattlegroundGroup(nullptr);
    }

    if (bg_raid)
    {
        bg_raid->SetBattlegroundGroup(this);
    }

    old_raid = bg_raid;
}

WorldSafeLocsEntry const* BattleGround::GetClosestGraveYard(Player* player)
{
    return sObjectMgr.GetClosestGraveYard(player->Where().X(), player->Where().Y(), player->Where().Z(), player->GetMapId(), player->GetTeam());
}

Team BattleGround::GetPrematureWinner()
{
    uint32 hordePlayers = GetPlayersCountByTeam(HORDE);
    uint32 alliancePlayers = GetPlayersCountByTeam(ALLIANCE);

    if (alliancePlayers > hordePlayers)
    {
        return ALLIANCE;
    }

    if (hordePlayers > alliancePlayers)
    {
        return HORDE;
    }

    return TEAM_NONE;
}
