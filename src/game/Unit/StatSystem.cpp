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

#include "Unit.h"
#include "Stats/PetNumbers.h"
#include "Stats/PlayerNumbers.h"
#include "Stats/Chances.h"
#include "Player.h"
#include "Pet.h"
#include "Creature.h"
#include "SharedDefines.h"
#include "SpellAuras.h"

/// The talent that pays a druid per level for the shape it is in.
uint32 const ICON_PREDATORY_STRIKES = 1563;

/// The one pet whose attack power is not doubled.
uint32 const ENTRY_IMP = 416;

// The pure part mirrors HappinessState by value, so the two are held together
// here rather than by anyone remembering.
static_assert(stats::PET_UNHAPPY == UNHAPPY && stats::PET_CONTENT == CONTENT &&
              stats::PET_HAPPY == HAPPY, "the pet moods have drifted apart");

/*#######################################
########                         ########
########   PLAYERS STAT SYSTEM   ########
########                         ########
#######################################*/

/**
 * @brief Recalculates one player stat and dependent values.
 *
 * @param stat The stat to update.
 * @return true if the stat was updated; otherwise, false.
 */
bool Player::UpdateStats(Stats stat)
{
    if (stat > STAT_SPIRIT)
    {
        return false;
    }

    // value = ((base_value * base_pct) + total_value) * total_pct
    float value  = GetTotalStatValue(stat);

    SetStat(stat, int32(value));

    switch (stat)
    {
        case STAT_STRENGTH:
            break;
        case STAT_AGILITY:
            UpdateArmor();
            UpdateAllCritPercentages();
            UpdateDodgePercentage();
            break;
        case STAT_STAMINA:   UpdateMaxHealth(); break;
        case STAT_INTELLECT:
            UpdateMaxPower(POWER_MANA);
            UpdateAllSpellCritChances();
            UpdateArmor();                                  // SPELL_AURA_MOD_RESISTANCE_OF_INTELLECT_PERCENT, only armor currently
            break;

        case STAT_SPIRIT:
            break;

        default:
            break;
    }
    // Need update (exist AP from stat auras)
    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);

    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();

    return true;
}

/**
 * @brief Updates player spell damage and healing bonus fields.
 */
void Player::UpdateSpellDamageAndHealingBonus()
{
    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    // Get healing bonus for all schools
    // Get damage bonus for all schools
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetStatInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, SpellBaseDamageBonusDone(GetSchoolMask(i)));
    }
}

/**
 * @brief Recalculates all player stats and derived combat values.
 *
 * @return true.
 */
bool Player::UpdateAllStats()
{
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        float value = GetTotalStatValue(Stats(i));
        SetStat(Stats(i), (int32)value);
    }

    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);
    UpdateArmor();
    UpdateMaxHealth();

    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
    {
        UpdateMaxPower(Powers(i));
    }

    UpdateAllCritPercentages();
    UpdateAllSpellCritChances();
    UpdateDefenseBonusesMod();
    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        UpdateResistances(i);
    }

    return true;
}

/**
 * @brief Updates player resistances for one school.
 *
 * @param school The spell school to update.
 */
void Player::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value  = GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));
        SetResistance(SpellSchools(school), int32(value));
    }
    else
    {
        UpdateArmor();
    }
}

/**
 * @brief Recalculates player armor.
 */
void Player::UpdateArmor()
{
    // What turns a stat into armour: a share of intellect, per aura that says so.
    float fromIntellect = 0.0f;

    for (auto* aura : GetAurasByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT))
    {
        Modifier* mod = aura->GetModifier();
        if (mod->m_miscvalue & SPELL_SCHOOL_MASK_NORMAL)
        {
            fromIntellect += int32(GetStat(STAT_INTELLECT) * mod->m_amount / 100.0f);
        }
    }

    SetArmor(int32(stats::PlayerArmour(ModifiersOf(UNIT_MOD_ARMOR),
                                       GetStat(STAT_AGILITY), fromIntellect)));
}

