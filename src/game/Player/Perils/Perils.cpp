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

#include "Perils/Perils.h"

#include "DBCStores.h"
#include "GridMap.h"
#include "Map.h"
#include "Opcodes.h"
#include "Player.h"
#include "SpellAuras.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

Perils::Perils(Player& who)
    : m_owner(who), m_standingIn(UNDERWATER_NONE), m_drawn(UNDERWATER_NONE),
      m_inWater(false), m_liquid(nullptr)
{
    for (int i = 0; i < MAX_TIMERS; ++i)
    {
        m_left[i] = DISABLED_MIRROR_TIMER;
    }
}

void Perils::Clear()
{
    for (int i = 0; i < MAX_TIMERS; ++i)
    {
        m_left[i] = DISABLED_MIRROR_TIMER;
    }

    m_standingIn = UNDERWATER_NONE;
    m_drawn = UNDERWATER_NONE;
    m_inWater = false;
    m_liquid = nullptr;
}

void Perils::Redraw()
{
    if (m_standingIn)
    {
        m_drawn = ~m_standingIn;
    }
}

bool Perils::Drowning() const
{
    return (m_standingIn & UNDERWATER_INWATER) &&
           m_left[BREATH_TIMER] != DISABLED_MIRROR_TIMER &&
           !m_owner.HasAuraType(SPELL_AURA_WATER_BREATHING) &&
           m_left[BREATH_TIMER] < 2000 && m_owner.IsAlive();
}

void Perils::Tell(MirrorTimerType which, uint32 most, uint32 left, int32 rate)
{
    if (int(most) == DISABLED_MIRROR_TIMER)
    {
        if (int(left) != DISABLED_MIRROR_TIMER)
        {
            Stop(which);
        }
        return;
    }

    WorldPacket data(SMSG_START_MIRROR_TIMER, 21);
    data << uint32(which);
    data << left;
    data << most;
    data << rate;
    data << uint8(0);
    data << uint32(0);                                      // Spell ID
    m_owner.GetSession()->SendPacket(&data);
}

void Perils::Stop(MirrorTimerType which)
{
    m_left[which] = DISABLED_MIRROR_TIMER;

    WorldPacket data(SMSG_STOP_MIRROR_TIMER, 4);
    data << uint32(which);
    m_owner.GetSession()->SendPacket(&data);
}

int32 Perils::Longest(MirrorTimerType which) const
{
    switch (which)
    {
        case FATIGUE_TIMER:
            if (m_owner.GetSession()->GetSecurity() >= AccountTypes(sWorld.getConfig(CONFIG_UINT32_TIMERBAR_FATIGUE_GMLEVEL)))
            {
                return DISABLED_MIRROR_TIMER;
            }
            return sWorld.getConfig(CONFIG_UINT32_TIMERBAR_FATIGUE_MAX) * IN_MILLISECONDS;

        case BREATH_TIMER:
        {
            if (!m_owner.IsAlive() || m_owner.HasAuraType(SPELL_AURA_WATER_BREATHING) ||
                m_owner.GetSession()->GetSecurity() >= AccountTypes(sWorld.getConfig(CONFIG_UINT32_TIMERBAR_BREATH_GMLEVEL)))
            {
                return DISABLED_MIRROR_TIMER;
            }

            int32 under = sWorld.getConfig(CONFIG_UINT32_TIMERBAR_BREATH_MAX) * IN_MILLISECONDS;
            for (auto* aura : m_owner.GetAurasByType(SPELL_AURA_MOD_WATER_BREATHING))
            {
                under = int32(under * (100.0f + aura->GetModifier()->m_amount) / 100.0f);
            }
            return under;
        }

        case FIRE_TIMER:
            if (!m_owner.IsAlive() || m_owner.GetSession()->GetSecurity() >= AccountTypes(sWorld.getConfig(CONFIG_UINT32_TIMERBAR_FIRE_GMLEVEL)))
            {
                return DISABLED_MIRROR_TIMER;
            }
            return sWorld.getConfig(CONFIG_UINT32_TIMERBAR_FIRE_MAX) * IN_MILLISECONDS;
    }

    return 0;
}

