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

#include <cmath>
#include "SpellAuras.h"
#include "Platform/Define.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "Policies/Singleton.h"
#include "Totem.h"
#include "Creature.h"
#include "Formulas.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "CreatureAI.h"
#include "ScriptMgr.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Language.h"
#include "TemporarySummon.h"
#include "Cast/Recipe/RecipeBook.h"

void Aura::HandleAuraProcTriggerSpell(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {
        switch (GetId())
        {

            case 28200:
                GetHolder()->SetAuraCharges(6);
                break;
            case 8179:
                target->CastSpell(target, 8178, true, 0, this);
                return;
            case 6474:
                target->CastSpell(target, 3600, true, 0, this);
                return;
            default:
                break;
        }
    }
}

void Aura::HandleAuraModStalked(bool apply, bool )
{

    if (apply)
    {
        GetTarget()->SetDynFlag(UNIT_DYNFLAG_TRACK_UNIT);
    }
    else
    {
        GetTarget()->RemoveDynFlag(UNIT_DYNFLAG_TRACK_UNIT);
    }
}

void Aura::HandlePeriodicTriggerSpell(bool apply, bool )
{
    m_isPeriodic = apply;

    if (!apply)
    {
        switch (GetId())
        {
            case 29213:
                if (m_removeMode != AURA_REMOVE_BY_DISPEL)

                {
                    Unit* target = GetTarget();
                    target->CastSpell(target, 29214, true, 0, this);
                }
                return;
            default:
                break;
        }
    }
}

void Aura::HandlePeriodicTriggerSpellWithValue(bool apply, bool )
{
    m_isPeriodic = apply;
}

void Aura::HandlePeriodicEnergize(bool apply, bool )
{
    m_isPeriodic = apply;
}

void Aura::HandleAuraPowerBurn(bool apply, bool )
{
    m_isPeriodic = apply;
}

void Aura::HandlePeriodicHeal(bool apply, bool )
{
    m_isPeriodic = apply;

    Unit* target = GetTarget();

    bool loading = (IsPlayer(target) && ((Player*)target)->GetSession()->PlayerLoading());

    if (apply)
    {
        if (loading)
        {
            return;
        }

        Unit* caster = GetCaster();
        if (!caster)
        {
            return;
        }

        m_modifier.m_amount = caster->SpellHealingBonusDone(target, GetSpellProto(), m_modifier.m_amount, DOT, GetStackAmount());
    }
}

void Aura::HandlePeriodicDamage(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    m_isPeriodic = apply;

    Unit* target = GetTarget();
    SpellEntry const* spellProto = GetSpellProto();

    bool loading = (IsPlayer(target) && ((Player*)target)->GetSession()->PlayerLoading());

    if (apply)
    {
        if (loading)
        {
            return;
        }

        Unit* caster = GetCaster();
        if (!caster)
        {
            return;
        }

        switch (spellProto->SpellClassSet)
        {
            case SPELLFAMILY_DRUID:
            {

                if (spellProto->SpellClassMask & UI64LIT(0x000000000000800000))
                {

                    if (IsPlayer(caster))
                    {
                        uint8 cp = ((Player*)caster)->GetComboPoints();

                        if (cp > 4)
                        {
                            cp = 4;
                        }
                        m_modifier.m_amount += int32(caster->GetTotalAttackPowerValue(BASE_ATTACK) * cp / 100);
                    }
                }
                break;
            }
            case SPELLFAMILY_ROGUE:
            {

                if (spellProto->SpellClassMask & UI64LIT(0x000000000000100000))
                {
                    if (!IsPlayer(caster))
                    {
                        break;
                    }

                    uint8 cp = ((Player*)caster)->GetComboPoints();
                    if (cp > 3)
                    {
                        cp = 3;
                    }
                    m_modifier.m_amount += int32(caster->GetTotalAttackPowerValue(BASE_ATTACK) * cp / 100);
                }
                break;
            }
            default:
                break;
        }

        if (m_modifier.m_auraname == SPELL_AURA_PERIODIC_DAMAGE)
        {

            if (spellProto->DefenseType == SPELL_DAMAGE_CLASS_NONE || spellProto->DefenseType == SPELL_DAMAGE_CLASS_MAGIC)
            {
                m_modifier.m_amount = caster->SpellDamageBonusDone(target, GetSpellProto(), m_modifier.m_amount, DOT, GetStackAmount());
            }

            else
            {
                WeaponAttackType attackType = Recipe().Swings();
                m_modifier.m_amount = caster->MeleeDamageBonusDone(target, m_modifier.m_amount, attackType, GetSpellProto(), DOT, GetStackAmount());
            }
        }
    }
}

