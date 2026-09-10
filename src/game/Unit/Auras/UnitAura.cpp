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
#include "PerKind.h"
#include "LootClaim.h"
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

int32 Unit::GetTotalAuraModifier(AuraType auratype) const
{
    int32 modifier = 0;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        modifier += aura->GetModifier()->m_amount;
    }

    return modifier;
}

float Unit::GetTotalAuraMultiplier(AuraType auratype) const
{
    float multiplier = 1.0f;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        multiplier *= (100.0f + aura->GetModifier()->m_amount) / 100.0f;
    }

    return multiplier;
}

int32 Unit::GetMaxPositiveAuraModifier(AuraType auratype) const
{
    int32 modifier = 0;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        if (aura->GetModifier()->m_amount > modifier)
        {
            modifier = aura->GetModifier()->m_amount;
        }
    }
    return modifier;
}

int32 Unit::GetMaxNegativeAuraModifier(AuraType auratype) const
{
    int32 modifier = 0;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        if (aura->GetModifier()->m_amount < modifier)
        {
            modifier = aura->GetModifier()->m_amount;
        }
    }
    return modifier;
}

int32 Unit::GetTotalAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const
{
    if (!misc_mask)
    {
        return 0;
    }

    int32 modifier = 0;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        Modifier* mod = aura->GetModifier();
        if (mod->m_miscvalue & misc_mask)
        {
            modifier += mod->m_amount;
        }
    }
    return modifier;
}

float Unit::GetTotalAuraMultiplierByMiscMask(AuraType auratype, uint32 misc_mask) const
{
    if (!misc_mask)
    {
        return 1.0f;
    }

    float multiplier = 1.0f;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        Modifier* mod = aura->GetModifier();
        if (mod->m_miscvalue & misc_mask)
        {
            multiplier *= (100.0f + mod->m_amount) / 100.0f;
        }
    }
    return multiplier;
}

int32 Unit::GetMaxPositiveAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const
{
    if (!misc_mask)
    {
        return 0;
    }

    int32 modifier = 0;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        Modifier* mod = aura->GetModifier();
        if (mod->m_miscvalue & misc_mask && mod->m_amount > modifier)
        {
            modifier = mod->m_amount;
        }
    }

    return modifier;
}

int32 Unit::GetMaxNegativeAuraModifierByMiscMask(AuraType auratype, uint32 misc_mask) const
{
    if (!misc_mask)
    {
        return 0;
    }

    int32 modifier = 0;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        Modifier* mod = aura->GetModifier();
        if (mod->m_miscvalue & misc_mask && mod->m_amount < modifier)
        {
            modifier = mod->m_amount;
        }
    }

    return modifier;
}

int32 Unit::GetTotalAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const
{
    int32 modifier = 0;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        Modifier* mod = aura->GetModifier();
        if (mod->m_miscvalue == misc_value)
        {
            modifier += mod->m_amount;
        }
    }
    return modifier;
}

float Unit::GetTotalAuraMultiplierByMiscValue(AuraType auratype, int32 misc_value) const
{
    float multiplier = 1.0f;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        Modifier* mod = aura->GetModifier();
        if (mod->m_miscvalue == misc_value)
        {
            multiplier *= (100.0f + mod->m_amount) / 100.0f;
        }
    }
    return multiplier;
}

int32 Unit::GetMaxPositiveAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const
{
    int32 modifier = 0;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        Modifier* mod = aura->GetModifier();
        if (mod->m_miscvalue == misc_value && mod->m_amount > modifier)
        {
            modifier = mod->m_amount;
        }
    }

    return modifier;
}

int32 Unit::GetMaxNegativeAuraModifierByMiscValue(AuraType auratype, int32 misc_value) const
{
    int32 modifier = 0;

    const auto mTotalAuraList = GetAurasByType(auratype);
    for (auto* aura : mTotalAuraList)
    {
        Modifier* mod = aura->GetModifier();
        if (mod->m_miscvalue == misc_value && mod->m_amount < modifier)
        {
            modifier = mod->m_amount;
        }
    }

    return modifier;
}

