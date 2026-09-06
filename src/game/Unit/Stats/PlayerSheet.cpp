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

#include "PlayerSheet.h"

#include "Chances.h"
#include "Player.h"
#include "PlayerNumbers.h"
#include "SpellAuras.h"

/// The talent that pays a druid per level for the shape it is in.
static uint32 const ICON_PREDATORY_STRIKES = 1563;

PlayerSheet::PlayerSheet(Player& whose) : StatSheet(whose), m_owner(whose)
{
}

float PlayerSheet::HealthFromStamina() const
{
    return stats::HealthFromStamina(m_owner.GetStat(STAT_STAMINA));
}

float PlayerSheet::ManaFromIntellect() const
{
    return stats::ManaFromIntellect(m_owner.GetStat(STAT_INTELLECT));
}

void PlayerSheet::Stat(Stats stat)
{
    if (stat > STAT_SPIRIT)
    {
        return;
    }

    m_owner.SetStat(stat, int32(m_owner.Tallied().FoldedOver(stat)));

    switch (stat)
    {
        case STAT_AGILITY:
            Armour();
            AllCrits();
            Dodge();
            break;

        case STAT_STAMINA:
            MaxHealth();
            break;

        case STAT_INTELLECT:
            MaxPower(POWER_MANA);
            AllSpellCrits();
            Armour();                                       // an aura turns a share of intellect into armour
            break;

        case STAT_STRENGTH:
        case STAT_SPIRIT:
        default:
            break;
    }

    // an aura can turn any stat into attack power, so both hands follow always
    AttackPower(false);
    AttackPower(true);

    SpellDamageAndHealing();
    ManaRegen();
}

void PlayerSheet::Everything()
{
    for (int stat = STAT_STRENGTH; stat < MAX_STATS; ++stat)
    {
        m_owner.SetStat(Stats(stat), int32(m_owner.Tallied().FoldedOver(Stats(stat))));
    }

    AttackPower(false);
    AttackPower(true);
    Armour();
    MaxHealth();

    for (int power = POWER_MANA; power < MAX_POWERS; ++power)
    {
        MaxPower(Powers(power));
    }

    AllCrits();
    AllSpellCrits();
    Defences();
    SpellDamageAndHealing();
    ManaRegen();

    for (int school = SPELL_SCHOOL_NORMAL; school < MAX_SPELL_SCHOOL; ++school)
    {
        Resistance(school);
    }
}

void PlayerSheet::Armour()
{
    // What turns a stat into armour: a share of intellect, per aura that says so.
    float fromIntellect = 0.0f;

    for (auto* aura : m_owner.GetAurasByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT))
    {
        Modifier* mod = aura->GetModifier();
        if (mod->m_miscvalue & SPELL_SCHOOL_MASK_NORMAL)
        {
            fromIntellect += int32(m_owner.GetStat(STAT_INTELLECT) * mod->m_amount / 100.0f);
        }
    }

    m_owner.SetArmor(int32(stats::PlayerArmour(m_owner.Tallied().Of(UNIT_MOD_ARMOR),
                                               m_owner.GetStat(STAT_AGILITY), fromIntellect)));
}

void PlayerSheet::MaxHealth()
{
    m_owner.SetMaxHealth(uint32(stats::PlayerMaxHealth(m_owner.Tallied().Of(UNIT_MOD_HEALTH),
                                                       m_owner.GetCreateHealth(), HealthFromStamina())));
}

void PlayerSheet::MaxPower(Powers power)
{
    UnitMods const unitMod = UnitMods(UNIT_MOD_POWER_START + power);
    uint32 const created = m_owner.GetCreatePowers(power);

    // A class with no mana of its own gains none from intellect either.
    float const fromIntellect = (power == POWER_MANA && created > 0) ? ManaFromIntellect() : 0.0f;

    m_owner.SetMaxPower(power, uint32(stats::PlayerMaxPower(m_owner.Tallied().Of(unitMod),
                                                            float(created), fromIntellect)));
}