void Aura::HandlePeriodicDamagePCT(bool apply, bool )
{
    m_isPeriodic = apply;
}

void Aura::HandlePeriodicLeech(bool apply, bool )
{
    m_isPeriodic = apply;

    bool loading = (IsPlayer(GetTarget()) && ((Player*)GetTarget())->GetSession()->PlayerLoading());

    if (apply)
    {
        if (loading)
        {
            return;
        }

        Unit* caster = GetCaster();
        if (!caster)
        {
            return;
        }

        m_modifier.m_amount = caster->SpellDamageBonusDone(GetTarget(), GetSpellProto(), m_modifier.m_amount, DOT, GetStackAmount());
    }
}

void Aura::HandlePeriodicManaLeech(bool apply, bool )
{
    m_isPeriodic = apply;
}

void Aura::HandlePeriodicHealthFunnel(bool apply, bool )
{
    m_isPeriodic = apply;

    bool loading = (IsPlayer(GetTarget()) && ((Player*)GetTarget())->GetSession()->PlayerLoading());

    if (apply)
    {
        if (loading)
        {
            return;
        }

        Unit* caster = GetCaster();
        if (!caster)
        {
            return;
        }

        m_modifier.m_amount = caster->SpellDamageBonusDone(GetTarget(), GetSpellProto(), m_modifier.m_amount, DOT, GetStackAmount());
    }
}

void Aura::HandleAuraModResistanceExclusive(bool apply, bool )
{
    for (int8 x = SPELL_SCHOOL_NORMAL; x < MAX_SPELL_SCHOOL; ++x)
    {
        if (m_modifier.m_miscvalue & int32(1 << x))
        {
            stats::Apply(*GetTarget(), UnitMods(UNIT_MOD_RESISTANCE_START + x), BASE_VALUE, float(m_modifier.m_amount), apply);
            if (IsPlayer(GetTarget()))
            {
                ((Player*)GetTarget())->ApplyResistanceBuffModsMod(SpellSchools(x), m_positive, float(m_modifier.m_amount), apply);
            }
        }
    }
}

void Aura::HandleAuraModResistance(bool apply, bool )
{
    Unit* target = GetTarget();
    SpellEntry const* spellProto = GetSpellProto();

    for (int8 x = SPELL_SCHOOL_NORMAL; x < MAX_SPELL_SCHOOL; ++x)
    {
        if (m_modifier.m_miscvalue & int32(1 << x))
        {
            stats::Apply(*target, UnitMods(UNIT_MOD_RESISTANCE_START + x), TOTAL_VALUE, float(m_modifier.m_amount), apply);
            if (IsPlayer(target))
            {
                ((Player*)target)->ApplyResistanceBuffModsMod(SpellSchools(x), m_positive, float(m_modifier.m_amount), apply);
            }
        }
    }

    if (spellProto->SpellIconID == 109 &&
        spellProto->SpellClassSet == SPELLFAMILY_DRUID &&
        spellProto->SpellClassMask & UI64LIT(0x0000000000000400))
    {
        target->ApplySpellDispelImmunity(spellProto, DISPEL_STEALTH, apply);
        target->ApplySpellDispelImmunity(spellProto, DISPEL_INVISIBILITY, apply);
    }
}

void Aura::HandleAuraModBaseResistancePCT(bool apply, bool )
{

    if (!IsPlayer(GetTarget()))
    {

        if (((Creature*)GetTarget())->IsPet() && (m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL))
        {
            stats::Apply(*GetTarget(), UNIT_MOD_ARMOR, BASE_PCT, float(m_modifier.m_amount), apply);
        }
    }
    else
    {
        for (int8 x = SPELL_SCHOOL_NORMAL; x < MAX_SPELL_SCHOOL; ++x)
        {
            if (m_modifier.m_miscvalue & int32(1 << x))
            {
                stats::Apply(*GetTarget(), UnitMods(UNIT_MOD_RESISTANCE_START + x), BASE_PCT, float(m_modifier.m_amount), apply);
            }
        }
    }
}