uint32 Perils::Harm(EnvironmentalDamageType type, uint32 damage)
{
    if (!m_owner.IsAlive() || m_owner.isGameMaster())
    {
        return 0;
    }

    // Absorb and resist some environmental damage types
    uint32 absorb = 0;
    uint32 resist = 0;
    if (type == DAMAGE_LAVA)
    {
        if (m_owner.IsImmuneToDamage(SPELL_SCHOOL_MASK_FIRE))
        {
            return 0;
        }

        m_owner.CalculateDamageAbsorbAndResist(&m_owner, SPELL_SCHOOL_MASK_FIRE, DIRECT_DAMAGE, damage, &absorb, &resist);
    }
    else if (type == DAMAGE_SLIME)
    {
        if (m_owner.IsImmuneToDamage(SPELL_SCHOOL_MASK_NATURE))
        {
            return 0;
        }

        m_owner.CalculateDamageAbsorbAndResist(&m_owner, SPELL_SCHOOL_MASK_NATURE, DIRECT_DAMAGE, damage, &absorb, &resist);
    }

    damage -= absorb + resist;

    m_owner.DealDamageMods(&m_owner, damage, &absorb);

    WorldPacket data(SMSG_ENVIRONMENTALDAMAGELOG, (21));
    data << m_owner.GetObjectGuid();
    data << uint8(type != DAMAGE_FALL_TO_VOID ? type : DAMAGE_FALL);
    data << uint32(damage);
    data << uint32(absorb);
    data << uint32(resist);
    Broadcast(m_owner, &data, true);

    DamageEffectType damageType = SELF_DAMAGE;
    if (type == DAMAGE_FALL && m_owner.getClass() == CLASS_ROGUE)
    {
        damageType = SELF_DAMAGE_ROGUE_FALL;
    }

    uint32 final_damage = m_owner.DealDamage(&m_owner, damage, nullptr, damageType, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);

    if (type == DAMAGE_FALL && !m_owner.IsAlive()) // DealDamage does not apply item durability loss at self-damage
    {
        DEBUG_LOG("We fell to death, losing 10 percent durability");
        m_owner.DurabilityLossAll(0.10f, false);
        // Durability lost message
        WorldPacket data2(SMSG_DURABILITY_DAMAGE_DEATH, 0);
        m_owner.GetSession()->SendPacket(&data2);
    }

    return final_damage;
}

void Perils::Emptied(MirrorTimerType which)
{
    // TODO: Check this formula
    uint32 const damage = m_owner.GetMaxHealth() / 5 + urand(0, m_owner.getLevel() - 1);

    if (which == BREATH_TIMER)
    {
        Harm(DAMAGE_DROWNING, damage);
        return;
    }

    if (m_owner.IsAlive())
    {
        Harm(DAMAGE_EXHAUSTED, damage);
    }
    else if (m_owner.HasPlayerFlag(PLAYER_FLAGS_GHOST))
    {
        m_owner.RepopAtGraveyard();
    }
}

void Perils::RunBar(MirrorTimerType which, uint8 standingIn, uint32 elapsed)
{
    if (m_standingIn & standingIn)
    {
        if (m_left[which] == DISABLED_MIRROR_TIMER)
        {
            m_left[which] = Longest(which);
            Tell(which, m_left[which], m_left[which], -1);
            return;
        }

        m_left[which] -= elapsed;

        if (m_left[which] < 0)
        {
            m_left[which] += 2 * IN_MILLISECONDS;
            Emptied(which);
        }
        else if (!(m_drawn & standingIn))
        {
            Tell(which, Longest(which), m_left[which], -1);
        }

        return;
    }

    if (m_left[which] == DISABLED_MIRROR_TIMER)
    {
        return;
    }

    int32 const most = Longest(which);
    m_left[which] += 10 * elapsed;

    if (m_left[which] >= most || !m_owner.IsAlive())
    {
        Stop(which);
    }
    else if (m_drawn & standingIn)
    {
        Tell(which, most, m_left[which], 10);
    }
}

void Perils::RunFire(uint32 elapsed)
{
    // A liquid that carries its own spell does the burning itself.
    if (!(m_standingIn & UNDERWATER_INLAVA) || (m_liquid && m_liquid->SpellID))
    {
        m_left[FIRE_TIMER] = DISABLED_MIRROR_TIMER;
        return;
    }

    if (m_left[FIRE_TIMER] == DISABLED_MIRROR_TIMER)
    {
        m_left[FIRE_TIMER] = Longest(FIRE_TIMER);
        return;
    }

    m_left[FIRE_TIMER] -= elapsed;

    if (m_left[FIRE_TIMER] < 0)
    {
        m_left[FIRE_TIMER] += 2 * IN_MILLISECONDS;
        // TODO: Check this formula
        Harm(DAMAGE_LAVA, urand(600, 700));
    }
}

