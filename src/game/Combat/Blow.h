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

    enum class Delivery : uint8
    {
        MeleeMain,
        MeleeOff,
        Ranged,
        Spell,
        Periodic,
        Environment,
    };

    inline bool IsWeaponSwing(Delivery delivery)
    {
        return delivery == Delivery::MeleeMain || delivery == Delivery::MeleeOff ||
               delivery == Delivery::Ranged;
    }

    enum class Result : uint8
    {
        Landed,
        Missed,
        Dodged,
        Parried,
        Blocked,
        Evaded,
        Immune,
        Deflected,
        Absorbed,
        Resisted,
        Reflected,
    };

    inline bool Landed(Result result)
    {
        return result == Result::Landed;
    }

    struct Strike
    {
        Result result = Result::Landed;

        bool crit = false;
        bool glancing = false;
        bool crushing = false;
        bool blocked = false;

        bool Landed() const { return combat::Landed(result); }
    };

    struct Blow
    {
        ObjectGuid attacker = 0;
        ObjectGuid victim = 0;

        Delivery delivery = Delivery::MeleeMain;

        uint32 spellId = 0;
        uint8 effectIndex = 0;

        School school = School::Physical;
        int32 amount = 0;

        bool canCrit = true;
        bool triggered = false;

        bool SelfInflicted() const { return attacker == victim; }
    };

    struct AbsorbShare
    {
        ObjectGuid caster = 0;
        uint32 spellId = 0;
        int32 amount = 0;
        int32 manaSpent = 0;
        bool exhausted = false;
    };

    struct SplitShare
    {
        ObjectGuid target = 0;
        uint32 spellId = 0;
        int32 amount = 0;
    };

    struct Outcome
    {
        Strike strike;

        int32 dealt = 0;
        int32 absorbed = 0;
        int32 resisted = 0;
        int32 blocked = 0;
        int32 overkill = 0;

        bool victimDies = false;

        std::vector<AbsorbShare> absorbs;
        std::vector<SplitShare> splits;

        int32 manaSpent = 0;

        int32 beforeMitigation = 0;

        Result Ending() const { return strike.result; }
        bool Landed() const { return strike.Landed(); }
    };
}
