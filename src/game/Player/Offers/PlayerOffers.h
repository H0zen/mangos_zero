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

#include "Geometry/Placement.h"
#include "Common/TimeConstants.h"
#include "ObjectGuid.h"
#include "Platform/Define.h"

#include <ctime>

/// How long a summons waits for an answer before it counts as refused.
#define MAX_PLAYER_SUMMON_DELAY (2*MINUTE)

/**
 * An offer to raise a dead character, standing until he answers it.
 *
 * The place is where the one offering stands, and a character raised by another
 * player is put there first so that he does not come back beside his corpse with
 * whatever killed him still on it. A spell that raises him where he lies leaves
 * the guid empty, and then nothing is moved.
 *
 * The health and mana are what the raising is worth; each is a ceiling, so a
 * character with less of either to give ends up full instead of over.
 *
 * There is no deadline. The client keeps the window open until it is answered or
 * the one who offered goes away.
 */
struct ResurrectOffer
{
    ObjectGuid from;
    Geometry::Placement at;
    uint32 health = 0;
    uint32 mana = 0;

    bool Stands() const { return !from.IsEmpty(); }
    bool StandsFrom(ObjectGuid who) const { return from == who; }

    /// Whether the one offering was a player, and so whether the place is worth
    /// moving to before raising him.
    bool MovesHim() const { return from.IsPlayer(); }

    void Withdraw() { *this = ResurrectOffer(); }
};

/**
 * An offer to bring a character to where someone else stands.
 *
 * Unlike a raising, this one runs out: a summons not answered within two minutes
 * is refused for him. The deadline is the whole of the offer's state, so a
 * deadline already past is the same thing as no offer at all -- which is why
 * refusing one is done by setting it to nothing rather than by a flag beside it.
 */
struct SummonOffer
{
    Geometry::Placement at;
    time_t expiresAt = 0;

    /// A summons stands up to and including the moment it expires.
    bool Stands(time_t now) const { return expiresAt >= now; }

    void Offer(uint32 mapId, float x, float y, float z, time_t now)
    {
        at = Geometry::Placement::Somewhere(mapId, Geometry::Vector3(x, y, z));
        expiresAt = now + MAX_PLAYER_SUMMON_DELAY;
    }

    void Withdraw() { expiresAt = 0; }
};
