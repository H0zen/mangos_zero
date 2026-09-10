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

#include "Utilities/MathDefines.h"
#include "Unit.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "Spell.h"
#include "Group.h"
#include "SpellAuras.h"
#include "CreatureAI.h"
#include "TemporarySummon.h"
#include "Formulas.h"
#include "Pet.h"
#include "Util.h"
#include "Totem.h"
#include "BattleGround/BattleGround.h"
#include "InstanceData.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "MapPersistentStateMgr.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "MovementGenerator.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "Movement/Spline/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"
#include <math.h>
#include <stdarg.h>
#include "Cast/Recipe/RecipeBook.h"

void Unit::AttackerStateUpdate(Unit* pVictim, WeaponAttackType attType, bool extra)
{
    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT) || HasUnitFlag(UNIT_FLAG_PACIFIED))
    {
        return;
    }

    if (!pVictim->IsAlive())
    {
        return;
    }

    if (IsNonMeleeSpellCasted(false))
    {
        return;
    }

    uint32 hitInfo;
    if (attType == BASE_ATTACK)
    {
        hitInfo = HITINFO_NORMALSWING2;
    }
    else if (attType == OFF_ATTACK)
    {
        hitInfo = HITINFO_LEFTSWING;
    }
    else
    {
        return;
    }

    uint32 extraAttacks = m_extraAttacks;

    if (attType == BASE_ATTACK && m_currentSpells[CURRENT_MELEE_SPELL])
    {
        m_currentSpells[CURRENT_MELEE_SPELL]->cast();

        if (!extra && extraAttacks)
        {
            HandleProcExtraAttackFor(pVictim);
        }

        return;
    }

    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_MELEE_ATTACK);

    CalcDamageInfo damageInfo;
    CalculateMeleeDamage(pVictim, &damageInfo, attType);

    DealDamageMods(pVictim, damageInfo.damage, &damageInfo.absorb);
    SendAttackStateUpdate(&damageInfo);
    ProcDamageAndSpell(damageInfo.target, damageInfo.procAttacker, damageInfo.procVictim, damageInfo.procEx, damageInfo.damage, damageInfo.attackType);
    DealMeleeDamage(&damageInfo, true);

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "AttackerStateUpdate: %s attacked %s for %u dmg, absorbed %u, blocked %u, resisted %u.",
        GetGuidStr().c_str(), pVictim->GetGuidStr().c_str(), damageInfo.damage, damageInfo.absorb, damageInfo.blocked_amount, damageInfo.resist);

    if (Unit* owner = GetOwner())
    {
        owner->AddThreat(pVictim);
        owner->SetInCombatWith(pVictim);
        pVictim->SetInCombatWith(owner);
    }

    pVictim->AttackedBy(this);

    if (!extra && extraAttacks)
    {
        HandleProcExtraAttackFor(pVictim);
    }
}

MeleeHitOutcome Unit::RollMeleeOutcomeAgainst(const Unit* pVictim, WeaponAttackType attType) const
{

    float miss_chance = MeleeMissChanceCalc(pVictim, attType);

    float crit_chance = GetUnitCriticalChance(attType, pVictim);

    float dodge_chance = pVictim->GetUnitDodgeChance();
    float block_chance = pVictim->GetUnitBlockChance();
    float parry_chance = pVictim->GetUnitParryChance();

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "MELEE OUTCOME: miss %f crit %f dodge %f parry %f block %f", miss_chance, crit_chance, dodge_chance, parry_chance, block_chance);

    return RollMeleeOutcomeAgainst(pVictim, attType, int32(crit_chance * 100), int32(miss_chance * 100), int32(dodge_chance * 100), int32(parry_chance * 100), int32(block_chance * 100), false);
}

