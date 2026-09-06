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

#include "Pace.h"

#include "Creature.h"
#include "Log.h"
#include "Opcodes.h"
#include "Pet.h"
#include "Player.h"
#include "SpellAuras.h"
#include "Unit.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>

namespace
{
    /// Bestial Swiftness, which pays a hunter's pet only while it is fighting.
    uint32 const SPELL_BESTIAL_SWIFTNESS = 19582;
    uint32 const AURA_BESTIAL_SWIFTNESS_HELD = 19596;

    /// The client draws a pet running a seventh faster than its master.
    float const PET_RUN_AHEAD = 1.14286f;

    /// The message pairs for each way of moving: one the client must accept,
    /// one it is merely told.
    uint16 const SPEED_OPCODES[MAX_MOVE_TYPE][2] =
    {
        { SMSG_FORCE_WALK_SPEED_CHANGE,      SMSG_SPLINE_SET_WALK_SPEED },
        { SMSG_FORCE_RUN_SPEED_CHANGE,       SMSG_SPLINE_SET_RUN_SPEED },
        { SMSG_FORCE_RUN_BACK_SPEED_CHANGE,  SMSG_SPLINE_SET_RUN_BACK_SPEED },
        { SMSG_FORCE_SWIM_SPEED_CHANGE,      SMSG_SPLINE_SET_SWIM_SPEED },
        { SMSG_FORCE_SWIM_BACK_SPEED_CHANGE, SMSG_SPLINE_SET_SWIM_BACK_SPEED },
        { SMSG_FORCE_TURN_RATE_CHANGE,       SMSG_SPLINE_SET_TURN_RATE },
    };

    /// An aura can pin a pace to a flat number of yards a second, whatever else
    /// is on the unit. Nothing may exceed it.
    void PinToNormal(Unit& who, UnitMoveType how, float& pace)
    {
        if (how != MOVE_RUN && how != MOVE_SWIM)
        {
            return;
        }

        int32 const pinned = who.GetMaxPositiveAuraModifier(SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED);
        if (!pinned)
        {
            return;
        }

        float const most = pinned / baseMoveSpeed[how];
        if (pace > most)
        {
            pace = most;
        }
    }

    /// The strongest slow it carries, applied last of all.
    void ApplySlow(Unit& who, float& pace)
    {
        if (int32 const slow = who.GetMaxNegativeAuraModifier(SPELL_AURA_MOD_DECREASE_SPEED))
        {
            pace *= (100.0f + slow) / 100.0f;
        }
    }

    struct ReckonAgain
    {
        ReckonAgain(UnitMoveType how, bool forced) : m_how(how), m_forced(forced) {}
        void operator()(Unit* unit) const { unit->Pacing().Reckon(m_how, m_forced); }

        UnitMoveType m_how;
        bool m_forced;
    };
}

Pace::Pace(Unit& whose) : m_owner(whose)
{
    for (int how = 0; how < MAX_MOVE_TYPE; ++how)
    {
        m_rate[how] = 1.0f;
    }
}

float Pace::At(UnitMoveType how) const
{
    return m_rate[how] * baseMoveSpeed[how];
}

void Pace::Reckon(UnitMoveType how, bool forced, float ratio)
{
    int32 hastest = 0;                                      // the strongest single haste
    float stacking = 1.0f;                                  // the hastes that add up
    float lonely = 1.0f;                                    // the strongest haste that does not

    switch (how)
    {
        case MOVE_WALK:
            break;

        case MOVE_RUN:
            if (m_owner.IsMounted())
            {
                hastest = m_owner.GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED);
                stacking = m_owner.GetTotalAuraMultiplier(SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS);
                lonely = (100.0f + m_owner.GetMaxPositiveAuraModifier(SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK)) / 100.0f;
            }
            else
            {
                hastest = m_owner.GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SPEED);
                stacking = m_owner.GetTotalAuraMultiplier(SPELL_AURA_MOD_SPEED_ALWAYS);
                lonely = (100.0f + m_owner.GetMaxPositiveAuraModifier(SPELL_AURA_MOD_SPEED_NOT_STACK)) / 100.0f;
            }
            break;

        case MOVE_SWIM:
            hastest = m_owner.GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SWIM_SPEED);
            break;

        case MOVE_RUN_BACK:
        case MOVE_SWIM_BACK:
            return;                                         // never anything but the ordinary

        default:
            sLog.outError("Pace::Reckon: unsupported move type (%d)", how);
            return;
    }

    float const bonus = std::max(lonely, stacking);
    float pace = hastest ? bonus * (100.0f + hastest) / 100.0f : bonus;

    PinToNormal(m_owner, how, pace);

    if (Creature* creature = ToCreature(&m_owner))
    {
        // one that has called for help gives up a third of its pace doing so
        if (creature->HasSearchedAssistance())
        {
            pace *= 0.66f;
        }
    }
    else if (m_owner.GetDeathState() == CORPSE)
    {
        Player& ghost = static_cast<Player&>(m_owner);
        pace *= sWorld.getConfig(ghost.Battle().InOne() ? CONFIG_FLOAT_GHOST_RUN_SPEED_BG
                                                        : CONFIG_FLOAT_GHOST_RUN_SPEED_WORLD);
    }

    ApplySlow(m_owner, pace);

    if (Creature* creature = ToCreature(&m_owner))
    {
        // what its row says it moves at
        if (how == MOVE_RUN)
        {
            pace *= creature->GetCreatureInfo()->SpeedRun;
        }
        else if (how == MOVE_WALK)
        {
            pace *= creature->GetCreatureInfo()->SpeedWalk;
        }
    }

    SetRate(how, pace * ratio, forced);
}

