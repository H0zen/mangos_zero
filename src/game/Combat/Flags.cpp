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

#include "Combat/Flags.h"

#include "SpellMgr.h"
#include "Unit.h"

namespace combat
{
    uint32 ToHitInfo(const Outcome& outcome)
    {
        uint32 info = HITINFO_NORMALSWING2;

        switch (outcome.landing)
        {
            case Landing::Miss:
                info |= HITINFO_MISS;
                break;
            case Landing::Crit:
                info |= HITINFO_CRITICALHIT;
                break;
            case Landing::Glance:
                info |= HITINFO_GLANCING;
                break;
            case Landing::Crush:
                info |= HITINFO_CRUSHING;
                break;
            case Landing::Block:
                info |= HITINFO_BLOCK;
                break;
            default:
                break;
        }

        // These two are about what happened to the damage, not about how it
        // landed, so they stack on top of any of the above.
        if (outcome.absorbed > 0)
        {
            info |= HITINFO_ABSORB;
        }
        if (outcome.resisted > 0)
        {
            info |= HITINFO_RESIST;
        }

        return info;
    }

    uint32 ToVictimState(const Outcome& outcome)
    {
        switch (outcome.landing)
        {
            case Landing::Miss:     return VICTIMSTATE_UNAFFECTED;
            case Landing::Dodge:    return VICTIMSTATE_DODGE;
            case Landing::Parry:    return VICTIMSTATE_PARRY;
            case Landing::Block:    return VICTIMSTATE_BLOCKS;
            case Landing::Evade:    return VICTIMSTATE_EVADES;
            case Landing::Immune:   return VICTIMSTATE_IS_IMMUNE;
            default:                return VICTIMSTATE_NORMAL;
        }
    }

    uint32 ToProcEx(const Outcome& outcome)
    {
        uint32 procEx = PROC_EX_NONE;

        switch (outcome.landing)
        {
            case Landing::Miss:     procEx |= PROC_EX_MISS;         break;
            case Landing::Dodge:    procEx |= PROC_EX_DODGE;        break;
            case Landing::Parry:    procEx |= PROC_EX_PARRY;        break;
            case Landing::Block:    procEx |= PROC_EX_BLOCK;        break;
            case Landing::Evade:    procEx |= PROC_EX_EVADE;        break;
            case Landing::Immune:   procEx |= PROC_EX_IMMUNE;       break;
            case Landing::Resist:   procEx |= PROC_EX_RESIST;       break;
            case Landing::Crit:     procEx |= PROC_EX_CRITICAL_HIT; break;

            // Glancing and crushing are ordinary hits as far as a proc is
            // concerned: they change the damage, not whether the blow connected.
            case Landing::Hit:
            case Landing::Glance:
            case Landing::Crush:
                procEx |= PROC_EX_NORMAL_HIT;
                break;
        }

        if (outcome.absorbed > 0)
        {
            procEx |= PROC_EX_ABSORB;
        }
        if (outcome.resisted > 0 && outcome.landing != Landing::Resist)
        {
            procEx |= PROC_EX_RESIST;
        }

        return procEx;
    }
}