MeleeHitOutcome Unit::RollMeleeOutcomeAgainst(const Unit* pVictim, WeaponAttackType attType, int32 crit_chance, int32 miss_chance, int32 dodge_chance, int32 parry_chance, int32 block_chance, bool SpellCasted) const
{
    if (IsCreature(pVictim) && ((Creature*)pVictim)->IsInEvadeMode())
    {
        return MELEE_HIT_EVADE;
    }

    int32 attackerMaxSkillValueForLevel = GetMaxSkillValueForLevel(pVictim);
    int32 victimMaxSkillValueForLevel = pVictim->GetMaxSkillValueForLevel(this);

    int32 attackerWeaponSkill = GetWeaponSkillValue(attType, pVictim);
    int32 victimDefenseSkill = pVictim->GetDefenseSkillValue(this);

    int32 skillBonus  = 4 * (attackerWeaponSkill - victimMaxSkillValueForLevel);
    int32 sum = 0;
    int32 roll = urand(0, 10000);
    int32 tmp = miss_chance;

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: skill bonus of %d for attacker", skillBonus);
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: rolled %d, miss %d, dodge %d, parry %d, block %d, crit %d",
        roll, miss_chance, dodge_chance, parry_chance, block_chance, crit_chance);

    if (tmp > 0 && roll < (sum += tmp))
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: MISS");
        return MELEE_HIT_MISS;
    }

    if (IsPlayer(pVictim) && crit_chance > 0 && !pVictim->IsStandState())
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: CRIT (sitting victim)");
        return MELEE_HIT_CRIT;
    }

    bool from_behind = !pVictim->Where().HasInArc(this->Where(), M_PI_F);

    if (from_behind)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: attack came from behind.");
    }

    if (!IsPlayer(pVictim) || !from_behind)
    {
        tmp = dodge_chance;
        if ((tmp > 0) &&
            ((tmp -= skillBonus) > 0) &&
            roll < (sum += tmp))
        {
            DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: DODGE <%d, %d)", sum - tmp, sum);
            return MELEE_HIT_DODGE;
        }
    }

    if (!from_behind)
    {
        if (parry_chance > 0 && (IsPlayer(pVictim) || !(((Creature*)pVictim)->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_PARRY)))
        {
            parry_chance -= skillBonus;

            if (parry_chance > 0 &&
                (roll < (sum += parry_chance)))
            {
                DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: PARRY <%d, %d)", sum - parry_chance, sum);
                return MELEE_HIT_PARRY;
            }
        }
    }

    if (attType != RANGED_ATTACK && !SpellCasted &&
        (IsPlayer(this) || ((Creature*)this)->IsPet()) &&
        !IsPlayer(pVictim) && !((Creature*)pVictim)->IsPet() &&
        getLevel() < pVictim->GetLevelForTarget(this))
    {

        int32 skill = attackerWeaponSkill;
        int32 maxskill = attackerMaxSkillValueForLevel;
        skill = (skill > maxskill) ? maxskill : skill;

        tmp = (10 + 2 * (victimDefenseSkill - skill)) * 100;
        tmp = tmp > 4000 ? 4000 : tmp;
        if (roll < (sum += tmp))
        {
            DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: GLANCING <%d, %d)", sum - 4000, sum);
            return MELEE_HIT_GLANCING;
        }
    }

    if (!from_behind)
    {
        if (IsPlayer(pVictim) || !(((Creature*)pVictim)->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_BLOCK))
        {
            tmp = block_chance;
            if ((tmp > 0) &&
                ((tmp -= skillBonus) > 0) &&
                (roll < (sum += tmp)))
            {

                tmp = crit_chance;
                if (IsPlayer(this) && SpellCasted && tmp > 0)
                {
                    if (roll_chance_i(tmp / 100))
                    {
                        DEBUG_LOG("RollMeleeOutcomeAgainst: BLOCKED CRIT");
                        return MELEE_HIT_BLOCK_CRIT;
                    }
                }
                DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: BLOCK <%d, %d)", sum - tmp, sum);
                return MELEE_HIT_BLOCK;
            }
        }
    }

    tmp = crit_chance;

    if (tmp > 0 && roll < (sum += tmp))
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: CRIT <%d, %d)", sum - tmp, sum);
        return MELEE_HIT_CRIT;
    }

    if ((!IsPlayer(this) && !((Creature*)this)->IsPet()) &&
        !(((Creature*)this)->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_CRUSH) &&
        !SpellCasted )
    {

        tmp = victimDefenseSkill;
        int32 tmpmax = victimMaxSkillValueForLevel;

        tmp = tmp > tmpmax ? tmpmax : tmp;

        tmp = attackerMaxSkillValueForLevel - tmp;
        if (tmp >= 15)
        {

            tmp = tmp * 200 - 1500;
            if (roll < (sum += tmp))
            {
                DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: CRUSHING <%d, %d)", sum - tmp, sum);
                return MELEE_HIT_CRUSHING;
            }
        }
    }

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: NORMAL");
    return MELEE_HIT_NORMAL;
}

