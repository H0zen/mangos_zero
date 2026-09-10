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

#include <list>
#include "OutdoorPvPEP.h"
#include "WorldPacket.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Occupant.h"
#include "Creature.h"
#include "GameObject.h"
#include "Player.h"

OutdoorPvPEP::OutdoorPvPEP() : OutdoorPvP(),
    m_towersAlliance(0),
    m_towersHorde(0)
{
    m_towerWorldState[0] = WORLD_STATE_EP_NORTHPASS_NEUTRAL;
    m_towerWorldState[1] = WORLD_STATE_EP_CROWNGUARD_NEUTRAL;
    m_towerWorldState[2] = WORLD_STATE_EP_EASTWALL_NEUTRAL;
    m_towerWorldState[3] = WORLD_STATE_EP_PLAGUEWOOD_NEUTRAL;

    for (uint8 i = 0; i < MAX_EP_TOWERS; ++i)
    {
        m_towerOwner[i] = TEAM_NONE;
    }

    sObjectMgr.SetGraveYardLinkTeam(GRAVEYARD_ID_EASTERN_PLAGUE, GRAVEYARD_ZONE_EASTERN_PLAGUE, TEAM_INVALID);
}

void OutdoorPvPEP::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    FillInitialWorldState(data, count, WORLD_STATE_EP_TOWER_COUNT_ALLIANCE, m_towersAlliance);
    FillInitialWorldState(data, count, WORLD_STATE_EP_TOWER_COUNT_HORDE, m_towersHorde);

    for (uint8 i = 0; i < MAX_EP_TOWERS; ++i)
    {
        FillInitialWorldState(data, count, m_towerWorldState[i], WORLD_STATE_ADD);
    }
}

void OutdoorPvPEP::SendRemoveWorldStates(Player* player)
{
    for (uint8 i = 0; i < MAX_EP_TOWERS; ++i)
    {
        player->SendUpdateWorldState(m_towerWorldState[i], WORLD_STATE_REMOVE);
    }
}

void OutdoorPvPEP::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    OutdoorPvP::HandlePlayerEnterZone(player, isMainZone);

    for (uint8 i = 0; i < MAX_EP_TOWERS; ++i)
    {
        player->RemoveAuras(player->GetTeam() == ALLIANCE ? plaguelandsTowerBuffs[i].spellIdAlliance : plaguelandsTowerBuffs[i].spellIdHorde);
    }

    switch (player->GetTeam())
    {
        case ALLIANCE:
            if (m_towersAlliance > 0)
            {
                player->CastSpell(player, plaguelandsTowerBuffs[m_towersAlliance - 1].spellIdAlliance, true);
            }
            break;
        case HORDE:
            if (m_towersHorde > 0)
            {
                player->CastSpell(player, plaguelandsTowerBuffs[m_towersHorde - 1].spellIdHorde, true);
            }
            break;
        default:
            break;
    }
}

void OutdoorPvPEP::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{

    for (uint8 i = 0; i < MAX_EP_TOWERS; ++i)
    {
        player->RemoveAuras(player->GetTeam() == ALLIANCE ? plaguelandsTowerBuffs[i].spellIdAlliance : plaguelandsTowerBuffs[i].spellIdHorde);
    }

    OutdoorPvP::HandlePlayerLeaveZone(player, isMainZone);
}

void OutdoorPvPEP::HandleCreatureCreate(Creature* creature)
{
    switch (creature->GetEntry())
    {
        case NPC_SPECTRAL_FLIGHT_MASTER:
            m_flightMaster = creature->GetObjectGuid();
            creature->setFaction(m_towerOwner[TOWER_ID_PLAGUEWOOD] == ALLIANCE ? FACTION_FLIGHT_MASTER_ALLIANCE : FACTION_FLIGHT_MASTER_HORDE);
            creature->CastSpell(creature, m_towerOwner[TOWER_ID_PLAGUEWOOD] == ALLIANCE ? SPELL_SPIRIT_PARTICLES_BLUE : SPELL_SPIRIT_PARTICLES_RED, true);
            break;
        case NPC_LORDAERON_COMMANDER:
        case NPC_LORDAERON_SOLDIER:
        case NPC_LORDAERON_VETERAN:
        case NPC_LORDAERON_FIGHTER:
            m_soldiers.push_back(creature->GetObjectGuid());
            break;
    }
}

