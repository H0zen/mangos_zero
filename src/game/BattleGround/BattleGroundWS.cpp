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

#include "Occupant.h"
#include "Player.h"
#include "BattleGround.h"
#include "BattleGroundWS.h"
#include "GameObject.h"
#include "ObjectMgr.h"
#include "BattleGroundMgr.h"
#include "WorldPacket.h"
#include "Language.h"

BattleGroundWS::BattleGroundWS()
{
    m_StartMessageIds[BG_STARTING_EVENT_FIRST]  = 0;
    m_StartMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_WS_START_ONE_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_THIRD]  = LANG_BG_WS_START_HALF_MINUTE;
    m_StartMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_WS_HAS_BEGUN;
}

void BattleGroundWS::Update(uint32 diff)
{
    BattleGround::Update(diff);

    if (GetStatus() == STATUS_IN_PROGRESS)
    {
        if (m_FlagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_WAIT_RESPAWN)
        {
            m_FlagsTimer[TEAM_INDEX_ALLIANCE] -= diff;

            if (m_FlagsTimer[TEAM_INDEX_ALLIANCE] < 0)
            {
                m_FlagsTimer[TEAM_INDEX_ALLIANCE] = 0;
                RespawnFlag(ALLIANCE, true);
            }
        }
        if (m_FlagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_GROUND)
        {
            m_FlagsDropTimer[TEAM_INDEX_ALLIANCE] -= diff;

            if (m_FlagsDropTimer[TEAM_INDEX_ALLIANCE] < 0)
            {
                m_FlagsDropTimer[TEAM_INDEX_ALLIANCE] = 0;
                RespawnDroppedFlag(ALLIANCE);
            }
        }
        if (m_FlagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_WAIT_RESPAWN)
        {
            m_FlagsTimer[TEAM_INDEX_HORDE] -= diff;

            if (m_FlagsTimer[TEAM_INDEX_HORDE] < 0)
            {
                m_FlagsTimer[TEAM_INDEX_HORDE] = 0;
                RespawnFlag(HORDE, true);
            }
        }
        if (m_FlagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_GROUND)
        {
            m_FlagsDropTimer[TEAM_INDEX_HORDE] -= diff;

            if (m_FlagsDropTimer[TEAM_INDEX_HORDE] < 0)
            {
                m_FlagsDropTimer[TEAM_INDEX_HORDE] = 0;
                RespawnDroppedFlag(HORDE);
            }
        }
    }
}

void BattleGroundWS::StartingEventOpenDoors()
{
    OpenDoorEvent(BG_EVENT_DOOR);

    SpawnEvent(WS_EVENT_SPIRITGUIDES_SPAWN, 0, true);
    SpawnEvent(WS_EVENT_FLAG_A, 0, true);
    SpawnEvent(WS_EVENT_FLAG_H, 0, true);
}

void BattleGroundWS::AddPlayer(Player* plr)
{
    BattleGround::AddPlayer(plr);

    BattleGroundWGScore* sc = new BattleGroundWGScore;

    m_PlayerScores[plr->GetObjectGuid()] = sc;
}

