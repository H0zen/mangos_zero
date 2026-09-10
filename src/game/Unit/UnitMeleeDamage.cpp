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

#include <set>
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
#include "CreatureLinkingMgr.h"
#include "GameTime.h"
#include "MovementGenerator.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "Movement/Spline/MoveSpline.h"

void Unit::CalculateMeleeDamage(Unit* pVictim, CalcDamageInfo* damageInfo, WeaponAttackType attackType )
{
    damageInfo->attacker         = this;
    damageInfo->target           = pVictim;
    damageInfo->damageSchoolMask = GetMeleeDamageSchoolMask();
    damageInfo->attackType       = attackType;
    damageInfo->damage           = 0;
    damageInfo->cleanDamage      = 0;
    damageInfo->absorb           = 0;
    damageInfo->resist           = 0;
    damageInfo->blocked_amount   = 0;

    damageInfo->TargetState      = VICTIMSTATE_UNAFFECTED;
    damageInfo->HitInfo          = HITINFO_NORMALSWING;
    damageInfo->procAttacker     = PROC_FLAG_NONE;
    damageInfo->procVictim       = PROC_FLAG_NONE;
    damageInfo->procEx           = PROC_EX_NONE;
    damageInfo->hitOutCome       = MELEE_HIT_EVADE;

    if (!pVictim)
    {
        return;
    }
    if (!this->IsAlive() || !pVictim->IsAlive())
    {
        return;
    }

    switch (attackType)
    {
        case BASE_ATTACK:
            damageInfo->procAttacker = PROC_FLAG_SUCCESSFUL_MELEE_HIT;
            damageInfo->procVictim   = PROC_FLAG_TAKEN_MELEE_HIT;
            damageInfo->HitInfo      = HITINFO_NORMALSWING2;
            break;
        case OFF_ATTACK:
            damageInfo->procAttacker = PROC_FLAG_SUCCESSFUL_MELEE_HIT | PROC_FLAG_SUCCESSFUL_OFFHAND_HIT;
            damageInfo->procVictim   = PROC_FLAG_TAKEN_MELEE_HIT;
            damageInfo->HitInfo = HITINFO_LEFTSWING;
            break;
        case RANGED_ATTACK:
            damageInfo->procAttacker = PROC_FLAG_SUCCESSFUL_RANGED_HIT;
            damageInfo->procVictim   = PROC_FLAG_TAKEN_RANGED_HIT;
            damageInfo->HitInfo = HITINFO_UNK3;
            break;
        default:
            break;
    }

    if (damageInfo->target->IsImmuneToDamage(damageInfo->damageSchoolMask))
    {
        damageInfo->HitInfo       |= HITINFO_NORMALSWING;
        damageInfo->TargetState    = VICTIMSTATE_IS_IMMUNE;

        damageInfo->procEx |= PROC_EX_IMMUNE;
        damageInfo->damage         = 0;
        damageInfo->cleanDamage    = 0;
        return;
    }
    uint32 damage = CalculateDamage(damageInfo->attackType, false);

    damage = MeleeDamageBonusDone(damageInfo->target, damage, damageInfo->attackType);
    damage = damageInfo->target->MeleeDamageBonusTaken(this, damage, damageInfo->attackType);

    if (damageInfo->damageSchoolMask < 2)
    {
        damageInfo->damage = CalcArmorReducedDamage(damageInfo->target, damage);
        damageInfo->cleanDamage += damage - damageInfo->damage;
    }
    else
    {
        damageInfo->damage = damage;
        damageInfo->cleanDamage += damage - damageInfo->damage;
    }
    damageInfo->hitOutCome = RollMeleeOutcomeAgainst(damageInfo->target, damageInfo->attackType);

    if (damageInfo->attackType == RANGED_ATTACK)
    {
        if (damageInfo->hitOutCome == MELEE_HIT_PARRY)
        {
            damageInfo->hitOutCome = MELEE_HIT_NORMAL;
        }
        if (damageInfo->hitOutCome == MELEE_HIT_DODGE)
        {
            damageInfo->hitOutCome = MELEE_HIT_MISS;
        }
    }

    switch (damageInfo->hitOutCome)
    {
        case MELEE_HIT_EVADE:
        {
            damageInfo->HitInfo    |= HITINFO_MISS | HITINFO_SWINGNOHITSOUND;
            damageInfo->TargetState = VICTIMSTATE_EVADES;

            damageInfo->procEx |= PROC_EX_EVADE;
            damageInfo->damage = 0;
            damageInfo->cleanDamage = 0;
            return;
        }
        case MELEE_HIT_MISS:
        {
            damageInfo->HitInfo    |= HITINFO_MISS;
            damageInfo->TargetState = VICTIMSTATE_UNAFFECTED;

            damageInfo->procEx |= PROC_EX_MISS;
            damageInfo->damage = 0;
            damageInfo->cleanDamage = 0;
            break;
        }
        case MELEE_HIT_NORMAL:
            damageInfo->TargetState = VICTIMSTATE_NORMAL;
            damageInfo->procEx |= PROC_EX_NORMAL_HIT;
            break;
        case MELEE_HIT_CRIT:
        {
            damageInfo->HitInfo     |= HITINFO_CRITICALHIT;
            damageInfo->TargetState  = VICTIMSTATE_NORMAL;

            damageInfo->procEx |= PROC_EX_CRITICAL_HIT;

            damageInfo->damage += damageInfo->damage;
            int32 mod = 0;

            uint32 crTypeMask = damageInfo->target->GetCreatureTypeMask();

            mod += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_CRIT_PERCENT_VERSUS, crTypeMask);
            if (mod != 0)
            {
                damageInfo->damage = int32((damageInfo->damage) * float((100.0f + mod) / 100.0f));
            }
            break;
        }
        case MELEE_HIT_PARRY:
            damageInfo->TargetState  = VICTIMSTATE_PARRY;
            damageInfo->procEx      |= PROC_EX_PARRY;
            damageInfo->cleanDamage += damageInfo->damage;
            damageInfo->damage = 0;
            break;

        case MELEE_HIT_DODGE:
            damageInfo->TargetState  = VICTIMSTATE_DODGE;
            damageInfo->procEx      |= PROC_EX_DODGE;
            damageInfo->cleanDamage += damageInfo->damage;
            damageInfo->damage = 0;
            break;
        case MELEE_HIT_BLOCK:
        {
            damageInfo->TargetState = VICTIMSTATE_NORMAL;
            damageInfo->procEx     |= PROC_EX_BLOCK;
            damageInfo->blocked_amount = damageInfo->target->Sheet().ShieldBlock();
            if (damageInfo->blocked_amount >= damageInfo->damage)
            {
                damageInfo->TargetState = VICTIMSTATE_BLOCKS;
                damageInfo->blocked_amount = damageInfo->damage;
            }
            else
            {
                damageInfo->procEx |= PROC_EX_NORMAL_HIT;
            }

            damageInfo->damage      -= damageInfo->blocked_amount;
            damageInfo->cleanDamage += damageInfo->blocked_amount;
            break;
        }
        case MELEE_HIT_GLANCING:
        {
            damageInfo->HitInfo |= HITINFO_GLANCING;
            damageInfo->TargetState = VICTIMSTATE_NORMAL;
            damageInfo->procEx |= PROC_EX_NORMAL_HIT;

            float baseLowEnd = 1.3f;
            float baseHighEnd = 1.2f;
            switch (getClass())
            {
                case CLASS_SHAMAN:
                case CLASS_PRIEST:
                case CLASS_MAGE:
                case CLASS_WARLOCK:
                case CLASS_DRUID:
                    baseLowEnd  -= 0.7f;
                    baseHighEnd -= 0.3f;
                    break;
            }

            float maxLowEnd = 0.6f;
            switch (getClass())
            {
                case CLASS_WARRIOR:
                case CLASS_ROGUE:
                    maxLowEnd = 0.91f;
            }

            int32 diff = damageInfo->target->GetDefenseSkillValue() - GetWeaponSkillValue(damageInfo->attackType);
            float lowEnd  = baseLowEnd - (0.05f * diff);
            float highEnd = baseHighEnd - (0.03f * diff);

            if (lowEnd < 0.01f)
            {
                lowEnd = 0.01f;
            }
            else if (lowEnd > maxLowEnd)
            {
                lowEnd = maxLowEnd;
            }

            if (highEnd < 0.2f)
            {
                highEnd = 0.2f;
            }
            if (highEnd > 0.99f)
            {
                highEnd = 0.99f;
            }

            if (lowEnd > highEnd)
            {
                lowEnd = highEnd;
            }

            float reducePercent = lowEnd + rand_norm_f() * (highEnd - lowEnd);

            damageInfo->cleanDamage += damageInfo->damage - uint32(reducePercent *  damageInfo->damage);
            damageInfo->damage   = uint32(reducePercent *  damageInfo->damage);
            break;
        }
        case MELEE_HIT_CRUSHING:
        {
            damageInfo->HitInfo     |= HITINFO_CRUSHING;
            damageInfo->TargetState  = VICTIMSTATE_NORMAL;
            damageInfo->procEx |= PROC_EX_NORMAL_HIT;

            damageInfo->damage += (damageInfo->damage / 2);
            break;
        }
        default:

            break;
    }

    if (int32(damageInfo->damage) > 0)
    {
        damageInfo->procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;

        damageInfo->target->CalculateDamageAbsorbAndResist(this, damageInfo->damageSchoolMask, DIRECT_DAMAGE, damageInfo->damage, &damageInfo->absorb, &damageInfo->resist, true);
        damageInfo->damage -= damageInfo->absorb + damageInfo->resist;
        if (damageInfo->absorb)
        {
            damageInfo->HitInfo |= HITINFO_ABSORB;
            damageInfo->procEx |= PROC_EX_ABSORB;
        }
        if (damageInfo->resist)
        {
            damageInfo->HitInfo |= HITINFO_RESIST;
        }
    }
    else
    {
        damageInfo->damage = 0;
    }
}

