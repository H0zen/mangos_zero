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

#define MAX_PLAYER_SUMMON_DELAY (2*MINUTE)

struct ResurrectOffer
{
    ObjectGuid from = 0;
    Geometry::Placement at;
    uint32 health = 0;
    uint32 mana = 0;

    bool Stands() const { return !(from == 0); }
    bool StandsFrom(ObjectGuid who) const { return from == who; }

    bool MovesHim() const { return (from != 0 && GuidHigh(from) == HIGHGUID_PLAYER); }

    void Withdraw() { *this = ResurrectOffer(); }
};

struct SummonOffer
{
    Geometry::Placement at;
    time_t expiresAt = 0;

    bool Stands(time_t now) const { return expiresAt >= now; }

    void Offer(uint32 mapId, float x, float y, float z, time_t now)
    {
        at = Geometry::Placement::Somewhere(mapId, Geometry::Vector3(x, y, z));
        expiresAt = now + MAX_PLAYER_SUMMON_DELAY;
    }

    void Withdraw() { expiresAt = 0; }
};
