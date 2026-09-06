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

class Player;
class Occupant;

/**
 * Who a packet reaches.
 *
 * Delivering a packet is one loop over the cameras near a position. What varies
 * between one broadcast and the next is only which viewers that loop skips, and
 * this is that difference, so the loop itself is written once.
 */
struct PacketReach
{
    /// Whose surroundings, and what the distance is measured from.
    Occupant const* subject = nullptr;

    /// One viewer not to deliver to. For an ordinary broadcast this is the
    /// subject itself when the subject has a client, because a player is told
    /// about their own doings directly and not through the grid.
    Player const* skip = nullptr;

    /// Zero means no distance filter, and the visit falls back to the map's own
    /// broadcast radius. A positive value is measured to the viewer's *body* --
    /// what they are looking through, which is not always where they stand.
    float dist = 0.0f;

    /// Deliver only to viewers on the subject's side. Meaningful only when the
    /// subject is a player, since nothing else has a side.
    bool ownTeamOnly = false;

    /// A deck and the shore it sails past are two maps, so a packet that stays inside
    /// one of them is never seen from the other. CROSSING IS THE DEFAULT: a sender that
    /// says nothing about the boundary reaches both sides, and one that must not reach
    /// a side has to say so here. Written this way round because the failure of the
    /// other is silent -- a sender that forgets leaves half the world watching statues.
    ///
    /// Outbound: a deck's packet also goes to the watchers the vessel gathered ashore.
    bool ashore = true;

    /// Inbound: a packet from the water also goes to everyone standing on a vessel
    /// crossing it. Unfiltered by distance -- the only object that could measure it is
    /// the vessel, whose pose is a waypoint guess we refuse to trust for anything that
    /// decides something. A range-limited packet therefore has no range to answer with
    /// and turns this off rather than guessing.
    bool aboard = true;
};
