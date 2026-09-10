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

#include "Reaction.h"
#include "Spell.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Pet.h"
#include "Unit.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "ObjectLookup.h"
#include "CellImpl.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "BattleGround/BattleGround.h"
#include "Util.h"
#include "Chat.h"
#include "TemporarySummon.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "Cast/Recipe/RecipeBook.h"

void Spell::DoAllEffectOnTarget(cast::UnitTarget* target)
{
    if (target->served)
    {
        return;
    }
    target->served = true;

    uint32 mask = target->slots;

    Unit* unit = m_caster->GetObjectGuid() == target->guid ? m_caster : ObjectLookup::GetUnit(*m_caster, target->guid);
    if (!unit)
    {
        return;
    }

    Unit* real_caster = GetAffectiveCaster();

    Unit* caster = real_caster ? real_caster : m_caster;

    SpellMissInfo missInfo = target->verdict;

    unitTarget = unit;

    ResetEffectDamageAndHeal();

    uint32 procAttacker = Recipe().Announces().byCaster;
    uint32 procVictim   = Recipe().Announces().byTarget;
    uint32 procEx       = PROC_EX_NONE;

    if (((procAttacker | procVictim) & NEGATIVE_TRIGGER_MASK) &&
        !(target->slots & Recipe().UnwantedSlots()) && missInfo == SPELL_MISS_NONE)
    {
        procAttacker = PROC_FLAG_NONE;
        procVictim   = PROC_FLAG_NONE;
    }

    float speed = m_spellInfo->Speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->Speed : m_spellInfo->Speed;
    if (speed > 0.0f)
    {

        for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (IsEffectHandledOnDelayedSpellLaunch(m_spellInfo, SpellEffectIndex(i)))
            {
                mask &= ~(1 << i);
            }
        }

        m_damage += target->damage;
    }

    if (missInfo == SPELL_MISS_NONE)
    {
        DoSpellHitOnUnit(unit, mask);
    }
    else if (missInfo == SPELL_MISS_REFLECT)
    {
        if (target->reflectedVerdict == SPELL_MISS_NONE)
        {
            DoSpellHitOnUnit(m_caster, mask, true);
            unitTarget = m_caster;

            if (IsCreature(m_caster))
            {
                static_cast<Creature*>(m_caster)->LowerPlayerDamageReq(target->damage);
            }
        }
    }
    else
    {
        if (real_caster)
        {

            if (m_spellInfo->SpellClassSet == SPELLFAMILY_WARRIOR && m_spellInfo->IsFitToFamilyMask(0x0000000020000000))
            {
                real_caster->SendSpellMiss(unit, 20647, missInfo);
            }
            else
            {
                real_caster->SendSpellMiss(unit, m_spellInfo->ID, missInfo);
            }
        }

        if (missInfo == SPELL_MISS_MISS || missInfo == SPELL_MISS_RESIST)
        {
            if (real_caster && real_caster != unit)
            {

                if (!Recipe().Says().makesNoInitialThreat && !Recipe().IsPositive() &&
                    m_caster->IsVisibleForOrDetect(unit, unit, false))
                {
                    if (!unit->IsInCombat() && !IsPlayer(unit) && ((Creature*)unit)->AI())
                    {
                        ((Creature*)unit)->AI()->AttackedBy(real_caster);
                    }

                    unit->AddThreat(real_caster);
                    unit->SetInCombatWith(real_caster);
                    real_caster->SetInCombatWith(unit);
                }
            }
        }
    }

    if (m_healing)
    {
        bool crit = real_caster && real_caster->IsSpellCrit(unitTarget, m_spellInfo, m_spellSchoolMask);
        uint32 addhealth = m_healing;
        if (crit)
        {
            procEx |= PROC_EX_CRITICAL_HIT;
            addhealth = caster->SpellCriticalHealingBonus(m_spellInfo, addhealth, nullptr);
        }
        else
        {
            procEx |= PROC_EX_NORMAL_HIT;
        }

        if (m_setsOffProcs && missInfo != SPELL_MISS_REFLECT)
        {

            SpellEntry const* spellInfo = m_spellInfo;
            switch (m_spellInfo->ID)
            {
                case 19968:
                case 19993:
                {

                    uint32 spellid = m_currentBasePoints[EFFECT_INDEX_1];
                    spellInfo = sSpellStore.LookupEntry(spellid);
                }
            }

            caster->ProcDamageAndSpell(unitTarget, real_caster ? procAttacker : uint32(PROC_FLAG_NONE), procVictim, procEx, addhealth, Recipe().Swings(), spellInfo);
        }

        int32 gain = caster->DealHeal(unitTarget, addhealth, m_spellInfo, crit);

        if (real_caster)
        {
            unitTarget->GetHostileRefManager().threatAssist(real_caster, float(gain) * 0.5f * Recipe().ThreatMultiplier(), m_spellInfo);
        }
    }

    else if (m_damage)
    {

        SpellNonMeleeDamage damageInfo(caster, unitTarget, m_spellInfo->ID, GetFirstSchoolInMask(m_spellSchoolMask));

        if (speed > 0.0f)
        {
            damageInfo.damage = m_damage;
            damageInfo.HitInfo = target->hitInfo;
        }

        else
        {
            caster->CalculateSpellDamage(&damageInfo, m_damage, m_spellInfo, Recipe().Swings());
        }

        unitTarget->CalculateAbsorbResistBlock(caster, &damageInfo, m_spellInfo);

        caster->DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);

        caster->SendSpellNonMeleeDamageLog(&damageInfo);

        procEx = createProcExtendMask(&damageInfo, missInfo);
        procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;

        if (m_setsOffProcs && missInfo != SPELL_MISS_REFLECT)
        {
            caster->ProcDamageAndSpell(unitTarget, real_caster ? procAttacker : uint32(PROC_FLAG_NONE), procVictim, procEx, damageInfo.damage, Recipe().Swings(), m_spellInfo);
        }

        if (IsPlayer(m_caster) && m_spellInfo->EquippedItemClass == ITEM_CLASS_WEAPON &&
            !Recipe().Says().stopsAttack)
        {
            ((Player*)m_caster)->CastItemCombatSpell(unitTarget, Recipe().Swings());
        }

        caster->DealSpellDamage(&damageInfo, true);

        if (m_spellInfo->SpellClassSet == SPELLFAMILY_WARRIOR &&
            m_spellInfo->SpellClassMask & UI64LIT(0x0000000002000000) &&
            m_spellInfo->SpellIconID == 38)
        {
            uint32 BTAura = 0;
            switch (m_spellInfo->ID)
            {
                case 23881: BTAura = 23885; break;
                case 23892: BTAura = 23886; break;
                case 23893: BTAura = 23887; break;
                case 23894: BTAura = 23888; break;
                default:
                    sLog.outError("Spell::EffectSchoolDMG: Spell %u not handled in BTAura", m_spellInfo->ID);
                    break;
            }
            if (BTAura)
            {
                m_caster->CastSpell(m_caster, BTAura, true);
            }
        }
    }

    else if (procAttacker || procVictim)
    {

        SpellNonMeleeDamage damageInfo(caster, unitTarget, m_spellInfo->ID, GetFirstSchoolInMask(m_spellSchoolMask));
        procEx = createProcExtendMask(&damageInfo, missInfo);

        if (m_setsOffProcs && missInfo != SPELL_MISS_REFLECT)
        {
            caster->ProcDamageAndSpell(unit, real_caster ? procAttacker : uint32(PROC_FLAG_NONE), procVictim, procEx, 0, Recipe().Swings(), m_spellInfo);
        }
    }

    if (IsCreature(unit))
    {

        if (real_caster && !((Creature*)unit)->IsPet() && !IsAutoRepeat() && !IsNextMeleeSwingSpell() && !IsChannelActive())
        {
            if (Player* p = real_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
            {
                p->RewardPlayerAndGroupAtCast(unit, m_spellInfo->ID);
            }
        }

        if (((Creature*)unit)->AI())
        {
            ((Creature*)unit)->AI()->SpellHit(m_caster, m_spellInfo);
        }
    }

    if (IsCreature(m_caster) && ((Creature*)m_caster)->AI())
    {
        ((Creature*)m_caster)->AI()->SpellHitTarget(unit, m_spellInfo);
    }
    if (real_caster && real_caster != m_caster &&IsCreature(real_caster) && ((Creature*)real_caster)->AI())
    {
        ((Creature*)real_caster)->AI()->SpellHitTarget(unit, m_spellInfo);
    }
}

