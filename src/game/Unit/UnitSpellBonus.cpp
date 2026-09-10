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
#include "Cast/Recipe/RecipeBook.h"

int32 Unit::SpellBonusWithCoeffs(Unit* pCaster, SpellEntry const* spellProto, int32 total, int32 benefit, int32 ap_benefit,  DamageEffectType damagetype, bool donePart, Spell const* spell)
{

    if (!benefit)
    {
        return total;
    }

    float coeff = 1.0f;
    SpellEntry const* levelPenaltySpell = spell ? spell->GetSpellBonusLevelPenaltySpell(spellProto) : spellProto;
    bool const useTriggeredHealBonus = damagetype == HEAL && levelPenaltySpell != spellProto;

    if (IsCreature(pCaster) && !((Creature*)this)->IsPet())
    {
        coeff = 1.0f;
    }

    else if (SpellBonusEntry const* bonus = cast::RecipeOf(*spellProto).Bonus())
    {
        switch (damagetype)
        {
            case DOT:
                coeff = bonus->dot_damage;
                break;
            case HEAL:
                if (useTriggeredHealBonus)
                {
                    coeff = donePart ? (bonus->direct_damage_done ? bonus->direct_damage_done : bonus->direct_damage)
                        : (bonus->direct_damage_taken ? bonus->direct_damage_taken : bonus->direct_damage);
                }
                break;
            case SPELL_DIRECT_DAMAGE:

                if (IsPlayer(pCaster) && damagetype == SPELL_DIRECT_DAMAGE)
                {
                    Item* item = ((Player*)pCaster)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);

                    if (donePart)
                    {
                        if (item)
                        {
                            switch (item->GetProto()->InventoryType)
                            {
                                case INVTYPE_2HWEAPON:
                                    coeff = (bonus->two_hand_direct_damage_done ? bonus->two_hand_direct_damage_done
                                        : ( bonus->two_hand_direct_damage ? bonus->two_hand_direct_damage : bonus->direct_damage_done ));
                                    break;
                                case INVTYPE_WEAPON:
                                case INVTYPE_WEAPONMAINHAND:
                                case INVTYPE_WEAPONOFFHAND:
                                    coeff = (bonus->one_hand_direct_damage_done ? bonus->one_hand_direct_damage_done
                                        : ( bonus->one_hand_direct_damage ? bonus->one_hand_direct_damage : bonus->direct_damage_done ));
                                    break;
                            }

                            if (!coeff)
                            {
                                coeff = bonus->direct_damage;
                            }
                        } else {
                            coeff = (bonus->direct_damage_done ? bonus->direct_damage_done : bonus->direct_damage);
                        }
                    }
                    else
                    {
                        if (item)
                        {
                            switch (item->GetProto()->InventoryType)
                            {
                                case INVTYPE_2HWEAPON:
                                    coeff = (bonus->two_hand_direct_damage_taken ? bonus->two_hand_direct_damage_taken
                                        : ( bonus->two_hand_direct_damage ? bonus->two_hand_direct_damage : bonus->direct_damage_taken ));
                                    break;
                                case INVTYPE_WEAPON:
                                case INVTYPE_WEAPONMAINHAND:
                                case INVTYPE_WEAPONOFFHAND:
                                    coeff = (bonus->one_hand_direct_damage_taken ? bonus->one_hand_direct_damage_taken
                                        : ( bonus->one_hand_direct_damage ? bonus->one_hand_direct_damage : bonus->direct_damage_taken ));
                                    break;
                            }

                            if (!coeff)
                            {
                                coeff = bonus->direct_damage;
                            }
                        }
                        else
                        {
                            coeff = (bonus->direct_damage_taken ? bonus->direct_damage_taken : bonus->direct_damage);
                        }
                    }
                    break;
                }
            default:
                break;
        }

        if (donePart && (bonus->ap_bonus || bonus->ap_dot_bonus))
        {
            float ap_bonus = damagetype == DOT ? bonus->ap_dot_bonus : bonus->ap_bonus;

            total += int32(ap_bonus * (GetTotalAttackPowerValue(IsSpellRequiresRangedAP(spellProto) ? RANGED_ATTACK : BASE_ATTACK) + ap_benefit));
        }
    }

    else
    {
        coeff = cast::RecipeOf(*spellProto).Coefficient(damagetype == DOT);
    }

    float LvlPenalty = CalculateLevelPenalty(levelPenaltySpell);

    if (spellProto->SpellClassSet == SPELLFAMILY_PALADIN && (spellProto->SpellIconID == 25 || spellProto->SpellIconID == 242))
    {
        LvlPenalty = 1.0f;
    }

    if (Player* modOwner = GetSpellModOwner())
    {
        coeff *= 100.0f;
        modOwner->SpellMods().Apply(spellProto->ID, SPELLMOD_SPELL_BONUS_DAMAGE, coeff);
        coeff /= 100.0f;
    }

    total += int32(benefit * coeff * LvlPenalty);

    return total;
};