/**
 * @brief Computes bonus health gained from stamina.
 *
 * @return The health bonus from stamina.
 */
float Player::GetHealthBonusFromStamina()
{
    return stats::HealthFromStamina(GetStat(STAT_STAMINA));
}

/**
 * @brief Computes bonus mana gained from intellect.
 *
 * @return The mana bonus from intellect.
 */
float Player::GetManaBonusFromIntellect()
{
    return stats::ManaFromIntellect(GetStat(STAT_INTELLECT));
}

/**
 * @brief Recalculates player maximum health.
 */
void Player::UpdateMaxHealth()
{
    SetMaxHealth(uint32(stats::PlayerMaxHealth(ModifiersOf(UNIT_MOD_HEALTH), GetCreateHealth(),
                                               GetHealthBonusFromStamina())));
}

/**
 * @brief Recalculates player maximum power for one resource type.
 *
 * @param power The power type to update.
 */
void Player::UpdateMaxPower(Powers power)
{
    UnitMods const unitMod = UnitMods(UNIT_MOD_POWER_START + power);
    uint32 const created = GetCreatePowers(power);

    // A class with no mana of its own gains none from intellect either.
    float const fromIntellect = (power == POWER_MANA && created > 0)
                                    ? GetManaBonusFromIntellect()
                                    : 0.0f;

    SetMaxPower(power, uint32(stats::PlayerMaxPower(ModifiersOf(unitMod), float(created), fromIntellect)));
}

/**
 * @brief Recalculates player attack power and derived damage.
 *
 * @param ranged true to update ranged attack power; otherwise, melee.
 */
void Player::UpdateAttackPowerAndDamage(bool ranged)
{
    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    stats::Physique who;
    who.klass = getClass();
    who.level = float(getLevel());
    who.strength = GetStat(STAT_STRENGTH);
    who.agility = GetStat(STAT_AGILITY);
    who.form = GetShapeshiftForm();

    // Predatory Strikes, which is the only talent that pays per level for the
    // shape its owner is in. Nobody else can have it, so nobody else is searched:
    // this walks every dummy aura a player carries, and there can be many.
    if (who.klass == CLASS_DRUID)
    {
        for (auto* aura : GetAurasByType(SPELL_AURA_DUMMY))
        {
            if (aura->GetSpellProto()->SpellIconID == ICON_PREDATORY_STRIKES)
            {
                who.predatoryStrikes = aura->GetModifier()->m_amount / 100.0f;
                break;
            }
        }
    }

    float const val2 = ranged ? stats::RangedAttackPower(who) : stats::MeleeAttackPower(who);

    SetModifierValue(unitMod, BASE_VALUE, val2);

    float base_attPower  = GetModifierValue(unitMod, BASE_VALUE) * GetModifierValue(unitMod, BASE_PCT);
    float attPowerMod = GetModifierValue(unitMod, TOTAL_VALUE);

    float attPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    SetAttackPower(ranged, static_cast<int32>(base_attPower), static_cast<int32>(attPowerMod),
                   attPowerMultiplier);

    // automatically update weapon damage after attack power modification
    if (ranged)
    {
        UpdateDamagePhysical(RANGED_ATTACK);
    }
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        if (CanDualWield() && haveOffhandWeapon())          // allow update offhand damage only if player knows DualWield Spec and has equipped offhand weapon
        {
            UpdateDamagePhysical(OFF_ATTACK);
        }
    }
}

/**
 * @brief Calculates the player's minimum and maximum weapon damage.
 *
 * @param attType The attack type to evaluate.
 * @param normalized true to use normalized attack speed.
 * @param min_damage Receives the minimum damage value.
 * @param max_damage Receives the maximum damage value.
 */
