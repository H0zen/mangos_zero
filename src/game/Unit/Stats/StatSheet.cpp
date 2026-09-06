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

#include "StatSheet.h"

#include "CreatureNumbers.h"
#include "Unit.h"

void StatSheet::Resistance(uint32 school)
{
    // The normal school is armour, and armour is kept in its own field.
    if (school == SPELL_SCHOOL_NORMAL)
    {
        Armour();
        return;
    }

    UnitMods const unitMod = UnitMods(UNIT_MOD_RESISTANCE_START + school);
    m_unit.SetResistance(SpellSchools(school), int32(stats::Simple(m_unit.Tallied().Of(unitMod))));
}
