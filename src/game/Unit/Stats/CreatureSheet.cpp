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

#include "CreatureSheet.h"

#include "Creature.h"
#include "CreatureNumbers.h"

CreatureSheet::CreatureSheet(Creature& whose) : StatSheet(whose), m_owner(whose)
{
}

void CreatureSheet::Stat(Stats /*stat*/)
{
}

void CreatureSheet::Everything()
{
    MaxHealth();
    AttackPower(false);

    for (int power = POWER_MANA; power < MAX_POWERS; ++power)
    {
        MaxPower(Powers(power));
    }

    for (int school = SPELL_SCHOOL_NORMAL; school < MAX_SPELL_SCHOOL; ++school)
    {
        Resistance(school);
    }
}

void CreatureSheet::Armour()
{
    m_owner.SetArmor(int32(stats::Simple(m_owner.ModifiersOf(UNIT_MOD_ARMOR))));
}

void CreatureSheet::MaxHealth()
{
    m_owner.SetMaxHealth(uint32(stats::Simple(m_owner.ModifiersOf(UNIT_MOD_HEALTH))));
}

void CreatureSheet::MaxPower(Powers power)
{
    UnitMods const unitMod = UnitMods(UNIT_MOD_POWER_START + power);
    m_owner.SetMaxPower(power, uint32(stats::Simple(m_owner.ModifiersOf(unitMod))));
}

void CreatureSheet::AttackPower(bool ranged)
{
    UnitMods const unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    stats::AttackPower const power = stats::CreatureAttackPower(m_owner.ModifiersOf(unitMod));
    m_owner.SetAttackPower(ranged, power.base, power.added, power.share);

    if (ranged)
    {
        return;
    }

    // what it swings for follows from what it swings with
    Swing(BASE_ATTACK);
    Swing(OFF_ATTACK);
}

void CreatureSheet::Swing(WeaponAttackType attType)
{
    if (attType > OFF_ATTACK)
    {
        return;
    }

    UnitMods const unitMod = attType == BASE_ATTACK ? UNIT_MOD_DAMAGE_MAINHAND : UNIT_MOD_DAMAGE_OFFHAND;

    // Only the attack power it has ABOVE the template's counts: what the template
    // was written with is already in the damage the template gives.
    float const gained = m_owner.GetTotalAttackPowerValue(attType) - m_owner.GetCreatureInfo()->MeleeAttackPower;

    stats::Swing const swing = stats::CreatureSwing(
        m_owner.ModifiersOf(unitMod),
        m_owner.GetWeaponDamageRange(attType, MINDAMAGE),
        m_owner.GetWeaponDamageRange(attType, MAXDAMAGE),
        gained,
        m_owner.GetAPMultiplier(attType, false),
        m_owner.GetCreatureInfo()->DamageMultiplier);

    m_owner.SetStatFloatValue(attType == BASE_ATTACK ? UNIT_FIELD_MINDAMAGE : UNIT_FIELD_MINOFFHANDDAMAGE, swing.least);
    m_owner.SetStatFloatValue(attType == BASE_ATTACK ? UNIT_FIELD_MAXDAMAGE : UNIT_FIELD_MAXOFFHANDDAMAGE, swing.most);
}

uint32 CreatureSheet::ShieldBlock() const
{
    return stats::CreatureShieldBlock(m_owner.getLevel(), m_owner.GetStat(STAT_STRENGTH));
}
