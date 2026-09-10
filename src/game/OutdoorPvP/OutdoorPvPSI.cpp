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

#include "OutdoorPvPSI.h"
#include "WorldPacket.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Object.h"
#include "Creature.h"
#include "GameObject.h"
#include "Player.h"
#include "Language.h"

OutdoorPvPSI::OutdoorPvPSI() : OutdoorPvP(),
    m_resourcesAlliance(0),
    m_resourcesHorde(0),
    m_zoneOwner(TEAM_NONE)
{
}

void OutdoorPvPSI::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    FillInitialWorldState(data, count, WORLD_STATE_SI_GATHERED_A, m_resourcesAlliance);
    FillInitialWorldState(data, count, WORLD_STATE_SI_GATHERED_H, m_resourcesHorde);
    FillInitialWorldState(data, count, WORLD_STATE_SI_SILITHYST_MAX, MAX_SILITHYST);
}

void OutdoorPvPSI::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    OutdoorPvP::HandlePlayerEnterZone(player, isMainZone);

    player->RemoveAuras(SPELL_CENARION_FAVOR);

    if (player->GetTeam() == m_zoneOwner)
    {
        player->CastSpell(player, SPELL_CENARION_FAVOR, true);
    }
}

void OutdoorPvPSI::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{

    player->RemoveAuras(SPELL_CENARION_FAVOR);

    OutdoorPvP::HandlePlayerLeaveZone(player, isMainZone);
}

bool OutdoorPvPSI::HandleAreaTrigger(Player* player, uint32 triggerId)
{
    if (player->isGameMaster() || player->IsDead())
    {
        return false;
    }

    switch (triggerId)
    {
        case AREATRIGGER_SILITHUS_ALLIANCE:
            if (player->GetTeam() != ALLIANCE || !player->HasAura(SPELL_SILITHYST))
            {
                return false;
            }

            ++ m_resourcesAlliance;
            SendUpdateWorldState(WORLD_STATE_SI_GATHERED_A, m_resourcesAlliance);

            if (m_resourcesAlliance == MAX_SILITHYST)
            {

                m_zoneOwner = ALLIANCE;
                m_resourcesAlliance = 0;
                m_resourcesHorde = 0;

                SendUpdateWorldState(WORLD_STATE_SI_GATHERED_H, m_resourcesHorde);

                BuffTeam(ALLIANCE, SPELL_CENARION_FAVOR);

                sWorld.SendDefenseMessage(ZONE_ID_SILITHUS, LANG_OPVP_SI_CAPTURE_A);
            }

            if (player->GetQuestStatus(QUEST_SCOURING_DESERT_ALLIANCE) == QUEST_STATUS_INCOMPLETE)
            {
                player->Journal().KillCredited(NPC_SILITHUS_DUST_QUEST_ALLIANCE);
            }
            break;
        case AREATRIGGER_SILITHUS_HORDE:
            if (player->GetTeam() != HORDE || !player->HasAura(SPELL_SILITHYST))
            {
                return false;
            }

            ++ m_resourcesHorde;
            SendUpdateWorldState(WORLD_STATE_SI_GATHERED_H, m_resourcesHorde);

            if (m_resourcesHorde == MAX_SILITHYST)
            {

                m_zoneOwner = HORDE;
                m_resourcesAlliance = 0;
                m_resourcesHorde = 0;

                SendUpdateWorldState(WORLD_STATE_SI_GATHERED_A, m_resourcesAlliance);

                BuffTeam(HORDE, SPELL_CENARION_FAVOR);

                sWorld.SendDefenseMessage(ZONE_ID_SILITHUS, LANG_OPVP_SI_CAPTURE_H);
            }

            if (player->GetQuestStatus(QUEST_SCOURING_DESERT_HORDE) == QUEST_STATUS_INCOMPLETE)
            {
                player->Journal().KillCredited(NPC_SILITHUS_DUST_QUEST_HORDE);
            }
            break;
        default:
            return false;
    }

    player->RemoveAuras(SPELL_SILITHYST);

    player->CastSpell(player, SPELL_TRACES_OF_SILITHYST, true);
    player->AddHonorCP(HONOR_REWARD_SILITHYST, HONORABLE, 0, 0);
    player->GetReputationMgr().ModifyReputation(sFactionStore.LookupEntry(FACTION_CENARION_CIRCLE), REPUTATION_REWARD_SILITHYST);

    return true;
}

struct SilithusSpawnLocation
{
    float x, y, z;
};

static SilithusSpawnLocation silithusFlagDropLocations[2] =
{
    { -7142.04f, 1397.92f, 4.327f},
    { -7588.48f, 756.806f, -16.425f}
};

bool OutdoorPvPSI::HandleDropFlag(Player* player, uint32 spellId)
{
    if (spellId != SPELL_SILITHYST)
    {
        return false;
    }

    switch (player->GetTeam())
    {
        case ALLIANCE:
            if (player->Where().WithinDist(Geometry::Vector3(silithusFlagDropLocations[0].x, silithusFlagDropLocations[0].y, silithusFlagDropLocations[0].z), 5.0f))
            {
                return false;
            }
            break;
        case HORDE:
            if (player->Where().WithinDist(Geometry::Vector3(silithusFlagDropLocations[1].x, silithusFlagDropLocations[1].y, silithusFlagDropLocations[1].z), 5.0f))
            {
                return false;
            }
            break;
        default:
            break;
    }

    player->CastSpell(player, SPELL_SILITHYST_FLAG_DROP, true);
    return true;
}

bool OutdoorPvPSI::HandleGameObjectUse(Player* player, GameObject* go)
{
    if (go->GetEntry() == GO_SILITHYST_MOUND || go->GetEntry() == GO_SILITHYST_GEYSER)
    {

        player->CastSpell(player, SPELL_SILITHYST, true);
        player->UpdatePvP(true, true);
        player->SetPlayerFlag(PLAYER_FLAGS_IN_PVP);

        go->SetLootState(GO_JUST_DEACTIVATED);
        return true;
    }

    return false;
}