void Player::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, float& min_damage, float& max_damage)
{
    UnitMods unitMod;

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    float att_speed = GetAPMultiplier(attType, normalized);

    float base_value  = GetModifierValue(unitMod, BASE_VALUE) + GetTotalAttackPowerValue(attType) / 14.0f * att_speed;
    float base_pct    = GetModifierValue(unitMod, BASE_PCT);
    float total_value = GetModifierValue(unitMod, TOTAL_VALUE);
    float total_pct   = GetModifierValue(unitMod, TOTAL_PCT);

    float weapon_mindamage = GetWeaponDamageRange(attType, MINDAMAGE);
    float weapon_maxdamage = GetWeaponDamageRange(attType, MAXDAMAGE);

    if (IsInFeralForm())                                    // check if player is druid and in cat or bear forms, non main hand attacks not allowed for this mode so not check attack type
    {
        uint32 lvl = getLevel();
        if (lvl > 60)
        {
            lvl = 60;
        }

        weapon_mindamage = lvl * 0.85f * att_speed;
        weapon_maxdamage = lvl * 1.25f * att_speed;
    }
    else if (!CanUseEquippedWeapon(attType))                // check if player not in form but still can't use weapon (broken/etc)
    {
        weapon_mindamage = BASE_MINDAMAGE;
        weapon_maxdamage = BASE_MAXDAMAGE;
    }
    else if (attType == RANGED_ATTACK)                      // add ammo DPS to ranged damage
    {
        std::pair<float,float> ammoDps = GetAmmoDPS();
        weapon_mindamage += ammoDps.first * att_speed;
        weapon_maxdamage += ammoDps.second * att_speed;
    }

    min_damage = ((base_value + weapon_mindamage) * base_pct + total_value) * total_pct;
    max_damage = ((base_value + weapon_maxdamage) * base_pct + total_value) * total_pct;
}

/**
 * @brief Updates the player's physical damage display for one attack type.
 *
 * @param attType The attack type to update.
 */
void Player::UpdateDamagePhysical(WeaponAttackType attType)
{
    float mindamage;
    float maxdamage;

    CalculateMinMaxDamage(attType, false, mindamage, maxdamage);

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            SetStatFloatValue(UNIT_FIELD_MINDAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, maxdamage);
            break;
        case OFF_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, maxdamage);
            break;
        case RANGED_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, mindamage);
            SetStatFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, maxdamage);
            break;
    }
}

/**
 * @brief Recalculates player defensive percentage bonuses.
 */
void Player::UpdateDefenseBonusesMod()
{
    UpdateBlockPercentage();
    UpdateParryPercentage();
    UpdateDodgePercentage();
}

/**
 * @brief Recalculates player block chance.
 */
void Player::UpdateBlockPercentage()
{
    // Nothing at all for anyone holding no shield.
    float const value = CanBlock()
        ? stats::Chance(stats::GUARD_FROM_NOTHING,
                        int32(GetDefenseSkillValue()), int32(GetMaxSkillValueForLevel()),
                        float(GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT)))
        : 0.0f;

    SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, value);
}

/**
 * @brief Recalculates player crit chance for one attack type.
 *
 * @param attType The attack type to update.
 */
void Player::UpdateCritPercentage(WeaponAttackType attType)
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
        case OFF_ATTACK:                                    // client have only main hand crit
        default:
            return;
    }

    // A crit is governed by the skill of the weapon in hand, not by defence.
    float const value = stats::Chance(GetTotalPercentageModValue(modGroup),
                                      int32(GetWeaponSkillValue(attType)),
                                      int32(GetMaxSkillValueForLevel()), 0.0f);

    SetStatFloatValue(index, value);
}

/**
 * @brief Recalculates all player melee and ranged crit values.
 */
void Player::UpdateAllCritPercentages()
{
    float value = GetMeleeCritFromAgility();

    SetBaseModValue(CRIT_PERCENTAGE, PCT_MOD, value);
    SetBaseModValue(OFFHAND_CRIT_PERCENTAGE, PCT_MOD, value);
    SetBaseModValue(RANGED_CRIT_PERCENTAGE, PCT_MOD, value);

    UpdateCritPercentage(BASE_ATTACK);
    UpdateCritPercentage(OFF_ATTACK);
    UpdateCritPercentage(RANGED_ATTACK);
}