uint32 Unit::CalculateDamage(WeaponAttackType attType, bool normalized)
{
    float min_damage, max_damage;

    if (normalized && IsPlayer(this))
    {
        ((Player*)this)->Sheet().SwingRange(attType, normalized, min_damage, max_damage);
    }
    else
    {
        switch (attType)
        {
            case RANGED_ATTACK:
                min_damage = GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE);
                max_damage = GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE);
                break;
            case BASE_ATTACK:
                min_damage = GetShownDamage(false, false);
                max_damage = GetShownDamage(false, true);
                break;
            case OFF_ATTACK:
                min_damage = GetShownDamage(true, false);
                max_damage = GetShownDamage(true, true);
                break;

            default:
                min_damage = 0.0f;
                max_damage = 0.0f;
                break;
        }
    }

    if (min_damage > max_damage)
    {
        std::swap(min_damage, max_damage);
    }

    if (max_damage == 0.0f)
    {
        max_damage = 5.0f;
    }

    if (min_damage < 0.0f)
    {
        min_damage = 0.0f;
    }

    if (max_damage < min_damage)
    {
        max_damage = min_damage;
    }

    return urand((uint32)min_damage, (uint32)max_damage);
}

float Unit::CalculateLevelPenalty(SpellEntry const* spellProto) const
{
    uint32 spellLevel = spellProto->SpellLevel;
    if (spellLevel <= 0)
    {
        return 1.0f;
    }

    float LvlPenalty = 0.0f;

    if (spellLevel < 20)
    {
        LvlPenalty = (20.0f - spellLevel) * 3.75f;
    }

    return (100.0f - LvlPenalty) / 100.0f;
}

void Unit::SendMeleeAttackStart(Unit* pVictim)
{
    WorldPacket data(SMSG_ATTACKSTART, 8 + 8);
    data << GetObjectGuid();
    data << pVictim->GetObjectGuid();

    Deliver(Audience::Around(*this).AndSubject(), &data);
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Sent SMSG_ATTACKSTART: %s -> %s", GetGuidStr().c_str(), pVictim->GetGuidStr().c_str());
}

void Unit::SendMeleeAttackStop(Unit* victim)
{
    if (!victim)
    {
        return;
    }

    WorldPacket data(SMSG_ATTACKSTOP, (8 + 8 + 4));
    data << GetPackGUID();
    data << victim->GetPackGUID();
    data << uint32(0);
    Deliver(Audience::Around(*this).AndSubject(), &data);
    DETAIL_FILTER_LOG(LOG_FILTER_COMBAT, "%s %u stopped attacking %s %u", (IsPlayer(this) ? "player" : "creature"), GetGUIDLow(), (IsPlayer(victim) ? "player" : "creature"), victim->GetGUIDLow());

}

bool Unit::IsSpellBlocked(Unit* pCaster, SpellEntry const* spellEntry, WeaponAttackType attackType)
{
    if (!Where().HasInArc(pCaster->Where(), M_PI_F))
    {
        return false;
    }

    if (spellEntry)
    {

        if (cast::RecipeOf(*spellEntry).Says().cannotBeAvoided)
        {
            return false;
        }
    }

    if (IsCreature(this))
    {
        if (((Creature*)this)->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_BLOCK)
        {
            return false;
        }
    }

    float blockChance = GetUnitBlockChance();
    blockChance += (int32(pCaster->GetWeaponSkillValue(attackType)) - int32(GetMaxSkillValueForLevel())) * 0.04f;

    return roll_chance_f(blockChance);
}

float Unit::MeleeSpellMissChance(Unit* pVictim, WeaponAttackType attType, int32 skillDiff, SpellEntry const* spell)
{

    float hitChance = 0.0f;

    if (IsPlayer(pVictim))
    {
        hitChance = 95.0f + skillDiff * 0.04f;
    }
    else if (skillDiff < -10)
    {
        hitChance = 93.0f + (skillDiff + 10) * 0.4f;
    }
    else
    {
        hitChance = 95.0f + skillDiff * 0.1f;
    }

    if (attType == RANGED_ATTACK)
    {
        hitChance += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE);
    }
    else
    {
        hitChance += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE);
    }

    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(spell->ID, SPELLMOD_RESIST_MISS_CHANCE, hitChance);
    }

    float missChance = 100.0f - hitChance;

    if (attType == RANGED_ATTACK)
    {
        missChance -= m_modRangedHitChance;
    }
    else
    {
        missChance -= m_modMeleeHitChance;
    }

    if (missChance < 0.0f)
    {
        return 0.0f;
    }
    if (missChance > 60.0f)
    {
        return 60.0f;
    }
    return missChance;
}