void Aura::HandleModResistancePercent(bool apply, bool )
{
    Unit* target = GetTarget();

    for (int8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
    {
        if (m_modifier.m_miscvalue & int32(1 << i))
        {
            stats::Apply(*target, UnitMods(UNIT_MOD_RESISTANCE_START + i), TOTAL_PCT, float(m_modifier.m_amount), apply);
            if (IsPlayer(target))
            {
                ((Player*)target)->ApplyResistanceBuffModsPercentMod(SpellSchools(i), true, float(m_modifier.m_amount), apply);
                ((Player*)target)->ApplyResistanceBuffModsPercentMod(SpellSchools(i), false, float(m_modifier.m_amount), apply);
            }
        }
    }
}

void Aura::HandleModBaseResistance(bool apply, bool )
{

    if (!IsPlayer(GetTarget()))
    {

        if (((Creature*)GetTarget())->IsPet() && (m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL))
        {
            stats::Apply(*GetTarget(), UNIT_MOD_ARMOR, TOTAL_VALUE, float(m_modifier.m_amount), apply);
        }
    }
    else
    {
        for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        {
            if (m_modifier.m_miscvalue & (1 << i))
            {
                stats::Apply(*GetTarget(), UnitMods(UNIT_MOD_RESISTANCE_START + i), TOTAL_VALUE, float(m_modifier.m_amount), apply);
            }
        }
    }
}

void Aura::HandleAuraModStat(bool apply, bool )
{
    if (m_modifier.m_miscvalue < -2 || m_modifier.m_miscvalue > 4)
    {
        sLog.outError("WARNING: Spell %u effect %u have unsupported misc value (%i) for SPELL_AURA_MOD_STAT ", GetId(), GetEffIndex(), m_modifier.m_miscvalue);
        return;
    }

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {

        if (m_modifier.m_miscvalue < 0 || m_modifier.m_miscvalue == i)
        {

            stats::Apply(*GetTarget(), UnitMods(UNIT_MOD_STAT_START + i), TOTAL_VALUE, float(m_modifier.m_amount), apply);
            if (IsPlayer(GetTarget()))
            {
                ((Player*)GetTarget())->ApplyStatBuffMod(Stats(i), float(m_modifier.m_amount), apply);
            }
        }
    }
}

void Aura::HandleModPercentStat(bool apply, bool )
{
    if (m_modifier.m_miscvalue < -1 || m_modifier.m_miscvalue > 4)
    {
        sLog.outError("WARNING: Misc Value for SPELL_AURA_MOD_PERCENT_STAT not valid");
        return;
    }

    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        if (m_modifier.m_miscvalue == i || m_modifier.m_miscvalue == -1)
        {
            stats::Apply(*GetTarget(), UnitMods(UNIT_MOD_STAT_START + i), BASE_PCT, float(m_modifier.m_amount), apply);
        }
    }
}

void Aura::HandleModSpellDamagePercentFromStat(bool , bool )
{
    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    ((Player*)GetTarget())->Sheet().SpellDamageAndHealing();
}

void Aura::HandleModSpellHealingPercentFromStat(bool , bool )
{
    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    ((Player*)GetTarget())->Sheet().SpellDamageAndHealing();
}

void Aura::HandleModHealingDone(bool , bool )
{
    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    ((Player*)GetTarget())->Sheet().SpellDamageAndHealing();
}

void Aura::HandleModTotalPercentStat(bool apply, bool )
{
    if (m_modifier.m_miscvalue < -1 || m_modifier.m_miscvalue > 4)
    {
        sLog.outError("WARNING: Misc Value for SPELL_AURA_MOD_PERCENT_STAT not valid");
        return;
    }

    Unit* target = GetTarget();

    uint32 curHPValue = target->GetHealth();
    uint32 maxHPValue = target->GetMaxHealth();

    for (int32 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        if (m_modifier.m_miscvalue == i || m_modifier.m_miscvalue == -1)
        {
            stats::Apply(*target, UnitMods(UNIT_MOD_STAT_START + i), TOTAL_PCT, float(m_modifier.m_amount), apply);
            if (IsPlayer(target))
            {
                ((Player*)target)->ApplyStatPercentBuffMod(Stats(i), float(m_modifier.m_amount), apply);
            }
        }
    }

    if (m_modifier.m_miscvalue == STAT_STAMINA && maxHPValue > 0 && Recipe().Says().ability)
    {

        uint32 newHPValue = (target->GetMaxHealth() * curHPValue) / maxHPValue;
        target->SetHealth(newHPValue);
    }
}