/**
 * @brief Recalculates player parry chance.
 */
void Player::UpdateParryPercentage()
{
    // Nothing at all for anyone who cannot parry, which is most classes.
    float const value = CanParry()
        ? stats::Chance(stats::GUARD_FROM_NOTHING,
                        int32(GetDefenseSkillValue()), int32(GetMaxSkillValueForLevel()),
                        float(GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT)))
        : 0.0f;

    SetStatFloatValue(PLAYER_PARRY_PERCENTAGE, value);
}

/**
 * @brief Recalculates player dodge chance.
 */
void Player::UpdateDodgePercentage()
{
    // A dodge starts from agility rather than from a flat five.
    float const value = stats::Chance(GetDodgeFromAgility(),
                                      int32(GetDefenseSkillValue()), int32(GetMaxSkillValueForLevel()),
                                      float(GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT)));

    SetStatFloatValue(PLAYER_DODGE_PERCENTAGE, value);
}

/**
 * @brief Recalculates spell crit chance for one school.
 *
 * @param school The spell school to update.
 */
void Player::UpdateSpellCritChance(uint32 school)
{
    // For normal school set zero crit chance
    if (school == SPELL_SCHOOL_NORMAL)
    {
        m_SpellCritPercentage[1] = 0.0f;
        return;
    }
    // For others recalculate it from:
    float crit = 0.0f;
    // Crit from Intellect
    crit += GetSpellCritFromIntellect();
    // Increase crit from SPELL_AURA_MOD_SPELL_CRIT_CHANCE
    crit += GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_CRIT_CHANCE);
    // Increase crit by school from SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL
    crit += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, 1 << school);

    // Store crit value
    m_SpellCritPercentage[school] = crit;
}

/**
 * @brief Recalculates spell crit chance for all schools.
 */
void Player::UpdateAllSpellCritChances()
{
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        UpdateSpellCritChance(i);
    }
}

/**
 * @brief Recalculates player mana regeneration values.
 */
void Player::UpdateManaRegen()
{
    stats::ManaRegen const regen = stats::Regeneration(
        OCTRegenMPPerSpirit(),
        GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA),
        float(GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA)),
        GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT));

    m_modManaRegen = regen.standing;
    m_modManaRegenInterrupt = regen.casting;
}

/**
 * @brief Applies all aura and item stat bonuses, then updates derived stats.
 */