void OutdoorPvPEP::HandleGameObjectCreate(GameObject* go)
{
    OutdoorPvP::HandleGameObjectCreate(go);

    switch (go->GetEntry())
    {
        case GO_TOWER_BANNER_NORTHPASS:
            InitBanner(go, TOWER_ID_NORTHPASS);
            break;
        case GO_TOWER_BANNER_CROWNGUARD:
            InitBanner(go, TOWER_ID_CROWNGUARD);
            break;
        case GO_TOWER_BANNER_EASTWALL:
            InitBanner(go, TOWER_ID_EASTWALL);
            break;
        case GO_TOWER_BANNER_PLAGUEWOOD:
            InitBanner(go, TOWER_ID_PLAGUEWOOD);
            break;
        case GO_TOWER_BANNER:

            if (go->Where().WithinDist(Geometry::Vector2(plaguelandsTowerLocations[TOWER_ID_NORTHPASS][0], plaguelandsTowerLocations[TOWER_ID_NORTHPASS][1]), 50.0f))
            {
                InitBanner(go, TOWER_ID_NORTHPASS);
            }
            else if (go->Where().WithinDist(Geometry::Vector2(plaguelandsTowerLocations[TOWER_ID_CROWNGUARD][0], plaguelandsTowerLocations[TOWER_ID_CROWNGUARD][1]), 50.0f))
            {
                InitBanner(go, TOWER_ID_CROWNGUARD);
            }
            else if (go->Where().WithinDist(Geometry::Vector2(plaguelandsTowerLocations[TOWER_ID_EASTWALL][0], plaguelandsTowerLocations[TOWER_ID_EASTWALL][1]), 50.0f))
            {
                InitBanner(go, TOWER_ID_EASTWALL);
            }
            else if (go->Where().WithinDist(Geometry::Vector2(plaguelandsTowerLocations[TOWER_ID_PLAGUEWOOD][0], plaguelandsTowerLocations[TOWER_ID_PLAGUEWOOD][1]), 50.0f))
            {
                InitBanner(go, TOWER_ID_PLAGUEWOOD);
            }
            break;
        case GO_LORDAERON_SHRINE_ALLIANCE:
            m_lordaeronShrineAlliance = go->GetObjectGuid();
            break;
        case GO_LORDAERON_SHRINE_HORDE:
            m_lordaeronShrineHorde = go->GetObjectGuid();
            break;
    }
}

void OutdoorPvPEP::HandleObjectiveComplete(uint32 eventId, const std::list<Player*> &players, Team team)
{
    uint32 credit;

    switch (eventId)
    {
        case EVENT_CROWNGUARD_PROGRESS_ALLIANCE:
        case EVENT_CROWNGUARD_PROGRESS_HORDE:
            credit = NPC_CROWNGUARD_TOWER_QUEST_DOODAD;
            break;
        case EVENT_EASTWALL_PROGRESS_ALLIANCE:
        case EVENT_EASTWALL_PROGRESS_HORDE:
            credit = NPC_EASTWALL_TOWER_QUEST_DOODAD;
            break;
        case EVENT_NORTHPASS_PROGRESS_ALLIANCE:
        case EVENT_NORTHPASS_PROGRESS_HORDE:
            credit = NPC_NORTHPASS_TOWER_QUEST_DOODAD;
            break;
        case EVENT_PLAGUEWOOD_PROGRESS_ALLIANCE:
        case EVENT_PLAGUEWOOD_PROGRESS_HORDE:
            credit = NPC_PLAGUEWOOD_TOWER_QUEST_DOODAD;
            break;
        default:
            return;
    }

    for (std::list<Player*>::const_iterator itr = players.begin(); itr != players.end(); ++itr)
    {
        if ((*itr) && (*itr)->GetTeam() == team)
        {
            (*itr)->Journal().KillCredited(credit);
            (*itr)->AddHonorCP(HONOR_REWARD_PLAGUELANDS, HONORABLE, 0, 0);
        }
    }
}

bool OutdoorPvPEP::HandleEvent(uint32 eventId, GameObject* go)
{
    for (uint8 i = 0; i < MAX_EP_TOWERS; ++i)
    {
        if (plaguelandsBanners[i] == go->GetEntry())
        {
            for (uint8 j = 0; j < 4; ++j)
            {
                if (plaguelandsTowerEvents[i][j].eventEntry == eventId)
                {

                    if (plaguelandsTowerEvents[i][j].team != m_towerOwner[i])
                    {
                        if (plaguelandsTowerEvents[i][j].defenseMessage)
                        {
                            sWorld.SendDefenseMessage(ZONE_ID_EASTERN_PLAGUELANDS, plaguelandsTowerEvents[i][j].defenseMessage);
                        }

                        return ProcessCaptureEvent(go, i, plaguelandsTowerEvents[i][j].team, plaguelandsTowerEvents[i][j].worldState);
                    }

                    return false;
                }
            }

            return false;
        }
    }

    return false;
}

