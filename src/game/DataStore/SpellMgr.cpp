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

#include "Utilities/Errors.h"
#include <string>
#include <list>
#include "SpellMgr.h"
#include "ObjectMgr.h"
#include "SpellAuraDefines.h"
#include "ProgressBar.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "Chat.h"
#include "Spell.h"
#include "Unit.h"
#include "World.h"
#include "Cast/Recipe/RecipeBook.h"

bool IsPrimaryProfessionSkill(uint32 skill)
{
    SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skill);
    if (!pSkill)
    {
        return false;
    }

    if (pSkill->CategoryID != SKILL_CATEGORY_PROFESSION)
    {
        return false;
    }

    return true;
}

SpellMgr::SpellMgr()
{
}

SpellMgr::~SpellMgr()
{
}

SpellMgr& SpellMgr::Instance()
{
    static SpellMgr spellMgr;
    return spellMgr;
}

int32 GetSpellDuration(SpellEntry const* spellInfo)
{
    if (!spellInfo)
    {
        return 0;
    }
    SpellDurationEntry const* du = sSpellDurationStore.LookupEntry(spellInfo->DurationIndex);
    if (!du)
    {
        return 0;
    }
    return (du->Duration[0] == -1) ? -1 : abs(du->Duration[0]);
}

int32 GetSpellMaxDuration(SpellEntry const* spellInfo)
{
    if (!spellInfo)
    {
        return 0;
    }
    SpellDurationEntry const* du = sSpellDurationStore.LookupEntry(spellInfo->DurationIndex);
    if (!du)
    {
        return 0;
    }
    return (du->Duration[2] == -1) ? -1 : abs(du->Duration[2]);
}

int32 CalculateSpellDuration(SpellEntry const* spellInfo, Unit const* caster)
{
    int32 duration = GetSpellDuration(spellInfo);

    if (duration != -1 && caster)
    {
        int32 maxduration = GetSpellMaxDuration(spellInfo);

        if (duration != maxduration &&IsPlayer(caster))
        {
            duration += int32((maxduration - duration) * ((Player*)caster)->GetComboPoints() / 5);
        }

        if (Player* modOwner = caster->GetSpellModOwner())
        {
            modOwner->SpellMods().Apply(spellInfo->ID, SPELLMOD_DURATION, duration);

            if (duration < 0)
            {
                duration = 0;
            }
        }
    }

    return duration;
}

uint32 GetSpellCastTime(SpellEntry const* spellInfo, Spell const* spell)
{
    if (spell)
    {

        if (spell->IsTriggeredSpellWithRedundentCastTime())
        {
            return 0;
        }

        if (IsPlayer(spell->GetCaster()))
        {
            if (TradeData* my_trade = ((Player*)(spell->GetCaster()))->GetTradeData())
            {
                if (Item* nonTrade = my_trade->GetTraderData()->GetItem(TRADE_SLOT_NONTRADED))
                {
                    if (nonTrade == spell->m_targets.getItemTarget())
                    {
                        return 0;
                    }
                }
            }
        }
    }

    SpellCastTimesEntry const* spellCastTimeEntry = sSpellCastTimesStore.LookupEntry(spellInfo->CastingTimeIndex);

    if (!spellCastTimeEntry)
    {
        return 0;
    }

    int32 castTime = spellCastTimeEntry->Base;

    if (spell)
    {
        if (Player* modOwner = spell->GetCaster()->GetSpellModOwner())
        {
            modOwner->SpellMods().Apply(spellInfo->ID, SPELLMOD_CASTING_TIME, castTime, spell);
        }

        if (!spellInfo->HasAttribute(SPELL_ATTR_ABILITY) && !spellInfo->HasAttribute(SPELL_ATTR_TRADESPELL))
        {
            castTime = int32(castTime * spell->GetCaster()->GetCastSpeedMod());
        }
        else
        {
            if (spell->IsRangedSpell() && !spell->IsAutoRepeat())
            {
                castTime = int32(castTime * spell->GetCaster()->m_modAttackSpeedPct[RANGED_ATTACK]);
            }
        }
    }

    if (spellInfo->HasAttribute(SPELL_ATTR_RANGED) && (!spell || !spell->IsAutoRepeat()))
    {
        castTime += 500;
    }

    if (spellInfo->ID == 19968)
    {
        castTime = 0;
    }
    return (castTime > 0) ? uint32(castTime) : 0;
}

bool IsNoStackAuraDueToAura(uint32 spellId_1, uint32 spellId_2)
{
    SpellEntry const* spellInfo_1 = sSpellStore.LookupEntry(spellId_1);
    SpellEntry const* spellInfo_2 = sSpellStore.LookupEntry(spellId_2);
    if (!spellInfo_1 || !spellInfo_2)
    {
        return false;
    }
    if (spellInfo_1->ID == spellId_2)
    {
        return false;
    }

    if ((spellId_1 == 11405 && spellId_2 == 17528) || (spellId_1 == 17528 && spellId_2 == 11405))
    {
        return false;
    }

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        for (int32 j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if (spellInfo_1->Effect[i] == spellInfo_2->Effect[j] &&
                spellInfo_1->EffectAura[i] == spellInfo_2->EffectAura[j] &&
                spellInfo_1->EffectMiscValue[i] == spellInfo_2->EffectMiscValue[j] &&
                spellInfo_1->EffectItemType[i] == spellInfo_2->EffectItemType[j] &&
                (spellInfo_1->Effect[i] != 0 || spellInfo_1->EffectAura[i] != 0 ||
                spellInfo_1->EffectMiscValue[i] != 0 || spellInfo_1->EffectItemType[i] != 0))
            {
                return true;
            }
        }
    }

    return false;
}

int32 CompareAuraRanks(uint32 spellId_1, uint32 spellId_2)
{
    SpellEntry const* spellInfo_1 = sSpellStore.LookupEntry(spellId_1);
    SpellEntry const* spellInfo_2 = sSpellStore.LookupEntry(spellId_2);
    if (!spellInfo_1 || !spellInfo_2)
    {
        return 0;
    }
    if (spellId_1 == spellId_2)
    {
        return 0;
    }

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo_1->Effect[i] != 0 && spellInfo_2->Effect[i] != 0 && spellInfo_1->Effect[i] == spellInfo_2->Effect[i])
        {
            int32 diff = spellInfo_1->EffectBasePoints[i] - spellInfo_2->EffectBasePoints[i];
            if (spellInfo_1->CalculateSimpleValue(SpellEffectIndex(i)) < 0 && spellInfo_2->CalculateSimpleValue(SpellEffectIndex(i)) < 0)
            {
                return -diff;
            }
            else
            {
                return diff;
            }
        }
    }
    return 0;
}

