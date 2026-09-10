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

#include "Platform/Define.h"
#include "Utilities/MathDefines.h"
#include <ctime>
#include "Log.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Totem.h"
#include "Creature.h"
#include "ScriptMgr.h"
#include "Util.h"
#include "Cast/Recipe/RecipeBook.h"

pAuraProcHandler AuraProcHandler[TOTAL_AURAS] =
{
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleDummyAuraProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleRemoveByDamageChanceProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleInvisibilityAuraProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleModResistanceAuraProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleRemoveByDamageChanceProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleProcTriggerSpellAuraProc,
    &Unit::HandleProcTriggerDamageAuraProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleRemoveByDamageChanceProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleModCastingSpeedNotStackAuraProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleModPowerCostSchoolAuraProc,
    &Unit::HandleModPowerCostSchoolAuraProc,
    &Unit::HandleReflectSpellsSchoolAuraProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleMechanicImmuneResistanceAuraProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleCantTrigger,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleOverrideClassScriptAuraProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleMechanicImmuneResistanceAuraProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleHasteAuraProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleCantTrigger,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
    &Unit::HandleNULLProc,
};

bool Unit::IsTriggeredAtSpellProcEvent(Unit* pVictim, SpellAuraHolder* holder, SpellEntry const* procSpell, uint32 procFlag, uint32 procExtra, WeaponAttackType attType, bool isVictim, SpellProcEventEntry const*& spellProcEvent)
{
    SpellEntry const* spellProto = holder->GetSpellProto();

    spellProcEvent = cast::RecipeOf(*spellProto).ProcRule();

    uint32 EventProcFlag;
    if (spellProcEvent && spellProcEvent->procFlags)
    {
        EventProcFlag = spellProcEvent->procFlags;
    }
    else
    {
        EventProcFlag = spellProto->ProcFlags;
    }

    if (!EventProcFlag)
    {
        return false;
    }

    if (!SpellMgr::IsSpellProcEventCanTriggeredBy(spellProcEvent, EventProcFlag, procSpell, procFlag, procExtra))
    {
        return false;
    }

    if (EventProcFlag & PROC_FLAG_KILL && IsPlayer(this))
    {
        bool allow = ((Player*)this)->isHonorOrXPTarget(pVictim);
        if (!allow)
        {
            return false;
        }
    }

    if (procSpell && procSpell->ID == spellProto->ID && !(EventProcFlag & PROC_FLAG_ON_TAKE_PERIODIC))
    {
        return false;
    }

    if (!isVictim && IsPlayer(this))
    {
        if (spellProto->EquippedItemClass == ITEM_CLASS_WEAPON)
        {
            Item* item = nullptr;
            if (attType == BASE_ATTACK)
            {
                item = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            }
            else if (attType == OFF_ATTACK)
            {
                item = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
            }
            else
            {
                item = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
            }

            if (!CanUseEquippedWeapon(attType))
            {
                return false;
            }

            if (!item || item->IsBroken() || item->GetProto()->Class != ITEM_CLASS_WEAPON || !((1 << item->GetProto()->SubClass) & spellProto->EquippedItemSubclass))
            {
                return false;
            }
        }
        else if (spellProto->EquippedItemClass == ITEM_CLASS_ARMOR)
        {

            Item* item = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
            if (!item || item->IsBroken() || item->GetProto()->Class != ITEM_CLASS_ARMOR || !((1 << item->GetProto()->SubClass) & spellProto->EquippedItemSubclass))
            {
                return false;
            }
        }
    }

    float chance = (float)spellProto->ProcChance;

    if (spellProcEvent && spellProcEvent->customChance)
    {
        chance = spellProcEvent->customChance;
    }

    if (!isVictim && spellProcEvent && spellProcEvent->ppmRate != 0)
    {
        uint32 WeaponSpeed = GetAttackTime(attType);
        chance = GetPPMProcChance(WeaponSpeed, spellProcEvent->ppmRate);
    }

    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(spellProto->ID, SPELLMOD_CHANCE_OF_SUCCESS, chance);
    }

    if (!roll_chance_f(chance))
    {

        if ((procSpell && procSpell->SpellIconID == 249 && procSpell->SpellVisualID == 257) && (spellProto->SpellClassSet == SPELLFAMILY_ROGUE && spellProto->SpellIconID == 249 && spellProto->SpellVisualID == 0))
        {
            RemoveAurasOfType(SPELL_AURA_MOD_STEALTH);
        }
        return false;
    }
    return true;
}