void Pace::SetRate(UnitMoveType how, float rate, bool forced)
{
    if (rate < 0.0f)
    {
        rate = 0.0f;
    }

    if (m_rate[how] != rate)
    {
        m_rate[how] = rate;

        m_owner.PropagateSpeedChange();

        if (forced && m_owner.IsPlayer())
        {
            // the acknowledgement is counted, because the client answers each one
            Player& who = static_cast<Player&>(m_owner);
            ++who.m_forced_speed_changes[how];

            WorldPacket data(SPEED_OPCODES[how][0], 18);
            data << m_owner.GetPackGUID();
            data << uint32(0);                              // moveEvent, NUM_PMOVE_EVTS = 0x39
            data << float(At(how));
            who.GetSession()->SendPacket(&data);
        }

        WorldPacket data(SPEED_OPCODES[how][1], 12);
        data << m_owner.GetPackGUID();
        data << float(At(how));
        Broadcast(m_owner, &data, false);
    }

    m_owner.CallForAllControlledUnits(ReckonAgain(how, forced),
                                      CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM | CONTROLLED_MINIPET);
}

void PetPace::Reckon(UnitMoveType how, bool forced, float ratio)
{
    Unit* master = m_owner.GetOwner();
    Player* owner = master ? ToPlayer(master) : nullptr;
    if (!owner)
    {
        Pace::Reckon(how, forced, ratio);                   // one nobody owns is a plain creature
        return;
    }

    int32 hastest = 0;
    float stacking = 1.0f;
    float lonely = 1.0f;

    switch (how)
    {
        case MOVE_WALK:
            break;

        case MOVE_RUN:
            // Bestial Swiftness pays the pet only while it is fighting, so while
            // it is merely following, that one haste is left out of the reckoning.
            if (!m_owner.getVictim() && owner->HasAura(AURA_BESTIAL_SWIFTNESS_HELD))
            {
                for (auto* aura : m_owner.GetAurasByType(SPELL_AURA_MOD_INCREASE_SPEED))
                {
                    if (aura->GetId() != SPELL_BESTIAL_SWIFTNESS)
                    {
                        hastest = std::max(aura->GetBasePoints(), hastest);
                    }
                }
            }
            else
            {
                hastest = m_owner.GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SPEED);
            }

            stacking = m_owner.GetTotalAuraMultiplier(SPELL_AURA_MOD_SPEED_ALWAYS);
            lonely = (100.0f + m_owner.GetMaxPositiveAuraModifier(SPELL_AURA_MOD_SPEED_NOT_STACK)) / 100.0f;
            break;

        case MOVE_SWIM:
            hastest = m_owner.GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SWIM_SPEED);
            break;

        case MOVE_RUN_BACK:
        case MOVE_SWIM_BACK:
            return;

        default:
            sLog.outError("PetPace::Reckon: unsupported move type (%d)", how);
            return;
    }

    // It keeps up with its master, but a dazed master does not daze his pet.
    float masterPace = owner->Pacing().RateOf(how);
    if (int32 const mastersSlow = owner->GetMaxNegativeAuraModifier(SPELL_AURA_MOD_DECREASE_SPEED))
    {
        masterPace *= 100.0f / (100.0f + mastersSlow);
    }

    float pace = std::max(lonely, stacking) * masterPace;
    if (hastest)
    {
        pace = pace * (100.0f + hastest) / 100.0f;
    }

    PinToNormal(m_owner, how, pace);
    ApplySlow(m_owner, pace);

    if (how == MOVE_RUN)
    {
        pace *= PET_RUN_AHEAD;
    }

    SetRate(how, pace * ratio, forced);
}
