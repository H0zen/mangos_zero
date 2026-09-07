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

#include "MapBroadcaster.h"

#include "Map.h"
#include "Occupant.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"

Audience Audience::Around(Occupant const& subject)
{
    Audience who;
    who.m_how = Gathering::Near;
    who.m_subject = &subject;
    who.m_on = subject.FindMap();
    who.m_skip = ToPlayer(&subject);

    return who;
}

Audience Audience::Within(Occupant const& subject, float yards)
{
    Audience who = Around(subject);
    who.m_how = Gathering::Ranged;
    who.m_range = yards;

    return who;
}

Audience Audience::Everyone(Map& map)
{
    Audience who;
    who.m_how = Gathering::Roll;
    who.m_on = &map;

    return who;
}

Audience Audience::InZone(Map& map, uint32 zoneId)
{
    Audience who;
    who.m_how = Gathering::Zone;
    who.m_on = &map;
    who.m_zone = zoneId;

    return who;
}

Audience& Audience::AndSubject(bool yes)
{
    m_withSubject = yes;

    return *this;
}

Audience& Audience::Except(Player const* one)
{
    m_skip = one;

    return *this;
}

Audience& Audience::OwnTeamOnly(bool yes)
{
    m_ownTeamOnly = yes;

    return *this;
}

bool Audience::Admits(Player const* listener) const
{
    if (!listener || listener == m_skip)
    {
        return false;
    }

    if (m_ownTeamOnly)
    {
        Player const* subject = ToPlayer(m_subject);
        if (subject && listener->GetTeam() != subject->GetTeam())
        {
            return false;
        }
    }

    return true;
}

uint32 MapBroadcaster::Reach(Audience const& who, Listener const& tell)
{
    return Hearers(who, tell) + Across(who, tell);
}

uint32 Deliver(Audience const& who, MapBroadcaster::Listener const& tell)
{
    uint32 told = 0;

    // A subject outside the world has no map: there is nothing standing around him to
    // hear this, while his own client is still owed it.
    if (MapBroadcaster* stage = who.On())
    {
        told += stage->Reach(who, tell);
    }

    if (who.WithSubject())
    {
        if (Player* self = const_cast<Player*>(ToPlayer(who.Subject())))
        {
            if (self->GetSession())
            {
                tell(self);
                ++told;
            }
        }
    }

    return told;
}

uint32 Deliver(Audience const& who, WorldPacket* what)
{
    return Deliver(who, MapBroadcaster::Listener([what](Player* listener)
    {
        listener->GetSession()->SendPacket(what);
    }));
}