bool Unit::AddSpellAuraHolder(SpellAuraHolder* holder)
{
    SpellEntry const* aurSpellInfo = holder->GetSpellProto();

    if (!IsAlive() && !IsDeathPersistentSpell(aurSpellInfo) &&
        !IsDeathOnlySpell(aurSpellInfo) &&
        (!IsPlayer(this) || !((Player*)this)->GetSession()->PlayerLoading()))
    {
        delete holder;
        return false;
    }

    if (holder->GetTarget() != this)
    {
        sLog.outError("Holder (spell %u) add to spell aura holder list of %s (lowguid: %u) but spell aura holder target is %s (lowguid: %u)",
            holder->GetId(), (IsPlayer(this) ? "player" : "creature"), GetGUIDLow(), (IsPlayer(holder->GetTarget()) ? "player" : "creature"), holder->GetTarget()->GetGUIDLow());
        delete holder;
        return false;
    }

    if ((!holder->IsPassive() && !holder->IsPersistent()) || holder->IsAreaAura())
    {
        SpellAuraHolderBounds spair = GetSpellAuraHolderBounds(aurSpellInfo->ID);

        for (SpellAuraHolderMap::iterator iter = spair.first; iter != spair.second; ++iter)
        {
            SpellAuraHolder* foundHolder = iter->second;
            if (foundHolder->GetCasterGuid() == holder->GetCasterGuid())
            {

                if (aurSpellInfo->CumulativeAura)
                {

                    foundHolder->ModStackAmount(holder->GetStackAmount());
                    delete holder;
                    return false;
                }

                if (holder->IsWeaponBuffCoexistableWith(foundHolder))
                {
                    continue;
                }

                RemoveHolder(foundHolder, AURA_REMOVE_BY_STACK);
                break;
            }

            bool stop = false;

            for (int32 i = 0; i < MAX_EFFECT_INDEX && !stop; ++i)
            {

                if (!foundHolder->m_auras[i] || !holder->m_auras[i])
                {
                    continue;
                }

                AuraType aurNameReal = AuraType(aurSpellInfo->EffectAura[i]);

                switch (aurNameReal)
                {

                    case SPELL_AURA_DUMMY:
                    case SPELL_AURA_PERIODIC_DAMAGE:
                    case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
                    case SPELL_AURA_PERIODIC_LEECH:
                    case SPELL_AURA_OBS_MOD_HEALTH:
                    case SPELL_AURA_PERIODIC_MANA_LEECH:
                    case SPELL_AURA_OBS_MOD_MANA:
                    case SPELL_AURA_POWER_BURN_MANA:
                        break;
                    case SPELL_AURA_PERIODIC_ENERGIZE:
                    default:

                        RemoveHolder(foundHolder, AURA_REMOVE_BY_STACK);
                        stop = true;
                        break;
                }
            }

            if (stop)
            {
                break;
            }
        }
    }

    if (!(cast::RecipeOf(*aurSpellInfo).Starts() == cast::Start::Passive) || !IsPassiveSpellStackableWithRanks(aurSpellInfo))
    {
        if (!RemoveConflictingAuras(holder))
        {
            delete holder;
            return false;
        }
    }

    if (TrackedAuraType trackedType = holder->GetTrackedAuraType())
    {
        if (Unit* caster = holder->GetCaster())
        {

            TrackedAuraTargetMap& scTargets = caster->GetTrackedAuraTargets(trackedType);
            for (TrackedAuraTargetMap::iterator itr = scTargets.begin(); itr != scTargets.end();)
            {
                SpellEntry const* itr_spellEntry = itr->first;
                ObjectGuid itr_targetGuid = itr->second;

                if (itr_targetGuid == GetObjectGuid())
                {
                    ++itr;
                    continue;
                }

                bool removed = false;
                switch (trackedType)
                {
                    case TRACK_AURA_TYPE_SINGLE_TARGET:
                        if (IsSingleTargetSpells(itr_spellEntry, aurSpellInfo))
                        {
                            removed = true;

                            if (Unit* itr_target = GetMap()->GetUnit(itr_targetGuid))
                            {
                                itr_target->RemoveAuras(itr_spellEntry->ID);
                            }
                            else
                            {
                                scTargets.erase(itr);
                            }
                        }
                        break;
                    case TRACK_AURA_TYPE_NOT_TRACKED:
                    case MAX_TRACKED_AURA_TYPES:
                        MANGOS_ASSERT(false);
                        break;
                }

                if (removed)
                {
                    itr = scTargets.begin();
                    continue;
                }

                ++itr;
            }

            switch (trackedType)
            {
                case TRACK_AURA_TYPE_SINGLE_TARGET:
                    scTargets[aurSpellInfo] = GetObjectGuid();
                    break;
                default:
                    break;
            }
        }
    }

    holder->_AddSpellAuraHolder();
    m_auras.Enter(holder);

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
        {
            AddAuraToModList(aur);
        }
    }

    holder->ApplyAuraModifiers(true, true);
    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Holder of spell %u now is in use", holder->GetId());

    if (holder->IsDeleted())
    {
        return false;
    }

    holder->HandleSpellSpecificBoosts(true);

    return true;
}