bool OutdoorPvPEP::ProcessCaptureEvent(GameObject* go, uint32 towerId, Team team, uint32 newWorldState)
{
    if (team == ALLIANCE)
    {

        for (GuidList::const_iterator itr = m_towerBanners[towerId].begin(); itr != m_towerBanners[towerId].end(); ++itr)
        {
            SetBannerVisual(go, (*itr), CAPTURE_ARTKIT_ALLIANCE, CAPTURE_ANIM_ALLIANCE);
        }

        ++m_towersAlliance;
        SendUpdateWorldState(WORLD_STATE_EP_TOWER_COUNT_ALLIANCE, m_towersAlliance);

        BuffTeam(ALLIANCE, plaguelandsTowerBuffs[m_towersAlliance - 1].spellIdAlliance);
    }
    else if (team == HORDE)
    {

        for (GuidList::const_iterator itr = m_towerBanners[towerId].begin(); itr != m_towerBanners[towerId].end(); ++itr)
        {
            SetBannerVisual(go, (*itr), CAPTURE_ARTKIT_HORDE, CAPTURE_ANIM_HORDE);
        }

        ++m_towersHorde;
        SendUpdateWorldState(WORLD_STATE_EP_TOWER_COUNT_HORDE, m_towersHorde);

        BuffTeam(HORDE, plaguelandsTowerBuffs[m_towersHorde - 1].spellIdHorde);
    }
    else
    {

        for (GuidList::const_iterator itr = m_towerBanners[towerId].begin(); itr != m_towerBanners[towerId].end(); ++itr)
        {
            SetBannerVisual(go, (*itr), CAPTURE_ARTKIT_NEUTRAL, CAPTURE_ANIM_NEUTRAL);
        }

        if (m_towerOwner[towerId] == ALLIANCE)
        {

            --m_towersAlliance;
            SendUpdateWorldState(WORLD_STATE_EP_TOWER_COUNT_ALLIANCE, m_towersAlliance);

            if (m_towersAlliance == 0)
            {
                BuffTeam(ALLIANCE, plaguelandsTowerBuffs[0].spellIdAlliance, true);
            }
        }
        else
        {

            --m_towersHorde;
            SendUpdateWorldState(WORLD_STATE_EP_TOWER_COUNT_HORDE, m_towersHorde);

            if (m_towersHorde == 0)
            {
                BuffTeam(HORDE, plaguelandsTowerBuffs[0].spellIdHorde, true);
            }
        }
    }

    bool eventHandled = true;

    if (team != TEAM_NONE)
    {

        m_towerOwner[towerId] = team;

        switch (towerId)
        {
            case TOWER_ID_NORTHPASS:
                RespawnGO(go, team == ALLIANCE ? m_lordaeronShrineAlliance : m_lordaeronShrineHorde, true);
                break;
            case TOWER_ID_CROWNGUARD:
                sObjectMgr.SetGraveYardLinkTeam(GRAVEYARD_ID_EASTERN_PLAGUE, GRAVEYARD_ZONE_EASTERN_PLAGUE, team);
                break;
            case TOWER_ID_EASTWALL:

                if (m_towerOwner[TOWER_ID_NORTHPASS] != team)
                {
                    eventHandled = false;
                }
                break;
            case TOWER_ID_PLAGUEWOOD:

                eventHandled = false;
                break;
        }
    }
    else
    {

        switch (towerId)
        {
            case TOWER_ID_NORTHPASS:
                RespawnGO(go, m_towerOwner[TOWER_ID_NORTHPASS] == ALLIANCE ? m_lordaeronShrineAlliance : m_lordaeronShrineHorde, false);
                break;
            case TOWER_ID_CROWNGUARD:
                sObjectMgr.SetGraveYardLinkTeam(GRAVEYARD_ID_EASTERN_PLAGUE, GRAVEYARD_ZONE_EASTERN_PLAGUE, TEAM_INVALID);
                break;
            case TOWER_ID_EASTWALL:
                UnsummonSoldiers(go);
                break;
            case TOWER_ID_PLAGUEWOOD:
                UnsummonFlightMaster(go);
                break;
        }

        m_towerOwner[towerId] = team;
    }

    SendUpdateWorldState(m_towerWorldState[towerId], WORLD_STATE_REMOVE);
    m_towerWorldState[towerId] = newWorldState;
    SendUpdateWorldState(m_towerWorldState[towerId], WORLD_STATE_ADD);

    return eventHandled;
}

bool OutdoorPvPEP::HandleGameObjectUse(Player* , GameObject* go)
{

    if (go->GetEntry() == GO_LORDAERON_SHRINE_ALLIANCE || go->GetEntry() == GO_LORDAERON_SHRINE_HORDE)
    {
        go->SetRespawnTime(0);
    }

    return false;
}

void OutdoorPvPEP::InitBanner(GameObject* go, uint32 towerId)
{
    m_towerBanners[towerId].push_back(go->GetObjectGuid());
    go->SetGoArtKit(GetBannerArtKit(m_towerOwner[towerId]));
}

void OutdoorPvPEP::UnsummonFlightMaster(const Occupant* objRef)
{
    if (Creature* flightMaster = objRef->GetMap()->GetCreature(m_flightMaster))
    {
        flightMaster->ForcedDespawn();
    }
}

void OutdoorPvPEP::UnsummonSoldiers(const Occupant* objRef)
{
    for (GuidList::const_iterator itr = m_soldiers.begin(); itr != m_soldiers.end(); ++itr)
    {
        if (Creature* soldier = objRef->GetMap()->GetCreature(*itr))
        {
            soldier->ForcedDespawn();
        }
    }

    m_soldiers.clear();
}
