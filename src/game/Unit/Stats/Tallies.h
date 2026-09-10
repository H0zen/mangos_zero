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

enum UnitModifierType
{
    BASE_VALUE = 0,
    BASE_PCT = 1,
    TOTAL_VALUE = 2,
    TOTAL_PCT = 3,
    MODIFIER_TYPE_END = 4
};

enum UnitMods
{
    UNIT_MOD_STAT_STRENGTH,
    UNIT_MOD_STAT_AGILITY,
    UNIT_MOD_STAT_STAMINA,
    UNIT_MOD_STAT_INTELLECT,
    UNIT_MOD_STAT_SPIRIT,
    UNIT_MOD_HEALTH,
    UNIT_MOD_MANA,
    UNIT_MOD_RAGE,
    UNIT_MOD_FOCUS,
    UNIT_MOD_ENERGY,
    UNIT_MOD_HAPPINESS,
    UNIT_MOD_ARMOR,
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

    bool Apply(Unit& who, UnitMods group, UnitModifierType which, float amount, bool apply);

    Stats StatOf(UnitMods group);

    SpellSchools SchoolOf(UnitMods group);

    Powers PowerOf(UnitMods group);
}

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

        void Put(UnitMods group, UnitModifierType which, float amount, bool apply);

        float Value(UnitMods group, UnitModifierType which) const;

        void Value(UnitMods group, UnitModifierType which, float value);

        Modifiers Of(UnitMods group) const;

        float Folded(UnitMods group) const;

        float FoldedOver(Stats stat) const;

        float Made(Stats stat) const { return m_made[stat]; }
        void Made(Stats stat, float value) { m_made[stat] = value; }

        bool Ready() const { return m_ready; }
        void Ready(bool yes) { m_ready = yes; }

    private:
        float m_group[UNIT_MOD_END][MODIFIER_TYPE_END];
        float m_made[MAX_STATS];
        bool m_ready = false;
};
