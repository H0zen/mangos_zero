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

#include "OutdoorPvP.h"
#include "ObjectMgr.h"
#include "Occupant.h"
#include "GameObject.h"
#include "Kinds.h"
#include "Player.h"

void OutdoorPvP::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    m_zonePlayers[player->GetObjectGuid()] = isMainZone;
}

void OutdoorPvP::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{
    if (m_zonePlayers.erase(player->GetObjectGuid()))
    {

        if (isMainZone && !player->GetSession()->PlayerLogout())
        {
            SendRemoveWorldStates(player);
        }

        sLog.outDebug("Player %s left an Outdoor PvP zone", player->GetName());
    }
}

void OutdoorPvP::SendUpdateWorldState(uint32 field, uint32 value)
{
    for (GuidZoneMap::const_iterator itr = m_zonePlayers.begin(); itr != m_zonePlayers.end(); ++itr)
    {

        if (!itr->second)
        {
            continue;
        }

        if (Player* player = sObjectMgr.GetPlayer(itr->first))
        {
            player->SendUpdateWorldState(field, value);
        }
    }
}

void OutdoorPvP::HandleGameObjectCreate(GameObject* go)
{

    if (auto* point = go->Behaves<CapturePointBehaviour>())
    {
        CapturePointSliderMap const* capturePoints = sOutdoorPvPMgr.GetCapturePointSliderMap();
        CapturePointSliderMap::const_iterator itr = capturePoints->find(go->GetEntry());
        if (itr != capturePoints->end())
        {
            point->Restore(itr->second.Value, itr->second.IsLocked);
        }
        else
        {
            point->Restore(CAPTURE_SLIDER_MIDDLE, false);
        }
    }
}

void OutdoorPvP::HandleGameObjectRemove(GameObject* go)
{

    if (auto* point = go->Behaves<CapturePointBehaviour>())
    {
        CapturePointSlider value(point->Bar().Slider(), go->getLootState() != GO_ACTIVATED);
        sOutdoorPvPMgr.SetCapturePointSlider(go->GetEntry(), value);
    }
}

void OutdoorPvP::HandlePlayerKill(Player* killer, Player* victim)
{
    if (Group* group = killer->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* groupMember = itr->getSource();

            if (!groupMember)
            {
                continue;
            }

            if (!groupMember->IsAtGroupRewardDistance(victim))
            {
                continue;
            }

            if (groupMember->CanUseCapturePoint())
            {
                HandlePlayerKillInsideArea(groupMember);
            }
        }
    }
    else
    {

        if (killer && killer->CanUseCapturePoint())
        {
            HandlePlayerKillInsideArea(killer);
        }
    }
}

void OutdoorPvP::BuffTeam(Team team, uint32 spellId, bool remove )
{
    for (GuidZoneMap::const_iterator itr = m_zonePlayers.begin(); itr != m_zonePlayers.end(); ++itr)
    {
        Player* player = sObjectMgr.GetPlayer(itr->first);
        if (player && player->GetTeam() == team)
        {
            if (remove)
            {
                player->RemoveAuras(spellId);
            }
            else
            {
                player->CastSpell(player, spellId, true);
            }
        }
    }
}

uint32 OutdoorPvP::GetBannerArtKit(Team team, uint32 artKitAlliance , uint32 artKitHorde , uint32 artKitNeutral )
{
    switch (team)
    {
        case ALLIANCE:
            return artKitAlliance;
        case HORDE:
            return artKitHorde;
        default:
            return artKitNeutral;
    }
}

void OutdoorPvP::SetBannerVisual(const Occupant* objRef, ObjectGuid goGuid, uint32 artKit, uint32 animId)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(goGuid))
    {
        SetBannerVisual(go, artKit, animId);
    }
}

void OutdoorPvP::SetBannerVisual(GameObject* go, uint32 artKit, uint32 animId)
{
    go->SendGameObjectCustomAnim(animId);
    go->SetGoArtKit(artKit);
    go->Refresh();
}

void OutdoorPvP::RespawnGO(const Occupant* objRef, ObjectGuid goGuid, bool respawn)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(goGuid))
    {
        go->SetRespawnTime(7 * DAY);

        if (respawn)
        {
            go->Refresh();
        }
        else if (go->isSpawned())
        {
            go->SetLootState(GO_JUST_DEACTIVATED);
        }
    }
}