SpellSpecific GetSpellSpecific(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        return SPELL_NORMAL;
    }

    switch (spellInfo->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {

            if (spellInfo->ID == 13161)
            {
                return SPELL_ASPECT;
            }

            if (spellInfo->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
            {
                bool food = false;
                bool drink = false;
                for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    switch (spellInfo->EffectAura[i])
                    {

                        case SPELL_AURA_MOD_REGEN:
                        case SPELL_AURA_OBS_MOD_HEALTH:
                            food = true;
                            break;

                        case SPELL_AURA_MOD_POWER_REGEN:
                        case SPELL_AURA_OBS_MOD_MANA:
                            drink = true;
                            break;
                        default:
                            break;
                    }
                }

                if (food && drink)
                {
                    return SPELL_FOOD_AND_DRINK;
                }
                else if (food)
                {
                    return SPELL_FOOD;
                }
                else if (drink)
                {
                    return SPELL_DRINK;
                }
            }
            else
            {

                if (spellInfo->HasAttribute(SPELL_ATTR_EX2_FOOD_BUFF))
                {
                    return SPELL_WELL_FED;
                }
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {

            if (spellInfo->SpellClassMask & UI64LIT(0x12000000))
            {
                return SPELL_MAGE_ARMOR;
            }

            if (spellInfo->EffectAura[EFFECT_INDEX_0] == SPELL_AURA_MOD_CONFUSE && spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
            {
                return SPELL_MAGE_POLYMORPH;
            }

            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            if (spellInfo->SpellClassMask & UI64LIT(0x00008000010000))
            {
                return SPELL_POSITIVE_SHOUT;
            }

            break;
        }
        case SPELLFAMILY_WARLOCK:
        {

            if (spellInfo->DispelType == DISPEL_CURSE)
            {
                return SPELL_CURSE;
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {

            if (spellInfo->HasAttribute(SPELL_ATTR_CASTABLE_WHILE_SITTING) &&
                (spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_AUTOATTACK) &&
                (spellInfo->SpellIconID == 52 || spellInfo->SpellIconID == 79))
            {
                return SPELL_WELL_FED;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {

            if (spellInfo->DispelType == DISPEL_POISON)
            {
                return SPELL_STING;
            }

            if (spellInfo->ActiveIconID == 122 && spellInfo->ID != 75)
            {
                return SPELL_ASPECT;
            }

            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            if (IsSealSpell(spellInfo))
            {
                return SPELL_SEAL;
            }

            if (spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000010000100)))
            {
                return SPELL_BLESSING;
            }

            if ((spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000020180400))) && spellInfo->BaseLevel != 0)
            {
                return SPELL_JUDGEMENT;
            }

            if (spellInfo->HasSpellEffect(SPELL_EFFECT_APPLY_AREA_AURA_PARTY))

            {
                return SPELL_AURA;
            }
        }
        case SPELLFAMILY_SHAMAN:
        {
            if (IsElementalShield(spellInfo))
            {
                return SPELL_ELEMENTAL_SHIELD;
            }

            break;
        }

        case SPELLFAMILY_POTION:
            return sSpellMgr.GetSpellElixirSpecific(spellInfo->ID);
    }

    if (spellInfo->SpellVisualID == 130 && spellInfo->SpellIconID == 89)
    {
        return SPELL_WARLOCK_ARMOR;
    }

    if (IsSpellHaveAura(spellInfo, SPELL_AURA_TRACK_CREATURES) ||
        IsSpellHaveAura(spellInfo, SPELL_AURA_TRACK_STEALTHED) ||
        (IsSpellHaveAura(spellInfo, SPELL_AURA_TRACK_RESOURCES) && !spellInfo->HasAttribute(SPELL_ATTR_PASSIVE) && !spellInfo->HasAttribute(SPELL_ATTR_CANT_CANCEL)))
    {
        return SPELL_TRACKER;
    }

    if (SpellSpecific sp = sSpellMgr.GetSpellElixirSpecific(spellInfo->ID))
    {
        return sp;
    }

    return SPELL_NORMAL;
}

bool IsSingleFromSpellSpecificPerTargetPerCaster(SpellSpecific spellSpec1, SpellSpecific spellSpec2)
{
    switch (spellSpec1)
    {
        case SPELL_BLESSING:
        case SPELL_AURA:
        case SPELL_STING:
        case SPELL_CURSE:
        case SPELL_ASPECT:
        case SPELL_POSITIVE_SHOUT:
        case SPELL_JUDGEMENT:
            return spellSpec1 == spellSpec2;
        default:
            return false;
    }
}

bool IsSingleFromSpellSpecificSpellRanksPerTarget(SpellSpecific spellSpec1, SpellSpecific spellSpec2)
{
    switch (spellSpec1)
    {
        case SPELL_BLESSING:
        case SPELL_AURA:
        case SPELL_CURSE:
        case SPELL_ASPECT:
        case SPELL_POSITIVE_SHOUT:
            return spellSpec1 == spellSpec2;
        default:
            return false;
    }
}

bool IsSingleFromSpellSpecificPerTarget(SpellSpecific spellSpec1, SpellSpecific spellSpec2)
{
    switch (spellSpec1)
    {
        case SPELL_SEAL:
        case SPELL_TRACKER:
        case SPELL_WARLOCK_ARMOR:
        case SPELL_MAGE_ARMOR:
        case SPELL_ELEMENTAL_SHIELD:
        case SPELL_MAGE_POLYMORPH:
        case SPELL_WELL_FED:
            return spellSpec1 == spellSpec2;
        case SPELL_BATTLE_ELIXIR:
            return spellSpec2 == SPELL_BATTLE_ELIXIR ||
                spellSpec2 == SPELL_FLASK_ELIXIR;
        case SPELL_GUARDIAN_ELIXIR:
            return spellSpec2 == SPELL_GUARDIAN_ELIXIR ||
                spellSpec2 == SPELL_FLASK_ELIXIR;
        case SPELL_FLASK_ELIXIR:
            return spellSpec2 == SPELL_BATTLE_ELIXIR ||
                spellSpec2 == SPELL_GUARDIAN_ELIXIR ||
                spellSpec2 == SPELL_FLASK_ELIXIR;
        case SPELL_FOOD:
            return spellSpec2 == SPELL_FOOD ||
                spellSpec2 == SPELL_FOOD_AND_DRINK;
        case SPELL_DRINK:
            return spellSpec2 == SPELL_DRINK ||
                spellSpec2 == SPELL_FOOD_AND_DRINK;
        case SPELL_FOOD_AND_DRINK:
            return spellSpec2 == SPELL_FOOD ||
                spellSpec2 == SPELL_DRINK ||
                spellSpec2 == SPELL_FOOD_AND_DRINK;
        default:
            return false;
    }
}

bool IsPositiveTarget(uint32 targetA, uint32 targetB)
{
    switch (targetA)
    {

        case TARGET_CHAIN_DAMAGE:
        case TARGET_ALL_ENEMY_IN_AREA:
        case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
        case TARGET_IN_FRONT_OF_CASTER:
        case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
        case TARGET_CURRENT_ENEMY_COORDINATES:
            return false;

        case TARGET_CASTER_COORDINATES:
            return (targetB == TARGET_ALL_PARTY || targetB == TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER);
        default:
            break;
    }
    if (targetB)
    {
        return IsPositiveTarget(targetB, 0);
    }
    return true;
}

bool IsExplicitPositiveTarget(uint32 targetA)
{

    switch (targetA)
    {
        case TARGET_SINGLE_FRIEND:
        case TARGET_SINGLE_PARTY:
        case TARGET_CHAIN_HEAL:
        case TARGET_SINGLE_FRIEND_2:
        case TARGET_AREAEFFECT_PARTY_AND_CLASS:
            return true;
        default:
            break;
    }
    return false;
}

bool IsExplicitNegativeTarget(uint32 targetA)
{

    switch (targetA)
    {
        case TARGET_CHAIN_DAMAGE:
        case TARGET_CURRENT_ENEMY_COORDINATES:
            return true;
        default:
            break;
    }
    return false;
}

bool IsPositiveEffect(SpellEntry const* spellproto, SpellEffectIndex effIndex)
{

    switch (spellproto->ID)
    {
        case 13003:
        case 13010:
        case 13139:
        case 23182:
        case 23445:
        case 25040:
            return false;
        default:
            break;
    }

    switch (spellproto->Effect[effIndex])
    {
        case SPELL_EFFECT_DUMMY:

            switch (spellproto->ID)
            {
                case 28441:
                    return false;
                case 10258:
                case 18153:
                    return true;
                default:
                    break;
            }
            break;

        case SPELL_EFFECT_HEAL:
        case SPELL_EFFECT_LEARN_SPELL:
        case SPELL_EFFECT_SKILL_STEP:
        case SPELL_EFFECT_QUEST_COMPLETE:
            return true;

        case SPELL_EFFECT_APPLY_AURA:
        {
            switch (spellproto->EffectAura[effIndex])
            {
                case SPELL_AURA_DUMMY:
                {

                    switch (spellproto->ID)
                    {
                        case 13139:
                        case 18172:
                        case 23445:
                            return false;

                        case 27184:
                        case 27190:
                        case 27191:
                        case 27201:
                        case 27202:
                        case 27203:
                            return true;
                        default:
                            break;
                    }
                }   break;
                case SPELL_AURA_MOD_DAMAGE_DONE:
                case SPELL_AURA_MOD_RESISTANCE:
                case SPELL_AURA_MOD_STAT:
                case SPELL_AURA_MOD_SKILL:
                case SPELL_AURA_MOD_DODGE_PERCENT:
                case SPELL_AURA_MOD_HEALING_PCT:
                case SPELL_AURA_MOD_HEALING_DONE:
                    if (spellproto->CalculateSimpleValue(effIndex) < 0)
                    {
                        return false;
                    }
                    break;
                case SPELL_AURA_MOD_DAMAGE_TAKEN:
                case SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN:
                    if (spellproto->CalculateSimpleValue(effIndex) < 0)
                    {
                        return true;
                    }

                    break;
                case SPELL_AURA_MOD_SPELL_CRIT_CHANCE:
                case SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT:
                case SPELL_AURA_MOD_DAMAGE_PERCENT_DONE:
                    if (spellproto->CalculateSimpleValue(effIndex) > 0)
                    {
                        return true;
                    }
                    break;
                case SPELL_AURA_ADD_TARGET_TRIGGER:
                    return true;
                case SPELL_AURA_PERIODIC_TRIGGER_SPELL:
                    if (spellproto->ID != spellproto->EffectTriggerSpell[effIndex])
                    {
                        uint32 spellTriggeredId = spellproto->EffectTriggerSpell[effIndex];
                        SpellEntry const* spellTriggeredProto = sSpellStore.LookupEntry(spellTriggeredId);

                        if (spellTriggeredProto)
                        {

                            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
                            {

                                if (spellTriggeredProto->Effect[i] &&
                                    IsPositiveTarget(spellTriggeredProto->ImplicitTargetA[i], spellTriggeredProto->ImplicitTargetB[i]) &&
                                    !IsPositiveEffect(spellTriggeredProto, SpellEffectIndex(i)))
                                {
                                    return false;
                                }
                            }
                        }
                    }
                    break;
                case SPELL_AURA_PROC_TRIGGER_SPELL:

                    break;
                case SPELL_AURA_MOD_STUN:
                    if (effIndex == EFFECT_INDEX_0 && spellproto->Effect[EFFECT_INDEX_1] == 0 && spellproto->Effect[EFFECT_INDEX_2] == 0)
                    {
                        return false;
                    }

                    if (spellproto->ID == 17624)
                    {
                        return false;
                    }
                    break;
                case SPELL_AURA_MOD_PACIFY_SILENCE:
                    if (spellproto->ID == 24740)
                    {
                        return true;
                    }
                    return false;
                case SPELL_AURA_MOD_ROOT:
                case SPELL_AURA_MOD_SILENCE:
                    if (spellproto->ID == 24732)
                    {
                        return true;
                    }
                case SPELL_AURA_GHOST:
                case SPELL_AURA_PERIODIC_LEECH:
                case SPELL_AURA_MOD_STALKED:
                case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
                    return false;
                case SPELL_AURA_PERIODIC_DAMAGE:

                    if (spellproto->ImplicitTargetA[effIndex] == TARGET_SELF)
                    {
                        return false;
                    }
                    break;
                case SPELL_AURA_MOD_DECREASE_SPEED:

                    if (spellproto->ImplicitTargetA[effIndex] == TARGET_SELF &&
                        spellproto->SpellClassSet == SPELLFAMILY_GENERIC)
                    {
                        return false;
                    }

                    if (spellproto->HasAttribute(SPELL_ATTR_AURA_IS_DEBUFF) && effIndex == EFFECT_INDEX_0)
                    {
                        return false;
                    }
                    break;
                case SPELL_AURA_MOD_SCALE:

                    switch (spellproto->ID)
                    {
                        case 802:
                            return true;
                    }
                    break;
                case SPELL_AURA_MECHANIC_IMMUNITY:
                {

                    switch (spellproto->EffectMiscValue[effIndex])
                    {
                        case MECHANIC_BANDAGE:
                        case MECHANIC_SHIELD:
                        case MECHANIC_MOUNT:
                        case MECHANIC_INVULNERABILITY:
                            return false;
                        default:
                            break;
                    }
                }   break;
                case SPELL_AURA_ADD_FLAT_MODIFIER:
                case SPELL_AURA_ADD_PCT_MODIFIER:
                {

                    switch (spellproto->EffectMiscValue[effIndex])
                    {
                        case SPELLMOD_COST:
                            if (spellproto->ID == 12042)
                            {
                                break;
                            }

                            if (spellproto->CalculateSimpleValue(effIndex) > 0)
                            {
                                return false;
                            }
                            break;
                        default:
                            break;
                    }
                }   break;
                default:
                    break;
            }
            break;
        }
        case SPELL_EFFECT_SCRIPT_EFFECT:
        {
            if (spellproto->ID == 5249)
            {
                return false;
            }
            break;
        }

        default:
            break;
    }

    if (!IsPositiveTarget(spellproto->ImplicitTargetA[effIndex], spellproto->ImplicitTargetB[effIndex]))
    {
        return false;
    }

    if (spellproto->HasAttribute(SPELL_ATTR_EX_CANT_BE_REFLECTED))
    {
        return false;
    }

    return true;
}

bool IsSingleTargetSpell(SpellEntry const* spellInfo)
{

    if (spellInfo->SpellVisualID == 3239)
    {
        return true;
    }

    switch (spellInfo->ID)
    {
        case 1833:
        case 4538:
        case 5106:
        case 5530:
        case 5648:
        case 5649:
        case 5726:
        case 5727:
        case 6927:
        case 8399:
        case 9159:
        case 9256:
        case 13902:
        case 14902:
        case 16104:
        case 17286:
        case 20277:
        case 20669:
        case 20683:
        case 24664:
            return false;
    }

    if (spellInfo->HasAttribute(SPELL_ATTR_EX_UNK18))
    {
        return true;
    }

    if ((spellInfo->SpellIconID == 98 && spellInfo->SpellVisualID == 336) ||

        (spellInfo->SpellIconID == 96 && spellInfo->SpellVisualID == 1305) ||

        spellInfo->IsFitToFamily(SPELLFAMILY_DRUID, ClassFamilyMask(UI64LIT(0x0200))))
    {
        return true;
    }

    switch (GetSpellSpecific(spellInfo->ID))
    {
        case SPELL_JUDGEMENT:
            return true;
        default:
            break;
    }

    return false;
}

bool IsSingleTargetSpells(SpellEntry const* spellInfo1, SpellEntry const* spellInfo2)
{

    if (spellInfo1->SpellClassSet == spellInfo2->SpellClassSet &&
        spellInfo1->SpellIconID == spellInfo2->SpellIconID)
    {
        return true;
    }

    SpellSpecific spec1 = GetSpellSpecific(spellInfo1->ID);

    switch (spec1)
    {
        case SPELL_JUDGEMENT:
        case SPELL_MAGE_POLYMORPH:
            if (GetSpellSpecific(spellInfo2->ID) == spec1)
            {
                return true;
            }
            break;
        default:
            break;
    }

    return false;
}

SpellCastResult GetErrorAtShapeshiftedCast(SpellEntry const* spellInfo, uint32 form)
{

    if (GetTalentSpellCost(spellInfo->ID) > 0 &&
        (spellInfo->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_LEARN_SPELL || spellInfo->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_LEARN_SPELL || spellInfo->Effect[EFFECT_INDEX_2] == SPELL_EFFECT_LEARN_SPELL))
    {
        return SPELL_CAST_OK;
    }

    uint32 stanceMask = (form ? 1 << (form - 1) : 0);

    if (stanceMask & spellInfo->ShapeshiftExclude)
    {
        return SPELL_FAILED_NOT_SHAPESHIFT;
    }

    if (stanceMask & spellInfo->ShapeshiftMask)
    {
        return SPELL_CAST_OK;
    }

    bool actAsShifted = false;
    if (form > 0)
    {
        SpellShapeshiftFormEntry const* shapeInfo = sSpellShapeshiftFormStore.LookupEntry(form);
        if (!shapeInfo)
        {
            sLog.outError("GetErrorAtShapeshiftedCast: unknown shapeshift %u", form);
            return SPELL_CAST_OK;
        }
        actAsShifted = !(shapeInfo->Flags & 1);
    }

    if (actAsShifted)
    {
        if (spellInfo->HasAttribute(SPELL_ATTR_NOT_SHAPESHIFT))
        {
            return SPELL_FAILED_NOT_SHAPESHIFT;
        }
        else if (spellInfo->ShapeshiftMask != 0)
        {
            return SPELL_FAILED_ONLY_SHAPESHIFT;
        }
    }
    else
    {

        if (!spellInfo->HasAttribute(SPELL_ATTR_EX2_NOT_NEED_SHAPESHIFT) && spellInfo->ShapeshiftMask != 0)
        {
            return SPELL_FAILED_ONLY_SHAPESHIFT;
        }
    }

    return SPELL_CAST_OK;
}

void SpellMgr::LoadSpellLinked()
{
    mSpellLinkedMap.clear();
    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `linked_entry`, `type`, `effect_mask` FROM `spell_linked`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString();
        sLog.outString(">> Spell linked definition not loaded - table empty");
        return;
    }

    BarGoLink bar(result->GetRowCount());
    do
    {
        Field *fields = result->Fetch();
        bar.step();
        uint32 entry       = fields[0].GetUInt32();
        uint32 linkedEntry = fields[1].GetUInt32();

        SpellEntry const* spell = sSpellStore.LookupEntry(entry);
        SpellEntry const* spell1 = sSpellStore.LookupEntry(linkedEntry);
        if (!spell || !spell1)
        {
            sLog.outErrorDb("Spells %u or %u listed in `spell_linked` does not exist", entry, linkedEntry);
            continue;
        }

        if (entry == linkedEntry)
        {
            sLog.outErrorDb("Spell %u linked with self!", entry);
            continue;
        }

        uint32 first_id = GetFirstSpellInChain(entry);

        if (first_id != entry)
        {
            sLog.outErrorDb("Spell %u listed in `spell_linked` is not first rank (%u) in chain", entry, first_id);
        }

        SpellLinkedEntry data;

        data.spellId      = entry;
        data.linkedId     = linkedEntry;
        data.type         = fields[2].GetUInt32();
        data.effectMask   = fields[3].GetUInt32();

        mSpellLinkedMap.insert(SpellLinkedMap::value_type(entry, data));

        ++count;

    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %u spell linked definitions", count);
}

SpellLinkedSet SpellMgr::GetSpellLinked(uint32 spell_id, SpellLinkedType type) const
{
    SpellLinkedSet result;

    SpellLinkedMapBounds const& bounds = GetSpellLinkedMapBounds(spell_id);

    if (type < SPELL_LINKED_TYPE_MAX && bounds.first != bounds.second)
    {
        for (SpellLinkedMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
        {
            if (itr->second.type == type)
            {
                result.insert(itr->second.linkedId);
            }
        }
    }
    return result;
}

void SpellMgr::ModDBCSpellAttributes()
{
    SpellEntry* spellInfo;

    std::list<uint32> list_spell_id;
    uint32 spell_id;

    list_spell_id.push_back(20647);
    list_spell_id.push_back(16870);

    for (std::list<uint32>::iterator it = list_spell_id.begin(); it != list_spell_id.end(); ++it)
    {
        spell_id = *it;
        spellInfo = (SpellEntry*)GetSpellStore()->LookupEntry(spell_id);
        if (!spellInfo)
        {
            continue;
        }

        switch (spell_id)
        {

            case 20647:
                spellInfo->Attributes |= SPELL_ATTR_IMPOSSIBLE_DODGE_PARRY_BLOCK;
                spellInfo->AttributesExC |= SPELL_ATTR_EX3_CANT_MISS;
                break;
            case 16870:
                spellInfo->ProcFlags = PROC_FLAG_NONE;
                break;
        }
    }
}

bool SpellMgr::IsRankSpellDueToSpell(SpellEntry const* spellInfo_1, uint32 spellId_2) const
{
    SpellEntry const* spellInfo_2 = sSpellStore.LookupEntry(spellId_2);
    if (!spellInfo_1 || !spellInfo_2)
    {
        return false;
    }
    if (spellInfo_1->ID == spellId_2)
    {
        return false;
    }

    return GetFirstSpellInChain(spellInfo_1->ID) == GetFirstSpellInChain(spellId_2);
}

bool SpellMgr::canStackSpellRanksInSpellBook(SpellEntry const* spellInfo) const
{
    if (cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive)
    {
        return false;
    }
    if (spellInfo->PowerType != POWER_MANA && spellInfo->PowerType != POWER_HEALTH)
    {
        return false;
    }
    if (IsProfessionOrRidingSpell(spellInfo->ID))
    {
        return false;
    }

    if (IsSkillBonusSpell(spellInfo->ID))
    {
        return false;
    }

    SkillLineAbilityMap::const_iterator itr = mSkillLineAbilityMap.find(spellInfo->ID);
    if (itr != mSkillLineAbilityMap.end())
    {
        if (itr->second->SupercededBySpell != 0)
        {
            return false;
        }
    }

    return true;
}

bool SpellMgr::IsNoStackSpellDueToSpell(uint32 spellId_1, uint32 spellId_2) const
{
    SpellEntry const* spellInfo_1 = sSpellStore.LookupEntry(spellId_1);
    SpellEntry const* spellInfo_2 = sSpellStore.LookupEntry(spellId_2);

    if (!spellInfo_1 || !spellInfo_2)
    {
        return false;
    }

    if (spellId_1 == spellId_2)
    {
        return false;
    }

    if ((spellInfo_1->ID == SPELL_ID_PASSIVE_RESURRECTION_SICKNESS) != (spellInfo_2->ID == SPELL_ID_PASSIVE_RESURRECTION_SICKNESS))
    {
        return false;
    }

    if (spellInfo_1->HasAttribute(SPELL_ATTR_PASSIVE) != spellInfo_2->HasAttribute(SPELL_ATTR_PASSIVE))
    {
        return false;
    }

    if (spellInfo_1->ID == 13278 || spellInfo_2->ID == 13278)
    {
        return false;
    }

    switch (spellInfo_1->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (spellInfo_2->SpellClassSet)
            {
                case SPELLFAMILY_GENERIC:
                {

                    if ((spellInfo_1->ID == 21992 && spellInfo_2->ID == 27648) ||
                        (spellInfo_2->ID == 21992 && spellInfo_1->ID == 27648))
                    {
                        return false;
                    }

                    if ((spellInfo_1->ID == 23182 && spellInfo_2->ID == 23183) ||
                        (spellInfo_2->ID == 23182 && spellInfo_1->ID == 23183))
                    {
                        return false;
                    }

                    if ((spellInfo_1->ID == 28093 && spellInfo_2->ID == 42084) ||
                        (spellInfo_2->ID == 28093 && spellInfo_1->ID == 42084))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 92 && spellInfo_2->SpellIconID == 92 &&
                        ((spellInfo_1->SpellVisualID == 99 && spellInfo_2->SpellVisualID == 0) ||
                        (spellInfo_2->SpellVisualID == 99 && spellInfo_1->SpellVisualID == 0)))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 240 && spellInfo_2->SpellIconID == 240 &&
                        ((spellInfo_1->SpellVisualID == 0 && spellInfo_2->SpellVisualID == 78) ||
                        (spellInfo_2->SpellVisualID == 0 && spellInfo_1->SpellVisualID == 78)))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 2606 && spellInfo_2->SpellIconID == 2606)
                    {
                        return false;
                    }

                    if ((spellInfo_1->ID == 23170 && spellInfo_2->ID == 23171) ||
                        (spellInfo_2->ID == 23170 && spellInfo_1->ID == 23171))
                    {
                        return false;
                    }

                    if ((spellInfo_1->ID == 8326 && spellInfo_2->ID == 20584) ||
                        (spellInfo_2->ID == 8326 && spellInfo_1->ID == 20584))
                    {
                        return false;
                    }

                    if ((spellInfo_1->ID == 23014 && spellInfo_2->ID == 19832) ||
                        (spellInfo_2->ID == 23014 && spellInfo_1->ID == 19832))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 1691 && spellInfo_2->SpellIconID == 1691)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 108 && spellInfo_2->SpellIconID == 108)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 320 && spellInfo_2->SpellIconID == 320)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 61 && spellInfo_2->SpellIconID == 61)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 502 && spellInfo_2->SpellIconID == 502)
                    {
                        return false;
                    }

                    if ((spellInfo_1->SpellIconID == 200 && ((spellInfo_2->ID == 16177 || spellInfo_2->ID == 16236 || spellInfo_2->ID == 16237) && !(spellInfo_1->ID == 16177 || spellInfo_1->ID == 16236 || spellInfo_1->ID == 16237))) ||
                        (spellInfo_2->SpellIconID == 200 && ((spellInfo_1->ID == 16177 || spellInfo_1->ID == 16236 || spellInfo_1->ID == 16237) && !(spellInfo_2->ID == 16177 || spellInfo_2->ID == 16236 || spellInfo_2->ID == 16237))))
                    {
                        return false;
                    }

                    if ((spellInfo_1->SpellIconID == 958 && ((spellInfo_2->ID == 14326 || spellInfo_2->ID == 14327 || spellInfo_2->ID == 1513) && !(spellInfo_1->ID == 1513 || spellInfo_1->ID == 14326 || spellInfo_1->ID == 14327))) ||
                        (spellInfo_2->SpellIconID == 958 && ((spellInfo_1->ID == 14326 || spellInfo_1->ID == 14327 || spellInfo_1->ID == 1513) && !(spellInfo_2->ID == 1513 || spellInfo_2->ID == 14326 || spellInfo_2->ID == 14327))))
                    {
                        return false;
                    }

                    break;
                }
                case SPELLFAMILY_MAGE:
                {

                    if (spellInfo_2->SpellIconID == 125 && spellInfo_1->ID == 18820)
                    {
                        return false;
                    }

                    if (spellInfo_1->ID == 132 && spellInfo_2->IsFitToFamilyMask(0x0000000000008000))
                    {
                        return false;
                    }

                    break;
                }
                case SPELLFAMILY_WARRIOR:
                {

                    if (spellInfo_1->ID == 5302 && spellInfo_2->ID == 2565)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 276 && spellInfo_2->ID == 71)
                    {
                        return false;
                    }

                    if (spellInfo_1->ID == 23694 && spellInfo_2->IsFitToFamilyMask(0x0000000000000002))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID==456 && spellInfo_2->IsFitToFamilyMask(0x0000000000010000))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID==245 && spellInfo_2->IsFitToFamilyMask(0x0000000000000020))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID==138 && spellInfo_2->IsFitToFamilyMask(0x0000000000000010))
                    {
                        return false;
                    }

                    break;
                }
                case SPELLFAMILY_DRUID:
                {

                    if (spellInfo_1->SpellIconID == 312 && spellInfo_2->ID == 24932)
                    {
                        return false;
                    }

                    if (spellId_1 == 40216 && spellId_2 == 42016)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID==108 && spellInfo_2->IsFitToFamilyMask(0x0000000000800000))
                    {
                        return false;
                    }

                    break;
                }
                case SPELLFAMILY_ROGUE:
                {

                    if (spellInfo_1->SpellIconID == 498 && spellInfo_1->SpellVisualID == 0 && spellInfo_2->SpellIconID == 498)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID==245 && spellInfo_2->IsFitToFamilyMask(0x0000000000000008))
                    {
                        return false;
                    }

                    break;
                }
                case SPELLFAMILY_HUNTER:
                {

                    if (spellInfo_1->SpellIconID == 15 && spellInfo_2->IsFitToFamilyMask(0x0000000000000200))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 517 && spellInfo_2->IsFitToFamilyMask(0x0000000000000040))
                    {
                        return false;
                    }
                    break;
                }
                case SPELLFAMILY_PALADIN:
                {

                    if (spellInfo_1->SpellIconID == 502 && spellInfo_2->IsFitToFamilyMask(0x0000000004000000))
                    {
                        return false;
                    }

                    if (spellId_1 == 35081 && spellInfo_2->SpellIconID == 561 && spellInfo_2->SpellVisualID == 7992)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID==140 && spellInfo_2->IsFitToFamilyMask(0x0000000004000000))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID==291 && spellInfo_2->IsFitToFamilyMask(0x0000000000000040))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID==80 && spellInfo_2->IsFitToFamilyMask(0x0000000010000010))
                    {
                        return false;
                    }

                    break;
                }
                case SPELLFAMILY_PRIEST:
                {

                    if (spellInfo_1->SpellIconID == 207 && spellInfo_2->IsFitToFamilyMask(0x0000000000000100))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 264 && spellInfo_2->IsFitToFamilyMask(0x0000000180000000))
                    {
                        return false;
                    }
                    break;
                }
                case SPELLFAMILY_WARLOCK:
                {

                    if (spellInfo_1->SpellIconID == 313 && spellInfo_2->IsFitToFamilyMask(0x0000000000000002))
                    {
                        return false;
                    }
                    break;
                }
            }

            break;
        }
        case SPELLFAMILY_MAGE:
        {
            switch (spellInfo_2->SpellClassSet)
            {
                case SPELLFAMILY_GENERIC:
                {

                    if (spellInfo_2->ID == 132 && spellInfo_1->IsFitToFamilyMask(0x0000000000008000))
                    {
                        return false;
                    }

                    if (spellInfo_2->ID == 23182 && spellInfo_1->ID == 11958)
                    {
                        return true;
                    }

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000400) && spellInfo_2->ID == 18820)
                    {
                        return false;
                    }

                    break;
                }
                case SPELLFAMILY_MAGE:
                {

                    if (((spellInfo_1->SpellClassMask & UI64LIT(0x80)) && (spellInfo_2->SpellClassMask & UI64LIT(0x100000))) ||
                        ((spellInfo_2->SpellClassMask & UI64LIT(0x80)) && (spellInfo_1->SpellClassMask & UI64LIT(0x100000))))
                    {
                        return false;
                    }

                    if (((spellInfo_1->SpellClassMask & UI64LIT(0x0000000000010000)) && (spellInfo_2->SpellVisualID == 72 && spellInfo_2->SpellIconID == 1499)) ||
                        ((spellInfo_2->SpellClassMask & UI64LIT(0x0000000000010000)) && (spellInfo_1->SpellVisualID == 72 && spellInfo_1->SpellIconID == 1499)))
                    {
                        return false;
                    }

                    if (((spellInfo_1->SpellClassMask & UI64LIT(0x1)) && (spellInfo_2->SpellClassMask & UI64LIT(0x400000))) ||
                        ((spellInfo_2->SpellClassMask & UI64LIT(0x1)) && (spellInfo_1->SpellClassMask & UI64LIT(0x400000))))
                    {
                        return false;
                    }

                    break;
                }
                case SPELLFAMILY_PALADIN:
                {

                    if (spellInfo_1->ID == 28682 && spellInfo_2->IsFitToFamilyMask(0x0000000004000000))
                    {
                        return false;
                    }

                    break;
                }
            }

            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            switch (spellInfo_2->SpellClassSet)
            {
                case SPELLFAMILY_GENERIC:

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000002) && spellInfo_2->SpellIconID == 313)
                    {
                        return false;
                    }
                    break;
                case SPELLFAMILY_WARLOCK:

                    if ((spellInfo_1->SpellIconID == 152 && spellInfo_2->SpellIconID == 546) ||
                        (spellInfo_2->SpellIconID == 152 && spellInfo_1->SpellIconID == 546))
                    {
                        return false;
                    }

                    if ((spellInfo_1->SpellIconID == 313 && spellInfo_2->SpellIconID == 1932) ||
                        (spellInfo_2->SpellIconID == 313 && spellInfo_1->SpellIconID == 1932))
                    {
                        if (spellInfo_1->SpellVisualID != 0 && spellInfo_2->SpellVisualID != 0)
                        {
                            return true;
                        }
                    }

                    if ((spellInfo_1->SpellIconID == 313 && (spellInfo_2->SpellIconID == 544  || spellInfo_2->SpellIconID == 91)) ||
                        (spellInfo_2->SpellIconID == 313 && (spellInfo_1->SpellIconID == 544  || spellInfo_1->SpellIconID == 91)))
                    {
                        return false;
                    }
                    break;
                case SPELLFAMILY_PRIEST:

                    if (spellInfo_1->SpellIconID == 1488 && spellInfo_2->IsFitToFamilyMask(0x0000000000010000))
                    {
                        return false;
                    }
                    break;
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            switch (spellInfo_2->SpellClassSet)
            {
                case SPELLFAMILY_GENERIC:

                    if (spellInfo_1->ID == 71 && spellInfo_2->SpellIconID == 276)
                    {
                        return false;
                    }

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000002) && spellInfo_2->ID == 23694)
                    {
                        return false;
                    }

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000010000) && spellInfo_2->SpellIconID==456)
                    {
                        return false;
                    }

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000020) && spellInfo_2->SpellIconID==245)
                    {
                        return false;
                    }

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000010) && spellInfo_2->SpellIconID==138)
                    {
                        return false;
                    }

                    break;
                case SPELLFAMILY_WARRIOR:

                    if (((spellInfo_1->SpellClassMask & UI64LIT(0x20)) && (spellInfo_2->SpellClassMask & UI64LIT(0x1000000000))) ||
                        ((spellInfo_2->SpellClassMask & UI64LIT(0x20)) && (spellInfo_1->SpellClassMask & UI64LIT(0x1000000000))))
                    {
                        return false;
                    }

                    if ((spellInfo_1->SpellIconID == 456 && spellInfo_2->SpellIconID == 2006) ||
                        (spellInfo_2->SpellIconID == 456 && spellInfo_1->SpellIconID == 2006))
                    {
                        return false;
                    }

                    if ((spellInfo_1->IsFitToFamilyMask(0x0000000000020000) && spellInfo_2->IsFitToFamilyMask(0x0000000000010000)) ||
                        (spellInfo_1->IsFitToFamilyMask(0x0000000000010000) && spellInfo_2->IsFitToFamilyMask(0x0000000000020000)))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID==84 && spellInfo_2->SpellIconID==84)
                    {
                        return false;
                    }
                    break;
                case SPELLFAMILY_PALADIN:

                    if (spellInfo_2->IsFitToFamilyMask(0x0000000000000040) && spellInfo_1->SpellIconID==291)
                    {
                        return false;
                    }
                    break;
                case SPELLFAMILY_ROGUE:
                {

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000020) && spellInfo_2->IsFitToFamilyMask(0x0000000000000008))
                    {
                        return false;
                    }

                    break;
                }
            }

            break;
        }
        case SPELLFAMILY_PRIEST:
            switch (spellInfo_2->SpellClassSet)
            {
                case SPELLFAMILY_GENERIC:

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000100) && spellInfo_2->SpellIconID == 207)
                    {
                        return false;
                    }

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000180000000) && spellInfo_2->SpellIconID == 264)
                    {
                        return false;
                    }

                    break;
                case SPELLFAMILY_PRIEST:

                    if (((spellInfo_1->SpellClassMask & UI64LIT(0x2000000)) && (spellInfo_2->SpellClassMask & UI64LIT(0x4000000))) ||
                        ((spellInfo_2->SpellClassMask & UI64LIT(0x2000000)) && (spellInfo_1->SpellClassMask & UI64LIT(0x4000000))))
                    {
                        return false;
                    }

                    if (((spellInfo_1->SpellClassMask & UI64LIT(0x200000)) && (spellInfo_2->SpellClassMask & UI64LIT(0x8000))) ||
                        ((spellInfo_2->SpellClassMask & UI64LIT(0x200000)) && (spellInfo_1->SpellClassMask & UI64LIT(0x8000))))
                    {
                        return false;
                    }
                    break;

                case SPELLFAMILY_WARLOCK:

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000010000) && spellInfo_2->SpellIconID == 1488)
                    {
                        return false;
                    }

                    break;

                case SPELLFAMILY_DRUID:

                    if (spellInfo_1->SpellIconID == 52 && spellInfo_2->ID == 28791)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 52 && spellInfo_2->ID == 28826)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 1873 && spellInfo_2->ID == 28795)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 1873 && spellInfo_2->ID == 28824)
                    {
                        return false;
                    }

                    break;
            }
            break;
        case SPELLFAMILY_DRUID:
            switch (spellInfo_2->SpellClassSet)
            {
                case SPELLFAMILY_GENERIC:

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000800000) && spellInfo_1->SpellIconID==108)
                    {
                        return false;
                    }

                    if (spellInfo_1->ID == 24932 && spellInfo_2->SpellIconID == 312)
                    {
                        return false;
                    }

                    break;
                case SPELLFAMILY_DRUID:

                    if (spellInfo_1->SpellIconID == 493 && spellInfo_2->SpellIconID == 493)
                    {
                        return false;
                    }

                    if (((!spellInfo_1->SpellClassMask && spellInfo_1->SpellIconID == 108) && (spellInfo_2->SpellClassMask & UI64LIT(0x20000000000000))) ||
                        ((!spellInfo_2->SpellClassMask && spellInfo_2->SpellIconID == 108) && (spellInfo_1->SpellClassMask & UI64LIT(0x20000000000000))))
                    {
                        return false;
                    }
                    break;

                case SPELLFAMILY_PALADIN:

                    if (spellInfo_1->ID == 28790 && spellInfo_2->IsFitToFamilyMask(0x0000000011000000))
                    {
                        return false;
                    }

                    break;

                case SPELLFAMILY_PRIEST:

                    if (spellInfo_1->ID == 28791 && spellInfo_2->SpellIconID == 52)
                    {
                        return false;
                    }

                    if (spellInfo_1->ID == 28826 && spellInfo_2->SpellIconID == 52)
                    {
                        return false;
                    }

                    if (spellInfo_1->ID == 28795 && spellInfo_2->SpellIconID == 1873)
                    {
                        return false;
                    }

                    if (spellInfo_1->ID == 28824 && spellInfo_2->SpellIconID == 1873)
                    {
                        return false;
                    }

                    break;
            }

            break;
        case SPELLFAMILY_ROGUE:
            switch (spellInfo_2->SpellClassSet)
            {
                case SPELLFAMILY_GENERIC:
                {

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000008) && spellInfo_2->SpellIconID==245)
                    {
                        return false;
                    }

                    break;
                }
                case SPELLFAMILY_WARRIOR:
                {

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000008) && spellInfo_2->IsFitToFamilyMask(0x0000000000000020))
                    {
                        return false;
                    }

                    break;
                }
            }

            break;
        case SPELLFAMILY_HUNTER:
            switch (spellInfo_2->SpellClassSet)
            {
                case SPELLFAMILY_GENERIC:

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000040) && spellInfo_2->SpellIconID == 517)
                    {
                        return false;
                    }

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000200) && spellInfo_2->SpellIconID == 15)
                    {
                        return false;
                    }

                    break;
                case SPELLFAMILY_HUNTER:

                    if (((spellInfo_1->SpellClassMask & UI64LIT(0x20)) && (spellInfo_2->SpellClassMask & UI64LIT(0x20000000000))) ||
                        ((spellInfo_2->SpellClassMask & UI64LIT(0x20)) && (spellInfo_1->SpellClassMask & UI64LIT(0x20000000000))))
                    {
                        return false;
                    }

                    if (((spellInfo_1->SpellClassMask & UI64LIT(0x4)) && (spellInfo_2->SpellClassMask & UI64LIT(0x00000004000))) ||
                        ((spellInfo_2->SpellClassMask & UI64LIT(0x4)) && (spellInfo_1->SpellClassMask & UI64LIT(0x00000004000))))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 1680 && spellInfo_2->SpellIconID == 1680)
                    {
                        return false;
                    }

                    break;
            }

            break;
        case SPELLFAMILY_PALADIN:
            switch (spellInfo_2->SpellClassSet)
            {
                case SPELLFAMILY_GENERIC:

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000004000000) && spellInfo_2->SpellIconID==140)
                    {
                        return false;
                    }

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000040) && spellInfo_2->SpellIconID==291)
                    {
                        return false;
                    }

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000010000010) && spellInfo_2->SpellIconID==80)
                    {
                        return false;
                    }

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000004000000) && spellInfo_2->SpellIconID == 502)
                    {
                        return false;
                    }

                    break;
                case SPELLFAMILY_PALADIN:

                    if (IsSealSpell(spellInfo_1) && IsSealSpell(spellInfo_2))
                    {
                        return true;
                    }

                    if ((spellInfo_1->SpellIconID == 1487) && (spellInfo_2->SpellIconID == 1487))
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 237 && spellInfo_2->SpellIconID == 237)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 299 && spellInfo_2->SpellIconID == 299)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 206 && spellInfo_2->SpellIconID == 206)
                    {
                        return false;
                    }

                    if (spellInfo_1->SpellIconID == 307 && spellInfo_2->SpellIconID == 307)
                    {
                        return false;
                    }
                    break;
                case SPELLFAMILY_WARRIOR:

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000000000040) && spellInfo_2->SpellIconID==291)
                    {
                        return false;
                    }
                    break;

                case SPELLFAMILY_DRUID:

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000011000000) && spellInfo_2->ID == 28790)
                    {
                        return false;
                    }

                    break;
                case SPELLFAMILY_MAGE:

                    if (spellInfo_1->IsFitToFamilyMask(0x0000000004000000) && spellInfo_2->ID == 28682)
                    {
                        return false;
                    }

                    break;
            }

            break;
        case SPELLFAMILY_SHAMAN:
            switch (spellInfo_2->SpellClassSet)
            {
                case SPELLFAMILY_SHAMAN:

                    if (spellInfo_1->SpellIconID == 220 && spellInfo_2->SpellIconID == 220 &&
                        !spellInfo_1->IsFitToFamilyMask(spellInfo_2->SpellClassMask))
                    {
                        return false;
                    }
                    break;
            }

            break;
        default:
            break;
    }

    if (spellInfo_1->SpellIconID == spellInfo_2->SpellIconID &&
        spellInfo_1->SpellIconID != 0 && spellInfo_2->SpellIconID != 0)
    {
        bool isModifier = false;
        bool hasMatchingAura = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (spellInfo_1->EffectAura[i] == SPELL_AURA_ADD_FLAT_MODIFIER ||
                spellInfo_1->EffectAura[i] == SPELL_AURA_ADD_PCT_MODIFIER  ||
                spellInfo_2->EffectAura[i] == SPELL_AURA_ADD_FLAT_MODIFIER ||
                spellInfo_2->EffectAura[i] == SPELL_AURA_ADD_PCT_MODIFIER)
            {
                isModifier = true;
            }

            if (spellInfo_1->EffectAura[i] && spellInfo_2->EffectAura[i] &&
                spellInfo_1->EffectAura[i] == spellInfo_2->EffectAura[i])
            {
                hasMatchingAura = true;
            }
        }

        if (!isModifier && hasMatchingAura)
        {
            return true;
        }
    }

    if (IsRankSpellDueToSpell(spellInfo_1, spellId_2))
    {
        return true;
    }

    if (spellInfo_1->SpellClassSet == 0 || spellInfo_2->SpellClassSet == 0)
    {
        return false;
    }

    if (spellInfo_1->SpellClassSet != spellInfo_2->SpellClassSet)
    {
        return false;
    }

    bool dummy_only = true;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo_1->Effect[i] != spellInfo_2->Effect[i] ||
            spellInfo_1->EffectItemType[i] != spellInfo_2->EffectItemType[i] ||
            spellInfo_1->EffectMiscValue[i] != spellInfo_2->EffectMiscValue[i] ||
            spellInfo_1->EffectAura[i] != spellInfo_2->EffectAura[i])
        {
            return false;
        }

        if (spellInfo_1->Effect[i] && spellInfo_1->Effect[i] != SPELL_EFFECT_DUMMY && spellInfo_1->EffectAura[i] != SPELL_AURA_DUMMY)
        {
            dummy_only = false;
        }
    }
    if (dummy_only)
    {
        return false;
    }

    return true;
}