void Aura::HandleAuraModResistenceOfStatPercent(bool , bool )
{
    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    if (m_modifier.m_miscvalue != SPELL_SCHOOL_MASK_NORMAL)
    {

        sLog.outError("Aura SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT(182) need adding support for non-armor resistances!");
        return;
    }

    GetTarget()->Sheet().Armour();
}

void Aura::HandleAuraModTotalHealthPercentRegen(bool apply, bool )
{
    m_isPeriodic = apply;
}

void Aura::HandleAuraModTotalManaPercentRegen(bool apply, bool )
{
    if (m_modifier.periodictime == 0)
    {
        m_modifier.periodictime = 1000;
    }

    m_periodicTimer = m_modifier.periodictime;
    m_isPeriodic = apply;
}

void Aura::HandleModRegen(bool apply, bool )
{
    if (m_modifier.periodictime == 0)
    {
        m_modifier.periodictime = 5000;
    }

    m_periodicTimer = 5000;
    m_isPeriodic = apply;
}

void Aura::HandleModPowerRegen(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Powers powerType = GetTarget()->GetPowerType();
    if (m_modifier.periodictime == 0)
    {

        if (powerType == POWER_RAGE)
        {
            m_modifier.periodictime = 3000;
        }
        else
        {
            m_modifier.periodictime = 2000;
        }
    }

    m_periodicTimer = 5000;

    if (IsPlayer(GetTarget()) && m_modifier.m_miscvalue == POWER_MANA)
    {
        ((Player*)GetTarget())->Sheet().ManaRegen();
    }

    m_isPeriodic = apply;
}

void Aura::HandleModPowerRegenPCT(bool , bool Real)
{

    if (!Real)
    {
        return;
    }

    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    if (m_modifier.m_miscvalue == POWER_MANA)
    {
        ((Player*)GetTarget())->Sheet().ManaRegen();
    }
}

void Aura::HandleAuraModIncreaseHealth(bool apply, bool Real)
{
    Unit* target = GetTarget();

    switch (GetId())
    {

        case 12976:
        {
            if (Real)
            {
                if (apply)
                {
                    stats::Apply(*target, UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_amount), apply);
                    target->ModifyHealth(m_modifier.m_amount);
                }
                else
                {
                    if (int32(target->GetHealth()) > m_modifier.m_amount)
                    {
                        target->ModifyHealth(-m_modifier.m_amount);
                    }
                    else if (int32(target->GetHealth()) > 0)
                    {
                        target->SetHealth(1);
                    }
                    stats::Apply(*target, UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_amount), apply);
                }
            }
            return;
        }

        case 1178:
        case 9635:
        {
            if (Real)
            {
                float pct = target->GetHealthPercent();
                stats::Apply(*target, UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_amount), apply);
                target->SetHealthPercent(pct);
            }
            return;
        }

        default:
            stats::Apply(*target, UNIT_MOD_HEALTH, TOTAL_VALUE, float(m_modifier.m_amount), apply);
    }
}

