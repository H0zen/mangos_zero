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
#include "ObjectGuid.h"

namespace combat
{

    struct Combatant
    {
        ObjectGuid guid = 0;

        uint32 level = 1;
        bool isPlayer = false;
        bool isPet = false;

        bool isEvading = false;

        bool isSitting = false;

        uint8 classId = 0;

        int32 health = 0;

        int32 missChance = 0;
        int32 critChance = 0;
        int32 dodgeChance = 0;
        int32 parryChance = 0;
        int32 blockChance = 0;

        int32 weaponSkill = 0;
        int32 maxSkillForLevel = 0;

        int32 defenceSkill = 0;
        int32 maxDefenceForLevel = 0;

        bool canParry = true;
        bool canBlock = true;
        bool canCrush = false;
    };
}
