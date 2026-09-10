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

#include <set>
#include "Reaction.h"
#include "GridNotifiers.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateData.h"
#include "Map.h"
#include "Transports.h"
#include "TransportMap.h"
#include "PlayerRegistry.h"
#include "BattleGround/BattleGroundMgr.h"
#include "CreatureAI.h"
#include "Corpse.h"

using namespace MaNGOS;

void VisibleChangesNotifier::Visit(CameraMapType& m)
{
    for (CameraMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        iter->getSource()->UpdateVisibilityOf(&i_object);
    }
}

void VisibleNotifier::Notify()
{
    Player& player = *i_camera.GetOwner();

    UpdateData& data = Data();

    if (player.GetMap()->AsTransport())
    {
        Map::PlayerList const& aboard = player.GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = aboard.begin(); itr != aboard.end(); ++itr)
        {
            Player* mate = itr->getSource();
            if (mate && i_clientGUIDs.find(mate->GetObjectGuid()) != i_clientGUIDs.end())
            {

                mate->UpdateVisibilityOf(mate, &player);
                player.UpdateVisibilityOf(&player, mate, data, i_visibleNow);
                i_clientGUIDs.erase(mate->GetObjectGuid());
            }
        }
    }

    data.AddOutOfRangeGUID(i_clientGUIDs);
    for (GuidSet::iterator itr = i_clientGUIDs.begin(); itr != i_clientGUIDs.end(); ++itr)
    {
        player.m_clientGUIDs.erase(*itr);

        DEBUG_FILTER_LOG(LOG_FILTER_VISIBILITY_CHANGES, "%s is out of range (no in active cells set) now for %s",
            GuidString(*itr).c_str(), player.GetGuidStr().c_str());
    }

    if (data.HasData())
    {

        WorldPacket packet;
        bool const built = BuildPacket(&packet);
        if (i_initialBatch && !built)
        {
            sLog.outError("Failed to build initial object update batch for player %u", player.GetGUIDLow());
            player.GetSession()->KickPlayer();
            return;
        }
        player.GetSession()->SendPacket(&packet);

        GuidSet const& oor = data.GetOutOfRangeGUIDs();
        for (GuidSet::const_iterator iter = oor.begin(); iter != oor.end(); ++iter)
        {
            if (!(GuidHigh(*iter) == HIGHGUID_PLAYER))
            {
                continue;
            }

            if (Player* plr = sPlayerRegistry.Find(*iter))
            {
                plr->UpdateVisibilityOf(plr->GetCamera().GetBody(), &player);
            }
        }

        if (i_initialBatch)
        {

            i_initialBatch->MarkSent();
        }
    }

    for (std::set<Occupant*>::const_iterator vItr = i_visibleNow.begin(); vItr != i_visibleNow.end(); ++vItr)
    {

        if ((*vItr) != &player && IsType(*vItr, TYPEMASK_UNIT))
        {
            player.SendAuraDurationsForTarget((Unit*)(*vItr));
        }
    }
}

template<class T>

void ObjectUpdater::Visit(GridRefManager<T>& m)
{
    for (typename GridRefManager<T>::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Occupant::UpdateHelper helper(iter->getSource());
        helper.Update(i_timeDiff);
    }
}

bool CannibalizeObjectCheck::operator()(Corpse* u)
{

    if (u->GetType() == CORPSE_BONES)
    {
        return false;
    }

    Player* owner = sPlayerRegistry.Find(u->GetOwnerGuid());

    if (!owner || IsFriendly(*i_fobj, *owner))
    {
        return false;
    }

    if (InReach(*i_fobj, *u, i_range))
    {
        return true;
    }

    return false;
}

void MaNGOS::RespawnDo::operator()(Creature* u) const
{

    Map* map = u->GetMap();
    if (map->IsBattleGround())
    {
        BattleGroundEventIdx eventId = sBattleGroundMgr.GetCreatureEventIndex(u->GetGUIDLow());
        if (!((BattleGroundMap*)map)->GetBG()->IsActiveEvent(eventId.event1, eventId.event2))
        {
            return;
        }
    }

    u->Respawn();
}

void MaNGOS::RespawnDo::operator()(GameObject* u) const
{

    Map* map = u->GetMap();
    if (map->IsBattleGround())
    {
        BattleGroundEventIdx eventId = sBattleGroundMgr.GetGameObjectEventIndex(u->GetGUIDLow());
        if (!((BattleGroundMap*)map)->GetBG()->IsActiveEvent(eventId.event1, eventId.event2))
        {
            return;
        }
    }

    u->Respawn();
}

void MaNGOS::CallOfHelpCreatureInRangeDo::operator()(Creature* u)
{
    if (u == i_funit)
    {
        return;
    }

    if (!u->CanAssistTo(i_funit, i_enemy, false))
    {
        return;
    }

    if (!InReach(*i_funit, *u, i_range))
    {
        return;
    }

    if (!HasLineOfSight(*i_funit, *u))
    {
        return;
    }

    if (u->AI())
    {
        u->AI()->AttackStart(i_enemy);
    }
}

bool MaNGOS::AnyAssistCreatureInRangeCheck::operator()(Creature* u)
{
    if (u == i_funit)
    {
        return false;
    }

    if (!u->CanAssistTo(i_funit, i_enemy))
    {
        return false;
    }

    if (!InReach(*i_funit, *u, i_range))
    {
        return false;
    }

    if (!HasLineOfSight(*i_funit, *u))
    {
        return false;
    }

    return true;
}

template void ObjectUpdater::Visit<GameObject>(GameObjectMapType&);
template void ObjectUpdater::Visit<DynamicObject>(DynamicObjectMapType&);
