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
#include "Geometry/Placement.h"
#include <cmath>
#include <iterator>
#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include "Utilities/MathDefines.h"
#include <cstdlib>
#include <map>
#include <set>
#include <list>
#include <ctime>
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
#include "ObjectLookup.h"
#include "Policies/Singleton.h"
#include "Totem.h"
#include "TemporarySummon.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "CreatureAI.h"
#include "ScriptMgr.h"
#include "Cast/Recipe/RecipeBook.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

#define NULL_AURA_SLOT 0xFF

pAuraHandler AuraHandler[TOTAL_AURAS] =
{
    &Aura::HandleNULL,
    &Aura::HandleBindSight,
    &Aura::HandleModPossess,
    &Aura::HandlePeriodicDamage,
    &Aura::HandleAuraDummy,
    &Aura::HandleModConfuse,
    &Aura::HandleModCharm,
    &Aura::HandleModFear,
    &Aura::HandlePeriodicHeal,
    &Aura::HandleModAttackSpeed,
    &Aura::HandleModThreat,
    &Aura::HandleModTaunt,
    &Aura::HandleAuraModStun,
    &Aura::HandleModDamageDone,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleModStealth,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleInvisibility,
    &Aura::HandleInvisibilityDetect,
    &Aura::HandleAuraModTotalHealthPercentRegen,
    &Aura::HandleAuraModTotalManaPercentRegen,
    &Aura::HandleAuraModResistance,
    &Aura::HandlePeriodicTriggerSpell,
    &Aura::HandlePeriodicEnergize,
    &Aura::HandleAuraModPacify,
    &Aura::HandleAuraModRoot,
    &Aura::HandleAuraModSilence,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleAuraModStat,
    &Aura::HandleAuraModSkill,
    &Aura::HandleAuraModIncreaseSpeed,
    &Aura::HandleAuraModIncreaseMountedSpeed,
    &Aura::HandleAuraModDecreaseSpeed,
    &Aura::HandleAuraModIncreaseHealth,
    &Aura::HandleAuraModIncreaseEnergy,
    &Aura::HandleAuraModShapeshift,
    &Aura::HandleAuraModEffectImmunity,
    &Aura::HandleAuraModStateImmunity,
    &Aura::HandleAuraModSchoolImmunity,
    &Aura::HandleAuraModDmgImmunity,
    &Aura::HandleAuraModDispelImmunity,
    &Aura::HandleAuraProcTriggerSpell,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleAuraTrackCreatures,
    &Aura::HandleAuraTrackResources,
    &Aura::HandleUnused,
    &Aura::HandleAuraModParryPercent,
    &Aura::HandleUnused,
    &Aura::HandleAuraModDodgePercent,
    &Aura::HandleUnused,
    &Aura::HandleAuraModBlockPercent,
    &Aura::HandleAuraModCritPercent,
    &Aura::HandlePeriodicLeech,
    &Aura::HandleModHitChance,
    &Aura::HandleModSpellHitChance,
    &Aura::HandleAuraTransform,
    &Aura::HandleModSpellCritChance,
    &Aura::HandleAuraModIncreaseSwimSpeed,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleAuraModPacifyAndSilence,
    &Aura::HandleAuraModScale,
    &Aura::HandlePeriodicHealthFunnel,
    &Aura::HandleUnused,
    &Aura::HandlePeriodicManaLeech,
    &Aura::HandleModCastingSpeed,
    &Aura::HandleFeignDeath,
    &Aura::HandleAuraModDisarm,
    &Aura::HandleAuraModStalked,
    &Aura::HandleSchoolAbsorb,
    &Aura::HandleUnused,
    &Aura::HandleModSpellCritChanceShool,
    &Aura::HandleModPowerCostPCT,
    &Aura::HandleModPowerCost,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleFarSight,
    &Aura::HandleModMechanicImmunity,
    &Aura::HandleAuraMounted,
    &Aura::HandleModDamagePercentDone,
    &Aura::HandleModPercentStat,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleWaterBreathing,
    &Aura::HandleModBaseResistance,
    &Aura::HandleModRegen,
    &Aura::HandleModPowerRegen,
    &Aura::HandleChannelDeathItem,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandlePeriodicDamagePCT,
    &Aura::HandleUnused,
    &Aura::HandleModDetectRange,
    &Aura::HandlePreventFleeing,
    &Aura::HandleModUnattackable,
    &Aura::HandleInterruptRegen,
    &Aura::HandleAuraGhost,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleManaShield,
    &Aura::HandleAuraModSkill,
    &Aura::HandleAuraModAttackPower,
    &Aura::HandleAurasVisible,
    &Aura::HandleModResistancePercent,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleAuraModTotalThreat,
    &Aura::HandleAuraWaterWalk,
    &Aura::HandleAuraFeatherFall,
    &Aura::HandleAuraHover,
    &Aura::HandleAddModifier,
    &Aura::HandleAddModifier,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleModPowerRegenPCT,
    &Aura::HandleUnused,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleUnused,
    &Aura::HandleAuraUntrackable,
    &Aura::HandleAuraEmpathy,
    &Aura::HandleModOffhandDamagePercent,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleAuraModRangedAttackPower,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleModPossessPet,
    &Aura::HandleAuraModIncreaseSpeed,
    &Aura::HandleAuraModIncreaseMountedSpeed,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleAuraModIncreaseEnergyPercent,
    &Aura::HandleAuraModIncreaseHealthPercent,
    &Aura::HandleAuraModRegenInterrupt,
    &Aura::HandleModHealingDone,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleModTotalPercentStat,
    &Aura::HandleModMeleeSpeedPct,
    &Aura::HandleForceReaction,
    &Aura::HandleAuraModRangedHaste,
    &Aura::HandleRangedAmmoHaste,
    &Aura::HandleAuraModBaseResistancePCT,
    &Aura::HandleAuraModResistanceExclusive,
    &Aura::HandleAuraSafeFall,
    &Aura::HandleUnused,
    &Aura::HandleUnused,
    &Aura::HandleModMechanicImmunityMask,
    &Aura::HandleAuraRetainComboPoints,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleShieldBlockValue,
    &Aura::HandleAuraTrackStealthed,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleUnused,
    &Aura::HandleShieldBlockValue,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleAuraPowerBurn,
    &Aura::HandleUnused,
    &Aura::HandleUnused,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleAuraModAttackPowerPercent,
    &Aura::HandleAuraModRangedAttackPowerPercent,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleDetectAmore,
    &Aura::HandleAuraModIncreaseSpeed,
    &Aura::HandleAuraModIncreaseMountedSpeed,
    &Aura::HandleUnused,
    &Aura::HandleModSpellDamagePercentFromStat,
    &Aura::HandleModSpellHealingPercentFromStat,
    &Aura::HandleSpiritOfRedemption,
    &Aura::HandleNULL,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleUnused,
    &Aura::HandleAuraModResistenceOfStatPercent,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleUnused,
    &Aura::HandleNoImmediateEffect,
    &Aura::HandleAuraModUseNormalSpeed,
};

Aura::Aura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster, Item* castItem)
    : m_spellmod(nullptr), m_periodicTimer(0), m_periodicTick(0), m_removeMode(AURA_REMOVE_BY_DEFAULT),
    m_effIndex(eff), m_positive(false), m_isPeriodic(false), m_isAreaAura(false),
    m_isPersistent(false), m_in_use(0), m_spellAuraHolder(holder)
{
    MANGOS_ASSERT(target);
    MANGOS_ASSERT(spellproto && spellproto == sSpellStore.LookupEntry(spellproto->ID));

    m_currentBasePoints = currentBasePoints ? *currentBasePoints : spellproto->CalculateSimpleValue(eff);

    m_operation = &cast::RecipeOf(*spellproto).At(static_cast<uint8>(m_effIndex));
    m_positive = m_operation->positive;
    m_applyTime = time(nullptr);

    int32 damage;
    if (!caster)
    {
        damage = m_currentBasePoints;
    }
    else
    {
        damage = caster->CalculateSpellDamage(target, cast::RecipeOf(*spellproto), cast::RecipeOf(*spellproto).At(static_cast<uint8>(m_effIndex)), &m_currentBasePoints);
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Aura: construct Spellid : %u, Aura : %u Target : %d Damage : %d", spellproto->ID, spellproto->EffectAura[eff], spellproto->ImplicitTargetA[eff], damage);

    SetModifier(AuraType(spellproto->EffectAura[eff]), damage, spellproto->EffectAuraPeriod[eff], spellproto->EffectMiscValue[eff]);

    Player* modOwner = caster ? caster->GetSpellModOwner() : nullptr;

    if (modOwner && m_modifier.periodictime)
    {
        modOwner->SpellMods().Apply(spellproto->ID, SPELLMOD_ACTIVATION_TIME, m_modifier.periodictime);
    }

    m_periodicTimer += m_modifier.periodictime;
}

Aura::~Aura()
{
}

AreaAura::AreaAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target,
    Unit* caster, Item* castItem, uint32 originalRankSpellId)
        : Aura(spellproto, eff, currentBasePoints, holder, target, caster, castItem), m_originalRankSpellId(originalRankSpellId)
{
    m_isAreaAura = true;

    Unit* caster_ptr = caster ? caster : target;

    m_radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(Operation().radiusIndex));
    if (Player* modOwner = caster_ptr->GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(spellproto->ID, SPELLMOD_RADIUS, m_radius);
    }

    switch (spellproto->Effect[eff])
    {
        case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
            m_areaAuraType = AREA_AURA_PARTY;
            break;
        case SPELL_EFFECT_APPLY_AREA_AURA_PET:
            m_areaAuraType = AREA_AURA_PET;
            break;
        default:
            sLog.outError("Wrong spell effect in AreaAura constructor");
            MANGOS_ASSERT(false);
            break;
    }

    if (IsCreature(target) && ((Creature*)target)->IsTotem())
    {
        m_modifier.m_auraname = SPELL_AURA_NONE;
    }
}