void Perils::Run(uint32 elapsed)
{
    if (!m_standingIn)
    {
        return;
    }

    RunBar(BREATH_TIMER, UNDERWATER_INWATER, elapsed);
    RunBar(FATIGUE_TIMER, UNDERWATER_INDARKWATER, elapsed);
    RunFire(elapsed);

    m_standingIn &= ~UNDERWATER_EXIST_TIMERS;
    for (int i = 0; i < MAX_TIMERS; ++i)
    {
        if (m_left[i] != DISABLED_MIRROR_TIMER)
        {
            m_standingIn |= UNDERWATER_EXIST_TIMERS;
            break;
        }
    }

    m_drawn = m_standingIn;
}

void Perils::InWater(bool apply)
{
    if (m_inWater == apply)
    {
        return;
    }

    // define player in water by opcodes
    // move player's guid into HateOfflineList of those mobs
    // which can't swim and move guid back into ThreatList when
    // on surface.
    // TODO: exist also swimming mobs, and function must be symmetric to enter/leave water
    m_inWater = apply;

    // remove auras that need water/land
    m_owner.RemoveAurasWithInterruptFlags(apply ? AURA_INTERRUPT_FLAG_NOT_ABOVEWATER : AURA_INTERRUPT_FLAG_NOT_UNDERWATER);

    m_owner.GetHostileRefManager().updateThreatTables();
}

void Perils::Look(Map* where, float x, float y, float z)
{
    GridMapLiquidData found;
    GridMapLiquidStatus const res = where->GetTerrain()->getLiquidStatus(x, y, z, MAP_ALL_LIQUIDS, &found);

    InWater((res & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER)) != 0);

    if (!res)
    {
        m_standingIn &= ~(UNDERWATER_INWATER | UNDERWATER_INLAVA | UNDERWATER_INSLIME | UNDERWATER_INDARKWATER);
        if (m_liquid && m_liquid->SpellID)
        {
            m_owner.RemoveAuras(m_liquid->SpellID);
        }
        m_liquid = nullptr;
        return;
    }

    if (uint32 const entry = found.entry)
    {
        LiquidTypeEntry const* liquid = sLiquidTypeStore.LookupEntry(entry);

        if (m_liquid && m_liquid->SpellID && m_liquid->ID != entry)
        {
            m_owner.RemoveAuras(m_liquid->SpellID);
        }

        if (liquid && liquid->SpellID)
        {
            if (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER))
            {
                if (!m_owner.HasAura(liquid->SpellID))
                {
                    m_owner.CastSpell(&m_owner, liquid->SpellID, true);
                }
            }
            else
            {
                m_owner.RemoveAuras(liquid->SpellID);
            }
        }

        m_liquid = liquid;
    }
    else if (m_liquid && m_liquid->SpellID)
    {
        m_owner.RemoveAuras(m_liquid->SpellID);
        m_liquid = nullptr;
    }

    // All liquids type - check under water position
    if (found.type_flags & (MAP_LIQUID_TYPE_WATER | MAP_LIQUID_TYPE_OCEAN | MAP_LIQUID_TYPE_MAGMA | MAP_LIQUID_TYPE_SLIME))
    {
        if (res & LIQUID_MAP_UNDER_WATER)
        {
            m_standingIn |= UNDERWATER_INWATER;
        }
        else
        {
            m_standingIn &= ~UNDERWATER_INWATER;
        }
    }

    // Allow travel in dark water on taxi or transport
    if ((found.type_flags & MAP_LIQUID_TYPE_DARK_WATER) && !m_owner.IsTaxiFlying() && !m_owner.GetTransport())
    {
        m_standingIn |= UNDERWATER_INDARKWATER;
    }
    else
    {
        m_standingIn &= ~UNDERWATER_INDARKWATER;
    }

    // in lava check, anywhere in lava level
    if (found.type_flags & MAP_LIQUID_TYPE_MAGMA)
    {
        if (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER | LIQUID_MAP_WATER_WALK))
        {
            m_standingIn |= UNDERWATER_INLAVA;
        }
        else
        {
            m_standingIn &= ~UNDERWATER_INLAVA;
        }
    }

    // in slime check, anywhere in slime level
    if (found.type_flags & MAP_LIQUID_TYPE_SLIME)
    {
        if (res & (LIQUID_MAP_UNDER_WATER | LIQUID_MAP_IN_WATER | LIQUID_MAP_WATER_WALK))
        {
            m_standingIn |= UNDERWATER_INSLIME;
        }
        else
        {
            m_standingIn &= ~UNDERWATER_INSLIME;
        }
    }
}