void Unit::AddAuraToModList(Aura* aura)
{
    if (aura->GetModifier()->m_auraname < TOTAL_AURAS)
    {
        m_auraIndex.Add(aura->GetModifier()->m_auraname, aura);
    }
}

void Unit::RemoveOtherRanks(uint32 spellId)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        return;
    }
    SpellAuraHolderMap::const_iterator i, next;
    for (i = m_auras.All().begin(); i != m_auras.All().end(); i = next)
    {
        next = i;
        ++next;
        uint32 i_spellId = (*i).second->GetId();
        if ((*i).second && i_spellId && i_spellId != spellId)
        {
            if (sSpellMgr.IsRankSpellDueToSpell(spellInfo, i_spellId))
            {
                RemoveAuras(i_spellId);

                if (m_auras.All().empty())
                {
                    break;
                }
                else
                {
                    next =  m_auras.All().begin();
                }
            }
        }
    }
}

bool Unit::RemoveConflictingAuras(SpellAuraHolder* holder)
{
    if (!holder)
    {
        return false;
    }

    SpellEntry const* spellProto = holder->GetSpellProto();
    if (!spellProto)
    {
        return false;
    }

    uint32 spellId = holder->GetId();

    if ((cast::RecipeOf(*spellProto).Starts() == cast::Start::Passive))
    {
        if (IsPassiveSpellStackableWithRanks(spellProto))
        {
            return true;
        }
    }

    uint32 firstHoT = 0;
    for (int eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
    {
        if (Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(eff)))
        {
            switch (aura->GetModifier()->m_auraname)
            {
                case SPELL_AURA_PERIODIC_HEAL:
                case SPELL_AURA_OBS_MOD_HEALTH:
                {
                    firstHoT = sSpellMgr.GetFirstSpellInChain(holder->GetId());
                    break;
                }
                default: break;
            }
        }

        if (firstHoT)
        {
            break;
        }
    }

    SpellSpecific spellId_spec = GetSpellSpecific(spellId);

    SpellAuraHolderMap::iterator i, next;
    for (i = m_auras.All().begin(); i != m_auras.All().end(); i = next)
    {
        next = i;
        ++next;
        if (!(*i).second)
        {
            continue;
        }

        SpellEntry const* i_spellProto = (*i).second->GetSpellProto();

        if (!i_spellProto)
        {
            continue;
        }

        uint32 i_spellId = i_spellProto->ID;

        if ((cast::RecipeOf(*i_spellProto).Starts() == cast::Start::Passive))
        {

            if (holder->GetCasterGuid() != i->second->GetCasterGuid())
            {
                continue;
            }

            if (!sSpellMgr.IsRankSpellDueToSpell(spellProto, i_spellId))
            {
                continue;
            }
        }

        if (i_spellId == spellId)
        {
            continue;
        }

        bool is_triggered_by_spell = false;

        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if (i_spellProto->EffectTriggerSpell[j] == spellId)
            {
                is_triggered_by_spell = true;
            }
        }

        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if (spellProto->EffectTriggerSpell[j] == i_spellId)
            {
                is_triggered_by_spell = true;
            }
        }

        if (is_triggered_by_spell)
        {
            continue;
        }

        SpellSpecific i_spellId_spec = GetSpellSpecific(i_spellId);

        bool is_spellSpecPerTargetPerCaster = IsSingleFromSpellSpecificPerTargetPerCaster(spellId_spec, i_spellId_spec);

        bool is_spellSpecPerTarget = IsSingleFromSpellSpecificPerTarget(spellId_spec, i_spellId_spec);

        if (!is_spellSpecPerTarget && firstHoT && firstHoT == sSpellMgr.GetFirstSpellInChain(i_spellId))
        {
            is_spellSpecPerTarget = true;
        }

        if (is_spellSpecPerTarget || (is_spellSpecPerTargetPerCaster && holder->GetCasterGuid() == (*i).second->GetCasterGuid()))
        {

            if (sSpellMgr.IsRankSpellDueToSpell(spellProto, i_spellId))
            {
                if (CompareAuraRanks(spellId, i_spellId) < 0)
                {
                    return false;
                }
            }

            if ((*i).second->IsInUse())
            {
                sLog.outError("SpellAuraHolder (Spell %u) is in process but attempt removed at SpellAuraHolder (Spell %u) adding, need add stack rule for Unit::RemoveConflictingAuras", i->second->GetId(), holder->GetId());
                continue;
            }
            RemoveAuras(i_spellId);

            if (m_auras.All().empty())
            {
                break;
            }
            else
            {
                next =  m_auras.All().begin();
            }

            continue;
        }

        bool is_spellPerTarget = IsSingleFromSpellSpecificSpellRanksPerTarget(spellId_spec, i_spellId_spec);
        if (is_spellPerTarget && holder->GetCasterGuid() != (*i).second->GetCasterGuid() && sSpellMgr.IsRankSpellDueToSpell(spellProto, i_spellId))
        {

            if (CompareAuraRanks(spellId, i_spellId) < 0)
            {
                return false;
            }

            if ((*i).second->IsInUse())
            {
                sLog.outError("SpellAuraHolder (Spell %u) is in process but attempt removed at SpellAuraHolder (Spell %u) adding, need add stack rule for Unit::RemoveConflictingAuras", i->second->GetId(), holder->GetId());
                continue;
            }
            RemoveAuras(i_spellId);

            if (m_auras.All().empty())
            {
                break;
            }
            else
            {
                next =  m_auras.All().begin();
            }

            continue;
        }

        if (!is_spellSpecPerTargetPerCaster && !is_spellSpecPerTarget && sSpellMgr.IsNoStackSpellDueToSpell(spellId, i_spellId))
        {

            if ((*i).second->IsInUse())
            {
                sLog.outError("SpellAuraHolder (Spell %u) is in process but attempt removed at SpellAuraHolder (Spell %u) adding, need add stack rule for Unit::RemoveConflictingAuras", i->second->GetId(), holder->GetId());
                continue;
            }
            RemoveAuras(i_spellId);

            if (m_auras.All().empty())
            {
                break;
            }
            else
            {
                next =  m_auras.All().begin();
            }

            continue;
        }

        if (spellProto->SpellClassSet == SPELLFAMILY_POTION && i_spellProto->SpellClassSet == SPELLFAMILY_POTION)
        {
            if (IsNoStackAuraDueToAura(spellId, i_spellId))
            {
                if (CompareAuraRanks(spellId, i_spellId) < 0)
                {
                    return false;
                }

                if ((*i).second->IsInUse())
                {
                    sLog.outError("SpellAuraHolder (Spell %u) is in process but attempt removed at SpellAuraHolder (Spell %u) adding, need add stack rule for Unit::RemoveConflictingAuras", i->second->GetId(), holder->GetId());
                    continue;
                }
                RemoveAuras(i_spellId);

                if (m_auras.All().empty())
                {
                    break;
                }
                else
                {
                    next =  m_auras.All().begin();
                }
            }
        }
    }
    return true;
}