AreaAura::~AreaAura()
{
}

PersistentAreaAura::PersistentAreaAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target,
    Unit* caster, Item* castItem) : Aura(spellproto, eff, currentBasePoints, holder, target, caster, castItem)
{
    m_isPersistent = true;
}

PersistentAreaAura::~PersistentAreaAura()
{
}

SingleEnemyTargetAura::SingleEnemyTargetAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target,
    Unit* caster, Item* castItem) : Aura(spellproto, eff, currentBasePoints, holder, target, caster, castItem)
{
    if (caster)
    {
        m_castersTargetGuid =IsPlayer(caster) ? ((Player*)caster)->GetSelectionGuid() : caster->GetTargetGuid();
    }
}

SingleEnemyTargetAura::~SingleEnemyTargetAura()
{
}

Unit* SingleEnemyTargetAura::GetTriggerTarget() const
{
    return ObjectLookup::GetUnit(*(m_spellAuraHolder->GetTarget()), m_castersTargetGuid);
}

Aura* CreateAura(SpellEntry const* spellproto, SpellEffectIndex eff, int32* currentBasePoints, SpellAuraHolder* holder, Unit* target, Unit* caster, Item* castItem)
{
    if (IsAreaAuraEffect(spellproto->Effect[eff]))
    {
        return new AreaAura(spellproto, eff, currentBasePoints, holder, target, caster, castItem);
    }

    return new Aura(spellproto, eff, currentBasePoints, holder, target, caster, castItem);
}

SpellAuraHolder* CreateSpellAuraHolder(SpellEntry const* spellproto, Unit* target, Occupant* caster, Item* castItem)
{
    return new SpellAuraHolder(spellproto, target, caster, castItem);
}

void Aura::SetModifier(AuraType t, int32 a, uint32 pt, int32 miscValue)
{
    m_modifier.m_auraname = t;
    m_modifier.m_amount = a;
    m_modifier.m_miscvalue = miscValue;
    m_modifier.periodictime = pt;
}

void Aura::Update(uint32 diff)
{
    if (m_isPeriodic)
    {
        m_periodicTimer -= diff;
        if (m_periodicTimer <= 0)
        {

            m_periodicTimer += m_modifier.periodictime;
            ++m_periodicTick;
            PeriodicTick();
        }
    }
}

void AreaAura::Update(uint32 diff)
{

    if (GetCasterGuid() == GetTarget()->GetObjectGuid())
    {
        Unit* caster = GetTarget();

        if (!caster->hasUnitState(UNIT_STAT_ISOLATED))
        {
            Unit* owner = caster->GetCharmerOrOwner();
            if (!owner)
            {
                owner = caster;
            }
            Spell::UnitList targets;

            switch (m_areaAuraType)
            {
                case AREA_AURA_PARTY:
                {
                    Group* pGroup = nullptr;

                    if (IsPlayer(owner))
                    {
                        pGroup = ((Player*)owner)->GetGroup();
                    }

                    if (pGroup)
                    {
                        uint8 subgroup = ((Player*)owner)->GetSubGroup();
                        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
                        {
                            Player* Target = itr->getSource();
                            if (Target && Target->IsAlive() && Target->GetSubGroup() == subgroup && IsFriendly(*caster, *Target))
                            {
                                if (InReach(*caster, *Target, m_radius))
                                {
                                    targets.push_back(Target);
                                }
                                Pet* pet = Target->GetPet();
                                if (pet && pet->IsAlive() && InReach(*caster, *pet, m_radius))
                                {
                                    targets.push_back(pet);
                                }
                            }
                        }
                    }
                    else
                    {

                        if (owner != caster && InReach(*caster, *owner, m_radius))
                        {
                            targets.push_back(owner);
                        }

                        Unit* pet = caster->GetPet();
                        if (pet && InReach(*caster, *pet, m_radius))
                        {
                            targets.push_back(pet);
                        }
                    }
                    break;
                }
                case AREA_AURA_PET:
                {
                    if (owner != caster && InReach(*caster, *owner, m_radius))
                    {
                        targets.push_back(owner);
                    }
                    break;
                }
            }

            for (Spell::UnitList::iterator tIter = targets.begin(); tIter != targets.end(); ++tIter)
            {

                bool apply = true;

                SpellEntry const* actualSpellInfo;
                if (GetCasterGuid() == (*tIter)->GetObjectGuid())
                {
                    actualSpellInfo = GetSpellProto();
                }
                else
                {
                    actualSpellInfo = sSpellMgr.SelectAuraRankForLevel(GetSpellProto(), (*tIter)->getLevel());
                }
                if (!actualSpellInfo)
                {
                    continue;
                }

                Unit::SpellAuraHolderBounds spair = (*tIter)->GetSpellAuraHolderBounds(actualSpellInfo->ID);

                for (Unit::SpellAuraHolderMap::const_iterator i = spair.first; i != spair.second; ++i)
                {
                    if (i->second->IsDeleted())
                    {
                        continue;
                    }

                    Aura* aur = i->second->GetAuraByEffectIndex(m_effIndex);

                    if (!aur)
                    {
                        continue;
                    }

                    apply = false;
                    break;
                }

                if (!apply)
                {
                    continue;
                }

                if (cast::RecipeOf(*actualSpellInfo).Says().playersOnly && !IsPlayer(*tIter))
                {
                    continue;
                }

                int32 actualBasePoints = m_currentBasePoints;

                if (actualSpellInfo != GetSpellProto())
                {
                    actualBasePoints = actualSpellInfo->CalculateSimpleValue(m_effIndex);
                }

                SpellAuraHolder* holder = (*tIter)->GetSpellAuraHolder(actualSpellInfo->ID, GetCasterGuid());

                bool addedToExisting = true;
                if (!holder)
                {
                    holder = CreateSpellAuraHolder(actualSpellInfo, (*tIter), caster);
                    addedToExisting = false;
                }

                holder->SetAuraDuration(GetAuraDuration());

                AreaAura* aur = new AreaAura(actualSpellInfo, m_effIndex, &actualBasePoints, holder, (*tIter), caster, nullptr, GetSpellProto()->ID);
                holder->AddAura(aur, m_effIndex);

                if (addedToExisting)
                {
                    (*tIter)->AddAuraToModList(aur);
                    holder->SetInUse(true);
                    aur->ApplyModifier(true, true);
                    holder->SetInUse(false);
                }
                else
                {
                    (*tIter)->AddSpellAuraHolder(holder);
                }

            }
        }
        Aura::Update(diff);
    }
    else
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        uint32 originalRankSpellId = m_originalRankSpellId ? m_originalRankSpellId : GetId();

        Aura::Update(diff);

        bool needFriendly = true;
        if (!caster ||
            caster->hasUnitState(UNIT_STAT_ISOLATED)               ||
            !caster->HasAura(originalRankSpellId, GetEffIndex())   ||
            !InReach(*caster, *target, m_radius)           ||
            IsFriendly(*caster, *target) != needFriendly)
        {
            target->RemoveAuraEffect(GetId(), GetEffIndex(), GetCasterGuid());
        }
        else if (m_areaAuraType == AREA_AURA_PARTY)
        {

            if (caster->GetCharmerOrOwnerGuid() != target->GetObjectGuid() && caster->GetObjectGuid() != target->GetCharmerOrOwnerGuid())
            {
                Player* check = caster->GetCharmerOrOwnerPlayerOrPlayerItself();

                Group* pGroup = check ? check->GetGroup() : nullptr;
                if (pGroup)
                {
                    Player* checkTarget = target->GetCharmerOrOwnerPlayerOrPlayerItself();
                    if (!checkTarget || !pGroup->SameSubGroup(check, checkTarget))
                    {
                        target->RemoveAuraEffect(GetId(), GetEffIndex(), GetCasterGuid());
                    }
                }
                else
                {
                    target->RemoveAuraEffect(GetId(), GetEffIndex(), GetCasterGuid());
                }
            }
        }
        else if (m_areaAuraType == AREA_AURA_PET)
        {
            if (target->GetObjectGuid() != caster->GetCharmerOrOwnerGuid())
            {
                target->RemoveAuraEffect(GetId(), GetEffIndex(), GetCasterGuid());
            }
        }
    }
}

void PersistentAreaAura::Update(uint32 diff)
{
    bool remove = false;

    if (Unit* caster = GetCaster())
    {
        DynamicObject* dynObj = caster->Conjured().AreaOf(GetId(), GetEffIndex());
        if (dynObj)
        {
            if (!InReach(*(GetTarget()), *dynObj, dynObj->GetRadius()))
            {
                remove = true;
                dynObj->RemoveAffected(GetTarget());
            }
        }
        else
        {
            remove = true;
        }
    }
    else
    {
        remove = true;
    }

    Aura::Update(diff);

    if (remove)
    {
        GetTarget()->RemoveAura(GetId(), GetEffIndex());
    }
}

void Aura::ApplyModifier(bool apply, bool Real)
{
    AuraType aura = m_modifier.m_auraname;

    GetHolder()->SetInUse(true);
    SetInUse(true);
    if (aura < TOTAL_AURAS)
    {
        (*this.*AuraHandler [aura])(apply, Real);
    }

    SetInUse(false);
    GetHolder()->SetInUse(false);
}

