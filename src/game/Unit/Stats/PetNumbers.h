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

/**
 * The numbers a pet fights with, worked out and nothing else.
 *
 * A pet sits between a creature and a player. Like a creature it has a template
 * it was made from, and like a player its stats feed its other numbers: agility
 * makes armour, stamina makes health, intellect makes mana, strength makes the
 * power behind its swing. What it does NOT share with either is the arithmetic
 * for turning them, which is why these are their own functions and not the
 * creature ones with an argument added.
 *
 * Everything derived is derived from what the pet has GAINED over what it was
 * created with. The created amount is already in the base.
 */
namespace stats
{
    /// Two points of armour for every point of agility.
    inline float PetArmour(Modifiers const& mods, float agility)
    {
        return (mods.Base() + agility * 2.0f + mods.totalValue) * mods.TotalPct();
    }

    /// Ten health for every point of stamina gained since it was made.
    inline float PetMaxHealth(Modifiers const& mods, float createdWith, float staminaGained)
    {
        return ((mods.baseValue + createdWith) * mods.basePct
                + mods.totalValue + staminaGained * 10.0f) * mods.TotalPct();
    }

    /**
     * @brief A pool of power at its fullest.
     *
     * @param gained Intellect above what it was made with -- and only for mana:
     *        nothing else in 1.12 grows from a stat, so the caller passes zero.
     */
    inline float PetMaxPower(Modifiers const& mods, float createdWith, float gained)
    {
        return ((mods.baseValue + createdWith) * mods.basePct
                + mods.totalValue + gained * 15.0f) * mods.TotalPct();
    }

    /**
     * @brief The attack power strength is worth.
     *
     * Twice strength, less the twenty a pet is assumed to start with. An imp is
     * the exception and gets it one for one -- it is the only pet in the game
     * whose attack power is not doubled.
     */
    inline float PetAttackPowerFromStrength(float strength, bool isImp)
    {
        return isImp ? strength - 10.0f : 2.0f * strength - 20.0f;
    }

    /**
     * @brief The damage a pet's swing does.
     *
     * Unlike a creature's, this takes the attack power WHOLE rather than the part
     * gained over a template, and it has no template multiplier over it.
     *
     * @param attackSeconds How long one swing takes, which is how attack power
     *        becomes damage: fourteen points is a second's worth.
     */
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

    /// The three moods, mirroring HappinessState. Where Pet.h is in scope the two
    /// are held together by a static assertion rather than by anyone remembering.
    uint32 const PET_UNHAPPY = 1;
    uint32 const PET_CONTENT = 2;
    uint32 const PET_HAPPY = 3;

    /**
     * @brief What a hunter pet's mood does to its damage.
     *
     * A happy one hits a quarter harder, an unhappy one a quarter softer, and a
     * merely content one exactly as it would anyway.
     */
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
