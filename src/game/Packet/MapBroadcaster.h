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

#pragma once

#include "Platform/Define.h"

#include <functional>

class Map;
class Occupant;
class Player;
class WorldPacket;
class MapBroadcaster;

class Audience
{
    public:

        enum class Gathering
        {
            Near,
            Ranged,
            Roll,
            Zone,
        };

        static Audience Around(Occupant const& subject);

        static Audience Within(Occupant const& subject, float yards);

        static Audience Everyone(Map& map);

        static Audience InZone(Map& map, uint32 zoneId);

        Audience& AndSubject(bool yes = true);

        Audience& Except(Player const* one);

        Audience& OwnTeamOnly(bool yes = true);

        Gathering How() const { return m_how; }
        Occupant const* Subject() const { return m_subject; }
        uint32 Zone() const { return m_zone; }

        float Range() const { return m_range; }
        bool HasRange() const { return m_how == Gathering::Ranged; }

        bool WithSubject() const { return m_withSubject; }

        MapBroadcaster* On() const { return m_on; }

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

class MapBroadcaster
{
    public:

        virtual ~MapBroadcaster() = default;

        typedef std::function<void(Player*)> Listener;

        uint32 Reach(Audience const& who, Listener const& tell);

    protected:

        virtual uint32 Hearers(Audience const& who, Listener const& tell) = 0;

        virtual uint32 Across(Audience const& who, Listener const& tell) = 0;
};

uint32 Deliver(Audience const& who, WorldPacket* what);

uint32 Deliver(Audience const& who, MapBroadcaster::Listener const& tell);

template<typename Maker>
uint32 Deliver(Audience const& who, Maker& make)
{
    return Deliver(who, MapBroadcaster::Listener([&make](Player* listener) { make(listener); }));
}
