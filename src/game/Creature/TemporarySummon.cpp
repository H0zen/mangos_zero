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

/**
 * @brief Creates a temporary summon instance.
 *
 * @param summoner The GUID of the summoning object.
 */
TemporarySummon::TemporarySummon(ObjectGuid summoner)
    : Creature(CREATURE_SUBTYPE_TEMPORARY_SUMMON)
{
    Term().SummonedBy(summoner);
    Term().Grant(TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN, 0);
}

/**
 * @brief Reads the term against the clock, and the charm against sight.
 *
 * @param update_diff The elapsed time since the last update in milliseconds.
 * @param diff The world update time forwarded to the base creature update.
 */
void TemporarySummon::Update(uint32 update_diff,  uint32 diff)
{
    if (Term().RunsOut(update_diff, tenure::BodyOf(*this)))
    {
        UnSummon();
        return;
    }

    // One held on a charm goes when whoever holds it is out of sight.
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

/**
 * @brief Activates the summon with a despawn policy and lifetime.
 *
 * @param type The temporary spawn despawn policy.
 * @param lifetime The lifetime in milliseconds.
 */
void TemporarySummon::Summon(TempSpawnType type, uint32 lifetime)
{
    Term().Grant(type, lifetime);

    GetMap()->Add((Creature*)this);
    AIM_Initialize();
}

/**
 * @brief Unsummons the creature and notifies the summoner AI.
 */
void TemporarySummon::UnSummon()
{
    if (GetSummonerGuid().IsCreature())
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

/**
 * @brief Temporary summons are not persisted to the database.
 */
void TemporarySummon::SaveToDB()
{
}

/**
 * @brief Removes the summon from the world and clears charm control if needed.
 */
void TemporarySummon::RemoveFromWorld()
{
    if (IsInWorld())
    {
        Unit* charmer = GetCharmer();
        if (charmer && charmer->GetCharmGuid() == GetObjectGuid())
        {
            charmer->Uncharm();
            if (charmer->GetCharmGuid() == GetObjectGuid() && charmer->IsPlayer())
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

/**
 * @brief Creates a waypoint-based temporary summon instance.
 *
 * @param summoner The GUID of the summoning object.
 * @param waypoint_id The starting waypoint identifier.
 * @param path_id The path identifier.
 * @param pathOrigin The origin source for the waypoint path.
 */
TemporarySummonWaypoint::TemporarySummonWaypoint(ObjectGuid summoner, uint32 waypoint_id, int32 path_id, uint32 pathOrigin)
    : TemporarySummon(summoner),
    m_waypoint_id(waypoint_id),
    m_path_id(path_id),
    m_pathOrigin(pathOrigin) {}