namespace
{

    template <typename Accept>
    std::vector<SpellAuraHolder*> HoldersOf(Unit& unit, uint32 spellId, Accept accept)
    {
        std::vector<SpellAuraHolder*> matched;
        const Unit::SpellAuraHolderBounds bounds = unit.GetSpellAuraHolderBounds(spellId);
        for (auto it = bounds.first; it != bounds.second; ++it)
        {
            if (accept(it->second))
            {
                matched.push_back(it->second);
            }
        }
        return matched;
    }

    bool StillHeld(Unit& unit, uint32 spellId, const SpellAuraHolder* holder)
    {
        const Unit::SpellAuraHolderBounds bounds = unit.GetSpellAuraHolderBounds(spellId);
        for (auto it = bounds.first; it != bounds.second; ++it)
        {
            if (it->second == holder)
            {
                return true;
            }
        }
        return false;
    }
}

void Unit::RemoveAura(uint32 spellId, SpellEffectIndex effindex, Aura* except)
{
    const auto accept = [effindex, except](const SpellAuraHolder* holder)
    {
        const Aura* aura = holder->GetAuraByEffectIndex(effindex);
        return aura && aura != except;
    };

    for (SpellAuraHolder* holder : HoldersOf(*this, spellId, accept))
    {
        if (StillHeld(*this, spellId, holder))
        {
            RemoveAuraEffect(holder, effindex);
        }
    }
}