void BattleGroundWS::RespawnFlag(Team team, bool captured)
{
    if (team == ALLIANCE)
    {
        DEBUG_LOG("Respawn Alliance flag");
        m_FlagState[TEAM_INDEX_ALLIANCE] = BG_WS_FLAG_STATE_ON_BASE;
        SpawnEvent(WS_EVENT_FLAG_A, 0, true);
    }
    else
    {
        DEBUG_LOG("Respawn Horde flag");
        m_FlagState[TEAM_INDEX_HORDE] = BG_WS_FLAG_STATE_ON_BASE;
        SpawnEvent(WS_EVENT_FLAG_H, 0, true);
    }

    if (captured)
    {

        SpawnEvent(WS_EVENT_FLAG_A, 0, true);
        SpawnEvent(WS_EVENT_FLAG_H, 0, true);
        SendMessageToAll(LANG_BG_WS_F_PLACED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
        PlaySoundToAll(BG_WS_SOUND_FLAGS_RESPAWNED);
    }
}

void BattleGroundWS::RespawnDroppedFlag(Team team)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }

    RespawnFlag(team, false);
    if (team == ALLIANCE)
    {
        SendMessageToAll(LANG_BG_WS_ALLIANCE_FLAG_RESPAWNED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
    }
    else
    {
        SendMessageToAll(LANG_BG_WS_HORDE_FLAG_RESPAWNED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
    }

    PlaySoundToAll(BG_WS_SOUND_FLAGS_RESPAWNED);

    GameObject* obj = GetBgMap()->GetGameObject(GetDroppedFlagGuid(team));
    if (obj)
    {
        obj->Delete();
    }
    else
    {
        sLog.outError("Unknown dropped flag bg: %s", GuidString(GetDroppedFlagGuid(team)).c_str());
    }

    ClearDroppedFlagGuid(team);
}

void BattleGroundWS::EventPlayerCapturedFlag(Player* source)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }

    source->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
    if (source->GetTeam() == ALLIANCE)
    {
        if (!IsHordeFlagPickedUp())
        {
            return;
        }
        ClearHordeFlagCarrier();

        m_FlagState[TEAM_INDEX_HORDE] = BG_WS_FLAG_STATE_WAIT_RESPAWN;

        source->RemoveAuras(BG_WS_SPELL_WARSONG_FLAG);
        if (m_TeamScores[TEAM_INDEX_ALLIANCE] < BG_WS_MAX_TEAM_SCORE)
        {
            m_TeamScores[TEAM_INDEX_ALLIANCE] += 1;
        }
        PlaySoundToAll(BG_WS_SOUND_FLAG_CAPTURED_ALLIANCE);
        RewardReputationToTeam(890, m_ReputationCapture, ALLIANCE);
    }
    else
    {
        if (!IsAllianceFlagPickedUp())
        {
            return;
        }
        ClearAllianceFlagCarrier();

        m_FlagState[TEAM_INDEX_ALLIANCE] = BG_WS_FLAG_STATE_WAIT_RESPAWN;

        source->RemoveAuras(BG_WS_SPELL_SILVERWING_FLAG);
        if (m_TeamScores[TEAM_INDEX_HORDE] < BG_WS_MAX_TEAM_SCORE)
        {
            m_TeamScores[TEAM_INDEX_HORDE] += 1;
        }
        PlaySoundToAll(BG_WS_SOUND_FLAG_CAPTURED_HORDE);
        RewardReputationToTeam(889, m_ReputationCapture, HORDE);
    }

    RewardHonorToTeam(BG_WSG_FlagCapturedHonor[GetBracketId()], source->GetTeam());

    SpawnEvent(WS_EVENT_FLAG_A, 0, false);
    SpawnEvent(WS_EVENT_FLAG_H, 0, false);

    if (source->GetTeam() == ALLIANCE)
    {
        SendMessageToAll(LANG_BG_WS_CAPTURED_HF, CHAT_MSG_BG_SYSTEM_ALLIANCE, source);
    }
    else
    {
        SendMessageToAll(LANG_BG_WS_CAPTURED_AF, CHAT_MSG_BG_SYSTEM_HORDE, source);
    }

    UpdateFlagState(source->GetTeam(), 1);
    UpdateTeamScore(source->GetTeam());

    UpdatePlayerScore(source, SCORE_FLAG_CAPTURES, 1);

    Team winner = TEAM_NONE;
    if (m_TeamScores[TEAM_INDEX_ALLIANCE] == BG_WS_MAX_TEAM_SCORE)
    {
        winner = ALLIANCE;
    }
    else if (m_TeamScores[TEAM_INDEX_HORDE] == BG_WS_MAX_TEAM_SCORE)
    {
        winner = HORDE;
    }

    if (winner)
    {
        UpdateWorldState(BG_WS_FLAG_UNK_ALLIANCE, 0);
        UpdateWorldState(BG_WS_FLAG_UNK_HORDE, 0);
        UpdateWorldState(BG_WS_FLAG_STATE_ALLIANCE, 1);
        UpdateWorldState(BG_WS_FLAG_STATE_HORDE, 1);

        EndBattleGround(winner);
    }
    else
    {
        m_FlagsTimer[GetOtherTeamIndex(GetTeamIndexByTeamId(source->GetTeam()))] = BG_WS_FLAG_RESPAWN_TIME;
    }
}

