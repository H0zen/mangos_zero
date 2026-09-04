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
    namespace
    {
        /// A blow the shield stopped entirely, and one it only softened, are the
        /// same event to everything that asks "was this blocked".
        bool WasBlocked(const Outcome& outcome)
        {
            return outcome.strike.blocked || outcome.Ending() == Result::Blocked;
        }
    }

    uint32 ToHitInfo(const Outcome& outcome)
    {
        uint32 info = HITINFO_NORMALSWING2;

        if (outcome.Ending() == Result::Missed)
        {
            info |= HITINFO_MISS;
        }

        // The amplifiers are independent of the ending: a critical blow that was
        // partly blocked says both.
        if (outcome.strike.crit)
        {
            info |= HITINFO_CRITICALHIT;
        }
        if (outcome.strike.glancing)
        {
            info |= HITINFO_GLANCING;
        }
        if (outcome.strike.crushing)
        {
            info |= HITINFO_CRUSHING;
        }
        if (WasBlocked(outcome))
        {
            info |= HITINFO_BLOCK;
        }

        // These two are about what happened to the damage, not about how the
        // blow ended, so they stack on top of any of the above.
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
        if (WasBlocked(outcome))
        {
            return VICTIMSTATE_BLOCKS;
        }

        switch (outcome.Ending())
        {
            case Result::Missed:    return VICTIMSTATE_UNAFFECTED;
            case Result::Dodged:    return VICTIMSTATE_DODGE;
            case Result::Parried:   return VICTIMSTATE_PARRY;
            case Result::Evaded:    return VICTIMSTATE_EVADES;
            case Result::Immune:    return VICTIMSTATE_IS_IMMUNE;
            case Result::Deflected: return VICTIMSTATE_DEFLECTS;
            default:                return VICTIMSTATE_NORMAL;
        }
    }

    uint32 ToProcEx(const Outcome& outcome)
    {
        uint32 procEx = PROC_EX_NONE;

        switch (outcome.Ending())
        {
            case Result::Missed:    procEx |= PROC_EX_MISS;    break;
            case Result::Dodged:    procEx |= PROC_EX_DODGE;   break;
            case Result::Parried:   procEx |= PROC_EX_PARRY;   break;
            case Result::Blocked:   procEx |= PROC_EX_BLOCK;   break;
            case Result::Evaded:    procEx |= PROC_EX_EVADE;   break;
            case Result::Immune:    procEx |= PROC_EX_IMMUNE;  break;
            case Result::Deflected: procEx |= PROC_EX_DEFLECT; break;
            case Result::Reflected: procEx |= PROC_EX_REFLECT; break;
            case Result::Resisted:  procEx |= PROC_EX_RESIST;  break;
            case Result::Absorbed:  procEx |= PROC_EX_ABSORB;  break;

            case Result::Landed:
                // Glancing and crushing are ordinary hits as far as a proc is
                // concerned: they change the damage, not whether the blow
                // connected.
                procEx |= outcome.strike.crit ? PROC_EX_CRITICAL_HIT : PROC_EX_NORMAL_HIT;
                break;
        }

        // A blow that landed through a shield still procs as blocked. Losing
        // this is one missing line, and every ability that keys on a block goes
        // quiet without anything failing.
        if (outcome.strike.blocked && outcome.Ending() == Result::Landed)
        {
            procEx |= PROC_EX_BLOCK;
        }

        if (outcome.absorbed > 0 && outcome.Ending() != Result::Absorbed)
        {
            procEx |= PROC_EX_ABSORB;
        }
        if (outcome.resisted > 0 && outcome.Ending() != Result::Resisted)
        {
            procEx |= PROC_EX_RESIST;
        }

        return procEx;
    }
}