bool Aura::isAffectedOnSpell(SpellEntry const* spell) const
{
    if (m_spellmod)
    {
        return m_spellmod->isAffectedOnSpell(spell);
    }

    if (spell->SpellClassSet != GetSpellProto()->SpellClassSet)
    {
        return false;
    }

    ClassFamilyMask mask = sSpellMgr.GetSpellAffectMask(GetId(), GetEffIndex());
    return spell->IsFitToFamilyMask(mask);
}

bool Aura::CanProcFrom(SpellEntry const* spell, uint32 EventProcEx, uint32 procEx, bool active, bool useClassMask) const
{

    if (GetId() == spell->ID && !IsPeriodic())
    {
        return false;
    }

    ClassFamilyMask mask = sSpellMgr.GetSpellAffectMask(GetId(), GetEffIndex());

    if (!useClassMask || !mask)
    {
        if (!(EventProcEx & PROC_EX_EX_TRIGGER_ALWAYS))
        {

            if (EventProcEx == PROC_EX_NONE)
            {

                if ((procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) && active)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else
            {

                if ((EventProcEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT) & procEx) && !active)
                {
                    return false;
                }
            }
        }
        return true;
    }
    else
    {

        return mask.IsFitToFamilyMask(spell->SpellClassMask);
    }
}

void Aura::ReapplyAffectedPassiveAuras(Unit* target)
{

    std::map<uint32, ObjectGuid> affectedSelf;

    for (Unit::SpellAuraHolderMap::const_iterator itr = target->GetSpellAuraHolderMap().begin(); itr != target->GetSpellAuraHolderMap().end(); ++itr)
    {

        if (itr->second->IsPassive() && itr->second->IsPermanent() &&

            !itr->second->IsDeleted() && itr->second->GetId() != GetId() &&

            itr->second->GetCasterGuid() == target->GetObjectGuid() &&

            isAffectedOnSpell(itr->second->GetSpellProto()))
        {
            affectedSelf[itr->second->GetId()] = itr->second->GetCastItemGuid();
        }
    }

    if (!affectedSelf.empty())
    {
        Player* pTarget =IsPlayer(target) ? (Player*)target : nullptr;

        for (std::map<uint32, ObjectGuid>::const_iterator map_itr = affectedSelf.begin(); map_itr != affectedSelf.end(); ++map_itr)
        {
            Item* item = pTarget && map_itr->second ? pTarget->GetItemByGuid(map_itr->second) : nullptr;
            target->RemoveAuras(map_itr->first);
            target->CastSpell(target, map_itr->first, true, item);
        }
    }
}

struct ReapplyAffectedPassiveAurasHelper
{
    explicit ReapplyAffectedPassiveAurasHelper(Aura* _aura) : aura(_aura) {}
    void operator()(Unit* unit) const { aura->ReapplyAffectedPassiveAuras(unit); }
    Aura* aura;
};

void Aura::ReapplyAffectedPassiveAuras()
{

    if (GetSpellProto()->ProcCharges)
    {
        return;
    }

    switch (m_modifier.m_miscvalue)
    {
        case SPELLMOD_DURATION:
        case SPELLMOD_CHARGES:
        case SPELLMOD_NOT_LOSE_CASTING_TIME:
        case SPELLMOD_CASTING_TIME:
        case SPELLMOD_COOLDOWN:
        case SPELLMOD_COST:
        case SPELLMOD_ACTIVATION_TIME:
        case SPELLMOD_CASTING_TIME_OLD:
        case SPELLMOD_SPEED:
        case SPELLMOD_HASTE:
        case SPELLMOD_ATTACK_POWER:
            return;
    }

    ReapplyAffectedPassiveAuras(GetTarget());

    GetTarget()->CallForAllControlledUnits(ReapplyAffectedPassiveAurasHelper(this), CONTROLLED_PET | CONTROLLED_TOTEMS);
}

void Aura::TriggerSpell()
{
    ObjectGuid casterGUID = GetCasterGuid();
    Unit* triggerTarget = GetTriggerTarget();

    if (!casterGUID || !triggerTarget)
    {
        return;
    }

    uint32 trigger_spell_id = Operation().triggerSpell;

    SpellEntry const* triggeredSpellInfo = sSpellStore.LookupEntry(trigger_spell_id);
    SpellEntry const* auraSpellInfo = GetSpellProto();
    uint32 auraId = auraSpellInfo->ID;
    Unit* target = GetTarget();
    Unit* triggerCaster = triggerTarget;
    Occupant* triggerTargetObject = nullptr;

    if (triggeredSpellInfo == nullptr)
    {
        switch (auraSpellInfo->SpellClassSet)
        {
            case SPELLFAMILY_GENERIC:
            {
                switch (auraId)
                {

                    case 9712:
                        if (Unit* caster = GetCaster())
                        {
                            caster->CastSpell(caster, 21029, true);
                        }
                        return;
                    case 23170:
                    {
                        target->CastSpell(target, 23171, true, nullptr, this);
                        return;
                    }
                    case 23493:
                    {
                        uint32 heal = triggerTarget->GetMaxHealth() / 10;
                        triggerTarget->DealHeal(triggerTarget, heal, auraSpellInfo);

                        if (int32 mana = triggerTarget->GetMaxPower(POWER_MANA))
                        {
                            mana /= 10;
                            triggerTarget->EnergizeBySpell(triggerTarget, 23493, mana, POWER_MANA);
                        }
                        return;
                    }

                    case 24834:
                    {
                        uint32 spellForTick[8] = { 24820, 24821, 24822, 24823, 24835, 24836, 24837, 24838 };
                        uint32 tick = (GetAuraTicks() + 7) % 8;

                        float forward = target->Where().Facing();
                        if (tick <= 3)
                        {
                            target->Place().Face(forward + 0.75f * M_PI_F - tick * M_PI_F / 8);
                        }
                        else
                        {
                            target->Place().Face(forward - 0.75f * M_PI_F + (8 - tick) * M_PI_F / 8);
                        }

                        triggerTarget->CastSpell(triggerTarget, spellForTick[tick], true, nullptr, this, casterGUID);
                        target->Place().Face(forward);
                        return;
                    }

                    case 25371:
                    {
                        int32 bpDamage = triggerTarget->GetMaxHealth() * 10 / 100;
                        triggerTarget->CastCustomSpell(triggerTarget, 25373, &bpDamage, nullptr, nullptr, true, nullptr, this, casterGUID);
                        return;
                    }
                    case 26009:
                    case 26136:
                    {
                        float newAngle = target->Where().Facing();

                        if (auraId == 26009)
                        {
                            newAngle += M_PI_F / 40;
                        }
                        else
                        {
                            newAngle -= M_PI_F / 40;
                        }

                        newAngle = Geometry::Placement::NormalizeOrientation(newAngle);

                        target->SetFacingTo(newAngle);

                        target->CastSpell(target, 26029, true);
                        return;
                    }

                    case 27808:
                    {
                        int32 bpDamage = triggerTarget->GetMaxHealth() * 26 / 100;
                        triggerTarget->CastCustomSpell(triggerTarget, 29879, &bpDamage, nullptr, nullptr, true, nullptr, this, casterGUID);
                        return;
                    }

                    case 27819:
                    {

                        int32 bpDamage = (int32)triggerTarget->GetPower(POWER_MANA) / 2;
                        triggerTarget->ModifyPower(POWER_MANA, -bpDamage);
                        triggerTarget->CastCustomSpell(triggerTarget, 27820, &bpDamage, nullptr, nullptr, true, nullptr, this, triggerTarget->GetObjectGuid());
                        return;
                    }

                    case 28096:
                    case 28111:
                    {

                        Unit* pCaster = GetCaster();
                        if (pCaster &&IsCreature(pCaster) && !InReach(*pCaster, *target, 60.0f))
                        {
                            pCaster->InterruptNonMeleeSpells(true);
                            ((Creature*)pCaster)->SetInCombatWithZone();

                            pCaster->CastSpell(pCaster, auraId == 28096 ? 28097 : 28109, true, nullptr, nullptr, target->GetObjectGuid());
                        }
                        return;
                    }

                    default:
                        break;
                }
                break;
            }

            case SPELLFAMILY_DRUID:
            {
                switch (auraId)
                {
                    case 768:

                        return;
                    case 22842:
                    case 22895:
                    case 22896:
                    {
                        int32 LifePerRage = GetModifier()->m_amount;

                        int32 lRage = target->GetPower(POWER_RAGE);
                        if (lRage > 100)
                        {
                            lRage = 100;
                        }
                        target->ModifyPower(POWER_RAGE, -lRage);
                        int32 FRTriggerBasePoints = int32(lRage * LifePerRage / 10);
                        target->CastCustomSpell(target, 22845, &FRTriggerBasePoints, nullptr, nullptr, true, nullptr, this);
                        return;
                    }
                    default:
                        break;
                }
                break;
            }

            default:
                break;
        }

        triggeredSpellInfo = sSpellStore.LookupEntry(trigger_spell_id);
    }
    else
    {

        if ((cast::RecipeOf(*GetSpellProto()).Starts() == cast::Start::Channelled) && Operation().verb != SPELL_EFFECT_PERSISTENT_AREA_AURA)
        {

            if (target->GetObjectGuid() == casterGUID)
            {
                triggerCaster = target;

                if (Occupant* channelTarget = target->GetMap()->GetOccupant(target->GetChannelObjectGuid()))
                {
                    if (IsType(channelTarget, TYPEMASK_UNIT))
                    {
                        triggerTarget = (Unit*)channelTarget;
                    }
                    else
                    {
                        triggerTargetObject = channelTarget;
                    }
                }
            }

            else if (Unit* caster = GetCaster())
            {
                if (target->GetObjectGuid() == caster->GetChannelObjectGuid())
                {
                    triggerCaster = caster;
                    triggerTarget = target;
                }
            }
        }

        switch (auraId)
        {
            case 9347:
            {
                if (!IsCreature(target))
                {
                    return;
                }

                triggerTarget = ((Creature*)target)->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 0, triggeredSpellInfo);
                if (!triggerTarget)
                {
                    return;
                }

                break;
            }
            case 1010:
            {

                if (casterGUID == triggerTarget->GetObjectGuid())
                {
                    return;
                }

                int32 intelectLoss = 0;
                int32 spiritLoss = 0;

                const auto mModStat = triggerTarget->GetAurasByType(SPELL_AURA_MOD_STAT);
                for (auto* aura : mModStat)
                {
                    if (aura->GetId() == 1010)
                    {
                        switch (aura->GetModifier()->m_miscvalue)
                        {
                            case STAT_INTELLECT: intelectLoss += aura->GetModifier()->m_amount; break;
                            case STAT_SPIRIT:    spiritLoss   += aura->GetModifier()->m_amount; break;
                            default: break;
                        }
                    }
                }

                if (intelectLoss <= -90 && spiritLoss <= -90)
                {
                    return;
                }

                break;
            }
            case 16191:
            {
                triggerTarget->CastCustomSpell(triggerTarget, trigger_spell_id, &m_modifier.m_amount, nullptr, nullptr, true, nullptr, this);
                return;
            }
            case 19695:
            {
                int32 damageForTick[8] = { 500, 500, 1000, 1000, 2000, 2000, 3000, 5000 };
                triggerTarget->CastCustomSpell(triggerTarget, 19698, &damageForTick[GetAuraTicks() - 1], nullptr, nullptr, true, nullptr);
                return;
            }
        }
    }

    if (triggeredSpellInfo)
    {
        if (triggerTargetObject)
        {
            triggerCaster->CastSpell(triggerTargetObject->Where().X(), triggerTargetObject->Where().Y(), triggerTargetObject->Where().Z(),
                triggeredSpellInfo, true, nullptr, this, casterGUID);
        }
        else
        {
            triggerCaster->CastSpell(triggerTarget, triggeredSpellInfo, true, nullptr, this, casterGUID);
        }
    }
    else
    {
        if (Unit* caster = GetCaster())
        {
            if (!IsCreature(triggerTarget) || !sScriptMgr.OnEffectDummy(caster, GetId(), GetEffIndex(), (Creature*)triggerTarget, 0))
            {
                sLog.outError("Aura::TriggerSpell: Spell %u have 0 in EffectTriggered[%d], not handled custom case?", GetId(), GetEffIndex());
            }
        }
    }
}