SpellMissInfo Unit::MeleeSpellHitResult(Unit* pVictim, SpellEntry const* spell)
{
    WeaponAttackType attType = BASE_ATTACK;

    if (spell->DefenseType == SPELL_DAMAGE_CLASS_RANGED)
    {
        attType = RANGED_ATTACK;
    }

    int32 attackerWeaponSkill = (spell->EquippedItemClass == ITEM_CLASS_WEAPON) ? int32(GetWeaponSkillValue(attType, pVictim)) : GetMaxSkillValueForLevel();
    int32 skillDiff = attackerWeaponSkill - int32(pVictim->GetMaxSkillValueForLevel(this));
    int32 fullSkillDiff = attackerWeaponSkill - int32(pVictim->GetDefenseSkillValue(this));

    uint32 roll = urand(0, 10000);

    uint32 missChance = uint32(MeleeSpellMissChance(pVictim, attType, fullSkillDiff, spell) * 100.0f);

    uint32 tmp = cast::RecipeOf(*spell).Says().cannotMiss ? 0 : missChance;
    if (roll < tmp)
    {
        return SPELL_MISS_MISS;
    }

    int32 resist_mech = 0;

    for (int eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
    {
        int32 effect_mech = GetEffectMechanic(spell, SpellEffectIndex(eff));
        if (effect_mech)
        {
            int32 temp = pVictim->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_MECHANIC_RESISTANCE, effect_mech);
            if (resist_mech < temp * 100)
            {
                resist_mech = temp * 100;
            }
        }
    }

    tmp += resist_mech;
    if (roll < tmp)
    {
        return SPELL_MISS_RESIST;
    }

    bool canDodge = true;
    bool canParry = true;

    if (cast::RecipeOf(*spell).Says().cannotBeAvoided)
    {
        return SPELL_MISS_NONE;
    }

    if (attType == RANGED_ATTACK)
    {
        return SPELL_MISS_NONE;
    }

    bool from_behind = !pVictim->Where().HasInArc(this->Where(), M_PI_F);

    if (from_behind)
    {

        if (IsPlayer(this) &&IsPlayer(pVictim))
        {
            canDodge = false;
        }

        canParry = false;
    }

    if (IsCreature(pVictim))
    {
        uint32 flagEx = ((Creature*)pVictim)->GetCreatureInfo()->ExtraFlags;
        if (flagEx & CREATURE_FLAG_EXTRA_NO_PARRY)
        {
            canParry = false;
        }
    }

    if (canDodge)
    {

        int32 dodgeChance = int32(pVictim->GetUnitDodgeChance() * 100.0f) - skillDiff * 4;

        if (dodgeChance < 0)
        {
            dodgeChance = 0;
        }

        tmp += dodgeChance;
        if (roll < tmp)
        {
            return SPELL_MISS_DODGE;
        }
    }

    if (canParry)
    {

        int32 parryChance = int32(pVictim->GetUnitParryChance() * 100.0f)  - skillDiff * 4;

        if (parryChance < 0)
        {
            parryChance = 0;
        }

        tmp += parryChance;
        if (roll < tmp)
        {
            return SPELL_MISS_PARRY;
        }
    }

    return SPELL_MISS_NONE;
}