void Player::_ApplyAllStatBonuses()
{
    SetCanModifyStats(false);

    _ApplyAllAuraMods();
    _ApplyAllItemMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

/**
 * @brief Removes all aura and item stat bonuses, then updates derived stats.
 */
void Player::_RemoveAllStatBonuses()
{
    SetCanModifyStats(false);

    _RemoveAllItemMods();
    _RemoveAllAuraMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

/*#######################################
########                         ########
########    MOBS STAT SYSTEM     ########
########                         ########
#######################################*/

/**
 * @brief Creature stat updates are handled elsewhere.
 *
 * @param stat The stat to update.
 * @return true.
 */
bool Creature::UpdateStats(Stats /*stat*/)
{
    return true;
}

/**
 * @brief Recalculates all creature stats and derived combat values.
 *
 * @return true.
 */
bool Creature::UpdateAllStats()
{
    UpdateMaxHealth();
    UpdateAttackPowerAndDamage();

    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
    {
        UpdateMaxPower(Powers(i));
    }

    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        UpdateResistances(i);
    }

    return true;
}

/**
 * @brief Updates creature resistances for one school.
 *
 * @param school The spell school to update.
 */
void Creature::UpdateResistances(uint32 school)
{
    // The normal school is armour, and armour is kept in its own field.
    if (school == SPELL_SCHOOL_NORMAL)
    {
        UpdateArmor();
        return;
    }

    SetResistance(SpellSchools(school),
                  int32(stats::Simple(ModifiersOf(UnitMods(UNIT_MOD_RESISTANCE_START + school)))));
}

/**
 * @brief Recalculates creature armor.
 */
void Creature::UpdateArmor()
{
    SetArmor(int32(stats::Simple(ModifiersOf(UNIT_MOD_ARMOR))));
}

/**
 * @brief Recalculates creature maximum health.
 */
void Creature::UpdateMaxHealth()
{
    SetMaxHealth(uint32(stats::Simple(ModifiersOf(UNIT_MOD_HEALTH))));
}

/**
 * @brief Recalculates creature maximum power for one resource type.
 *
 * @param power The power type to update.
 */
void Creature::UpdateMaxPower(Powers power)
{
    SetMaxPower(power, uint32(stats::Simple(ModifiersOf(UnitMods(UNIT_MOD_POWER_START + power)))));
}

/**
 * @brief Recalculates creature attack power and derived damage.
 *
 * @param ranged true to update ranged attack power; otherwise, melee.
 */
void Creature::UpdateAttackPowerAndDamage(bool ranged)
{
    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    stats::AttackPower const power = stats::CreatureAttackPower(ModifiersOf(unitMod));
    SetAttackPower(ranged, power.base, power.added, power.share);

    if (ranged)
    {
        return;
    }

    // automatically update weapon damage after attack power modification
    UpdateDamagePhysical(BASE_ATTACK);
    UpdateDamagePhysical(OFF_ATTACK);
}

/**
 * @brief Updates creature physical damage values for one attack type.
 *
 * @param attType The attack type to update.
 */
void Creature::UpdateDamagePhysical(WeaponAttackType attType)
{
    if (attType > OFF_ATTACK)
    {
        return;
    }

    UnitMods unitMod = (attType == BASE_ATTACK ? UNIT_MOD_DAMAGE_MAINHAND : UNIT_MOD_DAMAGE_OFFHAND);

    // Only the attack power it has ABOVE the template's counts: what the template
    // was written with is already in the damage the template gives.
    float const gained = GetTotalAttackPowerValue(attType) - GetCreatureInfo()->MeleeAttackPower;

    stats::Swing const swing = stats::CreatureSwing(
        ModifiersOf(unitMod),
        GetWeaponDamageRange(attType, MINDAMAGE),
        GetWeaponDamageRange(attType, MAXDAMAGE),
        gained,
        GetAPMultiplier(attType, false),
        GetCreatureInfo()->DamageMultiplier);

    SetStatFloatValue(attType == BASE_ATTACK ? UNIT_FIELD_MINDAMAGE : UNIT_FIELD_MINOFFHANDDAMAGE, swing.least);
    SetStatFloatValue(attType == BASE_ATTACK ? UNIT_FIELD_MAXDAMAGE : UNIT_FIELD_MAXOFFHANDDAMAGE, swing.most);
}

/*#######################################
########                         ########
########    PETS STAT SYSTEM     ########
########                         ########
#######################################*/

/**
 * @brief Recalculates one pet stat and dependent values.
 *
 * @param stat The stat to update.
 * @return true if the stat was updated; otherwise, false.
 */
bool Pet::UpdateStats(Stats stat)
{
    if (stat > STAT_SPIRIT)
    {
        return false;
    }

    // value = ((base_value * base_pct) + total_value) * total_pct
    float value  = GetTotalStatValue(stat);
    SetStat(stat, int32(value));

    switch (stat)
    {
        case STAT_STRENGTH:         UpdateAttackPowerAndDamage();        break;
        case STAT_AGILITY:          UpdateArmor();                       break;
        case STAT_STAMINA:          UpdateMaxHealth();                   break;
        case STAT_INTELLECT:        UpdateMaxPower(POWER_MANA);          break;
        case STAT_SPIRIT:
        default:
            break;
    }

    return true;
}

/**
 * @brief Recalculates all pet stats and derived values.
 *
 * @return true.
 */
bool Pet::UpdateAllStats()
{
    for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        UpdateStats(Stats(i));
    }

    for (int i = POWER_MANA; i < MAX_POWERS; ++i)
    {
        UpdateMaxPower(Powers(i));
    }

    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        UpdateResistances(i);
    }

    return true;
}

