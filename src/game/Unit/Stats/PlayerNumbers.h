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
#include "SharedDefines.h"

namespace stats
{

    inline uint32 PlayerShieldBlock(float flat, float pct, float strength)
    {
        float const value = (flat + strength / 0.5f - 10.0f) * pct;
        return value < 0.0f ? 0u : uint32(value);
    }

    inline float HealthFromStamina(float stamina)
    {
        float const cheap = stamina < 20.0f ? stamina : 20.0f;
        return cheap + (stamina - cheap) * 10.0f;
    }

    inline float ManaFromIntellect(float intellect)
    {
        float const cheap = intellect < 20.0f ? intellect : 20.0f;
        return cheap + (intellect - cheap) * 15.0f;
    }

    inline float PlayerArmour(Modifiers const& mods, float agility, float fromIntellect)
    {
        return (mods.Base() + agility * 2.0f + mods.totalValue + fromIntellect) * mods.TotalPct();
    }

    inline float PlayerMaxHealth(Modifiers const& mods, float createdWith, float fromStamina)
    {
        return ((mods.baseValue + createdWith) * mods.basePct
                + mods.totalValue + fromStamina) * mods.TotalPct();
    }

    inline float PlayerMaxPower(Modifiers const& mods, float createdWith, float fromIntellect)
    {
        return ((mods.baseValue + createdWith) * mods.basePct
                + mods.totalValue + fromIntellect) * mods.TotalPct();
    }

    struct Physique
    {
        uint8 klass = 0;
        float level = 0.0f;
        float strength = 0.0f;
        float agility = 0.0f;
        uint32 form = 0;

        float predatoryStrikes = 0.0f;
    };

    inline bool IsFeral(uint32 form)
    {
        return form == FORM_CAT || form == FORM_BEAR || form == FORM_DIREBEAR;
    }

    inline float MeleeAttackPower(Physique const& who)
    {
        switch (who.klass)
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_SHAMAN:
                return who.level * (who.klass == CLASS_SHAMAN ? 2.0f : 3.0f)
                       + who.strength * 2.0f - 20.0f;

            case CLASS_ROGUE:
            case CLASS_HUNTER:
                return who.level * 2.0f + who.strength + who.agility - 20.0f;

            case CLASS_DRUID:
                switch (who.form)
                {
                    case FORM_CAT:
                        return who.level * who.predatoryStrikes
                               + who.strength * 2.0f + who.agility - 20.0f;
                    case FORM_BEAR:
                    case FORM_DIREBEAR:
                        return who.level * who.predatoryStrikes + who.strength * 2.0f - 20.0f;
                    case FORM_MOONKIN:
                        return who.level * (who.predatoryStrikes + 1.5f)
                               + who.strength * 2.0f - 20.0f;
                    default:
                        return who.strength * 2.0f - 20.0f;
                }

            case CLASS_MAGE:
            case CLASS_PRIEST:
            case CLASS_WARLOCK:
                return who.strength - 10.0f;

            default:
                return 0.0f;
        }
    }

    inline float RangedAttackPower(Physique const& who)
    {
        switch (who.klass)
        {
            case CLASS_HUNTER:
                return who.level * 2.0f + who.agility * 2.0f - 10.0f;

            case CLASS_ROGUE:
            case CLASS_WARRIOR:
                return who.level + who.agility - 10.0f;

            case CLASS_DRUID:

                return IsFeral(who.form) ? 0.0f : who.agility - 10.0f;

            default:
                return who.agility - 10.0f;
        }
    }
}