void PlayerSheet::AttackPower(bool ranged)
{
    UnitMods const unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    stats::Physique who;
    who.klass = m_owner.getClass();
    who.level = float(m_owner.getLevel());
    who.strength = m_owner.GetStat(STAT_STRENGTH);
    who.agility = m_owner.GetStat(STAT_AGILITY);
    who.form = m_owner.GetShapeshiftForm();

    // Predatory Strikes, which is the only talent that pays per level for the
    // shape its owner is in. Nobody else can have it, so nobody else is searched:
    // this walks every dummy aura a player carries, and there can be many.
    if (who.klass == CLASS_DRUID)
    {
        for (auto* aura : m_owner.GetAurasByType(SPELL_AURA_DUMMY))
        {
            if (aura->GetSpellProto()->SpellIconID == ICON_PREDATORY_STRIKES)
            {
                who.predatoryStrikes = aura->GetModifier()->m_amount / 100.0f;
                break;
            }
        }
    }

    m_owner.Tallied().Value(unitMod, BASE_VALUE, ranged ? stats::RangedAttackPower(who)
                                                         : stats::MeleeAttackPower(who));

    float const base = m_owner.Tallied().Value(unitMod, BASE_VALUE) * m_owner.Tallied().Value(unitMod, BASE_PCT);
    float const added = m_owner.Tallied().Value(unitMod, TOTAL_VALUE);
    float const share = m_owner.Tallied().Value(unitMod, TOTAL_PCT) - 1.0f;

    m_owner.SetAttackPower(ranged, static_cast<int32>(base), static_cast<int32>(added), share);

    // what he swings for follows from what he swings with
    if (ranged)
    {
        Swing(RANGED_ATTACK);
        return;
    }

    Swing(BASE_ATTACK);
    if (m_owner.Arms().CanDualWield() && m_owner.haveOffhandWeapon())
    {
        Swing(OFF_ATTACK);
    }
}

void PlayerSheet::SwingRange(WeaponAttackType attType, bool normalized, float& least, float& most)
{
    UnitMods unitMod;

    switch (attType)
    {
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
        case BASE_ATTACK:
        default:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
    }

    float const speed = m_owner.GetAPMultiplier(attType, normalized);

    float const base_value = m_owner.Tallied().Value(unitMod, BASE_VALUE)
                             + m_owner.GetTotalAttackPowerValue(attType) / 14.0f * speed;
    float const base_pct = m_owner.Tallied().Value(unitMod, BASE_PCT);
    float const total_value = m_owner.Tallied().Value(unitMod, TOTAL_VALUE);
    float const total_pct = m_owner.Tallied().Value(unitMod, TOTAL_PCT);

    float weaponLeast = m_owner.GetWeaponDamageRange(attType, MINDAMAGE);
    float weaponMost = m_owner.GetWeaponDamageRange(attType, MAXDAMAGE);

    if (m_owner.IsInFeralForm())                            // a cat or a bear swings by its level, whatever it carries
    {
        uint32 lvl = m_owner.getLevel();
        if (lvl > 60)
        {
            lvl = 60;
        }

        weaponLeast = lvl * 0.85f * speed;
        weaponMost = lvl * 1.25f * speed;
    }
    else if (!m_owner.CanUseEquippedWeapon(attType))        // out of form and still unable to use it: broken, or forbidden
    {
        weaponLeast = BASE_MINDAMAGE;
        weaponMost = BASE_MAXDAMAGE;
    }
    else if (attType == RANGED_ATTACK)                      // the ammo adds its own damage per second
    {
        std::pair<float, float> const ammo = m_owner.Arms().Ammo();
        weaponLeast += ammo.first * speed;
        weaponMost += ammo.second * speed;
    }

    least = ((base_value + weaponLeast) * base_pct + total_value) * total_pct;
    most = ((base_value + weaponMost) * base_pct + total_value) * total_pct;
}

void PlayerSheet::Swing(WeaponAttackType attType)
{
    float least;
    float most;

    SwingRange(attType, false, least, most);

    switch (attType)
    {
        case OFF_ATTACK:
            m_owner.SetStatFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, least);
            m_owner.SetStatFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, most);
            break;
        case RANGED_ATTACK:
            m_owner.SetStatFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, least);
            m_owner.SetStatFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, most);
            break;
        case BASE_ATTACK:
        default:
            m_owner.SetStatFloatValue(UNIT_FIELD_MINDAMAGE, least);
            m_owner.SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, most);
            break;
    }
}

void PlayerSheet::SpellDamageAndHealing()
{
    for (int school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
    {
        m_owner.SetStatInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + school,
                                  m_owner.SpellBaseDamageBonusDone(GetSchoolMask(school)));
    }
}

void PlayerSheet::Defences()
{
    Block();
    Parry();
    Dodge();
}

