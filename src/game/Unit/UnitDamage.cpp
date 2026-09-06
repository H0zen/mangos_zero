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
#include "Combat/Mitigate.h"
#include "Combat/Reading.h"
#include "Combat/Shields.h"
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
#include "MapManager.h"
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

/**
 * @brief Reduces physical damage by the victim's effective armor.
 *
 * @param pVictim The victim whose armor is used.
 * @param damage The incoming physical damage.
 * @return The reduced damage amount.
 */
uint32 Unit::CalcArmorReducedDamage(Unit* pVictim, const uint32 damage)
{
    float armor = (float)pVictim->GetArmor();

    // Ignore enemy armor by SPELL_AURA_MOD_TARGET_RESISTANCE aura
    armor += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_TARGET_RESISTANCE, SPELL_SCHOOL_MASK_NORMAL);

    if (armor < 0.0f)
    {
        armor = 0.0f;
    }

    float levelModifier = (float)getLevel();

    float tmpvalue = 0.1f * armor / (8.5f * levelModifier + 40);
    tmpvalue = tmpvalue / (1.0f + tmpvalue);

    if (tmpvalue < 0.0f)
    {
        tmpvalue = 0.0f;
    }
    if (tmpvalue > 0.75f)
    {
        tmpvalue = 0.75f;
    }

    uint32 newdamage = uint32(damage - (damage * tmpvalue));

    return (newdamage > 1) ? newdamage : 1;
}

/**
 * @brief Calculates resistance, absorbs, and split-damage effects for incoming damage.
 *
 * @param pCaster The attacking caster.
 * @param schoolMask The incoming damage school mask.
 * @param damagetype The damage effect type.
 * @param damage The incoming damage amount.
 * @param absorb Output absorbed amount.
 * @param resist Output resisted amount.
 * @param canReflect Unused reflection flag placeholder.
 */
void Unit::CalculateDamageAbsorbAndResist(Unit* pCaster, SpellSchoolMask schoolMask, DamageEffectType damagetype, const uint32 damage, uint32* absorb, uint32* resist, bool /*canReflect*/)
{
    if (!pCaster || !IsAlive() || !damage)
    {
        return;
    }

    // Magic damage, check for resists
    if ((schoolMask & SPELL_SCHOOL_MASK_NORMAL) == 0)
    {
        // Get base victim resistance for school
        float tmpvalue2 = (float)GetResistance(GetFirstSchoolInMask(schoolMask));
        // Ignore resistance by self SPELL_AURA_MOD_TARGET_RESISTANCE aura
        tmpvalue2 += (float)pCaster->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_TARGET_RESISTANCE, schoolMask);

        tmpvalue2 *= (float)(0.15f / getLevel());
        if (tmpvalue2 < 0.0f)
        {
            tmpvalue2 = 0.0f;
        }
        if (tmpvalue2 > 0.75f)
        {
            tmpvalue2 = 0.75f;
        }
        uint32 ran = urand(0, 100);
        float faq[4] = {24.0f, 6.0f, 4.0f, 6.0f};
        uint8 m = 0;
        float Binom = 0.0f;
        for (uint8 i = 0; i < 4; ++i)
        {
            Binom += 2400 * (powf(tmpvalue2, float(i)) * powf((1 - tmpvalue2), float(4 - i))) / faq[i];
            if (ran > Binom)
            {
                ++m;
            }
            else
            {
                break;
            }
        }
        if (damagetype == DOT && m == 4)
        {
            *resist += uint32(damage - 1);
        }
        else
        {
            *resist += uint32(damage * m / 4);
        }
        if (*resist > damage)
        {
            *resist = damage;
        }
    }
    else
    {
        *resist = 0;
    }

    int32 remaining = damage - int32(*resist);

    // Shields and splitting are decided by the combat core, which reads and
    // decides but writes nothing, and then spent here. The split plan is handed
    // to the caster rather than dealt: it is owed by this blow and is delivered
    // once the blow itself has been applied, in DealDamage. Dealing it here
    // would let the recipient die, proc and pull threat before the hit that
    // split the damage had landed at all.
    // The blow has one school. Spell data stores a mask, so it narrows here,
    // once, rather than inside the core.
    const combat::School school = combat::FirstSchoolIn(schoolMask);

    const combat::Defences defences =
        combat::ReadDefences(*this, *pCaster, school);

    combat::Outcome plan;
    combat::Mitigate(remaining, school, defences, pCaster == this, plan);

    combat::ConsumeShields(*this, plan);

    if (!plan.splits.empty())
    {
        pCaster->OweSplits(plan.splits, schoolMask);
    }

    *absorb = uint32(plan.absorbed);
    if (remaining < 0)
    {
        remaining = 0;
    }
}

/**
 * @brief Calculates block, absorb, and resist results for spell-based damage.
 *
 * @param pCaster The attacking caster.
 * @param damageInfo The mutable spell damage information.
 * @param spellProto The spell entry causing damage.
 * @param attType The associated attack type.
 */
void Unit::CalculateAbsorbResistBlock(Unit* pCaster, SpellNonMeleeDamage* damageInfo, SpellEntry const* spellProto, WeaponAttackType attType)
{
    bool blocked = false;
    // Get blocked status
    switch (spellProto->DefenseType)
    {
        // Melee and Ranged Spells
        case SPELL_DAMAGE_CLASS_RANGED:
        case SPELL_DAMAGE_CLASS_MELEE:
            blocked = IsSpellBlocked(pCaster, spellProto, attType);
            break;
        default:
            break;
    }

    if (blocked)
    {
        damageInfo->blocked = Sheet().ShieldBlock();
        if (damageInfo->damage < damageInfo->blocked)
        {
            damageInfo->blocked = damageInfo->damage;
        }
        damageInfo->damage -= damageInfo->blocked;
    }

    CalculateDamageAbsorbAndResist(pCaster, GetSpellSchoolMask(spellProto), SPELL_DIRECT_DAMAGE, damageInfo->damage, &damageInfo->absorb, &damageInfo->resist, !spellProto->HasAttribute(SPELL_ATTR_EX2_IGNORE_LOS));
    damageInfo->damage -= damageInfo->absorb + damageInfo->resist;
}