void Aura::HandleModDetectRange(bool apply, bool Real)
{
    switch (GetId())
    {

        case 9901:
        case 8955:
        case 2908:
        {
            if (apply)
            {
                Aura* A = CreateAura(GetSpellProto(), EFFECT_INDEX_1, 0, m_spellAuraHolder, GetTarget(), GetCaster());
                m_spellAuraHolder->AddAura(A, EFFECT_INDEX_1);
                A->m_modifier.m_miscvalue = SPELL_SCHOOL_MASK_NATURE;
                A->m_modifier.periodictime = 0;
                A->m_modifier.m_auraname = SPELL_AURA_MOD_DETECT_RANGE;

                A->m_modifier.m_amount = -10;

            }
        }
        break;
        default:
            break;
    }
}

void Aura::HandleAurasVisible(bool apply, bool )
{
    GetTarget()->ApplyUnitFlag(UNIT_FLAG_AURAS_VISIBLE, apply);
}

void Aura::HandleAuraModBlockPercent(bool , bool )
{
    if (!IsPlayer(GetTarget()))
    {
        return;
    }

    ((Player*)GetTarget())->Sheet().Block();

}

void Aura::HandleShieldBlockValue(bool apply, bool )
{
    BaseModType modType = FLAT_MOD;
    if (m_modifier.m_auraname == SPELL_AURA_MOD_SHIELD_BLOCKVALUE_PCT)
    {
        modType = PCT_MOD;
    }

    if (IsPlayer(GetTarget()))
    {
        ((Player*)GetTarget())->HandleBaseModValue(SHIELD_BLOCK_VALUE, modType, float(m_modifier.m_amount), apply);
    }
}