SpellMissInfo Unit::MagicSpellHitResult(Unit* pVictim, SpellEntry const* spell)
{

    if (!pVictim->IsAlive())
    {
        return SPELL_MISS_NONE;
    }

    SpellSchoolMask schoolMask = GetSpellSchoolMask(spell);

    if (schoolMask == SPELL_SCHOOL_MASK_HOLY)
    {
        return SPELL_MISS_NONE;
    }

    int32 lchance =IsPlayer(pVictim) ? 7 : 11;
    int32 leveldif = int32(pVictim->GetLevelForTarget(this)) - int32(GetLevelForTarget(pVictim));

    int32 modHitChance;
    if (leveldif < 3)
    {
        modHitChance = 96 - leveldif;
    }
    else
    {
        modHitChance = 94 - (leveldif - 2) * lchance;
    }

    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(spell->ID, SPELLMOD_RESIST_MISS_CHANCE, modHitChance);
    }

    modHitChance += pVictim->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE, schoolMask);

    if (IsAreaOfEffectSpell(spell))
    {
        modHitChance -= pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_AOE_AVOIDANCE);
    }

    int32 resist_mech = 0;

    for (int eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
    {
        int32 effect_mech = GetEffectMechanic(spell, SpellEffectIndex(eff));
        if (effect_mech)
        {
            int32 temp = pVictim->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_MECHANIC_RESISTANCE, effect_mech);
            if (resist_mech < temp)
            {
                resist_mech = temp;
            }
        }
    }

    modHitChance -= resist_mech;

    modHitChance -= pVictim->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_DEBUFF_RESISTANCE, int32(spell->DispelType));

    int32 HitChance = modHitChance * 100;

    HitChance += int32(m_modSpellHitChance * 100.0f);

    if (HitChance <  100)
    {
        HitChance =  100;
    }
    if (HitChance > 9900)
    {
        HitChance = 9900;
    }

    int32 tmp = cast::RecipeOf(*spell).Says().cannotMiss ? 0 : (10000 - HitChance);

    int32 rand = irand(0, 10000);

    if (rand < tmp)
    {
        return SPELL_MISS_RESIST;
    }

    return SPELL_MISS_NONE;
}

SpellMissInfo Unit::SpellHitResult(Unit* pVictim, SpellEntry const* spell, bool CanReflect)
{
    SpellSchoolMask schoolMask = GetSpellSchoolMask(spell);

    bool wand = spell->ID == 5019;
    if (wand && !!(getClassMask() & CLASSMASK_WAND_USERS) && IsPlayer(this))
    {
        schoolMask = GetSchoolMask(GetWeaponDamageSchool(RANGED_ATTACK));
    }

    if (IsCreature(pVictim) && ((Creature*)pVictim)->IsInEvadeMode())
    {
        return SPELL_MISS_EVADE;
    }

    if (!wand && pVictim->IsImmuneToSpell(spell, this == pVictim) && !cast::RecipeOf(*spell).Says().ignoresInvulnerability)
    {
        return SPELL_MISS_IMMUNE;
    }

    if (cast::RecipeOf(*spell).IsPositive())
    {
        return SPELL_MISS_NONE;
    }

    if (pVictim->IsImmuneToDamage(schoolMask) && !cast::RecipeOf(*spell).Says().ignoresInvulnerability)
    {
        return SPELL_MISS_IMMUNE;
    }

    if (CanReflect)
    {
        int32 reflectchance = pVictim->GetTotalAuraModifier(SPELL_AURA_REFLECT_SPELLS);
        const auto mReflectSpellsSchool = pVictim->GetAurasByType(SPELL_AURA_REFLECT_SPELLS_SCHOOL);
        for (auto* aura : mReflectSpellsSchool)
        {
            if (aura->GetModifier()->m_miscvalue & schoolMask)
            {
                reflectchance += aura->GetModifier()->m_amount;
            }
        }

        if (reflectchance > 0 && roll_chance_i(reflectchance))
        {

            ProcDamageAndSpell(pVictim, PROC_FLAG_NONE, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT, PROC_EX_REFLECT, 1, BASE_ATTACK, spell);
            return SPELL_MISS_REFLECT;
        }
    }

    switch (spell->DefenseType)
    {
        case SPELL_DAMAGE_CLASS_NONE:
            return SPELL_MISS_NONE;
        case SPELL_DAMAGE_CLASS_MAGIC:
            return MagicSpellHitResult(pVictim, spell);
        case SPELL_DAMAGE_CLASS_MELEE:
        case SPELL_DAMAGE_CLASS_RANGED:
            return MeleeSpellHitResult(pVictim, spell);
    }
    return SPELL_MISS_NONE;
}