void BattleGroundWS::EventPlayerDroppedFlag(Player* source)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {

        if (source->GetTeam() == ALLIANCE)
        {
            if (!IsHordeFlagPickedUp())
            {
                return;
            }
            if (GetHordeFlagCarrierGuid() == source->GetObjectGuid())
            {
                ClearHordeFlagCarrier();
                source->RemoveAuras(BG_WS_SPELL_WARSONG_FLAG);
            }
        }
        else
        {
            if (!IsAllianceFlagPickedUp())
            {
                return;
            }
            if (GetAllianceFlagCarrierGuid() == source->GetObjectGuid())
            {
                ClearAllianceFlagCarrier();
                source->RemoveAuras(BG_WS_SPELL_SILVERWING_FLAG);
            }
        }
        return;
    }

    bool set = false;

    if (source->GetTeam() == ALLIANCE)
    {
        if (!IsHordeFlagPickedUp())
        {
            return;
        }
        if (GetHordeFlagCarrierGuid() == source->GetObjectGuid())
        {
            ClearHordeFlagCarrier();
            source->RemoveAuras(BG_WS_SPELL_WARSONG_FLAG);
            m_FlagState[TEAM_INDEX_HORDE] = BG_WS_FLAG_STATE_ON_GROUND;
            source->CastSpell(source, BG_WS_SPELL_WARSONG_FLAG_DROPPED, true);
            set = true;
        }
    }
    else
    {
        if (!IsAllianceFlagPickedUp())
        {
            return;
        }
        if (GetAllianceFlagCarrierGuid() == source->GetObjectGuid())
        {
            ClearAllianceFlagCarrier();
            source->RemoveAuras(BG_WS_SPELL_SILVERWING_FLAG);
            m_FlagState[TEAM_INDEX_ALLIANCE] = BG_WS_FLAG_STATE_ON_GROUND;
            source->CastSpell(source, BG_WS_SPELL_SILVERWING_FLAG_DROPPED, true);
            set = true;
        }
    }

    if (set)
    {
        UpdateFlagState(source->GetTeam(), 1);

        if (source->GetTeam() == ALLIANCE)
        {
            SendMessageToAll(LANG_BG_WS_DROPPED_HF, CHAT_MSG_BG_SYSTEM_HORDE, source);
            UpdateWorldState(BG_WS_FLAG_UNK_HORDE, uint32(-1));
        }
        else
        {
            SendMessageToAll(LANG_BG_WS_DROPPED_AF, CHAT_MSG_BG_SYSTEM_ALLIANCE, source);
            UpdateWorldState(BG_WS_FLAG_UNK_ALLIANCE, uint32(-1));
        }

        m_FlagsDropTimer[GetOtherTeamIndex(GetTeamIndexByTeamId(source->GetTeam()))] = BG_WS_FLAG_DROP_TIME;
    }
}