void Aura::PeriodicTick()
{
    Unit* target = GetTarget();
    SpellEntry const* spellProto = GetSpellProto();

    switch (m_modifier.m_auraname)
    {
        case SPELL_AURA_PERIODIC_DAMAGE:
        case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
        {

            if (!target->IsAlive())
            {
                return;
            }

            Unit* pCaster = GetCaster();
            if (!pCaster)
            {
                return;
            }

            if (Operation().verb == SPELL_EFFECT_PERSISTENT_AREA_AURA &&
                pCaster->SpellHitResult(target, spellProto, false) != SPELL_MISS_NONE)
            {
                return;
            }

            if (target->IsImmuneToDamage(GetSpellSchoolMask(spellProto)))
            {
                return;
            }

            uint32 absorb = 0;
            uint32 resist = 0;
            CleanDamage cleanDamage =  CleanDamage(0, BASE_ATTACK, MELEE_HIT_NORMAL);

            uint32 amount = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            uint32 pdamage;

            if (m_modifier.m_auraname == SPELL_AURA_PERIODIC_DAMAGE)
            {
                pdamage = amount;
            }
            else
            {
                pdamage = uint32(target->GetMaxHealth() * amount / 100);
            }

            if (spellProto->DefenseType == SPELL_DAMAGE_CLASS_NONE || spellProto->DefenseType == SPELL_DAMAGE_CLASS_MAGIC)
            {
                pdamage = target->SpellDamageBonusTaken(pCaster, spellProto, pdamage, DOT, GetStackAmount());
            }

            else
            {
                WeaponAttackType attackType = cast::RecipeOf(*spellProto).Swings();
                pdamage = target->MeleeDamageBonusTaken(pCaster, pdamage, attackType, spellProto, DOT, GetStackAmount());
            }

            if (GetSpellSchoolMask(spellProto) & SPELL_SCHOOL_MASK_NORMAL &&
                GetEffectMechanic(spellProto, m_effIndex) != MECHANIC_BLEED)
            {
                uint32 pdamageReductedArmor = pCaster->CalcArmorReducedDamage(target, pdamage);
                cleanDamage.damage += pdamage - pdamageReductedArmor;
                pdamage = pdamageReductedArmor;
            }

            if (spellProto->SpellClassSet == SPELLFAMILY_WARLOCK && (spellProto->SpellClassMask & UI64LIT(0x0000000000000400)) && spellProto->SpellIconID == 544)
            {

                if (GetAuraTicks() <= 4)
                {
                    pdamage = pdamage / 2;
                }

                else if (GetAuraTicks() >= 9)
                {
                    pdamage += (pdamage + 1) / 2;
                }

            }

            target->CalculateDamageAbsorbAndResist(pCaster, GetSpellSchoolMask(spellProto), DOT, pdamage, &absorb, &resist, !cast::RecipeOf(*spellProto).Says().ignoresLineOfSight);

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s attacked %s for %u dmg inflicted by %u",
                GuidString(GetCasterGuid()).c_str(), target->GetGuidStr().c_str(), pdamage, GetId());

            pCaster->DealDamageMods(target, pdamage, &absorb);

            uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC;
            uint32 procVictim   = PROC_FLAG_ON_TAKE_PERIODIC;
            pdamage = (pdamage <= absorb + resist) ? 0 : (pdamage - absorb - resist);

            SpellPeriodicAuraLogInfo pInfo(this, pdamage, absorb, resist, 0.0f);
            target->SendPeriodicAuraLog(&pInfo);

            if (pdamage)
            {
                procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;
            }

            pCaster->ProcDamageAndSpell(target, procAttacker, procVictim, PROC_EX_NORMAL_HIT, pdamage, BASE_ATTACK, spellProto);

            pCaster->DealDamage(target, pdamage, &cleanDamage, DOT, GetSpellSchoolMask(spellProto), spellProto, true);
            break;
        }
        case SPELL_AURA_PERIODIC_LEECH:
        case SPELL_AURA_PERIODIC_HEALTH_FUNNEL:
        {

            if (!target->IsAlive())
            {
                return;
            }

            Unit* pCaster = GetCaster();
            if (!pCaster)
            {
                return;
            }

            if (!pCaster->IsAlive())
            {
                return;
            }

            if (Operation().verb == SPELL_EFFECT_PERSISTENT_AREA_AURA &&
                pCaster->SpellHitResult(target, spellProto, false) != SPELL_MISS_NONE)
            {
                return;
            }

            if (target->IsImmuneToDamage(GetSpellSchoolMask(spellProto)))
            {
                return;
            }

            uint32 absorb = 0;
            uint32 resist = 0;
            CleanDamage cleanDamage =  CleanDamage(0, BASE_ATTACK, MELEE_HIT_NORMAL);

            uint32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            if (GetSpellSchoolMask(spellProto) & SPELL_SCHOOL_MASK_NORMAL)
            {
                uint32 pdamageReductedArmor = pCaster->CalcArmorReducedDamage(target, pdamage);
                cleanDamage.damage += pdamage - pdamageReductedArmor;
                pdamage = pdamageReductedArmor;
            }

            pdamage = target->SpellDamageBonusTaken(pCaster, spellProto, pdamage, DOT, GetStackAmount());

            target->CalculateDamageAbsorbAndResist(pCaster, GetSpellSchoolMask(spellProto), DOT, pdamage, &absorb, &resist, !cast::RecipeOf(*spellProto).Says().ignoresLineOfSight);

            if (target->GetHealth() < pdamage)
            {
                pdamage = uint32(target->GetHealth());
            }

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s health leech of %s for %u dmg inflicted by %u abs is %u",
                GuidString(GetCasterGuid()).c_str(), target->GetGuidStr().c_str(), pdamage, GetId(), absorb);

            pCaster->DealDamageMods(target, pdamage, &absorb);

            pCaster->SendSpellNonMeleeDamageLog(target, GetId(), pdamage, GetSpellSchoolMask(spellProto), absorb, resist, false, 0);

            float multiplier = Operation().amplitude > 0 ? Operation().amplitude : 1;

            uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC;
            uint32 procVictim   = PROC_FLAG_ON_TAKE_PERIODIC;

            pdamage = (pdamage <= absorb + resist) ? 0 : (pdamage - absorb - resist);
            if (pdamage)
            {
                procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;
            }

            pCaster->ProcDamageAndSpell(target, procAttacker, procVictim, PROC_EX_NORMAL_HIT, pdamage, BASE_ATTACK, spellProto);
            int32 new_damage = pCaster->DealDamage(target, pdamage, &cleanDamage, DOT, GetSpellSchoolMask(spellProto), spellProto, false);

            if (!target->IsAlive() && pCaster->IsNonMeleeSpellCasted(false))
            {
                for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
                {
                    if (Spell* spell = pCaster->GetCurrentSpell(CurrentSpellTypes(i)))
                    {
                        if (spell->m_spellInfo->ID == GetId())
                        {
                            spell->cancel();
                        }
                    }
                }
            }

            if (Player* modOwner = pCaster->GetSpellModOwner())
            {
                modOwner->SpellMods().Apply(GetId(), SPELLMOD_MULTIPLE_VALUE, multiplier);
            }

            uint32 heal = pCaster->SpellHealingBonusTaken(pCaster, spellProto, int32(new_damage * multiplier), DOT, GetStackAmount());

            int32 gain = pCaster->DealHeal(pCaster, heal, spellProto);
            pCaster->GetHostileRefManager().threatAssist(pCaster, gain * 0.5f * cast::RecipeOf(*spellProto).ThreatMultiplier(), spellProto);
            break;
        }
        case SPELL_AURA_PERIODIC_HEAL:
        case SPELL_AURA_OBS_MOD_HEALTH:
        {

            if (!target->IsAlive())
            {
                return;
            }

            Unit* pCaster = GetCaster();
            if (!pCaster)
            {
                return;
            }

            if (target->GetHealth() == target->GetMaxHealth())
            {
                return;
            }

            if (target != pCaster && spellProto->SpellVisualID == 163 && !pCaster->IsAlive())
            {
                return;
            }

            uint32 amount = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            uint32 pdamage;

            if (m_modifier.m_auraname == SPELL_AURA_OBS_MOD_HEALTH)
            {
                pdamage = uint32(target->GetMaxHealth() * amount / 100);
            }
            else
            {
                pdamage = amount;
            }

            pdamage = target->SpellHealingBonusTaken(pCaster, spellProto, pdamage, DOT, GetStackAmount());

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s heal of %s for %u health inflicted by %u",
                GuidString(GetCasterGuid()).c_str(), target->GetGuidStr().c_str(), pdamage, GetId());

            int32 gain = target->ModifyHealth(pdamage);
            SpellPeriodicAuraLogInfo pInfo(this, pdamage, 0, 0, 0.0f);
            target->SendPeriodicAuraLog(&pInfo);

            uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC;
            uint32 procVictim   = PROC_FLAG_ON_TAKE_PERIODIC;
            uint32 procEx = PROC_EX_NORMAL_HIT | PROC_EX_PERIODIC_POSITIVE;
            pCaster->ProcDamageAndSpell(target, procAttacker, procVictim, procEx, gain, BASE_ATTACK, spellProto);

            target->GetHostileRefManager().threatAssist(pCaster, float(gain) * 0.5f * cast::RecipeOf(*spellProto).ThreatMultiplier(), spellProto);

            if (target != pCaster && spellProto->SpellVisualID == 163)
            {
                uint32 dmg = spellProto->ManaPerSecond;
                if (pCaster->GetHealth() <= dmg &&IsPlayer(pCaster))
                {
                    pCaster->RemoveAuras(GetId());

                    pCaster->FinishSpell(CURRENT_GENERIC_SPELL);
                    pCaster->FinishSpell(CURRENT_CHANNELED_SPELL);
                }
                else
                {
                    uint32 damage = gain;
                    uint32 absorb = 0;
                    pCaster->DealDamageMods(pCaster, damage, &absorb);
                    pCaster->SendSpellNonMeleeDamageLog(pCaster, GetId(), damage, GetSpellSchoolMask(spellProto), absorb, 0, false, 0, false);

                    CleanDamage cleanDamage =  CleanDamage(0, BASE_ATTACK, MELEE_HIT_NORMAL);
                    pCaster->DealDamage(pCaster, damage, &cleanDamage, NODAMAGE, GetSpellSchoolMask(spellProto), spellProto, true);
                }
            }
            break;
        }
        case SPELL_AURA_PERIODIC_MANA_LEECH:
        {

            if (!target->IsAlive())
            {
                return;
            }

            if (m_modifier.m_miscvalue < 0 || m_modifier.m_miscvalue >= MAX_POWERS)
            {
                return;
            }

            Powers power = Powers(m_modifier.m_miscvalue);

            if (target->GetPowerType() != power)
            {
                return;
            }

            Unit* pCaster = GetCaster();
            if (!pCaster)
            {
                return;
            }

            if (!pCaster->IsAlive())
            {
                return;
            }

            if (Operation().verb == SPELL_EFFECT_PERSISTENT_AREA_AURA &&
                pCaster->SpellHitResult(target, spellProto, false) != SPELL_MISS_NONE)
            {
                return;
            }

            if (target->IsImmuneToDamage(GetSpellSchoolMask(spellProto)))
            {
                return;
            }

            uint32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s power leech of %s for %u dmg inflicted by %u",
                GuidString(GetCasterGuid()).c_str(), target->GetGuidStr().c_str(), pdamage, GetId());

            int32 drain_amount = target->GetPower(power) > pdamage ? pdamage : target->GetPower(power);

            target->ModifyPower(power, -drain_amount);

            float gain_multiplier = 0;

            if (pCaster->GetMaxPower(power) > 0)
            {
                gain_multiplier = Operation().amplitude;

                if (Player* modOwner = pCaster->GetSpellModOwner())
                {
                    modOwner->SpellMods().Apply(GetId(), SPELLMOD_MULTIPLE_VALUE, gain_multiplier);
                }
            }

            SpellPeriodicAuraLogInfo pInfo(this, drain_amount, 0, 0, gain_multiplier);
            target->SendPeriodicAuraLog(&pInfo);

            int32 gain_amount = int32(drain_amount * gain_multiplier);

            if (gain_amount)
            {
                int32 gain = pCaster->ModifyPower(power, gain_amount);
                target->AddThreat(pCaster, float(gain) * 0.5f, false, GetSpellSchoolMask(spellProto), spellProto);
            }

            switch (GetId())
            {
                case 21056:
                    if (IsPlayer(target) && target->GetPower(power) == 0)
                    {
                        target->CastSpell(target, 21058, true, nullptr, this);
                        target->RemoveAuras(GetId());
                    }
                    break;
            }
            break;
        }
        case SPELL_AURA_PERIODIC_ENERGIZE:
        {

            if (!target->IsAlive())
            {
                return;
            }

            uint32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s energize %s for %u dmg inflicted by %u",
                GuidString(GetCasterGuid()).c_str(), target->GetGuidStr().c_str(), pdamage, GetId());

            if (m_modifier.m_miscvalue < 0 || m_modifier.m_miscvalue >= MAX_POWERS)
            {
                break;
            }

            Powers power = Powers(m_modifier.m_miscvalue);

            if (target->GetMaxPower(power) == 0)
            {
                break;
            }

            SpellPeriodicAuraLogInfo pInfo(this, pdamage, 0, 0, 0.0f);
            target->SendPeriodicAuraLog(&pInfo);

            int32 gain = target->ModifyPower(power, pdamage);

            if (Unit* pCaster = GetCaster())
            {
                target->GetHostileRefManager().threatAssist(pCaster, float(gain) * 0.5f * cast::RecipeOf(*spellProto).ThreatMultiplier(), spellProto);
            }
            break;
        }
        case SPELL_AURA_OBS_MOD_MANA:
        {

            if (!target->IsAlive())
            {
                return;
            }

            uint32 amount = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            uint32 pdamage = uint32(target->GetMaxPower(POWER_MANA) * amount / 100);

            DETAIL_FILTER_LOG(LOG_FILTER_PERIODIC_AFFECTS, "PeriodicTick: %s energize %s for %u mana inflicted by %u",
                GuidString(GetCasterGuid()).c_str(), target->GetGuidStr().c_str(), pdamage, GetId());

            if (target->GetMaxPower(POWER_MANA) == 0)
            {
                break;
            }

            SpellPeriodicAuraLogInfo pInfo(this, pdamage, 0, 0, 0.0f);
            target->SendPeriodicAuraLog(&pInfo);

            int32 gain = target->ModifyPower(POWER_MANA, pdamage);

            if (Unit* pCaster = GetCaster())
            {
                target->GetHostileRefManager().threatAssist(pCaster, float(gain) * 0.5f * cast::RecipeOf(*spellProto).ThreatMultiplier(), spellProto);
            }
            break;
        }
        case SPELL_AURA_POWER_BURN_MANA:
        {

            if (!target->IsAlive())
            {
                return;
            }

            Unit* pCaster = GetCaster();
            if (!pCaster)
            {
                return;
            }

            if (target->IsImmuneToDamage(GetSpellSchoolMask(spellProto)))
            {
                return;
            }

            int32 pdamage = m_modifier.m_amount > 0 ? m_modifier.m_amount : 0;

            Powers powerType = Powers(m_modifier.m_miscvalue);

            if (!target->IsAlive() || target->GetPowerType() != powerType)
            {
                return;
            }

            uint32 gain = uint32(-target->ModifyPower(powerType, -pdamage));

            gain = uint32(gain * Operation().amplitude);

            SpellNonMeleeDamage damageInfo(pCaster, target, spellProto->ID, SpellSchools(spellProto->School));
            pCaster->CalculateSpellDamage(&damageInfo, gain, spellProto);

            damageInfo.target->CalculateAbsorbResistBlock(pCaster, &damageInfo, spellProto);

            pCaster->DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);

            pCaster->SendSpellNonMeleeDamageLog(&damageInfo);

            uint32 procAttacker = PROC_FLAG_ON_DO_PERIODIC;
            uint32 procVictim   = PROC_FLAG_ON_TAKE_PERIODIC;
            uint32 procEx       = createProcExtendMask(&damageInfo, SPELL_MISS_NONE);
            if (damageInfo.damage)
            {
                procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;
            }

            pCaster->ProcDamageAndSpell(damageInfo.target, procAttacker, procVictim, procEx, damageInfo.damage, BASE_ATTACK, spellProto);

            pCaster->DealSpellDamage(&damageInfo, true);
            break;
        }
        case SPELL_AURA_MOD_REGEN:
        {

            if (!target->IsAlive())
            {
                return;
            }

            int32 gain = target->ModifyHealth(m_modifier.m_amount);
            if (Unit* caster = GetCaster())
            {
                target->GetHostileRefManager().threatAssist(caster, float(gain) * 0.5f  * cast::RecipeOf(*spellProto).ThreatMultiplier(), spellProto);
            }
            break;
        }
        case SPELL_AURA_MOD_POWER_REGEN:
        {

            if (!target->IsAlive())
            {
                return;
            }

            Powers powerType = target->GetPowerType();
            if (int32(powerType) != m_modifier.m_miscvalue)
            {
                return;
            }

            if (spellProto->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
            {

                target->HandleEmoteCommand(EMOTE_ONESHOT_EAT);
            }

            if (powerType == POWER_RAGE)
            {
                static_cast<Player*>(target)->m_rageDecayMultiplier = m_modifier.m_amount;
            }
            break;
        }

        case SPELL_AURA_DUMMY:
        {
            PeriodicDummyTick();
            break;
        }
        case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
        {
            TriggerSpell();
            break;
        }
        default:
            break;
    }
}