void Unit::RemoveAurasCastBy(uint32 spellId, ObjectGuid casterGuid)
{
    const auto accept = [casterGuid](const SpellAuraHolder* holder)
    {
        return holder->GetCasterGuid() == casterGuid;
    };

    for (SpellAuraHolder* holder : HoldersOf(*this, spellId, accept))
    {
        if (StillHeld(*this, spellId, holder))
        {
            RemoveHolder(holder);
        }
    }
}

void Unit::RemoveAuraEffect(uint32 spellId, SpellEffectIndex effindex, ObjectGuid casterGuid, AuraRemoveMode mode)
{
    const auto accept = [effindex, casterGuid](const SpellAuraHolder* holder)
    {
        const Aura* aura = holder->GetAuraByEffectIndex(effindex);
        return aura && aura->GetCasterGuid() == casterGuid;
    };

    for (SpellAuraHolder* holder : HoldersOf(*this, spellId, accept))
    {
        if (StillHeld(*this, spellId, holder))
        {
            RemoveAuraEffect(holder, effindex, mode);
        }
    }
}

void Unit::CancelAuras(uint32 spellId)
{
    RemoveAuras(spellId, nullptr, AURA_REMOVE_BY_CANCEL);
}

void Unit::RemoveAurasWithDispelType(DispelType type, ObjectGuid casterGuid)
{
    const uint32 dispelMask = GetDispellMask(type);

    std::vector<uint32> doomed;
    for (const auto& entry : GetSpellAuraHolderMap())
    {
        const SpellEntry* spell = entry.second->GetSpellProto();
        if (((1u << spell->DispelType) & dispelMask) &&
            (!casterGuid || casterGuid == entry.second->GetCasterGuid()))
        {
            doomed.push_back(spell->ID);
        }
    }

    for (const uint32 spellId : doomed)
    {
        RemoveAuras(spellId);
    }
}

void Unit::RemoveStacks(uint32 spellId, uint32 stackAmount, ObjectGuid casterGuid, AuraRemoveMode mode)
{
    const auto accept = [casterGuid](const SpellAuraHolder* holder)
    {
        return !casterGuid || holder->GetCasterGuid() == casterGuid;
    };

    for (SpellAuraHolder* holder : HoldersOf(*this, spellId, accept))
    {

        if (holder->ModStackAmount(-static_cast<int32>(stackAmount)))
        {
            RemoveHolder(holder, mode);
            break;
        }
    }
}

void Unit::RemoveAuras(uint32 spellId, SpellAuraHolder* except, AuraRemoveMode mode)
{
    const auto accept = [except](const SpellAuraHolder* holder) { return holder != except; };

    for (SpellAuraHolder* holder : HoldersOf(*this, spellId, accept))
    {
        if (StillHeld(*this, spellId, holder))
        {
            RemoveHolder(holder, mode);
        }
    }
}

void Unit::RemoveAurasFromItem(Item* castItem, uint32 spellId)
{
    const ObjectGuid itemGuid = castItem->GetObjectGuid();
    const auto accept = [itemGuid](const SpellAuraHolder* holder)
    {
        return holder->GetCastItemGuid() == itemGuid;
    };

    for (SpellAuraHolder* holder : HoldersOf(*this, spellId, accept))
    {
        if (StillHeld(*this, spellId, holder))
        {
            RemoveHolder(holder);
        }
    }
}

void Unit::RemoveAurasWithInterruptFlags(uint32 flags)
{
    m_auras.RemoveWhere(
        [flags](SpellAuraHolder* holder) { return (holder->GetSpellProto()->AuraInterruptFlags & flags) != 0; },
        [this](SpellAuraHolder* holder) { RemoveHolder(holder); });
}