uint32 Unit::SpellDamageBonusDone(Unit* pVictim, SpellEntry const* spellProto, uint32 pdamage, DamageEffectType damagetype, uint32 stack)
{
    if (!spellProto || !pVictim || damagetype == DIRECT_DAMAGE)
    {
        return pdamage;
    }

    if (IsCreature(this) && ((Creature*)this)->IsTotem() && ((Totem*)this)->GetTotemType() != TOTEM_STATUE)
    {
        if (Unit* owner = GetOwner())
        {
            return owner->SpellDamageBonusDone(pVictim, spellProto, pdamage, damagetype);
        }
    }

    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
    float DoneTotalMod = 1.0f;
    int32 DoneTotal = 0;

    if (IsCreature(this) && !((Creature*)this)->IsPet())
    {
        DoneTotalMod *= Creature::RatesFor(((Creature*)this)->GetCreatureInfo()->Rank).spellDamage;
    }

    const auto mModDamagePercentDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
    for (auto* aura : mModDamagePercentDone)
    {
        if ((aura->GetModifier()->m_miscvalue & GetSpellSchoolMask(spellProto)) &&
            aura->GetSpellProto()->EquippedItemClass == -1 &&

            aura->GetSpellProto()->EquippedItemInvTypes == 0)

        {
            DoneTotalMod *= (aura->GetModifier()->m_amount + 100.0f) / 100.0f;
        }
    }

    DoneTotal += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_FLAT_SPELL_DAMAGE_VERSUS, creatureTypeMask);

    DoneTotalMod *= GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_DONE_VERSUS, creatureTypeMask);

    DoneTotal += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_DAMAGE_DONE_CREATURE, creatureTypeMask);

    Unit* owner = GetOwner();
    if (!owner)
    {
        owner = this;
    }

    const auto mOverrideClassScript = owner->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
    for (auto* aura : mOverrideClassScript)
    {
        if (!aura->isAffectedOnSpell(spellProto))
        {
            continue;
        }

        switch (aura->GetModifier()->m_miscvalue)
        {
            case 4418:
            case 4554:
            {
                DoneTotal += aura->GetModifier()->m_amount;
                break;
            }
            case 4555:
            {
                DoneTotalMod *= (aura->GetModifier()->m_amount + 100.0f) / 100.0f;
                break;
            }
        }
    }

    int32 DoneAdvertisedBenefit = SpellBaseDamageBonusDone(GetSpellSchoolMask(spellProto));

    if (IsCreature(this) && ((Creature*)this)->IsPet())
    {
        DoneAdvertisedBenefit += ((Pet*)this)->GetBonusDamage();
    }

    DoneTotal = SpellBonusWithCoeffs(this, spellProto, DoneTotal, DoneAdvertisedBenefit, 0, damagetype, true);

    float tmpDamage = (int32(pdamage) + DoneTotal * int32(stack)) * DoneTotalMod;

    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(spellProto->ID, damagetype == DOT ? SPELLMOD_DOT : SPELLMOD_DAMAGE, tmpDamage);
    }

    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

