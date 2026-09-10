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

#include "Combat/School.h"
#include "Platform/Define.h"
#include "ObjectGuid.h"

#include <vector>

namespace combat
{

    struct Absorber
    {
        ObjectGuid caster = 0;
        uint32 spellId = 0;
        int32 remaining = 0;
        SchoolSet covers = SchoolSet::All();
        float manaMultiplier = 0.f;

        bool CostsMana() const { return manaMultiplier > 0.f; }

        bool Covers(School school) const { return covers.Contains(school); }
    };

    struct Splitter
    {
        ObjectGuid target = 0;
        uint32 spellId = 0;
        int32 flat = 0;
        float fraction = 0.f;
    };

    struct Defences
    {
        int32 armour = 0;

        int32 resistance = 0;

        int32 blockValue = 0;

        bool immune = false;

        int32 mana = 0;

        std::vector<Absorber> absorbers;
        std::vector<Splitter> splitters;
    };
}