SpellAuraProcResult Unit::HandleHasteAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* , uint32 , uint32 , uint32 cooldown)
{
    SpellEntry const* hasteSpell = triggeredByAura->GetSpellProto();

    Item* castItem = triggeredByAura->GetCastItemGuid() && IsPlayer(this)
        ? ((Player*)this)->GetItemByGuid(triggeredByAura->GetCastItemGuid()) : nullptr;

    uint32 triggered_spell_id = 0;
    Unit* target = pVictim;
    int32 basepoints0 = 0;

    switch (hasteSpell->SpellClassSet)
    {
        case SPELLFAMILY_ROGUE:
        {
            switch (hasteSpell->ID)
            {

                case 13877:
                {
                    target = SelectRandomUnfriendlyTarget(pVictim);
                    if (!target)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }
                    basepoints0 = damage;
                    triggered_spell_id = 22482;
                    break;
                }
            }
            break;
        }
    }

    if (!triggered_spell_id)
    {
        return SPELL_AURA_PROC_OK;
    }

    SpellEntry const* triggerEntry = sSpellStore.LookupEntry(triggered_spell_id);

    if (!triggerEntry)
    {
        sLog.outError("Unit::HandleHasteAuraProc: Spell %u have nonexistent triggered spell %u", hasteSpell->ID, triggered_spell_id);
        return SPELL_AURA_PROC_FAILED;
    }

    if (!target || (target != this && !target->IsAlive()))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (cooldown && IsPlayer(this) && ((Player*)this)->HasSpellCooldown(triggered_spell_id))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (basepoints0)
    {
        CastCustomSpell(target, triggered_spell_id, &basepoints0, nullptr, nullptr, true, castItem, triggeredByAura);
    }
    else
    {
        CastSpell(target, triggered_spell_id, true, castItem, triggeredByAura);
    }

    if (cooldown && IsPlayer(this))
    {
        ((Player*)this)->AddSpellCooldown(triggered_spell_id, 0, time(nullptr) + cooldown);
    }

    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleDummyAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown)
{
    SpellEntry const* dummySpell = triggeredByAura->GetSpellProto();
    SpellEffectIndex effIndex = triggeredByAura->GetEffIndex();
    int32  triggerAmount = triggeredByAura->GetModifier()->m_amount;

    Item* castItem = triggeredByAura->GetCastItemGuid() && IsPlayer(this)
        ? ((Player*)this)->GetItemByGuid(triggeredByAura->GetCastItemGuid()) : nullptr;

    uint32 triggered_spell_id = 0;
    Unit* target = pVictim;
    int32  basepoints[MAX_EFFECT_INDEX] = {0, 0, 0};

    switch (dummySpell->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (dummySpell->ID)
            {

                case 9799:
                case 25988:
                {

                    if (!procSpell || procSpell->DefenseType != SPELL_DAMAGE_CLASS_MAGIC)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    basepoints[0] = triggerAmount * int32(damage) / 100;
                    if (basepoints[0] > (int32)GetMaxHealth() / 2)
                    {
                        basepoints[0] = (int32)GetMaxHealth() / 2;
                    }

                    triggered_spell_id = 25997;
                    break;
                }

                case 12292:
                case 18765:
                {

                    if (procSpell && procSpell->ID == 26654)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    target = SelectRandomUnfriendlyTarget(pVictim);
                    if (!target)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    triggered_spell_id = 26654;
                    break;
                }

                case 20230:
                {

                    if (!Where().HasInArc(pVictim->Where(), M_PI_F))
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    triggered_spell_id = 22858;
                    break;
                }

                case 21063:
                    triggered_spell_id = 21064;
                    break;

                case 24658:
                {
                    if (!procSpell || procSpell->ID == 24659)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    RemoveStacks(24659);
                    return SPELL_AURA_PROC_OK;
                }

                case 24661:
                {

                    RemoveStacks(24662);
                    return SPELL_AURA_PROC_OK;
                }

                case 28764:
                {
                    if (!procSpell)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    bool found = false;
                    const auto mRegenInterrupt = GetAurasByType(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT);
                    for (auto* aura : mRegenInterrupt)
                    {
                        if (SpellEntry const* iterSpellProto = aura->GetSpellProto())
                        {
                            if (iterSpellProto->SpellClassSet == SPELLFAMILY_MAGE && (iterSpellProto->SpellClassMask & UI64LIT(0x10000000)))
                            {
                                found = true;
                                break;
                            }
                        }
                    }
                    if (!found)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    switch (GetFirstSchoolInMask(GetSpellSchoolMask(procSpell)))
                    {
                        case SPELL_SCHOOL_NORMAL:
                        case SPELL_SCHOOL_HOLY:
                            return SPELL_AURA_PROC_FAILED;
                        case SPELL_SCHOOL_FIRE:   triggered_spell_id = 28765; break;
                        case SPELL_SCHOOL_NATURE: triggered_spell_id = 28768; break;
                        case SPELL_SCHOOL_FROST:  triggered_spell_id = 28766; break;
                        case SPELL_SCHOOL_SHADOW: triggered_spell_id = 28769; break;
                        case SPELL_SCHOOL_ARCANE: triggered_spell_id = 28770; break;
                        default:
                            return SPELL_AURA_PROC_FAILED;
                    }

                    target = this;
                    break;
                }

                case 27539:
                {
                    if (!procSpell)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    switch (GetFirstSchoolInMask(GetSpellSchoolMask(procSpell)))
                    {
                        case SPELL_SCHOOL_NORMAL:
                            return SPELL_AURA_PROC_FAILED;
                        case SPELL_SCHOOL_HOLY:   triggered_spell_id = 27536; break;
                        case SPELL_SCHOOL_FIRE:   triggered_spell_id = 27533; break;
                        case SPELL_SCHOOL_NATURE: triggered_spell_id = 27538; break;
                        case SPELL_SCHOOL_FROST:  triggered_spell_id = 27534; break;
                        case SPELL_SCHOOL_SHADOW: triggered_spell_id = 27535; break;
                        case SPELL_SCHOOL_ARCANE: triggered_spell_id = 27540; break;
                        default:
                            return SPELL_AURA_PROC_FAILED;
                    }

                    target = this;
                    break;
                }
            }
            break;
        }
        case SPELLFAMILY_MAGE:
        {

            if (dummySpell->SpellIconID == 459)
            {
                if (GetPowerType() != POWER_MANA)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                basepoints[0] = (triggerAmount * GetMaxPower(POWER_MANA) / 100);
                target = this;
                triggered_spell_id = 29442;
                break;
            }

            if (dummySpell->SpellIconID == 1920)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                int32 cost = procSpell->ManaCost + procSpell->ManaCostPct * GetCreateMana() / 100;
                basepoints[0] = cost * triggerAmount / 100;
                if (basepoints[0] <= 0)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                target = this;
                triggered_spell_id = 29077;
                break;
            }

            switch (dummySpell->ID)
            {

                case 11119:
                case 11120:
                case 12846:
                case 12847:
                case 12848:
                {
                    switch (dummySpell->ID)
                    {
                        case 11119: basepoints[0] = int32(0.04f * damage); break;
                        case 11120: basepoints[0] = int32(0.08f * damage); break;
                        case 12846: basepoints[0] = int32(0.12f * damage); break;
                        case 12847: basepoints[0] = int32(0.16f * damage); break;
                        case 12848: basepoints[0] = int32(0.20f * damage); break;
                        default:
                            sLog.outError("Unit::HandleDummyAuraProc: non handled spell id: %u (IG)", dummySpell->ID);
                            return SPELL_AURA_PROC_FAILED;
                    }

                    triggered_spell_id = 12654;
                    break;
                }

                case 11129:
                {

                    if (triggeredByAura->GetHolder()->GetAuraCharges() <= 1 && (procEx & PROC_EX_CRITICAL_HIT))
                    {
                        RemoveAuras(28682);
                        return SPELL_AURA_PROC_OK;
                    }

                    CastSpell(this, 28682, true, castItem, triggeredByAura);
                    return (procEx & PROC_EX_CRITICAL_HIT) ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;
                }
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {

            if (dummySpell->IsFitToFamilyMask(UI64LIT(0x0000000800000000)))
            {

                if (!Where().HasInArc(pVictim->Where(), M_PI_F))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                triggered_spell_id = 22858;
                break;
            }
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            switch (dummySpell->ID)
            {

                case 15286:
                {
                    if (!pVictim || !pVictim->IsAlive())
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    if (triggeredByAura->GetCasterGuid() != pVictim->GetObjectGuid())
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    basepoints[0] = triggerAmount * damage / 100;
                    pVictim->CastCustomSpell(pVictim, 15290, &basepoints[0], nullptr, nullptr, true, castItem, triggeredByAura);
                    return SPELL_AURA_PROC_OK;
                }

                case 26169:
                {

                    basepoints[0] = int32(damage * 10 / 100);
                    target = this;
                    triggered_spell_id = 26170;
                    break;
                }

                case 28809:
                {
                    triggered_spell_id = 28810;
                    break;
                }
            }
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            switch (dummySpell->ID)
            {

                case 28719:
                {

                    basepoints[0] = int32(procSpell->ManaCost * 30 / 100);
                    target = this;
                    triggered_spell_id = 28742;
                    break;
                }

                case 28847:
                {
                    target = this;
                    triggered_spell_id = 28848;
                    break;
                }
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            switch (dummySpell->ID)
            {

                case 23582:

                    if (!procSpell || procSpell->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_NONE)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }
                    triggered_spell_id = 23583;
                    break;
            }
            break;
        }
        case SPELLFAMILY_HUNTER:
            break;
        case SPELLFAMILY_PALADIN:
        {

            if ((dummySpell->SpellClassMask & UI64LIT(0x000000008000000)) && triggeredByAura->GetEffIndex() == EFFECT_INDEX_0)
            {
                if (!IsPlayer(this))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                uint32 spellId;
                switch (triggeredByAura->GetId())
                {
                    case 20154:
                    case 21084: spellId = 25742; break;
                    case 20287: spellId = 25740; break;
                    case 20288: spellId = 25739; break;
                    case 20289: spellId = 25738; break;
                    case 20290: spellId = 25737; break;
                    case 20291: spellId = 25736; break;
                    case 20292: spellId = 25735; break;
                    case 20293: spellId = 25713; break;
                    default:
                        sLog.outError("Unit::HandleDummyAuraProc: non handled possibly SoR (Id = %u)", triggeredByAura->GetId());
                        return SPELL_AURA_PROC_FAILED;
                }
                Item* item = ((Player*)this)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
                float speed = (item ? item->GetProto()->Delay : BASE_ATTACK_TIME) / 1000.0f;

                int damagePoint;

                uint32 ModSpellId[] = { 20224, 20225, 20330, 20331, 20332 };
                float ModPct = 1.0f;
                const auto mModDamagePercentModifier = GetAurasByType(SPELL_AURA_ADD_PCT_MODIFIER);
                for (auto* aura : mModDamagePercentModifier)
                {
                    for (int j = 0; j < 5; j++)
                    {
                        if (aura->GetId() == ModSpellId[j])
                        {
                            ModPct *= (aura->GetModifier()->m_amount + 100.0f) / 100.0f;
                        }
                    }
                }
                triggerAmount = triggerAmount * ModPct;

                int min=triggerAmount/87;
                int max=triggerAmount/25;

                damagePoint = (speed<=1.5?min:(speed>=4.0?max:min+(((max-min)/2.5f)*(speed-1.5))));

                CastCustomSpell(pVictim, spellId, &damagePoint, nullptr, nullptr, true, nullptr, triggeredByAura);
                return SPELL_AURA_PROC_OK;
            }

            switch (dummySpell->ID)
            {

                case 28789:
                {
                    if (!pVictim)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    switch (pVictim->getClass())
                    {
                        case CLASS_PALADIN:
                        case CLASS_PRIEST:
                        case CLASS_SHAMAN:
                        case CLASS_DRUID:
                            triggered_spell_id = 28795;
                            break;
                        case CLASS_MAGE:
                        case CLASS_WARLOCK:
                            triggered_spell_id = 28793;
                            break;
                        case CLASS_HUNTER:
                        case CLASS_ROGUE:
                            triggered_spell_id = 28791;
                            break;
                        case CLASS_WARRIOR:
                            triggered_spell_id = 28790;
                            break;
                        default:
                            return SPELL_AURA_PROC_FAILED;
                    }
                    break;
                }
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {
            switch (dummySpell->ID)
            {

                case 28823:
                {
                    if (!pVictim)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    switch (pVictim->getClass())
                    {
                        case CLASS_PALADIN:
                        case CLASS_PRIEST:
                        case CLASS_SHAMAN:
                        case CLASS_DRUID:
                            triggered_spell_id = 28824;
                            break;
                        case CLASS_MAGE:
                        case CLASS_WARLOCK:
                            triggered_spell_id = 28825;
                            break;
                        case CLASS_HUNTER:
                        case CLASS_ROGUE:
                            triggered_spell_id = 28826;
                            break;
                        case CLASS_WARRIOR:
                            triggered_spell_id = 28827;
                            break;
                        default:
                            return SPELL_AURA_PROC_FAILED;
                    }
                    break;
                }

                case 28849:
                {
                    target = this;
                    triggered_spell_id = 28850;
                    break;
                }
            }
            break;
        }
        default:
            break;
    }

    if (!triggered_spell_id)
    {

        SpellLinkedSet linkedSet = sSpellMgr.GetSpellLinked(dummySpell->ID, SPELL_LINKED_TYPE_PROC);
        if (linkedSet.size() > 0)
        {
            for (SpellLinkedSet::const_iterator itr = linkedSet.begin(); itr != linkedSet.end(); ++itr)
            {
                if (target == nullptr)
                {
                    target = !(procFlag & PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL) && cast::Recipes().IsPositive(*itr) ? this : pVictim;
                }
                CastSpell(this, *itr, true, castItem, triggeredByAura);
                if (cooldown && IsPlayer(this))
                {
                    ((Player*)this)->AddSpellCooldown(*itr, 0, time(nullptr) + cooldown);
                }
            }
        }
    }

    if (!triggered_spell_id)
    {
        return SPELL_AURA_PROC_OK;
    }

    SpellEntry const* triggerEntry = sSpellStore.LookupEntry(triggered_spell_id);

    if (!triggerEntry)
    {
        sLog.outError("Unit::HandleDummyAuraProc: Spell %u have nonexistent triggered spell %u", dummySpell->ID, triggered_spell_id);
        return SPELL_AURA_PROC_FAILED;
    }

    if (!target || (target != this && !target->IsAlive()))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (cooldown && IsPlayer(this) && ((Player*)this)->HasSpellCooldown(triggered_spell_id))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (basepoints[EFFECT_INDEX_0] || basepoints[EFFECT_INDEX_1] || basepoints[EFFECT_INDEX_2])
    {
        CastCustomSpell(target, triggered_spell_id,
            basepoints[EFFECT_INDEX_0] ? &basepoints[EFFECT_INDEX_0] : nullptr,
            basepoints[EFFECT_INDEX_1] ? &basepoints[EFFECT_INDEX_1] : nullptr,
            basepoints[EFFECT_INDEX_2] ? &basepoints[EFFECT_INDEX_2] : nullptr,
            true, castItem, triggeredByAura);
    }
    else
    {
        CastSpell(target, triggered_spell_id, true, castItem, triggeredByAura);
    }

    if (cooldown && IsPlayer(this))
    {
        ((Player*)this)->AddSpellCooldown(triggered_spell_id, 0, time(nullptr) + cooldown);
    }

    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleProcTriggerSpellAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const* procSpell, uint32 procFlags, uint32 procEx, uint32 cooldown)
{

    SpellEntry const* auraSpellInfo = triggeredByAura->GetSpellProto();

    int32 triggerAmount = triggeredByAura->GetModifier()->m_amount;

    uint32 trigger_spell_id = auraSpellInfo->EffectTriggerSpell[triggeredByAura->GetEffIndex()];
    Unit*  target = nullptr;
    int32  basepoints[MAX_EFFECT_INDEX] = {0, 0, 0};

    Item* castItem = triggeredByAura->GetCastItemGuid() && IsPlayer(this)
        ? ((Player*)this)->GetItemByGuid(triggeredByAura->GetCastItemGuid()) : nullptr;

    switch (auraSpellInfo->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
            switch (auraSpellInfo->ID)
            {

                case 23780:
                    trigger_spell_id = 23781;
                    break;

                case 27522:
                {

                    if (IsAlive())
                    {
                        CastSpell(this, 29471, true, castItem, triggeredByAura);
                    }
                    if (pVictim && pVictim->IsAlive())
                    {
                        CastSpell(pVictim, 27526, true, castItem, triggeredByAura);
                    }
                    return SPELL_AURA_PROC_OK;
                }
                case 31255:
                {

                    if (pVictim->GetHealth() > pVictim->GetMaxHealth() / 5)
                    {
                        return SPELL_AURA_PROC_FAILED;
                    }

                    target = this;
                    trigger_spell_id = 22588;
                    break;
                }
                break;
            }
            break;
        case SPELLFAMILY_MAGE:
            if (auraSpellInfo->ID == 26467)
            {

                basepoints[0] = damage * 15 / 100;
                target = pVictim;
                trigger_spell_id = 26470;
            }
            break;
        case SPELLFAMILY_WARRIOR:

            if (!auraSpellInfo->SpellClassMask && auraSpellInfo->SpellIconID == 243)
            {
                float weaponDamage;

                if (haveOffhandWeapon() && getAttackTimer(BASE_ATTACK) > getAttackTimer(OFF_ATTACK))
                {
                    weaponDamage = (GetShownDamage(true, false) + GetShownDamage(true, true)) / 2;
                }
                else
                {
                    weaponDamage = (GetShownDamage(false, false) + GetShownDamage(false, true)) / 2;
                }

                switch (auraSpellInfo->ID)
                {
                    case 12834: basepoints[0] = int32(weaponDamage * 0.2f); break;
                    case 12849: basepoints[0] = int32(weaponDamage * 0.4f); break;
                    case 12867: basepoints[0] = int32(weaponDamage * 0.6f); break;

                    default:
                        sLog.outError("Unit::HandleProcTriggerSpellAuraProc: DW unknown spell rank %u", auraSpellInfo->ID);
                        return SPELL_AURA_PROC_FAILED;
                }

                basepoints[0] /= 6;

                trigger_spell_id = 12721;
                break;
            }
            break;
        case SPELLFAMILY_WARLOCK:
        {

            if (auraSpellInfo->SpellIconID == 1137)
            {
                if (!pVictim || !pVictim->IsAlive() || pVictim == this || procSpell == nullptr)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                uint32 tick = 1;

                if (procSpell->SpellClassMask & UI64LIT(0x0000000000000040))
                {
                    tick = 15;
                }

                else if (procSpell->SpellClassMask & UI64LIT(0x0000000000000020))
                {
                    tick = 4;
                }
                else
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                float chance = 0;
                switch (auraSpellInfo->ID)
                {
                    case 18096: chance = 13.0f / tick; break;
                    case 18073: chance = 26.0f / tick; break;
                }

                if (!roll_chance_f(chance))
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                trigger_spell_id = 18093;
            }

            else if (auraSpellInfo->ID == 28845)
            {

                int32 health20 = int32(GetMaxHealth()) / 5;
                if (int32(GetHealth()) - int32(damage) >= health20 || int32(GetHealth()) < health20)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
            }
            break;
        }
        case SPELLFAMILY_PRIEST:
        {

            if (auraSpellInfo->SpellIconID == 19)
            {
                switch (auraSpellInfo->ID)
                {
                    case 18137: trigger_spell_id = 28377; break;
                    case 19308: trigger_spell_id = 28378; break;
                    case 19309: trigger_spell_id = 28379; break;
                    case 19310: trigger_spell_id = 28380; break;
                    case 19311: trigger_spell_id = 28381; break;
                    case 19312: trigger_spell_id = 28382; break;
                    default:
                        sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u not handled in SG", auraSpellInfo->ID);
                        return SPELL_AURA_PROC_FAILED;
                }
            }

            else if (auraSpellInfo->SpellIconID == 1875)
            {
                switch (auraSpellInfo->ID)
                {
                    case 27811: trigger_spell_id = 27813; break;
                    case 27815: trigger_spell_id = 27817; break;
                    case 27816: trigger_spell_id = 27818; break;
                    default:
                        sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u not handled in BR", auraSpellInfo->ID);
                        return SPELL_AURA_PROC_FAILED;
                }
                basepoints[0] = damage * triggerAmount / 100 / 3;
                target = this;
            }
            break;
        }
        case SPELLFAMILY_DRUID:
            break;
        case SPELLFAMILY_HUNTER:
            break;
        case SPELLFAMILY_PALADIN:
        {

            if (auraSpellInfo->SpellClassMask & UI64LIT(0x0000000000080000))
            {
                switch (auraSpellInfo->ID)
                {

                    case 20185: trigger_spell_id = 20267; break;
                    case 20344: trigger_spell_id = 20341; break;
                    case 20345: trigger_spell_id = 20342; break;
                    case 20346: trigger_spell_id = 20343; break;

                    case 20186: trigger_spell_id = 20268; break;
                    case 20354: trigger_spell_id = 20352; break;
                    case 20355: trigger_spell_id = 20353; break;
                    default:
                        sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u miss posibly Judgement of Light/Wisdom", auraSpellInfo->ID);
                        return SPELL_AURA_PROC_FAILED;
                }
                pVictim->CastSpell(pVictim, trigger_spell_id, true, castItem, triggeredByAura);
                return SPELL_AURA_PROC_OK;
            }

            else if (auraSpellInfo->SpellIconID == 241)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }

                uint32 originalSpellId = procSpell->ID;

                if (procSpell->SpellClassMask & UI64LIT(0x0000000000200000))
                {
                    switch (procSpell->ID)
                    {
                        case 25914: originalSpellId = 20473; break;
                        case 25913: originalSpellId = 20929; break;
                        case 25903: originalSpellId = 20930; break;
                        default:
                            sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u not handled in HShock", procSpell->ID);
                            return SPELL_AURA_PROC_FAILED;
                    }
                }
                SpellEntry const* originalSpell = sSpellStore.LookupEntry(originalSpellId);
                if (!originalSpell)
                {
                    sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u unknown but selected as original in Illu", originalSpellId);
                    return SPELL_AURA_PROC_FAILED;
                }
                basepoints[0] = originalSpell->ManaCost;
                trigger_spell_id = 20272;
                target = this;
            }
            break;
        }
        case SPELLFAMILY_SHAMAN:
        {

            if (auraSpellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000000400)) && auraSpellInfo->SpellVisualID == 37)
            {
                switch (auraSpellInfo->ID)
                {
                    case 324:
                        trigger_spell_id = 26364; break;
                    case 325:
                        trigger_spell_id = 26365; break;
                    case 905:
                        trigger_spell_id = 26366; break;
                    case 945:
                        trigger_spell_id = 26367; break;
                    case 8134:
                        trigger_spell_id = 26369; break;
                    case 10431:
                        trigger_spell_id = 26370; break;
                    case 10432:
                        trigger_spell_id = 26363; break;
                    default:
                        sLog.outError("Unit::HandleProcTriggerSpellAuraProc: Spell %u not handled in LShield", auraSpellInfo->ID);
                        return SPELL_AURA_PROC_FAILED;
                }
            }

            else if (auraSpellInfo->ID == 23551)
            {
                trigger_spell_id = 23552;
                target = pVictim;
            }

            else if (auraSpellInfo->ID == 23552)
            {
                trigger_spell_id = 27635;
            }

            else if (auraSpellInfo->ID == 23572)
            {
                if (!procSpell)
                {
                    return SPELL_AURA_PROC_FAILED;
                }
                basepoints[0] = procSpell->ManaCost * 35 / 100;
                trigger_spell_id = 23571;
                target = this;
            }
            break;
        }
        default:
            break;
    }

    SpellEntry const* triggerEntry = sSpellStore.LookupEntry(trigger_spell_id);
    if (!triggerEntry)
    {

        return SPELL_AURA_PROC_FAILED;
    }

    if (m_extraAttacks && triggerEntry->HasSpellEffect(SPELL_EFFECT_ADD_EXTRA_ATTACKS) && triggerEntry->ID != 20178)
    {
        return SPELL_AURA_PROC_FAILED;
    }

    switch (trigger_spell_id)
    {

        case 7099:
        case 29494:
        case 20233:
        {
            target = pVictim;
            break;
        }

        case 15250:
        {
            if (!pVictim || pVictim != getVictim())
            {
                return SPELL_AURA_PROC_FAILED;
            }
            break;
        }

        case 14189:
        case 14157:
        {

            if (Spell* spell = GetCurrentSpell(CURRENT_GENERIC_SPELL))
            {
                spell->AddTriggeredSpell(trigger_spell_id);
                return SPELL_AURA_PROC_OK;
            }
            return SPELL_AURA_PROC_FAILED;
        }
    }

    if (!trigger_spell_id)
    {

        SpellLinkedSet linkedSet = sSpellMgr.GetSpellLinked(auraSpellInfo->ID, SPELL_LINKED_TYPE_PROC);
        if (linkedSet.size() > 0)
        {
            for (SpellLinkedSet::const_iterator itr = linkedSet.begin(); itr != linkedSet.end(); ++itr)
            {
                if (target == nullptr)
                {
                    target = !(procFlags & PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL) && cast::Recipes().IsPositive(*itr) ? this : pVictim;
                }
                CastSpell(target, *itr, true, castItem, triggeredByAura);
                if (cooldown && IsPlayer(this))
                {
                    ((Player*)this)->AddSpellCooldown(*itr, 0, time(nullptr) + cooldown);
                }
            }
        }
    }

    if (cooldown && IsPlayer(this) && ((Player*)this)->HasSpellCooldown(trigger_spell_id))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (target == nullptr)
    {
        target = !(procFlags & PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL) && cast::Recipes().IsPositive(trigger_spell_id) ? this : pVictim;
    }

    if (!target || (target != this && !target->IsAlive()))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    if (basepoints[EFFECT_INDEX_0] || basepoints[EFFECT_INDEX_1] || basepoints[EFFECT_INDEX_2])
    {
        CastCustomSpell(target, trigger_spell_id,
            basepoints[EFFECT_INDEX_0] ? &basepoints[EFFECT_INDEX_0] : nullptr,
            basepoints[EFFECT_INDEX_1] ? &basepoints[EFFECT_INDEX_1] : nullptr,
            basepoints[EFFECT_INDEX_2] ? &basepoints[EFFECT_INDEX_2] : nullptr,
            true, castItem, triggeredByAura);
    }
    else
    {
        CastSpell(target, trigger_spell_id, true, castItem, triggeredByAura);
    }

    if (cooldown && IsPlayer(this))
    {
        ((Player*)this)->AddSpellCooldown(trigger_spell_id, 0, time(nullptr) + cooldown);
    }

    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleProcTriggerDamageAuraProc(Unit* pVictim, uint32 , Aura* triggeredByAura, SpellEntry const* , uint32 , uint32 , uint32 )
{
    SpellEntry const* spellInfo = triggeredByAura->GetSpellProto();
    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "ProcDamageAndSpell: doing %u damage from spell id %u (triggered by auratype %u of spell %u)",
        triggeredByAura->GetModifier()->m_amount, spellInfo->ID, triggeredByAura->GetModifier()->m_auraname, triggeredByAura->GetId());
    SpellNonMeleeDamage damageInfo(this, pVictim, spellInfo->ID, SpellSchools(spellInfo->School));
    CalculateSpellDamage(&damageInfo, triggeredByAura->GetModifier()->m_amount, spellInfo);
    damageInfo.target->CalculateAbsorbResistBlock(this, &damageInfo, spellInfo);
    DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);
    SendSpellNonMeleeDamageLog(&damageInfo);
    DealSpellDamage(&damageInfo, true);
    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleOverrideClassScriptAuraProc(Unit* pVictim, uint32 , Aura* triggeredByAura, SpellEntry const* procSpell, uint32 , uint32  , uint32 cooldown)
{
    int32 scriptId = triggeredByAura->GetModifier()->m_miscvalue;

    if (!pVictim || !pVictim->IsAlive())
    {
        return SPELL_AURA_PROC_FAILED;
    }

    Item* castItem = triggeredByAura->GetCastItemGuid() && IsPlayer(this)
        ? ((Player*)this)->GetItemByGuid(triggeredByAura->GetCastItemGuid()) : nullptr;

    int32 triggerAmount = triggeredByAura->GetModifier()->m_amount;

    uint32 triggered_spell_id = 0;

    switch (scriptId)
    {
        case 836:
        {
            if (!procSpell || procSpell->SpellVisualID != 259)
            {
                return SPELL_AURA_PROC_FAILED;
            }
            triggered_spell_id = 12484;
            break;
        }
        case 988:
        {
            if (!procSpell || procSpell->SpellVisualID != 259)
            {
                return SPELL_AURA_PROC_FAILED;
            }
            triggered_spell_id = 12485;
            break;
        }
        case 989:
        {
            if (!procSpell || procSpell->SpellVisualID != 259)
            {
                return SPELL_AURA_PROC_FAILED;
            }
            triggered_spell_id = 12486;
            break;
        }
        case 3656:
        {
            triggered_spell_id = 23402;
            break;
        }
        case 4086:
        case 4087:
        {
            if (!roll_chance_i(triggerAmount))
            {
                return SPELL_AURA_PROC_FAILED;
            }

            triggered_spell_id = 24406;
            break;
        }
        case 4309:
        {
            triggered_spell_id = 17941;
            break;
        }
        case 4533:
        {

            if (!roll_chance_i(50))
            {
                return SPELL_AURA_PROC_FAILED;
            }

            switch (pVictim->GetPowerType())
            {
                case POWER_MANA:   triggered_spell_id = 28722; break;
                case POWER_RAGE:   triggered_spell_id = 28723; break;
                case POWER_ENERGY: triggered_spell_id = 28724; break;
                default:
                    return SPELL_AURA_PROC_FAILED;
            }
            break;
        }
        case 4537:
            triggered_spell_id = 28750;
            break;
    }

    if (!triggered_spell_id)
    {
        return SPELL_AURA_PROC_OK;
    }

    SpellEntry const* triggerEntry = sSpellStore.LookupEntry(triggered_spell_id);

    if (!triggerEntry)
    {
        sLog.outError("Unit::HandleOverrideClassScriptAuraProc: Spell %u triggering for class script id %u", triggered_spell_id, scriptId);
        return SPELL_AURA_PROC_FAILED;
    }

    if (cooldown && IsPlayer(this) && ((Player*)this)->HasSpellCooldown(triggered_spell_id))
    {
        return SPELL_AURA_PROC_FAILED;
    }

    CastSpell(pVictim, triggered_spell_id, true, castItem, triggeredByAura);

    if (cooldown && IsPlayer(this))
    {
        ((Player*)this)->AddSpellCooldown(triggered_spell_id, 0, time(nullptr) + cooldown);
    }

    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleModCastingSpeedNotStackAuraProc(Unit* , uint32 , Aura* , SpellEntry const* procSpell, uint32 , uint32 , uint32 )
{

    return !(procSpell == nullptr || GetSpellCastTime(procSpell) == 0) ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;
}

SpellAuraProcResult Unit::HandleReflectSpellsSchoolAuraProc(Unit* , uint32 , Aura* triggeredByAura, SpellEntry const* procSpell, uint32 , uint32 , uint32 )
{

    return !(procSpell == nullptr || (triggeredByAura->GetModifier()->m_miscvalue & GetSchoolMask(procSpell->School)) == 0) ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;
}

SpellAuraProcResult Unit::HandleModPowerCostSchoolAuraProc(Unit* , uint32 , Aura* triggeredByAura, SpellEntry const* procSpell, uint32 , uint32 , uint32 )
{

    return !(procSpell == nullptr ||
        (procSpell->ManaCost == 0 && procSpell->ManaCostPct == 0) ||
        (triggeredByAura->GetModifier()->m_miscvalue & GetSchoolMask(procSpell->School)) == 0) ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;
}

SpellAuraProcResult Unit::HandleMechanicImmuneResistanceAuraProc(Unit* , uint32 , Aura* triggeredByAura, SpellEntry const* procSpell, uint32 , uint32 , uint32 )
{

    return !(procSpell == nullptr || procSpell->Mechanic != triggeredByAura->GetModifier()->m_miscvalue)
        ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED;
}

SpellAuraProcResult Unit::HandleModResistanceAuraProc(Unit* , uint32 damage, Aura* triggeredByAura, SpellEntry const* , uint32 , uint32 , uint32 )
{
    SpellEntry const* spellInfo = triggeredByAura->GetSpellProto();

    if (spellInfo->IsFitToFamily(SPELLFAMILY_PRIEST, UI64LIT(0x0000000000002)))
    {

        if (!damage)
        {
            return SPELL_AURA_PROC_FAILED;
        }
    }

    return SPELL_AURA_PROC_OK;
}

SpellAuraProcResult Unit::HandleRemoveByDamageChanceProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const *procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown)
{

    uint32 max_dmg = getLevel() > 8 ? 25 * getLevel() - 150 : 50;
    float chance = float(damage) / max_dmg * 100.0f;
    if (roll_chance_f(chance))
    {
        triggeredByAura->SetInUse(true);
        RemoveAurasCastBy(triggeredByAura->GetId(), triggeredByAura->GetCasterGuid());
        triggeredByAura->SetInUse(false);
        return SPELL_AURA_PROC_OK;
    }

    return SPELL_AURA_PROC_FAILED;
}

SpellAuraProcResult Unit::HandleInvisibilityAuraProc(Unit* pVictim, uint32 damage, Aura* triggeredByAura, SpellEntry const *procSpell, uint32 procFlag, uint32 procEx, uint32 cooldown)
{
    if (cast::RecipeOf(*triggeredByAura->GetSpellProto()).Says().passive || cast::RecipeOf(*triggeredByAura->GetSpellProto()).Says().cannotBeReflected)
    {
        return SPELL_AURA_PROC_FAILED;
    }

    RemoveAuras(triggeredByAura->GetId());
    return SPELL_AURA_PROC_OK;
}