uint32 Unit::SpellDamageBonusTaken(Unit* pCaster, SpellEntry const* spellProto, uint32 pdamage, DamageEffectType damagetype, uint32 stack)
{
    if (!spellProto || !pCaster || damagetype == DIRECT_DAMAGE)
    {
        return pdamage;
    }

    uint32 schoolMask = GetSpellSchoolMask(spellProto);

    float TakenTotalMod = 1.0f;
    int32 TakenTotal = 0;

    TakenTotalMod *= GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, schoolMask);

    int32 TakenAdvertisedBenefit = SpellBaseDamageBonusTaken(GetSpellSchoolMask(spellProto));

    TakenTotal = SpellBonusWithCoeffs(pCaster, spellProto, TakenTotal, TakenAdvertisedBenefit, 0, damagetype, false);

    float tmpDamage = (int32(pdamage) + TakenTotal * int32(stack)) * TakenTotalMod;

    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

int32 Unit::SpellBaseDamageBonusDone(SpellSchoolMask schoolMask)
{
    int32 DoneAdvertisedBenefit = 0;

    const auto mDamageDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for (auto* aura : mDamageDone)
    {
        if ((aura->GetModifier()->m_miscvalue & schoolMask) != 0 &&
            aura->GetSpellProto()->EquippedItemClass == -1 &&
            aura->GetSpellProto()->EquippedItemInvTypes == 0)
        {
            DoneAdvertisedBenefit += aura->GetModifier()->m_amount;
        }
    }

    if (IsPlayer(this))
    {

        const auto mDamageDoneOfStatPercent = GetAurasByType(SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT);
        for (auto* auraOf : mDamageDoneOfStatPercent)
        {
            if (auraOf->GetModifier()->m_miscvalue & schoolMask)
            {

                Stats usedStat = STAT_SPIRIT;
                DoneAdvertisedBenefit += int32(GetStat(usedStat) * auraOf->GetModifier()->m_amount / 100.0f);
            }
        }
    }
    return DoneAdvertisedBenefit;
}

int32 Unit::SpellBaseDamageBonusTaken(SpellSchoolMask schoolMask)
{
    int32 TakenAdvertisedBenefit = 0;

    const auto mDamageTaken = GetAurasByType(SPELL_AURA_MOD_DAMAGE_TAKEN);
    for (auto* aura : mDamageTaken)
    {
        if ((aura->GetModifier()->m_miscvalue & schoolMask) != 0)
        {
            TakenAdvertisedBenefit += aura->GetModifier()->m_amount;
        }
    }

    return TakenAdvertisedBenefit;
}