void Aura::PeriodicDummyTick()
{
    SpellEntry const* spell = GetSpellProto();
    Unit* target = GetTarget();

    switch (spell->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (spell->ID)
            {

                case 7054:
                {

                    return;
                }
                case 7057:
                    if (roll_chance_i(33))
                    {
                        target->CastSpell(target, m_modifier.m_amount, true, nullptr, this);
                    }
                    return;
            }
            break;
        }
        default:
            break;
    }

    if (Unit* caster = GetCaster())
    {
        if (target &&IsCreature(target))
        {
            sScriptMgr.OnEffectDummy(caster, GetId(), GetEffIndex(), (Creature*)target, 0);
        }
    }
}

bool Aura::IsLastAuraOnHolder()
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (i != GetEffIndex() && GetHolder()->m_auras[i])
        {
            return false;
        }
    }
    return true;
}

void Aura::HandleInterruptRegen(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    if (GetSpellProto()->ID != 5229 && GetSpellProto()->ID != 29131)
    {
        return;
    }

    GetTarget()->SetInDummyCombatState(apply);
}

SpellAuraHolder::SpellAuraHolder(SpellEntry const* spellproto, Unit* target, Occupant* caster, Item* castItem)
    : m_spellProto(spellproto),
    m_target(target), m_castItemGuid(castItem ? castItem->GetObjectGuid() : 0),
    m_auraSlot(MAX_AURAS), m_auraLevel(1),
    m_procCharges(0), m_stackAmount(1),
    m_timeCla(1000), m_removeMode(AURA_REMOVE_BY_DEFAULT), m_AuraDRGroup(DIMINISHING_NONE),
    m_permanent(false), m_isRemovedOnShapeLost(true), m_deleted(false), m_in_use(0)
{
    MANGOS_ASSERT(target);
    MANGOS_ASSERT(spellproto && spellproto == sSpellStore.LookupEntry(spellproto->ID));

    if (!caster)
    {
        m_casterGuid = target->GetObjectGuid();
    }
    else
    {

        MANGOS_ASSERT(IsType(caster, TYPEMASK_UNIT));
        m_casterGuid = caster->GetObjectGuid();
    }

    m_recipe         = &cast::RecipeOf(*spellproto);
    m_applyTime      = time(nullptr);
    m_isPassive      = (m_recipe->Starts() == cast::Start::Passive);
    m_isDeathPersist = IsDeathPersistentSpell(spellproto);
    m_trackedAuraType = IsSingleTargetSpell(spellproto) ? TRACK_AURA_TYPE_SINGLE_TARGET : TRACK_AURA_TYPE_NOT_TRACKED;
    m_procCharges    = spellproto->ProcCharges;

    m_isRemovedOnShapeLost = (GetCasterGuid() == m_target->GetObjectGuid() &&
        m_spellProto->ShapeshiftMask &&
        !Recipe().Says().worksWithoutShapeshift &&
        !Recipe().Says().notWhileShapeshifted);

    Unit* unitCaster = caster && IsType(caster, TYPEMASK_UNIT) ? (Unit*)caster : nullptr;

    m_duration = m_maxDuration = CalculateSpellDuration(spellproto, unitCaster);

    if (m_maxDuration == -1 || (m_isPassive && spellproto->DurationIndex == 0))
    {
        m_permanent = true;
    }

    if (unitCaster)
    {
        if (Player* modOwner = unitCaster->GetSpellModOwner())
        {
            modOwner->SpellMods().Apply(GetId(), SPELLMOD_CHARGES, m_procCharges);
        }
    }

    switch (m_spellProto->ID)
    {

        case 24575:
        case 24659:
        case 24662:
        case 26464:
            m_stackAmount = m_spellProto->CumulativeAura;
            break;
    }

    m_isHeartbeatSubject = (m_spellProto->Attributes & SPELL_ATTR_HEARTBEAT_RESIST_CHECK) && caster != target &&IsPlayer(caster) &&IsPlayer(target) && !(cast::RecipeOf(*m_spellProto).Starts() == cast::Start::Channelled);

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        m_auras[i] = nullptr;
    }
}

void SpellAuraHolder::AddAura(Aura* aura, SpellEffectIndex index)
{
    m_auras[index] = aura;
}

void SpellAuraHolder::RemoveAura(SpellEffectIndex index)
{
    m_auras[index] = nullptr;
}

void SpellAuraHolder::ApplyAuraModifiers(bool apply, bool real)
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX && !IsDeleted(); ++i)
    {
        if (Aura* aur = GetAuraByEffectIndex(SpellEffectIndex(i)))
        {
            aur->ApplyModifier(apply, real);
        }
    }
}