void Aura::HandleAuraModIncreaseEnergy(bool apply, bool )
{
    Unit* target = GetTarget();
    Powers powerType = target->GetPowerType();
    if (int32(powerType) != m_modifier.m_miscvalue)
    {
        return;
    }

    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + powerType);

    stats::Apply(*target, unitMod, TOTAL_VALUE, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModIncreaseEnergyPercent(bool apply, bool )
{
    Powers powerType = GetTarget()->GetPowerType();
    if (int32(powerType) != m_modifier.m_miscvalue)
    {
        return;
    }

    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + powerType);

    stats::Apply(*GetTarget(), unitMod, TOTAL_PCT, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModIncreaseHealthPercent(bool apply, bool )
{
    stats::Apply(*GetTarget(), UNIT_MOD_HEALTH, TOTAL_PCT, float(m_modifier.m_amount), apply);
}

void Aura::HandleAuraModParryPercent(bool , bool )
{
    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    ((Player*)GetTarget())->Sheet().Parry();
}

void Aura::HandleAuraModDodgePercent(bool , bool )
{
    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    ((Player*)GetTarget())->Sheet().Dodge();

}

void Aura::HandleAuraModRegenInterrupt(bool , bool Real)
{

    if (!Real)
    {
        return;
    }

    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    ((Player*)GetTarget())->Sheet().ManaRegen();
}

void Aura::HandleAuraModCritPercent(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (!IsPlayer(target))
    {
        return;
    }

    if (Real)
    {
        for (int i = 0; i < MAX_ATTACK; ++i)
        {
            if (Item* pItem = ((Player*)target)->GetWeaponForAttack(WeaponAttackType(i), true, false))
            {
                ((Player*)target)->_ApplyWeaponDependentAuraCritMod(pItem, WeaponAttackType(i), this, apply);
            }
        }
    }

    if (GetSpellProto()->EquippedItemClass == -1)
    {
        ((Player*)target)->HandleBaseModValue(CRIT_PERCENTAGE,         FLAT_MOD, float(m_modifier.m_amount), apply);
        ((Player*)target)->HandleBaseModValue(OFFHAND_CRIT_PERCENTAGE, FLAT_MOD, float(m_modifier.m_amount), apply);
        ((Player*)target)->HandleBaseModValue(RANGED_CRIT_PERCENTAGE,  FLAT_MOD, float(m_modifier.m_amount), apply);
    }
    else
    {

    }
}

void Aura::HandleModHitChance(bool apply, bool )
{
    Unit* target = GetTarget();

    if (GetSpellProto()->EquippedItemSubclass & UI64LIT(0x0004000C))
    {
        target->m_modRangedHitChance += apply ? m_modifier.m_amount : (-m_modifier.m_amount);
    }
    else if (GetSpellProto()->EquippedItemClass == -1)
    {
        target->m_modMeleeHitChance += apply ? m_modifier.m_amount : (-m_modifier.m_amount);
        target->m_modRangedHitChance += apply ? m_modifier.m_amount : (-m_modifier.m_amount);
    }
    else
    {
        target->m_modMeleeHitChance += apply ? m_modifier.m_amount : (-m_modifier.m_amount);
    }
}

void Aura::HandleModSpellHitChance(bool apply, bool )
{
    GetTarget()->m_modSpellHitChance += apply ? m_modifier.m_amount : (-m_modifier.m_amount);
}

void Aura::HandleModSpellCritChance(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    if (IsPlayer(GetTarget()))
    {
        ((Player*)GetTarget())->Sheet().AllSpellCrits();
    }
    else
    {
        GetTarget()->m_baseSpellCritChance += apply ? m_modifier.m_amount : (-m_modifier.m_amount);
    }
}

void Aura::HandleModSpellCritChanceShool(bool , bool Real)
{

    if (!Real)
    {
        return;
    }

    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    for (int school = SPELL_SCHOOL_NORMAL; school < MAX_SPELL_SCHOOL; ++school)
    {
        if (m_modifier.m_miscvalue & (1 << school))
        {
            ((Player*)GetTarget())->Sheet().SpellCrit(school);
        }
    }
}

void Aura::HandleModCastingSpeed(bool apply, bool )
{
    if (apply)
    {
        if (Unit* caster = GetCaster())
        {
            if (Player* modOwner = caster->GetSpellModOwner())
            {
                modOwner->SpellMods().Apply(GetSpellProto()->ID, SPELLMOD_HASTE, m_modifier.m_amount);
            }
        }
    }

    GetTarget()->ApplyCastTimePercentMod(m_modifier.m_amount, apply);
}

void Aura::HandleModAttackSpeed(bool apply, bool )
{
    if (apply)
    {
        if (Unit* caster = GetCaster())
        {
            if (Player* modOwner = caster->GetSpellModOwner())
            {
                modOwner->SpellMods().Apply(GetSpellProto()->ID, SPELLMOD_HASTE, m_modifier.m_amount);
            }
        }
    }

    GetTarget()->ApplyAttackTimePercentMod(BASE_ATTACK, m_modifier.m_amount, apply);
}

void Aura::HandleModMeleeSpeedPct(bool apply, bool )
{
    if (apply)
    {
        if (Unit* caster = GetCaster())
        {
            if (Player* modOwner = caster->GetSpellModOwner())
            {
                modOwner->SpellMods().Apply(GetSpellProto()->ID, SPELLMOD_HASTE, m_modifier.m_amount);
            }
        }
    }

    Unit* target = GetTarget();
    target->ApplyAttackTimePercentMod(BASE_ATTACK, m_modifier.m_amount, apply);
    target->ApplyAttackTimePercentMod(OFF_ATTACK, m_modifier.m_amount, apply);
}

void Aura::HandleAuraModRangedHaste(bool apply, bool )
{
    if (apply)
    {
        if (Unit* caster = GetCaster())
        {
            if (Player* modOwner = caster->GetSpellModOwner())
            {
                modOwner->SpellMods().Apply(GetSpellProto()->ID, SPELLMOD_HASTE, m_modifier.m_amount);
            }
        }
    }

    GetTarget()->ApplyAttackTimePercentMod(RANGED_ATTACK, m_modifier.m_amount, apply);
}

void Aura::HandleRangedAmmoHaste(bool apply, bool )
{
    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    Item* ranged_weapon = static_cast<Player*>(GetTarget())->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (!ranged_weapon || ranged_weapon->GetProto()->AmmoType == 0)
    {
        return;
    }

    if (apply)
    {
        if (Unit* caster = GetCaster())
        {
            if (Player* modOwner = caster->GetSpellModOwner())
            {
                modOwner->SpellMods().Apply(GetSpellProto()->ID, SPELLMOD_HASTE, m_modifier.m_amount);
            }
        }
    }

    GetTarget()->ApplyAttackTimePercentMod(RANGED_ATTACK, m_modifier.m_amount, apply);
}

void Aura::HandleAuraModAttackPower(bool apply, bool )
{
    if (apply)
    {
        if (Unit* caster = GetCaster())
        {
            if (Player* modOwner = caster->GetSpellModOwner())
            {
                modOwner->SpellMods().Apply(GetSpellProto()->ID, SPELLMOD_ATTACK_POWER, m_modifier.m_amount);
            }
        }
    }

    stats::Apply(*GetTarget(), UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, m_modifier.m_amount, apply);
}

void Aura::HandleAuraModRangedAttackPower(bool apply, bool )
{
    if ((GetTarget()->getClassMask() & CLASSMASK_WAND_USERS) != 0)
    {
        return;
    }

    if (apply)
    {
        if (Unit* caster = GetCaster())
        {
            if (Player* modOwner = caster->GetSpellModOwner())
            {
                modOwner->SpellMods().Apply(GetSpellProto()->ID, SPELLMOD_ATTACK_POWER, m_modifier.m_amount);
            }
        }
    }

    stats::Apply(*GetTarget(), UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, m_modifier.m_amount, apply);
}

void Aura::HandleAuraModAttackPowerPercent(bool apply, bool )
{
    if (apply)
    {
        if (Unit* caster = GetCaster())
        {
            if (Player* modOwner = caster->GetSpellModOwner())
            {
                modOwner->SpellMods().Apply(GetSpellProto()->ID, SPELLMOD_ATTACK_POWER, m_modifier.m_amount);
            }
        }
    }

    stats::Apply(*GetTarget(), UNIT_MOD_ATTACK_POWER, TOTAL_PCT, m_modifier.m_amount, apply);
}

void Aura::HandleAuraModRangedAttackPowerPercent(bool apply, bool )
{
    if ((GetTarget()->getClassMask() & CLASSMASK_WAND_USERS) != 0)
    {
        return;
    }

    float amount = m_modifier.m_amount;

    if (Unit* caster = GetCaster())
    {
        if (Player* modOwner = caster->GetSpellModOwner())
        {
            modOwner->SpellMods().Apply(GetSpellProto()->ID, SPELLMOD_ATTACK_POWER, amount);
        }
    }

    stats::Apply(*GetTarget(), UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_PCT, amount, apply);
}

void Aura::HandleModDamageDone(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (Real &&IsPlayer(target))
    {
        for (int i = 0; i < MAX_ATTACK; ++i)
        {
            if (Item* pItem = ((Player*)target)->GetWeaponForAttack(WeaponAttackType(i), true, false))
            {
                ((Player*)target)->_ApplyWeaponDependentAuraDamageMod(pItem, WeaponAttackType(i), this, apply);
            }
        }
    }

    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL) != 0)
    {

        if (GetSpellProto()->EquippedItemClass == -1 || !IsPlayer(target))
        {
            stats::Apply(*target, UNIT_MOD_DAMAGE_MAINHAND, TOTAL_VALUE, float(m_modifier.m_amount), apply);
            stats::Apply(*target, UNIT_MOD_DAMAGE_OFFHAND, TOTAL_VALUE, float(m_modifier.m_amount), apply);
            stats::Apply(*target, UNIT_MOD_DAMAGE_RANGED, TOTAL_VALUE, float(m_modifier.m_amount), apply);
        }
        else
        {

        }

        if (IsPlayer(target))
        {
            if (m_positive)
            {
                target->ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS, m_modifier.m_amount, apply);
            }
            else
            {
                target->ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG, m_modifier.m_amount, apply);
            }
        }
    }

    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_MAGIC) == 0)
    {
        return;
    }

    if (GetSpellProto()->EquippedItemClass != -1 || GetSpellProto()->EquippedItemInvTypes != 0)
    {

        return;
    }

    if (IsPlayer(target))
    {
        if (m_positive)
        {
            for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
            {
                if ((m_modifier.m_miscvalue & (1 << i)) != 0)
                {
                    target->ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, m_modifier.m_amount, apply);
                }
            }
        }
        else
        {
            for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
            {
                if ((m_modifier.m_miscvalue & (1 << i)) != 0)
                {
                    target->ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i, m_modifier.m_amount, apply);
                }
            }
        }
        Pet* pet = target->GetPet();
        if (pet)
        {
            pet->Sheet().AttackPower(false);
        }
    }
}