bool Unit::IsSpellCrit(Unit* pVictim, SpellEntry const* spellProto, SpellSchoolMask schoolMask, WeaponAttackType attackType)
{

    if (IsCreature(this) && !((Creature*)this)->IsPet() && !((Creature*)this)->IsTotem())
    {
        return false;
    }

    if (cast::RecipeOf(*spellProto).Says().cannotCrit)
    {
        return false;
    }

    float crit_chance = 0.0f;
    switch (spellProto->DefenseType)
    {
        case SPELL_DAMAGE_CLASS_NONE:
            return false;
        case SPELL_DAMAGE_CLASS_MAGIC:
        {
            if (schoolMask & SPELL_SCHOOL_MASK_NORMAL)
            {
                crit_chance = 0.0f;
            }

            else if (IsPlayer(this))
            {
                crit_chance = ((Player*)this)->Sheet().SpellCritChance(GetFirstSchoolInMask(schoolMask));
            }
            else
            {
                crit_chance = float(m_baseSpellCritChance);
                crit_chance += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, schoolMask);
            }

            if (pVictim)
            {
                if (!cast::RecipeOf(*spellProto).IsPositive())
                {

                    crit_chance += pVictim->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE, schoolMask);
                }

                const auto mOverrideClassScript = GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                for (auto* aura : mOverrideClassScript)
                {
                    if (!(aura->isAffectedOnSpell(spellProto)))
                    {
                        continue;
                    }
                    switch (aura->GetModifier()->m_miscvalue)
                    {

                        case 849: if (pVictim->IsFrozen()) { crit_chance += 10.0f; } break;
                        case 910: if (pVictim->IsFrozen()) { crit_chance += 20.0f; } break;
                        case 911: if (pVictim->IsFrozen()) { crit_chance += 30.0f; } break;
                        case 912: if (pVictim->IsFrozen()) { crit_chance += 40.0f; } break;
                        case 913: if (pVictim->IsFrozen()) { crit_chance += 50.0f; } break;
                        default:
                            break;
                    }
                }
            }
            break;
        }
        case SPELL_DAMAGE_CLASS_MELEE:
        case SPELL_DAMAGE_CLASS_RANGED:
        {
            if (pVictim)
            {
                crit_chance = GetUnitCriticalChance(attackType, pVictim);
            }

            crit_chance += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, schoolMask);
            break;
        }
        default:
            return false;
    }

    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(spellProto->ID, SPELLMOD_CRITICAL_CHANCE, crit_chance);
    }

    crit_chance = crit_chance > 0.0f ? crit_chance : 0.0f;
    if (roll_chance_f(crit_chance))
    {
        return true;
    }
    return false;
}

uint32 Unit::SpellCriticalDamageBonus(SpellEntry const* spellProto, uint32 damage, Unit* pVictim)
{

    int32 crit_bonus;
    switch (spellProto->DefenseType)
    {
        case SPELL_DAMAGE_CLASS_MELEE:
        case SPELL_DAMAGE_CLASS_RANGED:
            crit_bonus = damage;
            break;
        default:
            crit_bonus = damage / 2;
            break;
    }

    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(spellProto->ID, SPELLMOD_CRIT_DAMAGE_BONUS, crit_bonus);
    }

    if (!pVictim)
    {
        return damage += crit_bonus;
    }

    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();

    int32 critPctDamageMod = GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_CRIT_PERCENT_VERSUS, creatureTypeMask);

    if (critPctDamageMod != 0)
    {
        crit_bonus = int32(crit_bonus * float((100.0f + critPctDamageMod) / 100.0f));
    }

    if (crit_bonus > 0)
    {
        damage += crit_bonus;
    }

    return damage;
}

uint32 Unit::SpellCriticalHealingBonus(SpellEntry const* spellProto, uint32 damage, Unit* pVictim)
{

    int32 crit_bonus;
    switch (spellProto->DefenseType)
    {
        case SPELL_DAMAGE_CLASS_MELEE:
        case SPELL_DAMAGE_CLASS_RANGED:

            crit_bonus = damage;
            break;
        default:
            crit_bonus = damage / 2;
            break;
    }

    if (pVictim)
    {
        uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
        crit_bonus = int32(crit_bonus * GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_CRIT_PERCENT_VERSUS, creatureTypeMask));
    }

    if (crit_bonus > 0)
    {
        damage += crit_bonus;
    }

    return damage;
}