float Unit::MeleeMissChanceCalc(const Unit* pVictim, WeaponAttackType attType) const
{
    if (!pVictim)
    {
        return 0.0f;
    }

    float missChance = 5.0f;

    if (haveOffhandWeapon() && attType != RANGED_ATTACK)
    {
        bool isNormal = false;
        for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
        {
            if (m_currentSpells[i] && (GetSpellSchoolMask(m_currentSpells[i]->m_spellInfo) & SPELL_SCHOOL_MASK_NORMAL))
            {
                isNormal = true;
                break;
            }
        }
        if (!isNormal && !m_currentSpells[CURRENT_MELEE_SPELL])
        {
            missChance += 19.0f;
        }
    }

    int32 skillDiff = int32(GetWeaponSkillValue(attType, pVictim)) - int32(pVictim->GetDefenseSkillValue(this));

    if (IsPlayer(pVictim))
    {
        missChance -= skillDiff * 0.04f;
    }
    else if (skillDiff < -10)
    {
        missChance -= (skillDiff + 10) * 0.4f - 2.0f;
    }
    else
    {
        missChance -=  skillDiff * 0.1f;
    }

    if (attType == RANGED_ATTACK)
    {
        missChance -= m_modRangedHitChance;
    }
    else
    {
        missChance -= m_modMeleeHitChance;
    }

    if (attType == RANGED_ATTACK)
    {
        missChance -= pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE);
    }
    else
    {
        missChance -= pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE);
    }

    if (missChance < 0.0f)
    {
        return 0.0f;
    }
    if (missChance > 60.0f)
    {
        return 60.0f;
    }

    return missChance;
}

float Unit::GetUnitDodgeChance() const
{
    if (hasUnitState(UNIT_STAT_STUNNED))
    {
        return 0.0f;
    }
    if (IsPlayer(this))
    {
        return GetFloatValue(PLAYER_DODGE_PERCENTAGE);
    }
    else
    {
        if (((Creature const*)this)->IsTotem())
        {
            return 0.0f;
        }
        else
        {
            float dodge = 5.0f;
            dodge += GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT);
            return dodge > 0.0f ? dodge : 0.0f;
        }
    }
}

float Unit::GetUnitParryChance() const
{
    if (IsNonMeleeSpellCasted(false) || hasUnitState(UNIT_STAT_STUNNED))
    {
        return 0.0f;
    }

    float chance = 0.0f;

    if (IsPlayer(this))
    {
        Player const* player = (Player const*)this;
        if (player->Arms().CanParry())
        {
            Item* tmpitem = player->GetWeaponForAttack(BASE_ATTACK, true, true);
            if (!tmpitem)
            {
                tmpitem = player->GetWeaponForAttack(OFF_ATTACK, true, true);
            }

            if (tmpitem)
            {
                chance = GetFloatValue(PLAYER_PARRY_PERCENTAGE);
            }
        }
    }
    else if (IsCreature(this))
    {
        if (GetCreatureType() == CREATURE_TYPE_HUMANOID)
        {
            chance = 5.0f;
            chance += GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT);
        }
    }

    return chance > 0.0f ? chance : 0.0f;
}

float Unit::GetUnitBlockChance() const
{
    if (IsNonMeleeSpellCasted(false) || hasUnitState(UNIT_STAT_STUNNED))
    {
        return 0.0f;
    }

    if (IsPlayer(this))
    {
        Player const* player = (Player const*)this;
        if (player->Arms().CanBlock() && player->CanUseEquippedWeapon(OFF_ATTACK))
        {
            Item* tmpitem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
            if (tmpitem && !tmpitem->IsBroken() && tmpitem->GetProto()->Block)
            {
                return GetFloatValue(PLAYER_BLOCK_PERCENTAGE);
            }
        }

        return 0.0f;
    }
    else
    {
        if (((Creature const*)this)->IsTotem())
        {
            return 0.0f;
        }
        else
        {
            float block = 5.0f;
            block += GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT);
            return block > 0.0f ? block : 0.0f;
        }
    }
}

float Unit::GetUnitCriticalChance(WeaponAttackType attackType, const Unit* pVictim) const
{
    float crit;

    if (IsPlayer(this))
    {
        switch (attackType)
        {
            case OFF_ATTACK:
            case BASE_ATTACK:
                crit = GetFloatValue(PLAYER_CRIT_PERCENTAGE);
                break;
            case RANGED_ATTACK:
                crit = GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE);
                break;

            default:
                crit = 0.0f;
                break;
        }
    }
    else
    {
        crit = 5.0f;
        crit += GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PERCENT);
    }

    if (attackType == RANGED_ATTACK)
    {
        crit += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE);
    }
    else
    {
        crit += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE);
    }

    crit += (int32(GetMaxSkillValueForLevel(pVictim)) - int32(pVictim->GetDefenseSkillValue(this))) * 0.04f;

    if (crit < 0.0f)
    {
        crit = 0.0f;
    }
    return crit;
}
