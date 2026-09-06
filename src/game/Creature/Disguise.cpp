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

#include "Disguise.h"

#include "Creature.h"
#include "Unit.h"

namespace
{
    /// The five flags that keep a creature out of a fight, and the ask for each.
    struct Dropped
    {
        uint32 asked;
        uint32 flag;
    };

    Dropped const DROPPED[] =
    {
        { TEMPFACTION_TOGGLE_NON_ATTACKABLE, UNIT_FLAG_NON_ATTACKABLE },
        { TEMPFACTION_TOGGLE_OOC_NOT_ATTACK, UNIT_FLAG_OOC_NOT_ATTACKABLE },
        { TEMPFACTION_TOGGLE_PASSIVE,        UNIT_FLAG_PASSIVE },
        { TEMPFACTION_TOGGLE_PACIFIED,       UNIT_FLAG_PACIFIED },
        { TEMPFACTION_TOGGLE_NOT_SELECTABLE, UNIT_FLAG_NOT_SELECTABLE },
    };
}

void Disguise::Wear(uint32 factionId, uint32 flags)
{
    m_flags = flags;
    m_owner.setFaction(factionId);

    for (Dropped const& each : DROPPED)
    {
        if (m_flags & each.asked)
        {
            m_owner.RemoveUnitFlag(UnitFlags(each.flag));
        }
    }
}

void Disguise::TakeOff()
{
    // whoever is driving it decides what side it is on
    if (m_owner.IsCharmed())
    {
        return;
    }

    CreatureInfo const* row = m_owner.GetCreatureInfo();
    if (!row)
    {
        m_flags = TEMPFACTION_NONE;
        return;
    }

    m_owner.setFaction(row->FactionAlliance);

    for (Dropped const& each : DROPPED)
    {
        if (!(m_flags & each.asked) || !(row->UnitFlags & each.flag))
        {
            continue;
        }

        // saying it cannot be attacked out of combat, mid-swing, would be false
        if (each.flag == UNIT_FLAG_OOC_NOT_ATTACKABLE && m_owner.IsInCombat())
        {
            continue;
        }

        m_owner.SetUnitFlag(UnitFlags(each.flag));
    }

    m_flags = TEMPFACTION_NONE;
}
