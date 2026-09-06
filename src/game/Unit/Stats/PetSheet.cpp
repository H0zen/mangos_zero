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

#include "PetSheet.h"

#include "CreatureNumbers.h"
#include "Pet.h"
#include "PetNumbers.h"

/// The one pet whose attack power is not doubled.
static uint32 const ENTRY_IMP = 416;

// The pure part mirrors HappinessState by value, so the two are held together
// here rather than by anyone remembering.
static_assert(stats::PET_UNHAPPY == UNHAPPY && stats::PET_CONTENT == CONTENT &&
              stats::PET_HAPPY == HAPPY, "the pet moods have drifted apart");

PetSheet::PetSheet(Pet& whose) : StatSheet(whose), m_owner(whose)
{
}

void PetSheet::Stat(Stats stat)
{
    if (stat > STAT_SPIRIT)
    {
        return;
    }

    m_owner.SetStat(stat, int32(m_owner.GetTotalStatValue(stat)));

    switch (stat)
    {
        case STAT_STRENGTH:  AttackPower(false);       break;
        case STAT_AGILITY:   Armour();                 break;
        case STAT_STAMINA:   MaxHealth();              break;
        case STAT_INTELLECT: MaxPower(POWER_MANA);     break;
        case STAT_SPIRIT:
        default:
            break;
    }
}

void PetSheet::Everything()
{
    for (int stat = STAT_STRENGTH; stat < MAX_STATS; ++stat)
    {
        Stat(Stats(stat));
    }

    for (int power = POWER_MANA; power < MAX_POWERS; ++power)
    {
        MaxPower(Powers(power));
    }

    for (int school = SPELL_SCHOOL_NORMAL; school < MAX_SPELL_SCHOOL; ++school)
    {
        Resistance(school);
    }
}

void PetSheet::Armour()
{
    m_owner.SetArmor(int32(stats::PetArmour(m_owner.ModifiersOf(UNIT_MOD_ARMOR), m_owner.GetStat(STAT_AGILITY))));
}

void PetSheet::MaxHealth()
{
    float const gained = m_owner.GetStat(STAT_STAMINA) - m_owner.GetCreateStat(STAT_STAMINA);

    m_owner.SetMaxHealth(uint32(stats::PetMaxHealth(m_owner.ModifiersOf(UNIT_MOD_HEALTH),
                                                    m_owner.GetCreateHealth(), gained)));
}

void PetSheet::MaxPower(Powers power)
{
    // Only mana grows from a stat, and only from intellect gained since creation.
    float const gained = power == POWER_MANA
                             ? m_owner.GetStat(STAT_INTELLECT) - m_owner.GetCreateStat(STAT_INTELLECT)
                             : 0.0f;

    UnitMods const unitMod = UnitMods(UNIT_MOD_POWER_START + power);
    m_owner.SetMaxPower(power, uint32(stats::PetMaxPower(m_owner.ModifiersOf(unitMod),
                                                         m_owner.GetCreatePowers(power), gained)));
}

void PetSheet::AttackPower(bool ranged)
{
    // No pet holds a bow.
    if (ranged)
    {
        return;
    }

    float const fromStrength = stats::PetAttackPowerFromStrength(m_owner.GetStat(STAT_STRENGTH),
                                                                 m_owner.GetEntry() == ENTRY_IMP);
    m_owner.SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, fromStrength);

    stats::AttackPower const power = stats::CreatureAttackPower(m_owner.ModifiersOf(UNIT_MOD_ATTACK_POWER));
    m_owner.SetAttackPower(false, power.base, power.added, power.share);

    // what it swings for follows from what it swings with
    Swing(BASE_ATTACK);
}

void PetSheet::Swing(WeaponAttackType attType)
{
    if (attType > BASE_ATTACK)
    {
        return;
    }

    stats::Swing swing = stats::PetSwing(
        m_owner.ModifiersOf(UNIT_MOD_DAMAGE_MAINHAND),
        m_owner.GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE),
        m_owner.GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE),
        m_owner.GetTotalAttackPowerValue(attType),
        float(m_owner.GetAttackTime(BASE_ATTACK)) / 1000.0f);

    // A hunter's pet hits as well as it feels.
    if (m_owner.getPetType() == HUNTER_PET)
    {
        float const mood = stats::HappinessScale(m_owner.GetHappinessState());
        swing.least *= mood;
        swing.most *= mood;
    }

    m_owner.SetStatFloatValue(UNIT_FIELD_MINDAMAGE, swing.least);
    m_owner.SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, swing.most);
}