void Spell::DoSpellHitOnUnit(Unit* unit, uint32 effectMask, bool isReflected)
{
    if (!unit || !effectMask)
    {
        return;
    }

    Unit* realCaster = GetAffectiveCaster();

    float speed = m_spellInfo->Speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->Speed : m_spellInfo->Speed;
    if (speed &&
        (unit->IsImmuneToDamage(GetSpellSchoolMask(m_spellInfo)) ||
        unit->IsImmuneToSpell(m_spellInfo, unit == realCaster)))
    {
        if (realCaster)
        {
            realCaster->SendSpellMiss(unit, m_spellInfo->ID, SPELL_MISS_IMMUNE);
        }

        ResetEffectDamageAndHeal();
        return;
    }

    if (realCaster && realCaster != unit)
    {

        if (speed > 0.0f &&
            unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) &&
            unit->GetCharmerOrOwnerGuid() != m_caster->GetObjectGuid())
        {
            realCaster->SendSpellMiss(unit, m_spellInfo->ID, SPELL_MISS_EVADE);
            ResetEffectDamageAndHeal();
            return;
        }

        if (!IsFriendly(*realCaster, *unit))
        {

            if (speed > 0.0f && unit == m_targets.getUnitTarget() &&
                !unit->IsVisibleForOrDetect(m_caster, m_caster, false))
            {
                realCaster->SendSpellMiss(unit, m_spellInfo->ID, SPELL_MISS_EVADE);
                ResetEffectDamageAndHeal();
                return;
            }

            if (!(m_spellInfo->AttributesEx & SPELL_ATTR_EX_NOT_BREAK_STEALTH) && m_spellInfo->ID != 51690 && m_spellInfo->ID != 53055)
            {
                unit->RemoveAurasOfType(SPELL_AURA_MOD_STEALTH);
            }

            if (!Recipe().Says().makesNoInitialThreat && !Recipe().IsPositive() &&
                m_caster->IsVisibleForOrDetect(unit, unit, false))
            {

                if (Recipe().Says().doesNotBreakStealth)
                {
                    unit->RemoveAurasOfType(SPELL_AURA_MOD_STEALTH);
                }

                m_caster->RemoveAurasOfType(SPELL_AURA_MOD_STEALTH);

                if (!unit->IsStandState() && !unit->hasUnitState(UNIT_STAT_STUNNED))
                {
                    unit->SetStandState(UNIT_STAND_STATE_STAND);
                }

                switch (m_spellInfo->ID)
                {

                    case 453:
                    case 8192:
                    case 10953:

                    case 9901:
                    case 8955:
                    case 2908:

                    case 13180:
                        break;
                    default:
                    {
                        if (!unit->IsInCombat() && !IsPlayer(unit) && ((Creature*)unit)->AI())
                        {
                            unit->AttackedBy(realCaster);
                        }

                        unit->AddThreat(realCaster);
                        unit->SetInCombatWith(realCaster);
                        realCaster->SetInCombatWith(unit);

                        if (Player* attackedPlayer = unit->GetCharmerOrOwnerPlayerOrPlayerItself())
                        {
                            realCaster->SetContestedPvP(attackedPlayer);
                        }
                        break;
                    }
                }
            }
        }
        else
        {

            if (speed > 0.0f && !Recipe().IsPositive())
            {
                realCaster->SendSpellMiss(unit, m_spellInfo->ID, SPELL_MISS_EVADE);
                ResetEffectDamageAndHeal();
                return;
            }

            if (unit->hasUnitState(UNIT_STAT_ATTACK_PLAYER))
            {
                realCaster->SetContestedPvP();
            }

            if (unit->IsInCombat() && !Recipe().Says().makesNoInitialThreat)
            {
                realCaster->SetInCombatState(unit->GetCombatTimer() > 0);
                unit->GetHostileRefManager().threatAssist(realCaster, 0.0f, m_spellInfo);
            }
        }
    }

    m_diminishGroup = Recipe().Diminishes(m_triggeredByAuraSpell != nullptr);
    m_diminishLevel = unit->Diminishing().FadeOf(m_diminishGroup, GameTime::GetGameTimeMS());

    const DiminishingReturnsType type = GetDiminishingReturnsGroupType(m_diminishGroup);
    m_diminishApplies = (type == DRTYPE_PLAYER &&IsPlayer(unit)) ||
                        type == DRTYPE_ALL;

    if (m_diminishApplies)
    {
        unit->Diminishing().RecordHit(m_diminishGroup, GameTime::GetGameTimeMS());
    }

    CastPreCastSpells(unit);

    if (IsSpellAppliesAura(m_spellInfo, effectMask))
    {
        m_spellAuraHolder = CreateSpellAuraHolder(m_spellInfo, unit, realCaster, m_CastItem);
        m_spellAuraHolder->setDiminishGroup(m_diminishGroup);
    }
    else
    {
        m_spellAuraHolder = nullptr;
    }

    for (int effectNumber = 0; effectNumber < MAX_EFFECT_INDEX; ++effectNumber)
    {
        if (effectMask & (1 << effectNumber))
        {
            HandleEffects(unit, nullptr, nullptr, SpellEffectIndex(effectNumber), m_damageMultipliers[effectNumber]);
            if (m_applyMultiplierMask & (1 << effectNumber))
            {

                float multiplier = Recipe().At(static_cast<uint8>(effectNumber)).chainAmplitude;

                if (realCaster)
                {
                    if (Player* modOwner = realCaster->GetSpellModOwner())
                    {
                        modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_EFFECT_PAST_FIRST, multiplier, this);
                    }
                }
                m_damageMultipliers[effectNumber] *= multiplier;
            }
        }
    }

    if (m_spellAuraHolder)
    {

        if (!m_spellAuraHolder->IsEmptyHolder())
        {
            int32 duration = m_spellAuraHolder->GetAuraMaxDuration();
            int32 originalDuration = duration;

            if (duration > 0)
            {

                if (m_diminishApplies && (isReflected || !IsFriendly(*m_caster, *unit)))
                {
                    duration = unit::Diminishing::Shorten(duration, m_diminishLevel);
                }

                if (duration == 0)
                {
                    delete m_spellAuraHolder;
                    return;
                }
            }

            if (duration != originalDuration)
            {
                m_spellAuraHolder->SetAuraMaxDuration(duration);
                m_spellAuraHolder->SetAuraDuration(duration);
            }

            unit->AddSpellAuraHolder(m_spellAuraHolder);
        }
        else
        {
            delete m_spellAuraHolder;
        }
    }
}

