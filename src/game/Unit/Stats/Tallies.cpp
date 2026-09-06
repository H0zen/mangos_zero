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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Tallies.h"
#include "Log.h"

Stats stats::StatOf(UnitMods group)
{
    switch (group)
    {
        case UNIT_MOD_STAT_STRENGTH:    return STAT_STRENGTH;
        case UNIT_MOD_STAT_AGILITY:     return STAT_AGILITY;
        case UNIT_MOD_STAT_STAMINA:     return STAT_STAMINA;
        case UNIT_MOD_STAT_INTELLECT:   return STAT_INTELLECT;
        case UNIT_MOD_STAT_SPIRIT:      return STAT_SPIRIT;
        default:                        return STAT_STRENGTH;
    }
}

SpellSchools stats::SchoolOf(UnitMods group)
{
    switch (group)
    {
        case UNIT_MOD_RESISTANCE_HOLY:     return SPELL_SCHOOL_HOLY;
        case UNIT_MOD_RESISTANCE_FIRE:     return SPELL_SCHOOL_FIRE;
        case UNIT_MOD_RESISTANCE_NATURE:   return SPELL_SCHOOL_NATURE;
        case UNIT_MOD_RESISTANCE_FROST:    return SPELL_SCHOOL_FROST;
        case UNIT_MOD_RESISTANCE_SHADOW:   return SPELL_SCHOOL_SHADOW;
        case UNIT_MOD_RESISTANCE_ARCANE:   return SPELL_SCHOOL_ARCANE;
        default:                           return SPELL_SCHOOL_NORMAL;
    }
}

Powers stats::PowerOf(UnitMods group)
{
    switch (group)
    {
        case UNIT_MOD_MANA:       return POWER_MANA;
        case UNIT_MOD_RAGE:       return POWER_RAGE;
        case UNIT_MOD_FOCUS:      return POWER_FOCUS;
        case UNIT_MOD_ENERGY:     return POWER_ENERGY;
        case UNIT_MOD_HAPPINESS:  return POWER_HAPPINESS;
        default:                  return POWER_MANA;
    }
}

void Tallies::Put(UnitMods group, UnitModifierType which, float amount, bool apply)
{
    switch (which)
    {
        case BASE_VALUE:
        case TOTAL_VALUE:
            m_group[group][which] += apply ? amount : -amount;
            break;

        case BASE_PCT:
        case TOTAL_PCT:
        {
            if (amount <= -100.0f)                          // small hack-fix for -100% modifiers
            {
                amount = -200.0f;
            }

            float const val = (100.0f + amount) / 100.0f;
            m_group[group][which] *= apply ? val : (1.0f / val);
            break;
        }

        default:
            break;
    }
}

float Tallies::Value(UnitMods group, UnitModifierType which) const
{
    if (group >= UNIT_MOD_END || which >= MODIFIER_TYPE_END)
    {
        sLog.outError("attempt to access nonexistent modifier value from UnitMods!");
        return 0.0f;
    }

    if (which == TOTAL_PCT && m_group[group][which] <= 0.0f)
    {
        return 0.0f;
    }

    return m_group[group][which];
}

void Tallies::Value(UnitMods group, UnitModifierType which, float value)
{
    m_group[group][which] = value;
}

Modifiers Tallies::Of(UnitMods group) const
{
    Modifiers mods;

    if (group >= UNIT_MOD_END)
    {
        sLog.outError("attempt to access nonexistent UnitMods in Of()!");
        return mods;
    }

    mods.baseValue = m_group[group][BASE_VALUE];
    mods.basePct = m_group[group][BASE_PCT];
    mods.totalValue = m_group[group][TOTAL_VALUE];
    mods.totalPct = m_group[group][TOTAL_PCT];

    return mods;
}

float Tallies::Folded(UnitMods group) const
{
    if (group >= UNIT_MOD_END)
    {
        sLog.outError("attempt to access nonexistent UnitMods in Folded()!");
        return 0.0f;
    }

    return Of(group).Folded();
}

float Tallies::FoldedOver(Stats stat) const
{
    UnitMods const group = UnitMods(UNIT_MOD_STAT_START + stat);

    Modifiers mods = Of(group);
    mods.baseValue += m_made[stat];

    return mods.Folded();
}