/**
 * @brief Updates pet resistances for one school.
 *
 * @param school The spell school to update.
 */
void Pet::UpdateResistances(uint32 school)
{
    // The normal school is armour, and armour is kept in its own field.
    if (school == SPELL_SCHOOL_NORMAL)
    {
        UpdateArmor();
        return;
    }

    SetResistance(SpellSchools(school),
                  int32(stats::Simple(ModifiersOf(UnitMods(UNIT_MOD_RESISTANCE_START + school)))));
}

/**
 * @brief Recalculates pet armor.
 */
void Pet::UpdateArmor()
{
    SetArmor(int32(stats::PetArmour(ModifiersOf(UNIT_MOD_ARMOR), GetStat(STAT_AGILITY))));
}

/**
 * @brief Recalculates pet maximum health.
 */
void Pet::UpdateMaxHealth()
{
    float const gained = GetStat(STAT_STAMINA) - GetCreateStat(STAT_STAMINA);

    SetMaxHealth(uint32(stats::PetMaxHealth(ModifiersOf(UNIT_MOD_HEALTH), GetCreateHealth(), gained)));
}

/**
 * @brief Recalculates pet maximum power for one resource type.
 *
 * @param power The power type to update.
 */
void Pet::UpdateMaxPower(Powers power)
{
    // Only mana grows from a stat, and only from intellect gained since creation.
    float const gained = power == POWER_MANA
                             ? GetStat(STAT_INTELLECT) - GetCreateStat(STAT_INTELLECT)
                             : 0.0f;

    UnitMods const unitMod = UnitMods(UNIT_MOD_POWER_START + power);
    SetMaxPower(power, uint32(stats::PetMaxPower(ModifiersOf(unitMod), GetCreatePowers(power), gained)));
}

/**
 * @brief Recalculates pet attack power and derived damage.
 *
 * @param ranged true to update ranged attack power; otherwise, melee.
 */
void Pet::UpdateAttackPowerAndDamage(bool ranged)
{
    if (ranged)
    {
        return;
    }

    float val = 0.0f;
    UnitMods unitMod = UNIT_MOD_ATTACK_POWER;

    val = stats::PetAttackPowerFromStrength(GetStat(STAT_STRENGTH), GetEntry() == ENTRY_IMP);

    SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, val);

    stats::AttackPower const power = stats::CreatureAttackPower(ModifiersOf(unitMod));
    SetAttackPower(false, power.base, power.added, power.share);

    // automatically update weapon damage after attack power modification
    UpdateDamagePhysical(BASE_ATTACK);
}

/**
 * @brief Updates pet physical damage for the main attack.
 *
 * @param attType The attack type to update.
 */
void Pet::UpdateDamagePhysical(WeaponAttackType attType)
{
    if (attType > BASE_ATTACK)
    {
        return;
    }

    stats::Swing swing = stats::PetSwing(
        ModifiersOf(UNIT_MOD_DAMAGE_MAINHAND),
        GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE),
        GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE),
        GetTotalAttackPowerValue(attType),
        float(GetAttackTime(BASE_ATTACK)) / 1000.0f);

    // A hunter's pet hits as well as it feels.
    if (getPetType() == HUNTER_PET)
    {
        float const mood = stats::HappinessScale(GetHappinessState());
        swing.least *= mood;
        swing.most *= mood;
    }

    float mindamage = swing.least;
    float maxdamage = swing.most;

    SetStatFloatValue(UNIT_FIELD_MINDAMAGE, mindamage);
    SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, maxdamage);
}