void SpellAuraHolder::_AddSpellAuraHolder()
{
    if (!GetId())
    {
        return;
    }
    if (!m_target)
    {
        return;
    }

    uint8 slot = NULL_AURA_SLOT;
    Unit* caster = GetCaster();

    if (IsNeedVisibleSlot(caster))
    {
        if (IsPositive())
        {
            for (uint8 i = 0; i < MAX_POSITIVE_AURAS; i++)
            {
                if (m_target->GetUInt32Value((uint16)(UNIT_FIELD_AURA + i)) == 0)
                {
                    slot = i;
                    break;
                }
            }
        }
        else
        {
            for (uint8 i = MAX_POSITIVE_AURAS; i < MAX_AURAS; i++)
            {
                if (m_target->GetUInt32Value((uint16)(UNIT_FIELD_AURA + i)) == 0)
                {
                    slot = i;
                    break;
                }
            }
        }
    }

    if (caster &&IsPlayer(caster))
    {
        if (Recipe().Says().spentWhileActive)
        {
            Item* castItem = m_castItemGuid ? ((Player*)caster)->GetItemByGuid(m_castItemGuid) : nullptr;
            ((Player*)caster)->AddSpellAndCategoryCooldowns(m_spellProto, castItem ? castItem->GetEntry() : 0, nullptr, true);
        }
    }

    SetAuraSlot(slot);

    if (slot < MAX_AURAS)
    {
        SetAura(slot, false);
        SetAuraFlag(slot, true);
        SetAuraLevel(slot, caster ? caster->getLevel() : sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
        UpdateAuraApplication();

        m_target->UpdateAuraForGroup(slot);

        UpdateAuraDuration();
    }

    if (m_spellProto->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED && !m_target->IsSitState())
    {
        m_target->SetStandState(UNIT_STAND_STATE_SIT);
    }

    if (getDiminishGroup() != DIMINISHING_NONE)
    {
        m_target->Diminishing().Hold(getDiminishGroup());
    }

    if (IsSealSpell(GetSpellProto()))
    {
        m_target->ModifyAuraState(AURA_STATE_JUDGEMENT, true);
    }
}

void SpellAuraHolder::_RemoveSpellAuraHolder()
{

    if (m_removeMode != AURA_REMOVE_BY_STACK)
    {
        CleanupTriggeredSpells();
    }

    Unit* caster = GetCaster();

    if (caster && IsPersistent())
    {
        DynamicObject* dynObj = caster->Conjured().AreaOf(GetId());
        if (dynObj)
        {
            dynObj->RemoveAffected(m_target);
        }
    }

    uint8 slot = GetAuraSlot();

    if (slot >= MAX_AURAS)
    {
        return;
    }

    if (m_target->GetUInt32Value((uint16)(UNIT_FIELD_AURA + slot)) == 0)
    {
        return;
    }

    if (getDiminishGroup() != DIMINISHING_NONE)
    {
        m_target->Diminishing().Release(getDiminishGroup(), GameTime::GetGameTimeMS());
    }

    SetAura(slot, true);
    SetAuraFlag(slot, false);
    SetAuraLevel(slot, caster ? caster->getLevel() : sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));

    m_procCharges = 0;
    m_stackAmount = 1;
    UpdateAuraApplication();

    if (m_removeMode != AURA_REMOVE_BY_DELETE)
    {

        m_target->UpdateAuraForGroup(slot);

        uint32 removeState = 0;
        ClassFamilyMask removeFamilyFlag = m_spellProto->SpellClassMask;
        switch (m_spellProto->SpellClassSet)
        {
            case SPELLFAMILY_PALADIN:
                if (IsSealSpell(m_spellProto))
                {
                    removeState = AURA_STATE_JUDGEMENT;
                }
                break;
        }

        if (removeState)
        {
            bool found = false;
            Unit::SpellAuraHolderMap const& holders = m_target->GetSpellAuraHolderMap();
            for (Unit::SpellAuraHolderMap::const_iterator i = holders.begin(); i != holders.end(); ++i)
            {
                SpellEntry const* auraSpellInfo = (*i).second->GetSpellProto();
                if (auraSpellInfo->IsFitToFamily(SpellFamily(m_spellProto->SpellClassSet), removeFamilyFlag))
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                m_target->ModifyAuraState(AuraState(removeState), false);
            }
        }

        if (caster &&IsPlayer(caster))
        {
            if (Recipe().Says().spentWhileActive)

            {
                ((Player*)caster)->SendCooldownEvent(GetSpellProto());
            }
        }
    }
}

void SpellAuraHolder::CleanupTriggeredSpells()
{
    for (const auto& operation : Recipe().Does())
    {
        if (!operation.aura)
        {
            continue;
        }

        uint32 tSpellId = operation.triggerSpell;
        if (!tSpellId)
        {
            continue;
        }

        SpellEntry const* tProto = sSpellStore.LookupEntry(tSpellId);
        if (!tProto)
        {
            continue;
        }

        if (cast::RecipeOf(*tProto).DurationMs() != -1)
        {
            continue;
        }

        if (operation.aura == SPELL_AURA_PERIODIC_TRIGGER_SPELL &&
            Recipe().DurationMs() == int32(operation.periodMs))
        {
            continue;
        }

        m_target->RemoveAuras(tSpellId);
    }
}

bool SpellAuraHolder::ModStackAmount(int32 num)
{
    uint32 protoStackAmount = m_spellProto->CumulativeAura;

    if (!protoStackAmount)
    {
        return true;
    }

    int32 stackAmount = m_stackAmount + num;
    if (stackAmount > (int32)protoStackAmount)
    {
        stackAmount = protoStackAmount;
    }
    else if (stackAmount <= 0)
    {
        m_stackAmount = 0;
        return true;
    }

    SetStackAmount(stackAmount);
    return false;
}

void SpellAuraHolder::SetStackAmount(uint32 stackAmount)
{
    Unit* target = GetTarget();
    Unit* caster = GetCaster();
    if (!target || !caster)
    {
        return;
    }

    bool refresh = stackAmount >= m_stackAmount;
    if (stackAmount != m_stackAmount)
    {
        m_stackAmount = stackAmount;
        UpdateAuraApplication();

        for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (Aura* aur = m_auras[i])
            {
                int32 bp = aur->GetBasePoints();
                int32 amount = m_stackAmount * caster->CalculateSpellDamage(target, cast::RecipeOf(*m_spellProto), cast::RecipeOf(*m_spellProto).At(static_cast<uint8>(i)), &bp);

                if (amount != aur->GetModifier()->m_amount)
                {
                    aur->ApplyModifier(false, true);
                    aur->GetModifier()->m_amount = amount;
                    aur->ApplyModifier(true, true);
                }
            }
        }
    }

    if (refresh)

    {
        RefreshHolder();
    }
}

Unit* SpellAuraHolder::GetCaster() const
{
    if (GetCasterGuid() == m_target->GetObjectGuid())
    {
        return m_target;
    }

    return ObjectLookup::GetUnit(*m_target, m_casterGuid);
}

bool SpellAuraHolder::IsWeaponBuffCoexistableWith(SpellAuraHolder const* ref) const
{

    if (!GetCastItemGuid())
    {
        return false;
    }

    if (!IsPositive())
    {
        return false;
    }

    if (GetSpellProto()->SpellClassSet != SPELLFAMILY_GENERIC)
    {
        return false;
    }

    if (GetSpellProto()->CumulativeAura)
    {
        return false;
    }

    if (!IsPlayer(m_target) || m_target->GetObjectGuid() != GetCasterGuid())
    {
        return false;
    }

    Item* castItem = ((Player*)m_target)->GetItemByGuid(GetCastItemGuid());
    if (!castItem)
    {
        return false;
    }

    if (!castItem->IsEquipped() ||
        (castItem->GetSlot() != EQUIPMENT_SLOT_MAINHAND && castItem->GetSlot() != EQUIPMENT_SLOT_OFFHAND))
    {
        return false;
    }

    return ref->GetCastItemGuid() && ref->GetCastItemGuid() != GetCastItemGuid();
}

bool SpellAuraHolder::IsNeedVisibleSlot(Unit const* caster) const
{
    bool totemAura = caster &&IsCreature(caster) && ((Creature*)caster)->IsTotem();

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (!m_auras[i])
        {
            continue;
        }

        switch (Recipe().At(static_cast<uint8>(i)).verb)
        {
            case SPELL_EFFECT_APPLY_AREA_AURA_PET:
            case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:

                return (m_target != caster || totemAura || !m_isPassive) && m_auras[i]->GetModifier()->m_auraname != SPELL_AURA_NONE;
            default:
                break;
        }
    }

    return !m_isPassive || totemAura;
}