uint32 Unit::SpellHealingBonusDone(Unit* pVictim, SpellEntry const* spellProto, int32 healamount, DamageEffectType damagetype, uint32 stack, Spell const* spell)
{

    if (IsCreature(this) && ((Creature*)this)->IsTotem() && ((Totem*)this)->GetTotemType() != TOTEM_STATUE)
    {
        if (Unit* owner = GetOwner())
        {
            return owner->SpellHealingBonusDone(pVictim, spellProto, healamount, damagetype, stack, spell);
        }
    }

    if (spellProto->DefenseType == SPELL_DAMAGE_CLASS_NONE)
    {
        return healamount < 0 ? 0 : healamount;
    }

    float  DoneTotalMod = 1.0f;
    int32  DoneTotal = 0;

    const auto mHealingDonePct = GetAurasByType(SPELL_AURA_MOD_HEALING_DONE_PERCENT);
    for (auto* aura : mHealingDonePct)
    {
        DoneTotalMod *= (100.0f + aura->GetModifier()->m_amount) / 100.0f;
    }

    Unit* owner = GetOwner();
    if (!owner)
    {
        owner = this;
    }
    const auto mOverrideClassScript = owner->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
    for (auto* aura : mOverrideClassScript)
    {
        if (!aura->isAffectedOnSpell(spellProto))
        {
            continue;
        }
        switch (aura->GetModifier()->m_miscvalue)
        {
            case 4415:
            case 3736:
                DoneTotal += aura->GetModifier()->m_amount;
                break;
            default:
                break;
        }
    }

    int32 DoneAdvertisedBenefit  = SpellBaseHealingBonusDone(GetSpellSchoolMask(spellProto));

    DoneTotal = SpellBonusWithCoeffs(this, spellProto, DoneTotal, DoneAdvertisedBenefit, 0, damagetype, true, spell);

    float heal = (healamount + DoneTotal * int32(stack)) * DoneTotalMod;

    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(spellProto->ID, damagetype == DOT ? SPELLMOD_DOT : SPELLMOD_DAMAGE, heal);
    }

    return heal < 0 ? 0 : uint32(heal);
}

uint32 Unit::SpellHealingBonusTaken(Unit* pCaster, SpellEntry const* spellProto, int32 healamount, DamageEffectType damagetype, uint32 stack, Spell const* spell)
{
    float  TakenTotalMod = 1.0f;

    float minval = float(GetMaxNegativeAuraModifier(SPELL_AURA_MOD_HEALING_PCT));
    if (minval)
    {
        TakenTotalMod *= (100.0f + minval) / 100.0f;
    }

    float maxval = float(GetMaxPositiveAuraModifier(SPELL_AURA_MOD_HEALING_PCT));
    if (maxval)
    {
        TakenTotalMod *= (100.0f + maxval) / 100.0f;
    }

    if (spellProto->DefenseType == SPELL_DAMAGE_CLASS_NONE)
    {
        healamount = int32(healamount * TakenTotalMod);
        return healamount < 0 ? 0 : healamount;
    }

    int32  TakenTotal = 0;

    int32 TakenAdvertisedBenefit = SpellBaseHealingBonusTaken(GetSpellSchoolMask(spellProto));

    if (spellProto->SpellClassSet == SPELLFAMILY_PALADIN && (spellProto->SpellClassMask & UI64LIT(0x0000000000006000)))
    {
        const auto mDummyAuras = GetAurasByType(SPELL_AURA_DUMMY);
        for (auto* aura : mDummyAuras)
        {
            if (aura->GetSpellProto()->SpellVisualID == 300 && (aura->GetSpellProto()->SpellClassMask & UI64LIT(0x0000000010000000)))
            {

                if ((spellProto->SpellClassMask & UI64LIT(0x0000000000002000)) && aura->GetEffIndex() == EFFECT_INDEX_1)
                {
                    TakenTotal += aura->GetModifier()->m_amount;
                }

                else if ((spellProto->SpellClassMask & UI64LIT(0x0000000000004000)) && aura->GetEffIndex() == EFFECT_INDEX_0)
                {
                    TakenTotal += aura->GetModifier()->m_amount;
                }
            }
        }
    }

    TakenTotal = SpellBonusWithCoeffs(pCaster, spellProto, TakenTotal, TakenAdvertisedBenefit, 0, damagetype, false, spell);

    if (spellProto->SpellClassSet == SPELLFAMILY_SHAMAN && (spellProto->SpellClassMask & UI64LIT(0x0000000000000040)))
    {

        const auto auraDummy = GetAurasByType(SPELL_AURA_DUMMY);
        for (auto* aura : auraDummy)
        {
            if (aura->GetId() == 29203)
            {
                TakenTotalMod *= (aura->GetModifier()->m_amount + 100.0f) / 100.0f;
            }
        }
    }

    float heal = (healamount + TakenTotal * int32(stack)) * TakenTotalMod;

    return heal < 0 ? 0 : uint32(heal);
}

