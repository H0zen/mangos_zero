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

/**
 * @file Object.cpp
 * @brief Base implementation for all game objects
 *
 * This file implements the Object class, which is the base class for all
 * entities in the game world. It provides:
 * - Update field management (synchronized with clients)
 * - Object GUID handling
 * - Update data building for network transmission
 * - Object visibility and spawning
 * - Type identification
 *
 * The Object class uses an array of uint32 values (update fields) that
 * mirror the client's object state. Changes to these values are sent to
 * players who can see the object.
 */



#include "Utilities/Errors.h"
#include "Object.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "Creature.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "UpdateData.h"
#include "Util.h"
#include "MapManager.h"
#include "Transports.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "movement/packet_builder.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"
#include "Transports.h"
#include "TransportMap.h"
#include "MapManager.h"

/**
 * @brief Delivers a packet to the sessions that can see an object.
 *
 * @param from The object the packet is about.
 * @param data The packet to send.
 * @param toSubject Also deliver to the subject's own client, if it has one.
 */
void Broadcast(WorldObject const& from, WorldPacket* data, bool toSubject)
{
    if (from.IsInWorld())
    {
        // A boarded unit's audience is EVERYONE ON ITS OWN MAP -- which is the deck, and
        // which is exactly where the passengers are too. That is the whole point of the
        // vessel being a map: this reaches the people standing next to him.
        PacketReach reach;
        reach.subject = &from;
        reach.skip = ToPlayer(&from);
        from.GetMap()->DeliverPacket(data, reach);

        // THE RELAY, OUTBOUND. The people ashore are on another map and no cell of theirs
        // will ever hold this deckhand, so the same packet goes out again to the watchers
        // the vessel gathered at the top of this tick. Sent immediately: the deck runs
        // INSIDE the tick of the map it sails, on that map's own thread, so there is
        // nothing to wait for and no race to avoid. Without this a deckhand walks for his
        // shipmates and stands frozen for the pier.
        if (from.GetMap()->AsTransport())
        {
            for (Player* observer : from.GetMap()->ExternalObservers())
            {
                if (observer && observer->GetSession())
                {
                    observer->GetSession()->SendPacket(data);
                }
            }
        }
        // THE RELAY, INBOUND, and it is not filtered by anything. Whatever happens on the
        // water a ship is crossing has to reach the people standing on her: they are on
        // another map, no cell of theirs will ever hold the thing that moved, and a
        // passenger who is told nothing watches a harbour full of statues.
        //
        // No distance test. The one object that could measure it is the vessel, whose pose
        // is an estimate we already refuse to trust for anything that decides something --
        // and being told about a creature too far away costs a packet, while not being told
        // about one in front of you costs the illusion that the world is running.
        MapManager::TransportsByMapType::const_iterator vessels =
            sMapMgr.m_TransportsByMap.find(from.GetMapId());
        if (!from.GetMap()->AsTransport() && vessels != sMapMgr.m_TransportsByMap.end())
        {
            for (Transport* vessel : vessels->second)
            {
                TransportMap* hull = vessel->AsMap();
                if (!hull || vessel->GetMap() != from.GetMap())
                {
                    continue;
                }

                Map::PlayerList const& aboard = hull->GetPlayers();
                for (Map::PlayerList::const_iterator itr = aboard.begin(); itr != aboard.end(); ++itr)
                {
                    Player* passenger = itr->getSource();
                    if (passenger && passenger->GetSession())
                    {
                        passenger->GetSession()->SendPacket(data);
                    }
                }
            }
        }
    }

    if (toSubject)
    {
        if (Player const* self = ToPlayer(&from))
        {
            if (WorldSession* session = self->GetSession())
            {
                session->SendPacket(data);
            }
        }
    }
}

/**
 * @brief Delivers a packet to the sessions within a distance of an object.
 *
 * @param from The object the packet is about, and the distance origin.
 * @param data The packet to send.
 * @param dist The delivery distance.
 * @param toSubject Also deliver to the subject's own client, if it has one.
 * @param ownTeamOnly Deliver only to viewers on the subject's side.
 */
void BroadcastWithin(WorldObject const& from, WorldPacket* data, float dist, bool toSubject, bool ownTeamOnly)
{
    if (from.IsInWorld())
    {
        PacketReach reach;
        reach.subject = &from;
        reach.skip = ToPlayer(&from);
        reach.dist = dist;
        reach.ownTeamOnly = ownTeamOnly;
        from.GetMap()->DeliverPacket(data, reach);
    }

    if (toSubject)
    {
        if (Player const* self = ToPlayer(&from))
        {
            if (WorldSession* session = self->GetSession())
            {
                session->SendPacket(data);
            }
        }
    }
}

/**
 * @brief Delivers a packet to the sessions that can see an object, bar one.
 *
 * @param from The object the packet is about.
 * @param data The packet to send.
 * @param skip The viewer to leave out.
 */
void BroadcastExcept(WorldObject const& from, WorldPacket* data, Player const* skip)
{
    if (!from.IsInWorld())
    {
        return;
    }

    PacketReach reach;
    reach.subject = &from;
    reach.skip = skip;
    from.GetMap()->DeliverPacket(data, reach);
}

