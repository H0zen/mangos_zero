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

#include "CreatureNumbers.h"

namespace stats
{

    inline float PetArmour(Modifiers const& mods, float agility)
    {
        return (mods.Base() + agility * 2.0f + mods.totalValue) * mods.TotalPct();
    }

    inline float PetMaxHealth(Modifiers const& mods, float createdWith, float staminaGained)
    {
        return ((mods.baseValue + createdWith) * mods.basePct
                + mods.totalValue + staminaGained * 10.0f) * mods.TotalPct();
    }

    inline float PetMaxPower(Modifiers const& mods, float createdWith, float gained)
    {
        return ((mods.baseValue + createdWith) * mods.basePct
                + mods.totalValue + gained * 15.0f) * mods.TotalPct();
    }

    inline float PetAttackPowerFromStrength(float strength, bool isImp)
    {
        return isImp ? strength - 10.0f : 2.0f * strength - 20.0f;
    }

    inline Swing PetSwing(Modifiers const& mods, float weaponLeast, float weaponMost,
                          float attackPower, float attackSeconds)
    {
        float const base = mods.baseValue + attackPower / 14.0f * attackSeconds;
        float const share = mods.TotalPct();

        Swing swing;
        swing.least = ((base + weaponLeast) * mods.basePct + mods.totalValue) * share;
        swing.most = ((base + weaponMost) * mods.basePct + mods.totalValue) * share;

        return swing;
    }

    uint32 const PET_UNHAPPY = 1;
    uint32 const PET_CONTENT = 2;
    uint32 const PET_HAPPY = 3;

    inline float HappinessScale(uint32 happiness)
    {
        switch (happiness)
        {
            case PET_HAPPY: return 1.25f;
            case PET_UNHAPPY: return 0.75f;
            default: return 1.0f;
        }
    }
}