void BattleGroundWS::EventPlayerClickedOnFlag(Player* source, GameObject* target_obj)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }

    int32 message_id = 0;
    ChatMsg type;

    uint8 event = (sBattleGroundMgr.GetGameObjectEventIndex(target_obj->GetGUIDLow())).event1;

    if (source->GetTeam() == HORDE && GetFlagState(ALLIANCE) == BG_WS_FLAG_STATE_ON_BASE &&
        event == WS_EVENT_FLAG_A)
    {
        message_id = LANG_BG_WS_PICKEDUP_AF;
        type = CHAT_MSG_BG_SYSTEM_HORDE;
        PlaySoundToAll(BG_WS_SOUND_ALLIANCE_FLAG_PICKED_UP);
        SpawnEvent(WS_EVENT_FLAG_A, 0, false);
        SetAllianceFlagCarrier(source->GetObjectGuid());
        m_FlagState[TEAM_INDEX_ALLIANCE] = BG_WS_FLAG_STATE_ON_PLAYER;

        UpdateFlagState(HORDE, BG_WS_FLAG_STATE_ON_PLAYER);
        UpdateWorldState(BG_WS_FLAG_UNK_ALLIANCE, 1);
        source->CastSpell(source, BG_WS_SPELL_SILVERWING_FLAG, true);
    }

    if (source->GetTeam() == ALLIANCE && GetFlagState(HORDE) == BG_WS_FLAG_STATE_ON_BASE &&
        event == WS_EVENT_FLAG_H)
    {
        message_id = LANG_BG_WS_PICKEDUP_HF;
        type = CHAT_MSG_BG_SYSTEM_ALLIANCE;
        PlaySoundToAll(BG_WS_SOUND_HORDE_FLAG_PICKED_UP);
        SpawnEvent(WS_EVENT_FLAG_H, 0, false);
        SetHordeFlagCarrier(source->GetObjectGuid());
        m_FlagState[TEAM_INDEX_HORDE] = BG_WS_FLAG_STATE_ON_PLAYER;

        UpdateFlagState(ALLIANCE, BG_WS_FLAG_STATE_ON_PLAYER);
        UpdateWorldState(BG_WS_FLAG_UNK_HORDE, 1);
        source->CastSpell(source, BG_WS_SPELL_WARSONG_FLAG, true);
    }

    if (GetFlagState(ALLIANCE) == BG_WS_FLAG_STATE_ON_GROUND && InReach(*source, *target_obj, 10))
    {
        if (source->GetTeam() == ALLIANCE)
        {
            message_id = LANG_BG_WS_RETURNED_AF;
            type = CHAT_MSG_BG_SYSTEM_ALLIANCE;
            UpdateFlagState(HORDE, BG_WS_FLAG_STATE_WAIT_RESPAWN);
            RespawnFlag(ALLIANCE, false);
            PlaySoundToAll(BG_WS_SOUND_FLAG_RETURNED);
            UpdatePlayerScore(source, SCORE_FLAG_RETURNS, 1);
        }
        else
        {
            message_id = LANG_BG_WS_PICKEDUP_AF;
            type = CHAT_MSG_BG_SYSTEM_HORDE;
            PlaySoundToAll(BG_WS_SOUND_ALLIANCE_FLAG_PICKED_UP);
            SpawnEvent(WS_EVENT_FLAG_A, 0, false);
            SetAllianceFlagCarrier(source->GetObjectGuid());
            source->CastSpell(source, BG_WS_SPELL_SILVERWING_FLAG, true);
            m_FlagState[TEAM_INDEX_ALLIANCE] = BG_WS_FLAG_STATE_ON_PLAYER;
            UpdateFlagState(HORDE, BG_WS_FLAG_STATE_ON_PLAYER);
            UpdateWorldState(BG_WS_FLAG_UNK_ALLIANCE, 1);
        }

    }

    if (GetFlagState(HORDE) == BG_WS_FLAG_STATE_ON_GROUND && InReach(*source, *target_obj, 10))
    {
        if (source->GetTeam() == HORDE)
        {
            message_id = LANG_BG_WS_RETURNED_HF;
            type = CHAT_MSG_BG_SYSTEM_HORDE;
            UpdateFlagState(ALLIANCE, BG_WS_FLAG_STATE_WAIT_RESPAWN);
            RespawnFlag(HORDE, false);
            PlaySoundToAll(BG_WS_SOUND_FLAG_RETURNED);
            UpdatePlayerScore(source, SCORE_FLAG_RETURNS, 1);
        }
        else
        {
            message_id = LANG_BG_WS_PICKEDUP_HF;
            type = CHAT_MSG_BG_SYSTEM_ALLIANCE;
            PlaySoundToAll(BG_WS_SOUND_HORDE_FLAG_PICKED_UP);
            SpawnEvent(WS_EVENT_FLAG_H, 0, false);
            SetHordeFlagCarrier(source->GetObjectGuid());
            source->CastSpell(source, BG_WS_SPELL_WARSONG_FLAG, true);
            m_FlagState[TEAM_INDEX_HORDE] = BG_WS_FLAG_STATE_ON_PLAYER;
            UpdateFlagState(ALLIANCE, BG_WS_FLAG_STATE_ON_PLAYER);
            UpdateWorldState(BG_WS_FLAG_UNK_HORDE, 1);
        }

    }

    if (!message_id)
    {
        return;
    }

    SendMessageToAll(message_id, type, source);
    source->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
}