void SpellAuraHolder::HandleSpellSpecificBoosts(bool apply)
{
    uint32 spellId1 = 0;
    uint32 spellId2 = 0;
    uint32 spellId3 = 0;
    uint32 spellId4 = 0;

    SpellLinkedSet linkedSet = sSpellMgr.GetSpellLinked(GetId(), SPELL_LINKED_TYPE_BOOST);
    if (linkedSet.size() > 0)
    {
        for (SpellLinkedSet::const_iterator itr = linkedSet.begin(); itr != linkedSet.end(); ++itr)
        {
            apply ?
            m_target->CastSpell(m_target, *itr, true, nullptr, nullptr, GetCasterGuid()) :
            m_target->RemoveAurasCastBy(*itr, GetCasterGuid());
        }
    }

    if (!apply)
    {

        linkedSet = sSpellMgr.GetSpellLinked(GetId(), SPELL_LINKED_TYPE_CASTONREMOVE);
        if (linkedSet.size() > 0)
        {
            for (SpellLinkedSet::const_iterator itr = linkedSet.begin(); itr != linkedSet.end(); ++itr)
            {
                m_target->CastSpell(m_target, *itr, true, nullptr, nullptr, GetCasterGuid());
            }
        }

        linkedSet = sSpellMgr.GetSpellLinked(GetId(), SPELL_LINKED_TYPE_REMOVEONREMOVE);
        if (linkedSet.size() > 0)
        {
            for (SpellLinkedSet::const_iterator itr = linkedSet.begin(); itr != linkedSet.end(); ++itr)
            {
                m_target->RemoveAurasCastBy(*itr, GetCasterGuid());
            }
        }
    }

    switch (GetSpellProto()->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (GetId())
            {
                case 20594:
                {
                    spellId1 = 20612;
                    break;
                }
                default:
                {
                    return;
                }
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {
            switch (GetId())
            {
                case 11129:
                {
                    if (!apply)
                    {
                        spellId1 = 28682;
                    }
                    else
                    {
                        return;
                    }
                    break;
                }
                case 28682:
                {
                    if (!apply)
                    {
                        spellId1 = 11129;
                    }
                    else
                    {
                        return;
                    }
                    break;
                }
                case 11189:
                case 28332:
                {
                    if (IsPlayer(m_target) && !apply)
                    {

                        if (SpellModifier* mod = ((Player*)m_target)->SpellMods().From(SPELLMOD_RESIST_MISS_CHANCE, GetId()))
                        {
                            ((Player*)m_target)->SpellMods().Add(mod, false);
                        }
                    }
                    return;
                }
                default:
                    return;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            switch (GetId())
            {

                case 19574:
                {
                    spellId1 = 24395;
                    spellId2 = 24396;
                    spellId3 = 24397;
                    spellId4 = 26592;
                    break;
                }
                default:
                    return;
            }
            break;
        }
        default:
            return;
    }

    SetInUse(true);

    if (apply)
    {
        if (spellId1)
        {
            m_target->CastSpell(m_target, spellId1, true, nullptr, nullptr, GetCasterGuid());
        }
        if (spellId2 && !IsDeleted())
        {
            m_target->CastSpell(m_target, spellId2, true, nullptr, nullptr, GetCasterGuid());
        }
        if (spellId3 && !IsDeleted())
        {
            m_target->CastSpell(m_target, spellId3, true, nullptr, nullptr, GetCasterGuid());
        }
        if (spellId4 && !IsDeleted())
        {
            m_target->CastSpell(m_target, spellId4, true, nullptr, nullptr, GetCasterGuid());
        }
    }
    else
    {
        if (spellId1)
        {
            m_target->RemoveAurasCastBy(spellId1, GetCasterGuid());
        }
        if (spellId2)
        {
            m_target->RemoveAurasCastBy(spellId2, GetCasterGuid());
        }
        if (spellId3)
        {
            m_target->RemoveAurasCastBy(spellId3, GetCasterGuid());
        }
        if (spellId4)
        {
            m_target->RemoveAurasCastBy(spellId4, GetCasterGuid());
        }
    }

    SetInUse(false);
}

SpellAuraHolder::~SpellAuraHolder()
{

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (Aura* aur = m_auras[i])
        {
            delete aur;
        }
    }
}

void SpellAuraHolder::Update(uint32 diff)
{
    if (m_duration > 0)
    {
        m_duration -= diff;
        if (m_duration < 0)
        {
            m_duration = 0;
        }

        m_timeCla -= diff;

        if (m_timeCla <= 0)
        {
            if (Unit* caster = GetCaster())
            {
                Powers powertype = Powers(GetSpellProto()->PowerType);
                int32 manaPerSecond = GetSpellProto()->ManaPerSecond + GetSpellProto()->ManaPerSecondPerLevel * caster->getLevel();
                m_timeCla = 1 * IN_MILLISECONDS;

                if (manaPerSecond)
                {
                    if (powertype == POWER_HEALTH)
                    {
                        caster->ModifyHealth(-manaPerSecond);
                    }
                    else
                    {
                        caster->ModifyPower(powertype, -manaPerSecond);
                    }
                }
            }
        }
    }

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (Aura* aura = m_auras[i])
        {
            aura->UpdateAura(diff);
        }
    }

    if (m_isHeartbeatSubject && m_duration)
    {
        if (HeartbeatResist(diff))
        {
            if (diff >= 1 * IN_MILLISECONDS || urand(1, 1 * IN_MILLISECONDS) < diff)
            {
                sLog.outError("Heartbeat: spell %u max timed %u, elapsed %u, chance to resist %f", m_spellProto->ID, m_maxDuration, m_maxDuration - m_duration, pow((m_maxDuration - m_duration + diff) / 15.0f, 2) / (IN_MILLISECONDS*IN_MILLISECONDS));
                m_duration = 1;
            }
        }
    }

    if ((cast::RecipeOf(*m_spellProto).Starts() == cast::Start::Channelled) && GetCasterGuid() != m_target->GetObjectGuid())
    {
        Unit* caster = GetCaster();
        if (!caster)
        {
            m_target->RemoveAurasCastBy(GetId(), GetCasterGuid());
            return;
        }

        if (caster->GetChannelObjectGuid() == m_target->GetObjectGuid())
        {

            float max_range = Recipe().Takes().rangeMax;

            if (Player* modOwner = caster->GetSpellModOwner())
            {
                modOwner->SpellMods().Apply(GetId(), SPELLMOD_RANGE, max_range, nullptr);
            }

            if (!InReach(*caster, *m_target, max_range))
            {
                caster->InterruptSpell(CURRENT_CHANNELED_SPELL);
                return;
            }
        }
    }
}

void SpellAuraHolder::RefreshHolder()
{
    SetAuraDuration(GetAuraMaxDuration());
    UpdateAuraDuration();
}

void SpellAuraHolder::SetAuraMaxDuration(int32 duration)
{
    m_maxDuration = duration;

    if (duration > 0)
    {
        if (!(IsPassive() && GetSpellProto()->DurationIndex == 0))
        {
            SetPermanent(false);
        }
    }
}

bool SpellAuraHolder::HasMechanic(uint32 mechanic) const
{
    if (mechanic == m_spellProto->Mechanic)
    {
        return true;
    }

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (m_auras[i] && Recipe().At(static_cast<uint8>(i)).mechanic == mechanic)
        {
            return true;
        }
    }
    return false;
}

bool SpellAuraHolder::HasMechanicMask(uint32 mechanicMask) const
{
    if (mechanicMask & (1 << (m_spellProto->Mechanic - 1)))
    {
        return true;
    }

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (m_auras[i] && Recipe().At(static_cast<uint8>(i)).mechanic &&
            ((1 << (Recipe().At(static_cast<uint8>(i)).mechanic - 1)) & mechanicMask))
        {
            return true;
        }
    }
    return false;
}

bool SpellAuraHolder::IsPersistent() const
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (Aura* aur = m_auras[i])
        {
            if (aur->IsPersistent())
            {
                return true;
            }
        }
    }
    return false;
}

bool SpellAuraHolder::IsAreaAura() const
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (Aura* aur = m_auras[i])
        {
            if (aur->IsAreaAura())
            {
                return true;
            }
        }
    }
    return false;
}

bool SpellAuraHolder::IsPositive() const
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (Aura* aur = m_auras[i])
        {
            if (!aur->IsPositive())
            {
                return false;
            }
        }
    }
    return true;
}

bool SpellAuraHolder::IsEmptyHolder() const
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (m_auras[i])
        {
            return false;
        }
    }
    return true;
}

void SpellAuraHolder::UnregisterAndCleanupTrackedAuras()
{
    TrackedAuraType trackedType = GetTrackedAuraType();
    if (!trackedType)
    {
        return;
    }

    if (trackedType == TRACK_AURA_TYPE_SINGLE_TARGET)
    {
        if (Unit* caster = GetCaster())
        {
            caster->GetTrackedAuraTargets(trackedType).erase(GetSpellProto());
        }
    }

    m_trackedAuraType = TRACK_AURA_TYPE_NOT_TRACKED;
}

void SpellAuraHolder::SetAuraFlag(uint32 slot, bool add)
{
    uint32 index    = slot >> 3;
    uint32 byte     = (slot & 7) << 2;
    uint32 val      = m_target->GetUInt32Value(UNIT_FIELD_AURAFLAGS + index);
    if (add)
    {
        val |= ((uint32)AFLAG_MASK << byte);
    }
    else
    {
        val &= ~((uint32)AFLAG_MASK << byte);
    }

    m_target->SetUInt32Value(UNIT_FIELD_AURAFLAGS + index, val);
}

void SpellAuraHolder::SetAuraLevel(uint32 slot, uint32 level)
{
    uint32 index    = slot / 4;
    uint32 byte     = (slot % 4) * 8;
    uint32 val      = m_target->GetUInt32Value(UNIT_FIELD_AURALEVELS + index);
    val &= ~(0xFF << byte);
    val |= (level << byte);
    m_target->SetUInt32Value(UNIT_FIELD_AURALEVELS + index, val);
}

void SpellAuraHolder::UpdateAuraApplication()
{
    if (m_auraSlot >= MAX_AURAS)
    {
        return;
    }

    uint32 stackCount = m_procCharges > 0 ? m_procCharges * m_stackAmount : m_stackAmount;

    uint32 index    = m_auraSlot / 4;
    uint32 byte     = (m_auraSlot % 4) * 8;
    uint32 val      = m_target->GetUInt32Value(UNIT_FIELD_AURAAPPLICATIONS + index);
    val &= ~(0xFF << byte);

    val |= ((uint8(stackCount <= 255 ? stackCount - 1 : 255 - 1)) << byte);
    m_target->SetUInt32Value(UNIT_FIELD_AURAAPPLICATIONS + index, val);
}

void SpellAuraHolder::UpdateAuraDuration()
{
    if (GetAuraSlot() >= MAX_AURAS || m_isPassive)
    {
        return;
    }

    if (IsPlayer(m_target))
    {
        WorldPacket data(SMSG_UPDATE_AURA_DURATION, 5);
        data << uint8(GetAuraSlot());
        data << uint32(GetAuraDuration());
        ((Player*)m_target)->SendDirectMessage(&data);
    }

    if (IsPlayer(m_target) && ((Player*)m_target)->GetSession()->PlayerLoading())
    {
        return;
    }

    Unit* caster = GetCaster();

    if (caster &&IsPlayer(caster) && caster != m_target)
    {
        SendAuraDurationForCaster((Player*)caster);
    }
}

bool SpellAuraHolder::HeartbeatResist(uint32 diff)
{

    return (urand(0, IN_MILLISECONDS*IN_MILLISECONDS) < pow((m_maxDuration - m_duration + diff) / 15.0f, 2));
}

void SpellAuraHolder::SendAuraDurationForCaster(Player* caster)
{

}
