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
#include "Occupant.h"
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
#include "Movement/Spline/packet_builder.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"
#include "Transports.h"
#include "TransportMap.h"
#include "MapManager.h"


namespace
{
    /**
     * @brief OUTBOUND -- from a deck to the watchers ashore.
     *
     * A deckhand's own map is the hull, and no cell ashore will ever hold him, so a packet
     * that stays inside his map is never seen from the pier. The audience is whoever the
     * vessel gathered at the top of this tick. Sent immediately: the deck runs INSIDE the
     * tick of the map she sails, on that map's own thread, so there is nothing to wait for.
     */
    void RelayAshore(Occupant const& from, WorldPacket* data, Player const* skip)
    {
        if (!from.GetMap()->AsTransport())
        {
            return;
        }

        for (Player* observer : from.GetMap()->ExternalObservers())
        {
            if (observer && observer != skip && observer->GetSession())
            {
                observer->GetSession()->SendPacket(data);
            }
        }
    }

    /**
     * @brief INBOUND -- from the water to everyone standing on a vessel crossing it.
     *
     * Not filtered by anything. The one object that could measure the distance is the
     * vessel, whose pose is an estimate we already refuse to trust for anything that
     * decides something -- and being told about a creature too far off costs a packet,
     * while not being told about one in front of you costs the illusion that the world
     * is running.
     */
    void RelayAboard(Occupant const& from, WorldPacket* data, Player const* skip)
    {
        if (from.GetMap()->AsTransport())
        {
            return;
        }

        MapManager::TransportsByMapType::const_iterator vessels =
            sMapMgr.m_TransportsByMap.find(from.GetMapId());
        if (vessels == sMapMgr.m_TransportsByMap.end())
        {
            return;
        }

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
                if (passenger && passenger != skip && passenger->GetSession())
                {
                    passenger->GetSession()->SendPacket(data);
                }
            }
        }
    }
}

/**
 * @brief Delivers a packet to the sessions that can see an object.
 *
 * @param from The object the packet is about.
 * @param data The packet to send.
 * @param toSubject Also deliver to the subject's own client, if it has one.
 */
void Broadcast(Occupant const& from, WorldPacket* data, bool toSubject)
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

        RelayAshore(from, data, ToPlayer(&from));
        RelayAboard(from, data, ToPlayer(&from));
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
void BroadcastWithin(Occupant const& from, WorldPacket* data, float dist, bool toSubject, bool ownTeamOnly)
{
    if (from.IsInWorld())
    {
        PacketReach reach;
        reach.subject = &from;
        reach.skip = ToPlayer(&from);
        reach.dist = dist;
        reach.ownTeamOnly = ownTeamOnly;
        from.GetMap()->DeliverPacket(data, reach);

        // The radius above applies among those who share the speaker's map. Whoever is on
        // the other side of a vessel's boundary is not measured at all. Only outbound:
        // whether a shout from the pier carries to the passengers is a question about
        // range, and there is no range across the boundary to answer it with.
        RelayAshore(from, data, ToPlayer(&from));
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
void BroadcastExcept(Occupant const& from, WorldPacket* data, Player const* skip)
{
    if (!from.IsInWorld())
    {
        return;
    }

    PacketReach reach;
    reach.subject = &from;
    reach.skip = skip;
    from.GetMap()->DeliverPacket(data, reach);

    // Both sides of the boundary, exactly as Broadcast does. This carries a player's own
    // movement: he walks ashore, his packets start coming from the continent's map, and
    // his shipmates are on the hull's. Whoever is told nothing keeps replaying the last
    // thing he saw -- the walk off the gangplank -- for as long as the man stands still.
    RelayAshore(from, data, skip);
    RelayAboard(from, data, skip);
}