bool SpellMgr::IsProfessionOrRidingSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        return false;
    }

    if (spellInfo->Effect[EFFECT_INDEX_1] != SPELL_EFFECT_SKILL)
    {
        return false;
    }

    uint32 skill = spellInfo->EffectMiscValue[EFFECT_INDEX_1];

    return IsProfessionOrRidingSkill(skill);
}

bool SpellMgr::IsProfessionSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        return false;
    }

    if (spellInfo->Effect[EFFECT_INDEX_1] != SPELL_EFFECT_SKILL)
    {
        return false;
    }

    uint32 skill = spellInfo->EffectMiscValue[EFFECT_INDEX_1];

    return IsProfessionSkill(skill);
}

bool SpellMgr::IsPrimaryProfessionSpell(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        return false;
    }

    if (spellInfo->Effect[EFFECT_INDEX_1] != SPELL_EFFECT_SKILL)
    {
        return false;
    }

    uint32 skill = spellInfo->EffectMiscValue[EFFECT_INDEX_1];

    return IsPrimaryProfessionSkill(skill);
}

bool SpellMgr::IsPrimaryProfessionFirstRankSpell(uint32 spellId) const
{
    return IsPrimaryProfessionSpell(spellId) && GetSpellRank(spellId) == 1;
}

bool SpellMgr::IsSkillBonusSpell(uint32 spellId) const
{
    SkillLineAbilityMapBounds bounds = GetSkillLineAbilityMapBounds(spellId);

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        SkillLineAbilityEntry const* pAbility = _spell_idx->second;
        if (!pAbility || pAbility->AcquireMethod != ABILITY_LEARNED_ON_GET_PROFESSION_SKILL)
        {
            continue;
        }

        if (pAbility->MinSkillLineRank > 0)
        {
            return true;
        }
    }

    return false;
}