int32 Unit::SpellBaseHealingBonusDone(SpellSchoolMask schoolMask)
{
    int32 AdvertisedBenefit = 0;

    const auto mHealingDone = GetAurasByType(SPELL_AURA_MOD_HEALING_DONE);
    for (auto* aura : mHealingDone)
    {
        if ((aura->GetModifier()->m_miscvalue & schoolMask) != 0)
        {
            AdvertisedBenefit += aura->GetModifier()->m_amount;
        }
    }

    if (IsPlayer(this))
    {

        const auto mHealingDoneOfStatPercent = GetAurasByType(SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT);
        for (auto* aura : mHealingDoneOfStatPercent)
        {

            Stats usedStat = STAT_SPIRIT;
            AdvertisedBenefit += int32(GetStat(usedStat) * aura->GetModifier()->m_amount / 100.0f);
        }
    }
    return AdvertisedBenefit;
}

int32 Unit::SpellBaseHealingBonusTaken(SpellSchoolMask schoolMask)
{
    int32 AdvertisedBenefit = 0;
    const auto mDamageTaken = GetAurasByType(SPELL_AURA_MOD_HEALING);
    for (auto* aura : mDamageTaken)
    {
        if (aura->GetModifier()->m_miscvalue & schoolMask)
        {
            AdvertisedBenefit += aura->GetModifier()->m_amount;
        }
    }
    return AdvertisedBenefit;
}

bool Unit::IsImmuneToSpell(SpellEntry const* spellInfo, bool )
{
    if (!spellInfo)
    {
        return false;
    }

    SpellImmuneList const& dispelList = m_immune.Of(IMMUNITY_DISPEL);
    for (SpellImmuneList::const_iterator itr = dispelList.begin(); itr != dispelList.end(); ++itr)
    {
        if (itr->type == spellInfo->DispelType)
        {
            return true;
        }
    }

    if (!cast::RecipeOf(*spellInfo).Says().ignoresSchoolImmunity &&
        !cast::RecipeOf(*spellInfo).Says().dispelsOnImmunity)
    {
        SpellImmuneList const& schoolList = m_immune.Of(IMMUNITY_SCHOOL);
        for (SpellImmuneList::const_iterator itr = schoolList.begin(); itr != schoolList.end(); ++itr)
        {
            if (!(cast::Recipes().IsPositive(itr->spellId) && cast::RecipeOf(*spellInfo).IsPositive()) &&
                (itr->type & GetSpellSchoolMask(spellInfo)))
            {
                return true;
            }
        }
    }

    if (uint32 mechanic = spellInfo->Mechanic)
    {
        SpellImmuneList const& mechanicList = m_immune.Of(IMMUNITY_MECHANIC);
        for (SpellImmuneList::const_iterator itr = mechanicList.begin(); itr != mechanicList.end(); ++itr)
        {
            if (itr->type == mechanic)
            {
                return true;
            }
        }

        const auto immuneAuraApply = GetAurasByType(SPELL_AURA_MECHANIC_IMMUNITY_MASK);
        for (auto* aura : immuneAuraApply)
        {
            if (aura->GetModifier()->m_miscvalue & (1 << (mechanic - 1)))
            {
                return true;
            }
        }
    }

    return false;
}