void Spell::DoAllEffectOnTarget(cast::ObjectTarget* target)
{
    if (target->served)
    {
        return;
    }
    target->served = true;

    uint32 effectMask = target->slots;
    if (!effectMask)
    {
        return;
    }

    GameObject* go = m_caster->GetMap()->GetGameObject(target->guid);
    if (!go)
    {
        return;
    }

    for (int effectNumber = 0; effectNumber < MAX_EFFECT_INDEX; ++effectNumber)
    {
        if (effectMask & (1 << effectNumber))
        {
            HandleEffects(nullptr, nullptr, go, SpellEffectIndex(effectNumber));
        }
    }

    if (!IsAutoRepeat() && !IsNextMeleeSwingSpell() && !IsChannelActive())
    {
        if (Player* p = m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
        {
            p->RewardPlayerAndGroupAtCast(go, m_spellInfo->ID);
        }
    }
}

void Spell::DoAllEffectOnTarget(cast::ItemTarget* target)
{
    uint32 effectMask = target->slots;
    if (!target->item || !effectMask)
    {
        return;
    }

    for (int effectNumber = 0; effectNumber < MAX_EFFECT_INDEX; ++effectNumber)
    {
        if (effectMask & (1 << effectNumber))
        {
            HandleEffects(nullptr, target->item, nullptr, SpellEffectIndex(effectNumber));
        }
    }
}

void Spell::HandleDelayedSpellLaunch(cast::UnitTarget* target)
{

    uint32 mask = target->slots;

    Unit* unit = m_caster->GetObjectGuid() == target->guid ? m_caster : ObjectLookup::GetUnit(*m_caster, target->guid);
    if (!unit)
    {
        return;
    }

    Unit* real_caster = GetAffectiveCaster();

    Unit* caster = real_caster ? real_caster : m_caster;

    SpellMissInfo missInfo = target->verdict;

    unitTarget = unit;

    m_damage = 0;
    m_healing = 0;

    SpellNonMeleeDamage damageInfo(caster, unitTarget, m_spellInfo->ID, GetFirstSchoolInMask(m_spellSchoolMask));

    if (missInfo == SPELL_MISS_NONE || (missInfo == SPELL_MISS_REFLECT && target->reflectedVerdict == SPELL_MISS_NONE))
    {
        for (int32 effectNumber = 0; effectNumber < MAX_EFFECT_INDEX; ++effectNumber)
        {
            if (mask & (1 << effectNumber) && IsEffectHandledOnDelayedSpellLaunch(m_spellInfo, SpellEffectIndex(effectNumber)))
            {
                HandleEffects(unit, nullptr, nullptr, SpellEffectIndex(effectNumber), m_damageMultipliers[effectNumber]);
                if (m_applyMultiplierMask & (1 << effectNumber))
                {

                    float multiplier = Recipe().At(static_cast<uint8>(effectNumber)).chainAmplitude;

                    if (real_caster)
                    {
                        if (Player* modOwner = real_caster->GetSpellModOwner())
                        {
                            modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_EFFECT_PAST_FIRST, multiplier, this);
                        }
                    }
                    m_damageMultipliers[effectNumber] *= multiplier;
                }
            }
        }

        if (m_damage > 0)
        {
            caster->CalculateSpellDamage(&damageInfo, m_damage, m_spellInfo, Recipe().Swings());
        }
    }

    target->damage = damageInfo.damage;
    target->hitInfo = damageInfo.HitInfo;
}

void Spell::InitializeDamageMultipliers()
{
    for (const auto& operation : Recipe().Does())
    {
        const int32 i = operation.slot;

        uint32 EffectChainTarget = operation.chainTargets;
        if (Unit* realCaster = GetAffectiveCaster())
        {
            if (Player* modOwner = realCaster->GetSpellModOwner())
            {
                modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_JUMP_TARGETS, EffectChainTarget, this);
            }
        }
        m_damageMultipliers[i] = 1.0f;
        if ((operation.targetA == TARGET_CHAIN_DAMAGE || operation.targetA == TARGET_CHAIN_HEAL) &&
            (EffectChainTarget > 1))
        {
            m_applyMultiplierMask |= (1 << i);
        }
    }
}