SpellEntry const* SpellMgr::SelectAuraRankForLevel(SpellEntry const* spellInfo, uint32 level) const
{

    if (level + 10 >= spellInfo->SpellLevel)
    {
        return spellInfo;
    }

    if (cast::RecipeOf(*spellInfo).Starts() == cast::Start::Passive)
    {
        return spellInfo;
    }

    bool needRankSelection = false;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {

        if (((spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AURA &&
            (IsExplicitPositiveTarget(spellInfo->ImplicitTargetA[i]) ||
            IsAreaEffectPossitiveTarget(Targets(spellInfo->ImplicitTargetA[i])))) ||
            spellInfo->Effect[i] == SPELL_EFFECT_APPLY_AREA_AURA_PARTY) &&
            IsPositiveEffect(spellInfo, SpellEffectIndex(i)))
        {
            needRankSelection = true;
            break;
        }
    }

    if (!needRankSelection || GetSpellRank(spellInfo->ID) == 0)
    {
        return spellInfo;
    }

    for (uint32 nextSpellId = spellInfo->ID; nextSpellId != 0; nextSpellId = GetPrevSpellInChain(nextSpellId))
    {
        SpellEntry const* nextSpellInfo = sSpellStore.LookupEntry(nextSpellId);
        if (!nextSpellInfo)
        {
            break;
        }

        if (level + 10 >= nextSpellInfo->SpellLevel)
        {
            return nextSpellInfo;
        }

    }

    return nullptr;
}