void BattleGroundWS::RemovePlayer(Player* plr, ObjectGuid guid)
{

    if (IsAllianceFlagPickedUp() && m_flagCarrierAlliance == guid)
    {
        if (!plr)
        {
            sLog.outError("BattleGroundWS: Removing offline player who has the FLAG!!");
            ClearAllianceFlagCarrier();
            RespawnFlag(ALLIANCE, false);
        }
        else
        {
            EventPlayerDroppedFlag(plr);
        }
    }
    if (IsHordeFlagPickedUp() && m_flagCarrierHorde == guid)
    {
        if (!plr)
        {
            sLog.outError("BattleGroundWS: Removing offline player who has the FLAG!!");
            ClearHordeFlagCarrier();
            RespawnFlag(HORDE, false);
        }
        else
        {
            EventPlayerDroppedFlag(plr);
        }
    }
}

void BattleGroundWS::UpdateFlagState(Team team, uint32 value)
{
    if (team == ALLIANCE)
    {
        UpdateWorldState(BG_WS_FLAG_STATE_ALLIANCE, value);
    }
    else
    {
        UpdateWorldState(BG_WS_FLAG_STATE_HORDE, value);
    }
}

void BattleGroundWS::UpdateTeamScore(Team team)
{
    if (team == ALLIANCE)
    {
        UpdateWorldState(BG_WS_FLAG_CAPTURES_ALLIANCE, m_TeamScores[TEAM_INDEX_ALLIANCE]);
    }
    else
    {
        UpdateWorldState(BG_WS_FLAG_CAPTURES_HORDE, m_TeamScores[TEAM_INDEX_HORDE]);
    }
}

bool BattleGroundWS::HandleAreaTrigger(Player* source, uint32 trigger)
{

    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return false;
    }

    switch (trigger)
    {
        case 3646:
            if (m_FlagState[TEAM_INDEX_HORDE] && !m_FlagState[TEAM_INDEX_ALLIANCE])
            {
                if (GetHordeFlagCarrierGuid() == source->GetObjectGuid())
                {
                    EventPlayerCapturedFlag(source);
                }
            }
            break;
        case 3647:
            if (m_FlagState[TEAM_INDEX_ALLIANCE] && !m_FlagState[TEAM_INDEX_HORDE])
            {
                if (GetAllianceFlagCarrierGuid() == source->GetObjectGuid())
                {
                    EventPlayerCapturedFlag(source);
                }
            }
            break;
        case 3669:
            if (source->GetTeam() != HORDE)
            {
                source->GetSession()->SendNotification(LANG_BATTLEGROUND_ONLY_HORDE_USE);
            }
            else
            {
                source->Battle().Leave();
            }
            break;
        case 3671:
            if (source->GetTeam() != ALLIANCE)
            {
                source->GetSession()->SendNotification(LANG_BATTLEGROUND_ONLY_ALLIANCE_USE);
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

void BattleGroundWS::Reset()
{

    BattleGround::Reset();

    m_ActiveEvents[WS_EVENT_SPIRITGUIDES_SPAWN] = BG_EVENT_NONE;
    m_ActiveEvents[WS_EVENT_FLAG_A] = BG_EVENT_NONE;
    m_ActiveEvents[WS_EVENT_FLAG_H] = BG_EVENT_NONE;

    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        m_DroppedFlagGuid[i] = 0;
        m_FlagState[i]       = BG_WS_FLAG_STATE_ON_BASE;
        m_TeamScores[i]      = 0;
    }

    m_flagCarrierAlliance = 0;
    m_flagCarrierHorde = 0;

    bool isBGWeekend = BattleGroundMgr::IsBGWeekend(GetTypeID());
    m_ReputationCapture = (isBGWeekend) ? 25 : 15;
    m_HonorWinKills = (isBGWeekend) ? 3 : 1;
    m_HonorEndKills = (isBGWeekend) ? 4 : 2;
}

void BattleGroundWS::EndBattleGround(Team winner)
{

    if (winner == ALLIANCE)
    {
        RewardHonorToTeam(BG_WSG_WinMatchHonor[GetBracketId()], ALLIANCE);
    }
    if (winner == HORDE)
    {
        RewardHonorToTeam(BG_WSG_WinMatchHonor[GetBracketId()], HORDE);
    }

    BattleGround::EndBattleGround(winner);
}

void BattleGroundWS::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        return;
    }

    EventPlayerDroppedFlag(player);

    BattleGround::HandleKillPlayer(player, killer);
}