void Unit::RemoveAurasWithAttribute(uint32 flags)
{
    m_auras.RemoveWhere(
        [flags](SpellAuraHolder* holder) { return holder->GetSpellProto()->HasAttribute((SpellAttributes)flags); },
        [this](SpellAuraHolder* holder) { RemoveHolder(holder); });
}

void Unit::RemoveTrackedAurasOfOthers()
{

    m_auras.RemoveWhere(
        [this](SpellAuraHolder* holder)
        {
            return holder->GetTrackedAuraType() && holder->GetCasterGuid() != GetObjectGuid();
        },
        [this](SpellAuraHolder* holder) { RemoveHolder(holder); });

    for (uint8 type = TRACK_AURA_TYPE_SINGLE_TARGET; type < MAX_TRACKED_AURA_TYPES; ++type)
    {
        TrackedAuraTargetMap& scTargets = GetTrackedAuraTargets(TrackedAuraType(type));
        for (TrackedAuraTargetMap::iterator itr = scTargets.begin(); itr != scTargets.end();)
        {
            SpellEntry const* itr_spellEntry = itr->first;
            ObjectGuid itr_targetGuid = itr->second;

            if (itr_targetGuid != GetObjectGuid())
            {
                scTargets.erase(itr);

                if (Unit* itr_target = GetMap()->GetUnit(itr_targetGuid))
                {
                    itr_target->RemoveAurasCastBy(itr_spellEntry->ID, GetObjectGuid());
                }

                itr = scTargets.begin();
                continue;
            }

            ++itr;
        }
    }
}

void Unit::RemoveHolder(SpellAuraHolder* holder, AuraRemoveMode mode)
{

    SpellEntry const* AurSpellInfo = holder->GetSpellProto();
    Totem* statue = nullptr;
    Unit* caster = holder->GetCaster();
    if ((cast::RecipeOf(*AurSpellInfo).Starts() == cast::Start::Channelled) && caster)
    {
        if (IsCreature(caster) && ((Creature*)caster)->IsTotem() && ((Totem*)caster)->GetTotemType() == TOTEM_STATUE)
        {
            statue = ((Totem*)caster);
        }
    }

    m_auras.Strike(holder);

    holder->SetRemoveMode(mode);
    holder->UnregisterAndCleanupTrackedAuras();

    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (Aura* aura = holder->m_auras[i])
        {
            RemoveAura(aura, mode);
        }
    }

    holder->_RemoveSpellAuraHolder();

    if (mode != AURA_REMOVE_BY_DELETE)
    {
        holder->HandleSpellSpecificBoosts(false);
    }

    if (statue)
    {
        statue->UnSummon();
    }

    if (holder->IsInUse())
    {
        holder->SetDeleted();
        m_auras.Defer(holder);
    }
    else
    {
        delete holder;
    }

    if (mode != AURA_REMOVE_BY_EXPIRE && (cast::RecipeOf(*AurSpellInfo).Starts() == cast::Start::Channelled) && !IsAreaOfEffectSpell(AurSpellInfo) &&
        caster && caster->GetObjectGuid() != GetObjectGuid())
    {
        caster->InterruptSpell(CURRENT_CHANNELED_SPELL);
    }
}

void Unit::RemoveAuraEffect(SpellAuraHolder* holder, SpellEffectIndex index, AuraRemoveMode mode)
{
    Aura* aura = holder->GetAuraByEffectIndex(index);
    if (!aura)
    {
        return;
    }

    if (aura->IsLastAuraOnHolder())
    {
        RemoveHolder(holder, mode);
    }
    else
    {
        RemoveAura(aura, mode);
    }
}

void Unit::RemoveAura(Aura* Aur, AuraRemoveMode mode)
{

    if (Aur->GetModifier()->m_auraname < TOTAL_AURAS)
    {
        m_auraIndex.Remove(Aur->GetModifier()->m_auraname, Aur);
    }

    Aur->SetRemoveMode(mode);

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Aura %u now is remove mode %d", Aur->GetModifier()->m_auraname, mode);

    Aur->GetHolder()->RemoveAura(Aur->GetEffIndex());

    if (mode == AURA_REMOVE_BY_DELETE)
    {
        switch (Aur->GetModifier()->m_auraname)
        {

            case SPELL_AURA_MOD_POSSESS:
            case SPELL_AURA_MOD_POSSESS_PET:
                Aur->ApplyModifier(false, true);
                break;
            default: break;
        }
    }
    else
    {
        Aur->ApplyModifier(false, true);
    }

    if (Aur->IsInUse())
    {
        m_auras.Defer(Aur);
    }
    else
    {
        delete Aur;
    }
}