void SpellMgr::CheckUsedSpells(char const* table)
{
    uint32 countSpells = 0;
    uint32 countMasks = 0;

    QueryResult* result = WorldDatabase.PQuery("SELECT `spellid`,`SpellFamilyName`,`SpellFamilyMask`,`SpellIcon`,`SpellVisual`,`SpellCategory`,`EffectType`,`EffectAura`,`EffectIdx`,`Name`,`Code` FROM `%s`", table);

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outErrorDb("`%s` table is empty!", table);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 spell       = fields[0].GetUInt32();
        int32  family      = fields[1].GetInt32();
        uint64 familyMask  = fields[2].GetUInt64();
        int32  spellIcon   = fields[3].GetInt32();
        int32  spellVisual = fields[4].GetInt32();
        int32  category    = fields[5].GetInt32();
        int32  effectType  = fields[6].GetInt32();
        int32  auraType    = fields[7].GetInt32();
        int32  effectIdx   = fields[8].GetInt32();
        std::string name   = fields[9].GetCppString();
        std::string code   = fields[10].GetCppString();

        if (family < -1 || family > SPELLFAMILY_POTION)
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellFamily value(%u), skipped.", table, spell, family);
            continue;
        }

        if (spellIcon < -1)
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellIcon value(%u), skipped.", table, spell, spellIcon);
            continue;
        }

        if (spellVisual < -1)
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellVisual value(%u), skipped.", table, spell, spellVisual);
            continue;
        }

        if (category < -1 || (category >= 0 && sSpellCategoryStore.find(category) == sSpellCategoryStore.end()))
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellCategory value(%u), skipped.", table, spell, category);
            continue;
        }

        if (effectType < -1 || effectType >= TOTAL_SPELL_EFFECTS)
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellEffect type value(%u), skipped.", table, spell, effectType);
            continue;
        }

        if (auraType < -1 || auraType >= TOTAL_AURAS)
        {
            sLog.outError("Table '%s' for spell %u have wrong SpellAura type value(%u), skipped.", table, spell, auraType);
            continue;
        }

        if (effectIdx < -1 || effectIdx >= 3)
        {
            sLog.outError("Table '%s' for spell %u have wrong EffectIdx value(%u), skipped.", table, spell, effectIdx);
            continue;
        }

        if (spell)
        {
            ++countSpells;

            SpellEntry const* spellEntry = sSpellStore.LookupEntry(spell);
            if (!spellEntry)
            {
                sLog.outError("Spell %u '%s' not exist but used in %s.", spell, name.c_str(), code.c_str());
                continue;
            }

            if (family >= 0 && spellEntry->SpellClassSet != uint32(family))
            {
                sLog.outError("Spell %u '%s' family(%u) <> %u but used in %s.", spell, name.c_str(), spellEntry->SpellClassSet, family, code.c_str());
                continue;
            }

            if (familyMask != UI64LIT(0xFFFFFFFFFFFFFFFF))
            {
                if (familyMask == UI64LIT(0x0000000000000000))
                {
                    if (spellEntry->SpellClassMask)
                    {
                        sLog.outError("Spell %u '%s' not fit to (" UI64FMTD ") but used in %s.",
                            spell, name.c_str(), familyMask, code.c_str());
                        continue;
                    }
                }
                else
                {
                    if (!spellEntry->IsFitToFamilyMask(familyMask))
                    {
                        sLog.outError("Spell %u '%s' not fit to (" I64FMT ") but used in %s.", spell, name.c_str(), familyMask, code.c_str());
                        continue;
                    }
                }
            }

            if (spellIcon >= 0 && spellEntry->SpellIconID != uint32(spellIcon))
            {
                sLog.outError("Spell %u '%s' icon(%u) <> %u but used in %s.", spell, name.c_str(), spellEntry->SpellIconID, spellIcon, code.c_str());
                continue;
            }

            if (spellVisual >= 0 && spellEntry->SpellVisualID != uint32(spellVisual))
            {
                sLog.outError("Spell %u '%s' visual(%u) <> %u but used in %s.", spell, name.c_str(), spellEntry->SpellVisualID, spellVisual, code.c_str());
                continue;
            }

            if (category >= 0 && spellEntry->Category != uint32(category))
            {
                sLog.outError("Spell %u '%s' category(%u) <> %u but used in %s.", spell, name.c_str(), spellEntry->Category, category, code.c_str());
                continue;
            }

            if (effectIdx >= EFFECT_INDEX_0)
            {
                if (effectType >= 0 && spellEntry->Effect[effectIdx] != uint32(effectType))
                {
                    sLog.outError("Spell %u '%s' effect%d <> %u but used in %s.", spell, name.c_str(), effectIdx + 1, effectType, code.c_str());
                    continue;
                }

                if (auraType >= 0 && spellEntry->EffectAura[effectIdx] != uint32(auraType))
                {
                    sLog.outError("Spell %u '%s' aura%d <> %u but used in %s.", spell, name.c_str(), effectIdx + 1, auraType, code.c_str());
                    continue;
                }
            }
            else
            {
                if (effectType >= 0 && !spellEntry->HasSpellEffect(SpellEffects(effectType)))
                {
                    sLog.outError("Spell %u '%s' not have effect %u but used in %s.", spell, name.c_str(), effectType, code.c_str());
                    continue;
                }

                if (auraType >= 0 && !IsSpellHaveAura(spellEntry, AuraType(auraType)))
                {
                    sLog.outError("Spell %u '%s' not have aura %u but used in %s.", spell, name.c_str(), auraType, code.c_str());
                    continue;
                }
            }
        }
        else
        {
            ++countMasks;

            bool found = false;
            for (uint32 spellId = 1; spellId < sSpellStore.GetNumRows(); ++spellId)
            {
                SpellEntry const* spellEntry = sSpellStore.LookupEntry(spellId);
                if (!spellEntry)
                {
                    continue;
                }

                if (family >= 0 && spellEntry->SpellClassSet != uint32(family))
                {
                    continue;
                }

                if (familyMask != UI64LIT(0xFFFFFFFFFFFFFFFF))
                {
                    if (familyMask == UI64LIT(0x0000000000000000))
                    {
                        if (spellEntry->SpellClassMask)
                        {
                            continue;
                        }
                    }
                    else
                    {
                        if (!spellEntry->IsFitToFamilyMask(familyMask))
                        {
                            continue;
                        }
                    }
                }

                if (spellIcon >= 0 && spellEntry->SpellIconID != uint32(spellIcon))
                {
                    continue;
                }

                if (spellVisual >= 0 && spellEntry->SpellVisualID != uint32(spellVisual))
                {
                    continue;
                }

                if (category >= 0 && spellEntry->Category != uint32(category))
                {
                    continue;
                }

                if (effectIdx >= 0)
                {
                    if (effectType >= 0 && spellEntry->Effect[effectIdx] != uint32(effectType))
                    {
                        continue;
                    }

                    if (auraType >= 0 && spellEntry->EffectAura[effectIdx] != uint32(auraType))
                    {
                        continue;
                    }
                }
                else
                {
                    if (effectType >= 0 && !spellEntry->HasSpellEffect(SpellEffects(effectType)))
                    {
                        continue;
                    }

                    if (auraType >= 0 && !IsSpellHaveAura(spellEntry, AuraType(auraType)))
                    {
                        continue;
                    }
                }

                found = true;
                break;
            }

            if (!found)
            {
                if (effectIdx >= 0)
                {
                    sLog.outError("Spells '%s' not found for family %i (" I64FMT ") icon(%i) visual(%i) category(%i) effect%d(%i) aura%d(%i) but used in %s",
                        name.c_str(), family, familyMask, spellIcon, spellVisual, category, effectIdx + 1, effectType, effectIdx + 1, auraType, code.c_str());
                }
                else
                {
                    sLog.outError("Spells '%s' not found for family %i (" I64FMT ") icon(%i) visual(%i) category(%i) effect(%i) aura(%i) but used in %s",
                        name.c_str(), family, familyMask, spellIcon, spellVisual, category, effectType, auraType, code.c_str());
                }
                continue;
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Checked %u spells and %u spell masks", countSpells, countMasks);
}

DiminishingGroup GetDiminishingReturnsGroupForSpell(SpellEntry const* spellproto, bool triggered)
{

    switch (spellproto->SpellClassSet)
    {
        case SPELLFAMILY_ROGUE:
        {

            if (spellproto->IsFitToFamilyMask(UI64LIT(0x00000200000)))
            {
                return DIMINISHING_KIDNEYSHOT;
            }

            else if (spellproto->IsFitToFamilyMask(UI64LIT(0x00001000000)))
            {
                return DIMINISHING_BLIND;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
        {

            if (spellproto->IsFitToFamilyMask(UI64LIT(0x00000000008)))
            {
                return DIMINISHING_FREEZE;
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {

            if (spellproto->IsFitToFamilyMask(UI64LIT(0x0000000080000000)) && spellproto->Mechanic == MECHANIC_FEAR)
            {
                return DIMINISHING_WARLOCK_FEAR;
            }

            if (spellproto->IsFitToFamilyMask(UI64LIT(0x0000000080000000)))
            {
                return DIMINISHING_LIMITONLY;
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {

            if (spellproto->IsFitToFamilyMask(UI64LIT(0x00000000002)))
            {
                return DIMINISHING_LIMITONLY;
            }
            break;
        }
        default:
            break;
    }

    uint32 mechanic = GetAllSpellMechanicMask(spellproto);
    if (!mechanic)
    {
        return DIMINISHING_NONE;
    }

    if (mechanic & (1 << (MECHANIC_STUN - 1)))
    {
        return triggered ? DIMINISHING_TRIGGER_STUN : DIMINISHING_CONTROL_STUN;
    }
    if (mechanic & (1 << (MECHANIC_SLEEP - 1)))
    {
        return DIMINISHING_SLEEP;
    }
    if (mechanic & (1 << (MECHANIC_POLYMORPH - 1)))
    {
        return DIMINISHING_POLYMORPH;
    }
    if (mechanic & (1 << (MECHANIC_ROOT - 1)))
    {
        return triggered ? DIMINISHING_TRIGGER_ROOT : DIMINISHING_CONTROL_ROOT;
    }
    if (mechanic & (1 << (MECHANIC_FEAR - 1)))
    {
        return DIMINISHING_FEAR;
    }
    if (mechanic & (1 << (MECHANIC_CHARM - 1)))
    {
        return DIMINISHING_CHARM;
    }
    if (mechanic & (1 << (MECHANIC_SILENCE - 1)))
    {
        return DIMINISHING_SILENCE;
    }
    if (mechanic & (1 << (MECHANIC_DISARM - 1)))
    {
        return DIMINISHING_DISARM;
    }
    if (mechanic & (1 << (MECHANIC_FREEZE - 1)))
    {
        return DIMINISHING_FREEZE;
    }
    if (mechanic & ((1 << (MECHANIC_KNOCKOUT - 1)) | (1 << (MECHANIC_SAPPED - 1))))
    {
        return DIMINISHING_KNOCKOUT;
    }
    if (mechanic & (1 << (MECHANIC_BANISH - 1)))
    {
        return DIMINISHING_BANISH;
    }
    if (mechanic & (1 << (MECHANIC_HORROR - 1)))
    {
        return DIMINISHING_DEATHCOIL;
    }

    return DIMINISHING_NONE;
}

bool IsDiminishingReturnsGroupDurationLimited(DiminishingGroup group)
{
    switch (group)
    {
        case DIMINISHING_CONTROL_STUN:
        case DIMINISHING_TRIGGER_STUN:
        case DIMINISHING_KIDNEYSHOT:
        case DIMINISHING_SLEEP:
        case DIMINISHING_CONTROL_ROOT:
        case DIMINISHING_TRIGGER_ROOT:
        case DIMINISHING_FEAR:
        case DIMINISHING_WARLOCK_FEAR:
        case DIMINISHING_CHARM:
        case DIMINISHING_POLYMORPH:
        case DIMINISHING_FREEZE:
        case DIMINISHING_KNOCKOUT:
        case DIMINISHING_BLIND:
        case DIMINISHING_BANISH:
        case DIMINISHING_LIMITONLY:
            return true;
        default:
            return false;
    }
}

DiminishingReturnsType GetDiminishingReturnsGroupType(DiminishingGroup group)
{
    switch (group)
    {
        case DIMINISHING_BLIND:
        case DIMINISHING_CONTROL_STUN:
        case DIMINISHING_TRIGGER_STUN:
        case DIMINISHING_KIDNEYSHOT:
            return DRTYPE_ALL;
        case DIMINISHING_SLEEP:
        case DIMINISHING_CONTROL_ROOT:
        case DIMINISHING_TRIGGER_ROOT:
        case DIMINISHING_FEAR:
        case DIMINISHING_CHARM:
        case DIMINISHING_POLYMORPH:
        case DIMINISHING_SILENCE:
        case DIMINISHING_DISARM:
        case DIMINISHING_DEATHCOIL:
        case DIMINISHING_FREEZE:
        case DIMINISHING_BANISH:
        case DIMINISHING_WARLOCK_FEAR:
        case DIMINISHING_KNOCKOUT:
            return DRTYPE_PLAYER;
        default:
            break;
    }

    return DRTYPE_NONE;
}

bool SpellArea::IsFitToRequirements(Player const* player, uint32 newZone, uint32 newArea) const
{
    if (areaId)
    {

        if (newZone != areaId && newArea != areaId)
        {
            return false;
        }
    }

    if (!player)
    {
        return false;
    }

    if (conditionId)
    {
        if (!sObjectMgr.IsPlayerMeetToCondition(conditionId, player, player->GetMap(), nullptr, CONDITION_FROM_SPELL_AREA))
        {
            return false;
        }
    }
    else
    {
        if (gender != GENDER_NONE)
        {

            if (gender != player->getGender())
            {
                return false;
            }
        }

        if (raceMask)
        {

            if (!(raceMask & player->getRaceMask()))
            {
                return false;
            }
        }

        if (questStart)
        {

            if ((!questStartCanActive || !player->IsActiveQuest(questStart)) && !player->GetQuestRewardStatus(questStart))
            {
                return false;
            }
        }

        if (questEnd)
        {

            if (player->GetQuestRewardStatus(questEnd))
            {
                return false;
            }
        }
    }

    if (auraSpell)
    {

        if (auraSpell > 0)

        {
            return player->HasAura(auraSpell);
        }
        else

        {
            return !player->HasAura(-auraSpell);
        }
    }

    return true;
}

void SpellArea::ApplyOrRemoveSpellIfCan(Player* player, uint32 newZone, uint32 newArea, bool onlyApply) const
{
    MANGOS_ASSERT(player);

    if (IsFitToRequirements(player, newZone, newArea))
    {
        if (autocast && !player->HasAura(spellId))
        {
            player->CastSpell(player, spellId, true);
        }
    }
    else if (!onlyApply && player->HasAura(spellId))
    {
        player->RemoveAuras(spellId);
    }
}

void SpellMgr::LoadSpellAffects()
{
    mSpellAffectMap.clear();

    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `effectId`, `SpellFamilyMask` FROM `spell_affect`");
    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u spell affect definitions", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();
        uint8 effectId = fields[1].GetUInt8();

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(entry);

        if (!spellInfo)
        {
            sLog.outErrorDb("Spell %u listed in `spell_affect` does not exist", entry);
            continue;
        }

        if (effectId >= MAX_EFFECT_INDEX)
        {
            sLog.outErrorDb("Spell %u listed in `spell_affect` have invalid effect index (%u)", entry, effectId);
            continue;
        }

        if (spellInfo->Effect[effectId] != SPELL_EFFECT_APPLY_AURA ||
            (spellInfo->EffectAura[effectId] != SPELL_AURA_ADD_FLAT_MODIFIER &&
            spellInfo->EffectAura[effectId] != SPELL_AURA_ADD_PCT_MODIFIER  &&
            spellInfo->EffectAura[effectId] != SPELL_AURA_ADD_TARGET_TRIGGER &&
            spellInfo->EffectAura[effectId] != SPELL_AURA_OVERRIDE_CLASS_SCRIPTS))
        {
            sLog.outErrorDb("Spell %u listed in `spell_affect` have not SPELL_AURA_ADD_FLAT_MODIFIER (%u) or SPELL_AURA_ADD_PCT_MODIFIER (%u) or SPELL_AURA_ADD_TARGET_TRIGGER (%u) or SPELL_AURA_OVERRIDE_CLASS_SCRIPTS (%u) for effect index (%u)", entry, SPELL_AURA_ADD_FLAT_MODIFIER, SPELL_AURA_ADD_PCT_MODIFIER, SPELL_AURA_ADD_TARGET_TRIGGER, SPELL_AURA_OVERRIDE_CLASS_SCRIPTS, effectId);
            continue;
        }

        uint64 spellAffectMask = fields[2].GetUInt64();

        if (spellInfo->EffectItemType[effectId])
        {
            if (static_cast<uint64>(spellInfo->EffectItemType[effectId]) == spellAffectMask)
            {
                sLog.outErrorDb("Spell %u listed in `spell_affect` have redundant (same with EffectItemType%d) data for effect index (%u) and not needed, skipped.", entry, effectId + 1, effectId);
                continue;
            }
        }

        mSpellAffectMap.insert(SpellAffectMap::value_type((entry << 8) + effectId, spellAffectMask));

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %u spell affect definitions", count);

    for (uint32 id = 0; id < sSpellStore.GetNumRows(); ++id)
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(id);
        if (!spellInfo)
        {
            continue;
        }

        for (int effectId = 0; effectId < MAX_EFFECT_INDEX; ++effectId)
        {
            if (spellInfo->Effect[effectId] != SPELL_EFFECT_APPLY_AURA ||
                (spellInfo->EffectAura[effectId] != SPELL_AURA_ADD_FLAT_MODIFIER &&
                spellInfo->EffectAura[effectId] != SPELL_AURA_ADD_PCT_MODIFIER  &&
                spellInfo->EffectAura[effectId] != SPELL_AURA_ADD_TARGET_TRIGGER))
            {
                continue;
            }

            if (spellInfo->EffectItemType[effectId] != 0)
            {
                continue;
            }

            if (mSpellAffectMap.find((id << 8) + effectId) !=  mSpellAffectMap.end())
            {
                continue;
            }

            sLog.outErrorDb("Spell %u (%s) misses spell_affect for effect %u", id, spellInfo->Name_lang[sWorld.GetDefaultDbcLocale()], effectId);
        }
    }
}

void SpellMgr::LoadFacingCasterFlags()
{
    mSpellFacingFlagMap.clear();
    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `facingcasterflag` FROM `spell_facing`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString();
        sLog.outString(">> Loaded %u facing caster flags", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 entry              = fields[0].GetUInt32();
        uint32 FacingCasterFlags  = fields[1].GetUInt32();

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(entry);
        if (!spellInfo)
        {
            sLog.outErrorDb("Spell %u listed in `spell_facing` does not exist", entry);
            continue;
        }
        mSpellFacingFlagMap[entry]    = FacingCasterFlags;

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %u facing caster flags", count);
}