void Aura::HandleModDamagePercentDone(bool apply, bool Real)
{
    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "AURA MOD DAMAGE type:%u negative:%u", m_modifier.m_miscvalue, m_positive ? 0 : 1);
    Unit* target = GetTarget();

    if (Real &&IsPlayer(target))
    {
        for (int i = 0; i < MAX_ATTACK; ++i)
        {
            if (Item* pItem = ((Player*)target)->GetWeaponForAttack(WeaponAttackType(i), true, false))
            {
                ((Player*)target)->_ApplyWeaponDependentAuraDamageMod(pItem, WeaponAttackType(i), this, apply);
            }
        }
    }

    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_NORMAL) != 0)
    {

        if (GetSpellProto()->EquippedItemClass == -1 || !IsPlayer(target))
        {
            stats::Apply(*target, UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, float(m_modifier.m_amount), apply);
            stats::Apply(*target, UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT, float(m_modifier.m_amount), apply);
            stats::Apply(*target, UNIT_MOD_DAMAGE_RANGED, TOTAL_PCT, float(m_modifier.m_amount), apply);
        }
        else
        {

        }

        if (Player* player = static_cast<Player*>(target))
        {
            player->ApplyDamageDonePercent(SPELL_SCHOOL_NORMAL, m_modifier.m_amount / 100.0f, apply);
        }
    }

    if ((m_modifier.m_miscvalue & SPELL_SCHOOL_MASK_MAGIC) == 0)
    {
        return;
    }

    if (GetSpellProto()->EquippedItemClass != -1 || GetSpellProto()->EquippedItemInvTypes != 0)
    {

        return;
    }

    if (Player* player = static_cast<Player*>(target))
    {
        for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        {
            player->ApplyDamageDonePercent(i, m_modifier.m_amount / 100.0f, apply);
        }
    }
}

void Aura::HandleModOffhandDamagePercent(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "AURA MOD OFFHAND DAMAGE");

    stats::Apply(*GetTarget(), UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT, float(m_modifier.m_amount), apply);
}

void Aura::HandleModPowerCostPCT(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    float amount = m_modifier.m_amount / 100.0f;
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        if (m_modifier.m_miscvalue & (1 << i))
        {
            GetTarget()->ApplyPowerCostMultiplier(i, amount, apply);
        }
    }
}

void Aura::HandleModPowerCost(bool apply, bool Real)
{

    if (!Real)
    {
        return;
    }

    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        if (m_modifier.m_miscvalue & (1 << i))
        {
            GetTarget()->ApplyModInt32Value(UNIT_FIELD_POWER_COST_MODIFIER + i, m_modifier.m_amount, apply);
        }
    }
}