bool Unit::IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool ) const
{

    uint32 effect = spellInfo->Effect[index];
    SpellImmuneList const& effectList = m_immune.Of(IMMUNITY_EFFECT);
    for (SpellImmuneList::const_iterator itr = effectList.begin(); itr != effectList.end(); ++itr)
    {
        if (itr->type == effect)
        {
            return true;
        }
    }

    if (uint32 mechanic = spellInfo->EffectMechanic[index])
    {
        SpellImmuneList const& mechanicList = m_immune.Of(IMMUNITY_MECHANIC);
        for (SpellImmuneList::const_iterator itr = mechanicList.begin(); itr != mechanicList.end(); ++itr)
        {
            if (itr->type == mechanic)
            {
                return true;
            }
        }

        const auto immuneAuraApply = GetAurasByType(SPELL_AURA_MECHANIC_IMMUNITY_MASK);
        for (auto* aura : immuneAuraApply)
        {
            if (aura->GetModifier()->m_miscvalue & (1 << (mechanic - 1)))
            {
                return true;
            }
        }
    }

    if (uint32 aura = spellInfo->EffectAura[index])
    {
        SpellImmuneList const& list = m_immune.Of(IMMUNITY_STATE);
        for (SpellImmuneList::const_iterator itr = list.begin(); itr != list.end(); ++itr)
        {
            if (itr->type == aura)
            {
                return true;
            }
        }
    }
    return false;
}

uint32 Unit::MeleeDamageBonusDone(Unit* pVictim, uint32 pdamage, WeaponAttackType attType, SpellEntry const* spellProto, DamageEffectType damagetype, uint32 stack)
{
    if (!pVictim)
    {
        return pdamage;
    }

    if (pdamage == 0)
    {
        return pdamage;
    }

    if (spellProto && GetSpellSchoolMask(spellProto) == SPELL_SCHOOL_MASK_HOLY && IsPlayer(this))
    {
        return pdamage;
    }

    bool isWeaponDamageBasedSpell = !(spellProto && (damagetype == DOT || spellProto->HasSpellEffect(SPELL_EFFECT_SCHOOL_DAMAGE)));
    Item*  pWeapon          = IsPlayer(this) ? ((Player*)this)->GetWeaponForAttack(attType, true, false) : nullptr;
    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
    uint32 schoolMask       = uint32(spellProto ? GetSpellSchoolMask(spellProto) : GetMeleeDamageSchoolMask());

    int32 DoneFlat  = 0;
    int32 APbonus   = 0;

    if (!isWeaponDamageBasedSpell)
    {
        const auto mModDamageDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE);
        for (auto* auraOf : mModDamageDone)
        {
            if (auraOf->GetModifier()->m_miscvalue & schoolMask &&
                auraOf->GetModifier()->m_miscvalue & GetMeleeDamageSchoolMask() &&
                ((auraOf->GetSpellProto()->EquippedItemClass == -1) ||
                (pWeapon && pWeapon->IsFitToSpellRequirements(auraOf->GetSpellProto()))))
            {
                DoneFlat += auraOf->GetModifier()->m_amount;
            }
        }

        if (IsCreature(this) && ((Creature*)this)->IsPet())
        {
            DoneFlat += ((Pet*)this)->GetBonusDamage();
        }
    }

    DoneFlat += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_DAMAGE_DONE_CREATURE, creatureTypeMask);

    if (attType == RANGED_ATTACK)
    {
        APbonus += pVictim->GetTotalAuraModifier(SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS);
        APbonus += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_RANGED_ATTACK_POWER_VERSUS, creatureTypeMask);
    }
    else
    {
        APbonus += pVictim->GetTotalAuraModifier(SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS);
        APbonus += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_MELEE_ATTACK_POWER_VERSUS, creatureTypeMask);
    }

    float DonePercent   = 1.0f;

    if (!isWeaponDamageBasedSpell)
    {
        const auto mModDamagePercentDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        for (auto* auraOf : mModDamagePercentDone)
        {
            if (auraOf->GetModifier()->m_miscvalue & schoolMask &&
                ((auraOf->GetSpellProto()->EquippedItemClass == -1) ||
                (pWeapon && pWeapon->IsFitToSpellRequirements(auraOf->GetSpellProto()))))
            {
                DonePercent *= (auraOf->GetModifier()->m_amount + 100.0f) / 100.0f;
            }
        }

        if (attType == OFF_ATTACK)
        {
            DonePercent *= Tallied().Value(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT);
        }
    }

    DonePercent *= GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_DONE_VERSUS, creatureTypeMask);

    Unit* owner = GetOwner();
    if (!owner)
    {
        owner = this;
    }

    float DoneTotal = 0.0f;

    if (!isWeaponDamageBasedSpell)
    {

        DoneTotal = SpellBonusWithCoeffs(this, spellProto, DoneTotal, DoneFlat, APbonus, damagetype, true);
    }

    else if (APbonus || DoneFlat)
    {
        bool normalized = spellProto ? spellProto->HasSpellEffect(SPELL_EFFECT_NORMALIZED_WEAPON_DMG) : false;
        DoneTotal += int32(APbonus / 14.0f * GetAPMultiplier(attType, normalized));

        UnitMods unitMod;
        switch (attType)
        {
            default:
            case BASE_ATTACK:   unitMod = UNIT_MOD_DAMAGE_MAINHAND; break;
            case OFF_ATTACK:    unitMod = UNIT_MOD_DAMAGE_OFFHAND;  break;
            case RANGED_ATTACK: unitMod = UNIT_MOD_DAMAGE_RANGED;   break;
        }

        DoneTotal += DoneFlat;

        DoneTotal *= Tallied().Value(unitMod, TOTAL_PCT);
    }

    float tmpDamage = float(int32(pdamage) + DoneTotal * int32(stack)) * DonePercent;

    if (spellProto)
    {
        if (Player* modOwner = GetSpellModOwner())
        {
            modOwner->SpellMods().Apply(spellProto->ID, damagetype == DOT ? SPELLMOD_DOT : SPELLMOD_DAMAGE, tmpDamage);
        }
    }

    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