void Unit::RemoveAllAuras(AuraRemoveMode mode )
{
    while (!m_auras.Empty())
    {
        RemoveHolder(m_auras.First(), mode);
    }
}

void Unit::RemoveAllAurasOnDeath()
{

    m_auras.RemoveWhere(
        [](SpellAuraHolder* holder) { return !holder->IsPassive() && !holder->IsDeathPersistent(); },
        [this](SpellAuraHolder* holder) { RemoveHolder(holder, AURA_REMOVE_BY_DEATH); });
}

void Unit::RemoveAllAurasOnEvade()
{

    m_auras.RemoveWhere(
        [](SpellAuraHolder* holder) { return IsSpellRemovedOnEvade(holder->GetSpellProto()); },
        [this](SpellAuraHolder* holder) { RemoveHolder(holder, AURA_REMOVE_BY_DEFAULT); });

    if (IsCreature(this))
    {
        RemoveDynFlag(UNIT_DYNFLAG_TAPPED);

        if (LootClaim* claim = ClaimOn(*this))
        {
            claim->StakedBy(nullptr);
        }
    }
}

void Unit::DelaySpellAuraHolder(uint32 spellId, int32 delaytime, ObjectGuid casterGuid)
{
    SpellAuraHolderBounds bounds = GetSpellAuraHolderBounds(spellId);
    for (SpellAuraHolderMap::iterator iter = bounds.first; iter != bounds.second; ++iter)
    {
        SpellAuraHolder* holder = iter->second;

        if (casterGuid != holder->GetCasterGuid())
        {
            continue;
        }

        if (holder->GetAuraDuration() < delaytime)
        {
            holder->SetAuraDuration(0);
        }
        else
        {
            holder->SetAuraDuration(holder->GetAuraDuration() - delaytime);
        }

        holder->UpdateAuraDuration();

        DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell %u partially interrupted on %s, new duration: %u ms", spellId, GetGuidStr().c_str(), holder->GetAuraDuration());
    }
}

void Unit::_RemoveAllAuraMods()
{
    for (SpellAuraHolderMap::const_iterator i = m_auras.All().begin(); i != m_auras.All().end(); ++i)
    {
        (*i).second->ApplyAuraModifiers(false);
    }
}

void Unit::_ApplyAllAuraMods()
{
    for (SpellAuraHolderMap::const_iterator i = m_auras.All().begin(); i != m_auras.All().end(); ++i)
    {
        (*i).second->ApplyAuraModifiers(true);
    }
}

bool Unit::HasAuraType(AuraType auraType) const
{
    return !GetAurasByType(auraType).empty();
}

bool Unit::HasAffectedAura(AuraType auraType, SpellEntry const* spellProto) const
{
    const auto auras = GetAurasByType(auraType);

    for (auto* aura : auras)
    {
        if (aura->isAffectedOnSpell(spellProto))
        {
            return true;
        }
    }

    return false;
}

Aura* Unit::GetAura(uint32 spellId, SpellEffectIndex effindex)
{
    SpellAuraHolderBounds bounds = GetSpellAuraHolderBounds(spellId);
    if (bounds.first != bounds.second)
    {
        return bounds.first->second->GetAuraByEffectIndex(effindex);
    }
    return nullptr;
}

Aura* Unit::GetAura(AuraType type, SpellFamily family, uint64 familyFlag, ObjectGuid casterGuid)
{
    const auto auras = GetAurasByType(type);
    for (auto* aura : auras)
    {
        if (aura->GetSpellProto()->IsFitToFamily(family, familyFlag) &&
            (!casterGuid || aura->GetCasterGuid() == casterGuid))
        {
            return aura;
        }
    }
    return nullptr;
}

bool Unit::HasAura(uint32 spellId, SpellEffectIndex effIndex) const
{

    SpellAuraHolderConstBounds spair = GetSpellAuraHolderBounds(spellId);
    for (SpellAuraHolderMap::const_iterator i_holder = spair.first; i_holder != spair.second; ++i_holder)
    {
        if (i_holder->second->GetAuraByEffectIndex(effIndex))
        {
            return true;
        }
    }
    return false;
}
