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

#include "TemporarySummon.h"
#include "Log.h"
#include "CreatureAI.h"
#include "Corpse.h"

TemporarySummon::TemporarySummon(ObjectGuid summoner)
    : Creature(CREATURE_SUBTYPE_TEMPORARY_SUMMON)
{
    Term().SummonedBy(summoner);
    Term().Grant(TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN, 0);
}

void TemporarySummon::Update(uint32 update_diff,  uint32 diff)
{
    if (Term().RunsOut(update_diff, tenure::BodyOf(*this)))
    {
        UnSummon();
        return;
    }

    if (IsAlive() && GetCharmerGuid())
    {
        Unit* charmer = GetCharmer();
        if (!charmer || !InReach(*this, *charmer, GetMap()->GetVisibilityDistance()))
        {
            UnSummon();
            return;
        }
    }

    Creature::Update(update_diff, diff);
}

void TemporarySummon::Summon(TempSpawnType type, uint32 lifetime)
{
    Term().Grant(type, lifetime);

    GetMap()->Add((Creature*)this);
    AIM_Initialize();
}

void TemporarySummon::UnSummon()
{
    if ((GuidHigh(GetSummonerGuid()) == HIGHGUID_UNIT))
    {
        if (Creature* sum = GetMap()->GetCreature(GetSummonerGuid()))
        {
            if (sum->AI())
            {
                sum->AI()->SummonedCreatureDespawn(this);
            }
        }
    }
    AddObjectToRemoveList();
}

void TemporarySummon::RemoveFromWorld()
{
    if (IsInWorld())
    {
        Unit* charmer = GetCharmer();
        if (charmer && charmer->GetCharmGuid() == GetObjectGuid())
        {
            charmer->Uncharm();
            if (charmer->GetCharmGuid() == GetObjectGuid() &&IsPlayer(charmer))
            {
                Player* player = (Player*)charmer;
                Camera& camera = player->GetCamera();
                player->InterruptSpell(CURRENT_CHANNELED_SPELL);

                player->SetClientControl(player, 1);
                player->SetMover(nullptr);
                camera.ResetView();
                player->RemovePetActionBar();
            }
        }
    }
    Creature::RemoveFromWorld();
}

TemporarySummonWaypoint::TemporarySummonWaypoint(ObjectGuid summoner, uint32 waypoint_id, int32 path_id, uint32 pathOrigin)
    : TemporarySummon(summoner),
    m_waypoint_id(waypoint_id),
    m_path_id(path_id),
    m_pathOrigin(pathOrigin) {}