void PlayerSheet::Block()
{
    // Nothing at all for anyone holding no shield.
    float const value = m_owner.Arms().CanBlock()
        ? stats::Chance(stats::GUARD_FROM_NOTHING,
                        int32(m_owner.GetDefenseSkillValue()), int32(m_owner.GetMaxSkillValueForLevel()),
                        float(m_owner.GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT)))
        : 0.0f;

    m_owner.SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, value);
}

void PlayerSheet::Parry()
{
    // Nothing at all for anyone who cannot parry, which is most classes.
    float const value = m_owner.Arms().CanParry()
        ? stats::Chance(stats::GUARD_FROM_NOTHING,
                        int32(m_owner.GetDefenseSkillValue()), int32(m_owner.GetMaxSkillValueForLevel()),
                        float(m_owner.GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT)))
        : 0.0f;

    m_owner.SetStatFloatValue(PLAYER_PARRY_PERCENTAGE, value);
}

void PlayerSheet::Dodge()
{
    // A dodge starts from agility rather than from a flat five.
    float const value = stats::Chance(m_owner.GetDodgeFromAgility(),
                                      int32(m_owner.GetDefenseSkillValue()), int32(m_owner.GetMaxSkillValueForLevel()),
                                      float(m_owner.GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT)));

    m_owner.SetStatFloatValue(PLAYER_DODGE_PERCENTAGE, value);
}

void PlayerSheet::Crit(WeaponAttackType attType)
{
    BaseModGroup modGroup;
    uint16 index;

    switch (attType)
    {
        case RANGED_ATTACK:
            modGroup = RANGED_CRIT_PERCENTAGE;
            index = PLAYER_RANGED_CRIT_PERCENTAGE;
            break;
        case BASE_ATTACK:
            modGroup = CRIT_PERCENTAGE;
            index = PLAYER_CRIT_PERCENTAGE;
            break;
        case OFF_ATTACK:                                    // the client shows a crit chance for the main hand only
        default:
            return;
    }

    // A crit is governed by the skill of the weapon in hand, not by defence.
    float const value = stats::Chance(m_owner.GetTotalPercentageModValue(modGroup),
                                      int32(m_owner.GetWeaponSkillValue(attType)),
                                      int32(m_owner.GetMaxSkillValueForLevel()), 0.0f);

    m_owner.SetStatFloatValue(index, value);
}

void PlayerSheet::AllCrits()
{
    float const fromAgility = m_owner.GetMeleeCritFromAgility();

    m_owner.SetBaseModValue(CRIT_PERCENTAGE, PCT_MOD, fromAgility);
    m_owner.SetBaseModValue(OFFHAND_CRIT_PERCENTAGE, PCT_MOD, fromAgility);
    m_owner.SetBaseModValue(RANGED_CRIT_PERCENTAGE, PCT_MOD, fromAgility);

    Crit(BASE_ATTACK);
    Crit(OFF_ATTACK);
    Crit(RANGED_ATTACK);
}

void PlayerSheet::SpellCrit(uint32 school)
{
    // No spell is of the normal school, so nothing crits there.
    if (school == SPELL_SCHOOL_NORMAL)
    {
        m_spellCrit[SPELL_SCHOOL_NORMAL] = 0.0f;
        return;
    }

    float crit = m_owner.GetSpellCritFromIntellect();
    crit += m_owner.GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_CRIT_CHANCE);
    crit += m_owner.GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, 1 << school);

    m_spellCrit[school] = crit;
}

void PlayerSheet::AllSpellCrits()
{
    for (int school = SPELL_SCHOOL_NORMAL; school < MAX_SPELL_SCHOOL; ++school)
    {
        SpellCrit(school);
    }
}

void PlayerSheet::ManaRegen()
{
    stats::ManaRegen const regen = stats::Regeneration(
        m_owner.OCTRegenMPPerSpirit(),
        m_owner.GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA),
        float(m_owner.GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA)),
        m_owner.GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT));

    m_manaRegenStanding = regen.standing;
    m_manaRegenCasting = regen.casting;
}

uint32 PlayerSheet::ShieldBlock() const
{
    return stats::PlayerShieldBlock(m_owner.GetBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD),
                                    m_owner.GetBaseModValue(SHIELD_BLOCK_VALUE, PCT_MOD),
                                    m_owner.GetStat(STAT_STRENGTH));
}