void BattleGroundWS::UpdatePlayerScore(Player* source, uint32 type, uint32 value)
{
    BattleGroundScoreMap::iterator itr = m_PlayerScores.find(source->GetObjectGuid());
    if (itr == m_PlayerScores.end())
    {
        return;
    }

    switch (type)
    {
        case SCORE_FLAG_CAPTURES:
            ((BattleGroundWGScore*)itr->second)->FlagCaptures += value;
            break;
        case SCORE_FLAG_RETURNS:
            ((BattleGroundWGScore*)itr->second)->FlagReturns += value;
            break;
        default:
            BattleGround::UpdatePlayerScore(source, type, value);
            break;
    }
}

WorldSafeLocsEntry const* BattleGroundWS::GetClosestGraveYard(Player* player)
{

    if (player->GetTeam() == ALLIANCE)
    {
        if (GetStatus() == STATUS_IN_PROGRESS)
        {
            return sWorldSafeLocsStore.LookupEntry(WS_GRAVEYARD_MAIN_ALLIANCE);
        }
        else
        {
            return sWorldSafeLocsStore.LookupEntry(WS_GRAVEYARD_FLAGROOM_ALLIANCE);
        }
    }
    else
    {
        if (GetStatus() == STATUS_IN_PROGRESS)
        {
            return sWorldSafeLocsStore.LookupEntry(WS_GRAVEYARD_MAIN_HORDE);
        }
        else
        {
            return sWorldSafeLocsStore.LookupEntry(WS_GRAVEYARD_FLAGROOM_HORDE);
        }
    }
}

void BattleGroundWS::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    FillInitialWorldState(data, count, BG_WS_FLAG_CAPTURES_ALLIANCE, m_TeamScores[TEAM_INDEX_ALLIANCE]);
    FillInitialWorldState(data, count, BG_WS_FLAG_CAPTURES_HORDE, m_TeamScores[TEAM_INDEX_HORDE]);

    if (m_FlagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_GROUND)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_ALLIANCE, -1);
    }
    else if (m_FlagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_PLAYER)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_ALLIANCE, 1);
    }
    else
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_ALLIANCE, 0);
    }

    if (m_FlagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_GROUND)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_HORDE, -1);
    }
    else if (m_FlagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_PLAYER)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_HORDE, 1);
    }
    else
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_UNK_HORDE, 0);
    }

    FillInitialWorldState(data, count, BG_WS_FLAG_CAPTURES_MAX, BG_WS_MAX_TEAM_SCORE);

    if (m_FlagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_PLAYER)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_STATE_HORDE, 2);
    }
    else
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_STATE_HORDE, 1);
    }

    if (m_FlagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_PLAYER)
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_STATE_ALLIANCE, 2);
    }
    else
    {
        FillInitialWorldState(data, count, BG_WS_FLAG_STATE_ALLIANCE, 1);
    }
}

Team BattleGroundWS::GetPrematureWinner()
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
