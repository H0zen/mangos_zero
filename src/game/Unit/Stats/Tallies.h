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

#pragma once

#include "Platform/Define.h"
#include "SharedDefines.h"
#include "Modifiers.h"

/**
 * @brief The four running numbers kept per stat group.
 */
enum UnitModifierType
{
    BASE_VALUE = 0,
    BASE_PCT = 1,
    TOTAL_VALUE = 2,
    TOTAL_PCT = 3,
    MODIFIER_TYPE_END = 4
};

/**
 * @brief The groups a tally is kept in.
 */
enum UnitMods
{
    UNIT_MOD_STAT_STRENGTH,                                 // UNIT_MOD_STAT_STRENGTH..UNIT_MOD_STAT_SPIRIT must be in existing order, it's accessed by index values of Stats enum.
    UNIT_MOD_STAT_AGILITY,
    UNIT_MOD_STAT_STAMINA,
    UNIT_MOD_STAT_INTELLECT,
    UNIT_MOD_STAT_SPIRIT,
    UNIT_MOD_HEALTH,
    UNIT_MOD_MANA,                                          // UNIT_MOD_MANA..UNIT_MOD_HAPPINESS must be in existing order, it's accessed by index values of Powers enum.
    UNIT_MOD_RAGE,
    UNIT_MOD_FOCUS,
    UNIT_MOD_ENERGY,
    UNIT_MOD_HAPPINESS,
    UNIT_MOD_ARMOR,                                         // UNIT_MOD_ARMOR..UNIT_MOD_RESISTANCE_ARCANE must be in existing order, it's accessed by index values of SpellSchools enum.
    UNIT_MOD_RESISTANCE_HOLY,
    UNIT_MOD_RESISTANCE_FIRE,
    UNIT_MOD_RESISTANCE_NATURE,
    UNIT_MOD_RESISTANCE_FROST,
    UNIT_MOD_RESISTANCE_SHADOW,
    UNIT_MOD_RESISTANCE_ARCANE,
    UNIT_MOD_ATTACK_POWER,
    UNIT_MOD_ATTACK_POWER_RANGED,
    UNIT_MOD_DAMAGE_MAINHAND,
    UNIT_MOD_DAMAGE_OFFHAND,
    UNIT_MOD_DAMAGE_RANGED,
    UNIT_MOD_END,
    // synonyms
    UNIT_MOD_STAT_START = UNIT_MOD_STAT_STRENGTH,
    UNIT_MOD_STAT_END = UNIT_MOD_STAT_SPIRIT + 1,
    UNIT_MOD_RESISTANCE_START = UNIT_MOD_ARMOR,
    UNIT_MOD_RESISTANCE_END = UNIT_MOD_RESISTANCE_ARCANE + 1,
    UNIT_MOD_POWER_START = UNIT_MOD_MANA,
    UNIT_MOD_POWER_END = UNIT_MOD_HAPPINESS + 1
};

class Unit;

namespace stats
{
    /// Puts an aura's share on a group, then tells the sheet what to work out
    /// again. The one way a tally is moved on a live unit.
    bool Apply(Unit& who, UnitMods group, UnitModifierType which, float amount, bool apply);

    /// Which stat a group of stat tallies is kept for.
    Stats StatOf(UnitMods group);

    /// Which school a group of resistance tallies is kept for.
    SpellSchools SchoolOf(UnitMods group);

    /// Which power a group of power tallies is kept for.
    Powers PowerOf(UnitMods group);
}

/**
 * @brief What the auras have put on each stat group, over what the unit was made with.
 *
 * Four running numbers per group: a flat and a percentage settled before anything
 * else, and a flat and a percentage applied over the result. Auras add into them
 * and take back out of them; nothing reads a single aura's share again.
 */
class Tallies
{
    public:
        Tallies()
        {
            for (int group = 0; group < UNIT_MOD_END; ++group)
            {
                m_group[group][BASE_VALUE] = 0.0f;
                m_group[group][BASE_PCT] = 1.0f;
                m_group[group][TOTAL_VALUE] = 0.0f;
                m_group[group][TOTAL_PCT] = 1.0f;
            }

            for (int stat = 0; stat < MAX_STATS; ++stat)
            {
                m_made[stat] = 0.0f;
            }
        }

        /// Adds one aura's share to a group, or takes it back out.
        void Put(UnitMods group, UnitModifierType which, float amount, bool apply);

        /// One stored number. A total percentage that is not positive reads as nothing.
        float Value(UnitMods group, UnitModifierType which) const;

        /// Writes one stored number outright.
        void Value(UnitMods group, UnitModifierType which, float value);

        /// The four numbers of a group.
        Modifiers Of(UnitMods group) const;

        /// The four folded, over nothing.
        float Folded(UnitMods group) const;

        /// The four folded over what the unit was made with.
        float FoldedOver(Stats stat) const;

        /// What the unit was made with, before anything was put on it.
        float Made(Stats stat) const { return m_made[stat]; }
        void Made(Stats stat, float value) { m_made[stat] = value; }

        /// Whether the sheet is built, and a change is worth passing on to it.
        bool Ready() const { return m_ready; }
        void Ready(bool yes) { m_ready = yes; }

    private:
        float m_group[UNIT_MOD_END][MODIFIER_TYPE_END];
        float m_made[MAX_STATS];
        bool m_ready = false;
};
