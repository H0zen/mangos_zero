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

#include "Summoning.h"

#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureLinkingMgr.h"
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "TemporarySummon.h"

Creature* SummonCreature(WorldObject& summoner, uint32 id, float x, float y, float z, float ang,
                         TempSpawnType spwtype, uint32 despwtime, bool asActiveObject, bool setRun)
{
    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(id);
    if (!cinfo)
    {
        sLog.outErrorDb("SummonCreature: Creature (Entry: %u) not existed for summoner: %s. ",
                        id, summoner.GetGuidStr().c_str());
        return nullptr;
    }

    Map* map = summoner.GetMap();
    TemporarySummon* pCreature = new TemporarySummon(summoner.GetObjectGuid());

    // A summon inherits its summoner's side, and only a player has one.
    Team team = TEAM_NONE;
    if (Player const* owner = ToPlayer(&summoner))
    {
        team = owner->GetTeam();
    }

    CreatureCreatePos pos(map, x, y, z, ang);
    if (x == 0.0f && y == 0.0f && z == 0.0f)
    {
        pos = CreatureCreatePos(&summoner, summoner.Where().Facing(), CONTACT_DISTANCE, ang);
    }

    if (!pCreature->Create(map->GenerateLocalLowGuid(cinfo->GetHighGuid()), pos, cinfo, team))
    {
        delete pCreature;
        return nullptr;
    }

    pCreature->SetSpawn(pos);

    // Set run or walk before any other movement starts
    pCreature->SetWalk(!setRun);

    // Active state set before added to map
    pCreature->SetActiveObjectState(asActiveObject);

    pCreature->Summon(spwtype, despwtime);                  // Also initializes the AI and MMGen

    if (Creature* maker = ToCreature(&summoner))
    {
        if (CreatureAI* ai = maker->AI())
        {
            ai->JustSummoned(pCreature);
        }
    }

    // Creature Linking, Initial load is handled like respawn
    if (pCreature->IsLinkingEventTrigger())
    {
        map->GetCreatureLinkingHolder()->DoCreatureLinkingEvent(LINKING_EVENT_RESPAWN, pCreature);
    }

    // return the creature therewith the summoner has access to it
    return pCreature;
}

GameObject* SummonGameObject(WorldObject& summoner, uint32 id,
                             float x, float y, float z, float angle, uint32 despwtime)
{
    Map* map = summoner.FindMap();
    if (!map)
    {
        return nullptr;
    }

    GameObject* pGameObj = new GameObject;

    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), id, map, x, y, z, angle))
    {
        delete pGameObj;
        return nullptr;
    }

    pGameObj->SetRespawnTime(despwtime / IN_MILLISECONDS);

    map->Add(pGameObj);
    pGameObj->AIM_Initialize();

    return pGameObj;
}