void Unit::DealMeleeDamage(CalcDamageInfo* damageInfo, bool durabilityLoss)
{
    if (damageInfo == 0)
    {
        return;
    }
    Unit* pVictim = damageInfo->target;

    if (!pVictim)
    {
        return;
    }

    if (!pVictim->IsAlive() || pVictim->IsTaxiFlying() || (IsCreature(pVictim) && ((Creature*)pVictim)->IsInEvadeMode()))
    {
        return;
    }

    if (damageInfo->HitInfo & HITINFO_CRITICALHIT)
    {
        pVictim->HandleEmoteCommand(EMOTE_ONESHOT_WOUNDCRITICAL);
    }
    if (damageInfo->blocked_amount && damageInfo->TargetState != VICTIMSTATE_BLOCKS)
    {
        pVictim->HandleEmoteCommand(EMOTE_ONESHOT_PARRYSHIELD);
    }

    if (damageInfo->TargetState == VICTIMSTATE_PARRY)
    {
        if (!IsCreature(pVictim) ||
            !(((Creature*)pVictim)->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_PARRY_HASTEN))
        {

            float offtime = float(pVictim->getAttackTimer(OFF_ATTACK));
            float basetime = float(pVictim->getAttackTimer(BASE_ATTACK));

            if (pVictim->haveOffhandWeapon() && offtime < basetime)
            {
                float percent20 = pVictim->GetAttackTime(OFF_ATTACK) * 0.20f;
                float percent60 = 3.0f * percent20;
                if (offtime > percent20 && offtime <= percent60)
                {
                    pVictim->setAttackTimer(OFF_ATTACK, uint32(percent20));
                }
                else if (offtime > percent60)
                {
                    offtime -= 2.0f * percent20;
                    pVictim->setAttackTimer(OFF_ATTACK, uint32(offtime));
                }
            }
            else
            {
                float percent20 = pVictim->GetAttackTime(BASE_ATTACK) * 0.20f;
                float percent60 = 3.0f * percent20;
                if (basetime > percent20 && basetime <= percent60)
                {
                    pVictim->setAttackTimer(BASE_ATTACK, uint32(percent20));
                }
                else if (basetime > percent60)
                {
                    basetime -= 2.0f * percent20;
                    pVictim->setAttackTimer(BASE_ATTACK, uint32(basetime));
                }
            }
        }
    }

    CleanDamage cleanDamage(damageInfo->cleanDamage, damageInfo->attackType, damageInfo->hitOutCome);
    DealDamage(pVictim, damageInfo->damage, &cleanDamage, DIRECT_DAMAGE, SpellSchoolMask(damageInfo->damageSchoolMask), nullptr, durabilityLoss);

    if ((!damageInfo->absorb) && (damageInfo->hitOutCome == MELEE_HIT_CRIT || damageInfo->hitOutCome == MELEE_HIT_CRUSHING || damageInfo->hitOutCome == MELEE_HIT_NORMAL || damageInfo->hitOutCome == MELEE_HIT_GLANCING) &&
        !IsPlayer(this) && !((Creature*)this)->GetCharmerOrOwnerGuid() && !pVictim->Where().HasInArc(this->Where(), M_PI_F))
    {

        float Probability = 20.0f;

        if (pVictim->getLevel() < 30)
        {
            Probability = 0.65f * pVictim->getLevel() + 0.5f;
        }

        uint32 VictimDefense = pVictim->GetDefenseSkillValue();
        uint32 AttackerMeleeSkill = GetUnitMeleeSkill();

        Probability *= AttackerMeleeSkill / (float)VictimDefense;

        if (Probability > 40.0f)
        {
            Probability = 40.0f;
        }

        if (roll_chance_f(Probability))
        {
            CastSpell(pVictim, 1604, true);
        }

    }

    if (damageInfo->damage)
    {
        SpellAuraHolderMap const& vAuras = pVictim->GetSpellAuraHolderMap();
        for (SpellAuraHolderMap::const_iterator itr = vAuras.begin(); itr != vAuras.end(); ++itr)
        {
            SpellEntry const* spellInfo = (*itr).second->GetSpellProto();
            if (spellInfo->AttributesExC & 0x40000 && spellInfo->SpellClassSet == SPELLFAMILY_PALADIN && ((*itr).second->GetCasterGuid() == GetObjectGuid()))
            {
                (*itr).second->RefreshHolder();
            }
        }
    }

    if (!(damageInfo->HitInfo & HITINFO_MISS))
    {

        if (IsPlayer(this) && pVictim->IsAlive())
        {
            ((Player*)this)->CastItemCombatSpell(pVictim, damageInfo->attackType);
        }

        for (const auto* shield : pVictim->GetAurasByType(SPELL_AURA_DAMAGE_SHIELD))
        {
            uint32 damage = shield->GetModifier()->m_amount;
            const SpellEntry* shieldProto = shield->GetSpellProto();

            pVictim->DealDamageMods(this, damage, nullptr);

            WorldPacket data(SMSG_SPELLDAMAGESHIELD, (8 + 8 + 4 + 4));
            data << pVictim->GetObjectGuid();
            data << GetObjectGuid();
            data << uint32(damage);
            data << uint32(shieldProto->School);
            Deliver(Audience::Around(*pVictim).AndSubject(), &data);

            pVictim->DealDamage(this, damage, 0, SPELL_DIRECT_DAMAGE, GetSpellSchoolMask(shieldProto), shieldProto, true);
        }
    }
}
