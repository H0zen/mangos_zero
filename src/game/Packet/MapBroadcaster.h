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
 * @file MapBroadcaster.h
 * @brief Everything a map says out loud, and the one way it says it.
 *
 * A packet leaving a map is a pair: an audience, and something to say to it. Audience is
 * the first half and Deliver() takes both -- there is no second way, no per-caller loop
 * over players, and no map method that sends one particular packet.
 *
 * @code
 *     Deliver(Audience::Around(*this).AndSubject(), &data);
 *     Deliver(Audience::Within(speaker, yellRange).AndSubject(), &data);
 *     Deliver(Audience::InZone(map, zoneId), &data);
 *     Deliver(Audience::Everyone(map), localizedLine);   // one packet per locale
 * @endcode
 *
 * MapBroadcaster is the map's half: two virtuals, one for the listeners standing on this
 * map and one for those across its boundary. Map answers the first for every kind of map;
 * TransportMap is the map whose boundary runs the other way, and answers the second
 * differently. Nothing else in the engine tests what kind of map it is holding.
 */

#pragma once

#include "Platform/Define.h"

#include <functional>

class Map;
class Occupant;
class Player;
class WorldPacket;
class MapBroadcaster;

/**
 * @brief The people who hear one packet.
 *
 * Named the way the game names them -- everyone around him, everyone within thirty yards
 * of him, everyone on this map, everyone in this zone -- and never as a loop over a
 * container. Built by a named constructor, then narrowed by modifiers that chain.
 *
 * What it deliberately does NOT carry is the vessel boundary. Whether a packet crosses
 * between a deck and the water she sails is the boundary's own rule, and it is answered
 * by the map that owns it -- see MapBroadcaster::Across.
 */
class Audience
{
    public:

        /// Which container the listeners come out of.
        enum class Gathering
        {
            Near,       ///< the cells around the subject, at the map's own broadcast radius
            Ranged,     ///< the same, cut to a distance measured from the subject
            Roll,       ///< every player on the map, wherever they stand
            Zone,       ///< every player on the map standing in one zone
        };

        /// Everyone who can see him, at whatever distance this map broadcasts.
        static Audience Around(Occupant const& subject);

        /// Everyone within `yards` of him. Measured to what a viewer looks THROUGH, which
        /// is not always where the viewer stands.
        static Audience Within(Occupant const& subject, float yards);

        /// Every player on the map, cells and distance irrelevant.
        static Audience Everyone(Map& map);

        /// Every player on the map standing in this zone.
        static Audience InZone(Map& map, uint32 zoneId);

        /// Also the subject's own client: a player is told about his own doings directly
        /// rather than through the cells, and this is how that is said.
        Audience& AndSubject(bool yes = true);

        /// All but this one viewer.
        Audience& Except(Player const* one);

        /// Only viewers on the subject's own side. Meaningful only when the subject is a
        /// player, since nothing else has a side.
        Audience& OwnTeamOnly(bool yes = true);

        Gathering How() const { return m_how; }
        Occupant const* Subject() const { return m_subject; }
        uint32 Zone() const { return m_zone; }

        /// The distance listeners are cut to; zero means the map's own broadcast radius.
        float Range() const { return m_range; }
        bool HasRange() const { return m_how == Gathering::Ranged; }

        bool WithSubject() const { return m_withSubject; }

        /// The map that will carry it, or nullptr when the subject is not in the world.
        MapBroadcaster* On() const { return m_on; }

        /// Everything the audience decides about a listener without measuring anything.
        bool Admits(Player const* listener) const;

    private:

        Audience() = default;

        Gathering m_how = Gathering::Near;
        Occupant const* m_subject = nullptr;
        MapBroadcaster* m_on = nullptr;
        Player const* m_skip = nullptr;
        float m_range = 0.0f;
        uint32 m_zone = 0;
        bool m_withSubject = false;
        bool m_ownTeamOnly = false;
};

/**
 * @brief A map, seen as the thing that talks to the clients on and around it.
 *
 * The two virtuals are the whole of what a kind of map gets to decide. Everything above
 * them -- who is admitted, whether the subject hears himself, what is actually sent -- is
 * settled once, here and in Audience.
 */
class MapBroadcaster
{
    public:

        virtual ~MapBroadcaster() = default;

        /// What one listener is handed. A ready packet for most senders; a packet built in
        /// the listener's own locale for a line read from the string table.
        typedef std::function<void(Player*)> Listener;

        /// Hand it to everyone the audience names, here and across the boundary.
        /// @return how many clients heard it.
        uint32 Reach(Audience const& who, Listener const& tell);

    protected:

        /// The listeners on this map itself: the cells around the subject, or the roll.
        virtual uint32 Hearers(Audience const& who, Listener const& tell) = 0;

        /// The listeners on the far side of this map's boundary. Water hands to the decks
        /// crossing it; a deck hands to the shore her vessel gathered. A map with no
        /// boundary of its own reaches nobody.
        virtual uint32 Across(Audience const& who, Listener const& tell) = 0;
};

/// Say one thing to one audience. THE way a packet leaves a map.
/// @return how many clients heard it, so a sender who needs to know whether anybody was
///         there -- the weather, for one -- need not walk anything itself.
uint32 Deliver(Audience const& who, WorldPacket* what);

/// The same, with each listener handed to `tell` instead of a ready packet.
uint32 Deliver(Audience const& who, MapBroadcaster::Listener const& tell);

/// The same, with the packet built afresh for each listener: `make` is anything callable
/// with a Player*, which is what MaNGOS::LocalizedPacketDo is.
template<typename Maker>
uint32 Deliver(Audience const& who, Maker& make)
{
    return Deliver(who, MapBroadcaster::Listener([&make](Player* listener) { make(listener); }));
}