uint32 Unit::MeleeDamageBonusTaken(Unit* pCaster, uint32 pdamage, WeaponAttackType attType, SpellEntry const* spellProto, DamageEffectType damagetype, uint32 stack)
{
    if (!pCaster)
    {
        return pdamage;
    }

    if (pdamage == 0)
    {
        return pdamage;
    }

    if (spellProto && GetSpellSchoolMask(spellProto) == SPELL_SCHOOL_MASK_HOLY &&IsPlayer(pCaster))
    {
        return pdamage;
    }

    bool isWeaponDamageBasedSpell = !(spellProto && (damagetype == DOT || spellProto->HasSpellEffect(SPELL_EFFECT_SCHOOL_DAMAGE)));
    uint32 schoolMask       = uint32(spellProto ? GetSpellSchoolMask(spellProto) :GetMeleeDamageSchoolMask());

    int32 TakenFlat = 0;

    if (attType == RANGED_ATTACK)
    {
        TakenFlat += GetTotalAuraModifier(SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN);
    }
    else
    {
        TakenFlat += GetTotalAuraModifier(SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN);
    }

    TakenFlat += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_DAMAGE_TAKEN, schoolMask);

    float TakenPercent  = 1.0f;

    TakenPercent *= GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, schoolMask);

    if (attType == RANGED_ATTACK)
    {
        TakenPercent *= GetTotalAuraMultiplier(SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN_PCT);
    }
    else
    {
        TakenPercent *= GetTotalAuraMultiplier(SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN_PCT);
    }

    if (!isWeaponDamageBasedSpell)
    {

        TakenFlat = SpellBonusWithCoeffs(pCaster, spellProto, 0, TakenFlat, 0, damagetype, false);
    }

    float tmpDamage = float(int32(pdamage) + TakenFlat * int32(stack)) * TakenPercent;

    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}
