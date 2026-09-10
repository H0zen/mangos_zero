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

namespace stats
{

    float const PER_SKILL_POINT = 0.04f;

    float const GUARD_FROM_NOTHING = 5.0f;

    inline float Chance(float from, int32 skill, int32 allowedByLevel, float fromAuras)
    {
        float const chance = from + (skill - allowedByLevel) * PER_SKILL_POINT + fromAuras;

        return chance < 0.0f ? 0.0f : chance;
    }

    struct ManaRegen
    {
        float standing = 0.0f;
        float casting = 0.0f;
    };

    inline ManaRegen Regeneration(float fromSpirit, float spiritShare, float flatPerFive,
                                  int32 survivesCasting)
    {
        float const spirit = fromSpirit * spiritShare;
        float const flat = flatPerFive / 5.0f;

        if (survivesCasting > 100)
        {
            survivesCasting = 100;
        }

        ManaRegen regen;
        regen.standing = flat + spirit;
        regen.casting = flat + spirit * survivesCasting / 100.0f;

        return regen;
    }
}
