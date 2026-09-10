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

void Spell::cancel()
{
    if (m_spellState == SPELL_STATE_FINISHED)
    {
        return;
    }

    bool sendInterrupt = Recipe().Starts() == cast::Start::Channelled ? false : true;

    m_autoRepeat = false;
    switch (m_spellState)
    {
        case SPELL_STATE_PREPARING:
            CancelGlobalCooldown();

        case SPELL_STATE_DELAYED:
        {
            SendInterrupted(SPELL_FAILED_INTERRUPTED);

            if (sendInterrupt)
            {
                SendCastResult(SPELL_FAILED_INTERRUPTED);
            }
            break;
        }
        case SPELL_STATE_CASTING:
        {
            for (const auto& enrolled : m_roster.Units())
            {
                if (enrolled.verdict == SPELL_MISS_NONE)
                {
                    Unit* unit = m_caster->GetObjectGuid() == enrolled.guid ? m_caster : ObjectLookup::GetUnit(*m_caster, enrolled.guid);
                    if (unit && unit->IsAlive())
                    {
                        unit->RemoveAurasCastBy(m_spellInfo->ID, m_caster->GetObjectGuid());
                    }
                }
            }

            SendChannelUpdate(0);
            SendInterrupted(SPELL_FAILED_INTERRUPTED);

            if (sendInterrupt)
            {
                SendCastResult(SPELL_FAILED_INTERRUPTED);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    finish(false);
    m_caster->Conjured().RemoveAreas(m_spellInfo->ID);
    m_caster->Conjured().RemoveObjects(m_spellInfo->ID, true);
}

void Spell::cast(bool skipCheck)
{
    SetExecutedCurrently(true);

    if (!m_caster->CheckAndIncreaseCastCounter())
    {
        if (m_triggeredByAuraSpell)
        {
            sLog.outError("Spell %u triggered by aura spell %u too deep in cast chain for cast. Cast not allowed for prevent overflow stack crash.", m_spellInfo->ID, m_triggeredByAuraSpell->ID);
        }
        else
        {
            sLog.outError("Spell %u too deep in cast chain for cast. Cast not allowed for prevent overflow stack crash.", m_spellInfo->ID);
        }

        SendCastResult(SPELL_FAILED_ERROR);
        finish(false);
        SetExecutedCurrently(false);
        return;
    }

    UpdatePointers();

    if (!m_targets.getUnitTarget() && m_targets.getUnitTargetGuid() && m_targets.getUnitTargetGuid() != m_caster->GetObjectGuid())
    {
        cancel();
        m_caster->DecreaseCastCounter();
        SetExecutedCurrently(false);
        return;
    }

    if (!IsPlayer(m_caster) && m_targets.getUnitTarget() && m_targets.getUnitTarget() != m_caster)
    {
        m_caster->SetInFront(m_targets.getUnitTarget());
    }

    SpellCastResult castResult = CheckPower();
    if (castResult != SPELL_CAST_OK)
    {
        SendInterrupted(castResult);
        SendCastResult(castResult);
        finish(false);
        m_caster->DecreaseCastCounter();
        SetExecutedCurrently(false);
        return;
    }

    if (!skipCheck)
    {
        castResult = CheckCast(false);
        if (castResult != SPELL_CAST_OK)
        {
            SendInterrupted(castResult);
            SendCastResult(castResult);
            finish(false);
            m_caster->DecreaseCastCounter();
            SetExecutedCurrently(false);
            return;
        }
    }

    switch (m_spellInfo->SpellClassSet)
    {
        case SPELLFAMILY_GENERIC:
        {

            if (m_spellInfo->Mechanic == MECHANIC_BANDAGE)
            {
                AddPrecastSpell(SPELL_ID_RECENTLY_BANDAGED);
            }

            else if (m_spellInfo->Mechanic == MECHANIC_INVULNERABILITY)
            {
                AddPrecastSpell(25771);
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {

            if (m_spellInfo->SpellClassMask & UI64LIT(0x00000080) &&IsPlayer(m_caster) && (!m_caster->GetAura(14076, SpellEffectIndex(0)) && !m_caster->GetAura(14094, SpellEffectIndex(0)) && !m_caster->GetAura(14095, SpellEffectIndex(0))))
            {
                m_caster->RemoveAurasOfType(SPELL_AURA_MOD_STEALTH);
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            break;
        }
        case SPELLFAMILY_PRIEST:
        {

            if (m_spellInfo->SpellClassSet == SPELLFAMILY_PRIEST && m_spellInfo->SpellClassMask & UI64LIT(0x0000000000000001))
            {
                AddPrecastSpell(6788);
            }

            switch (m_spellInfo->ID)
            {
                case 15237: AddTriggeredSpell(23455); break;
                case 15430: AddTriggeredSpell(23458); break;
                case 15431: AddTriggeredSpell(23459); break;
                case 27799: AddTriggeredSpell(27803); break;
                case 27800: AddTriggeredSpell(27804); break;
                case 27801: AddTriggeredSpell(27805); break;
                case 25331: AddTriggeredSpell(25329); break;
                default: break;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {

            if (m_spellInfo->Mechanic == MECHANIC_INVULNERABILITY && m_spellInfo->ID != 25771)
            {
                AddPrecastSpell(25771);
            }
            break;
        }
        default:
            break;
    }

    if (m_spellInfo->ID == 12042)
    {
        m_targets.getUnitTarget()->RemoveAuras(10060);
    }

    SpellLinkedSet linkedSet = sSpellMgr.GetSpellLinked(m_spellInfo->ID, SPELL_LINKED_TYPE_PRECAST);
    if (linkedSet.size() > 0)
    {
        for (SpellLinkedSet::const_iterator itr = linkedSet.begin(); itr != linkedSet.end(); ++itr)
        {
            AddPrecastSpell(*itr);
        }
    }

    linkedSet.clear();
    linkedSet = sSpellMgr.GetSpellLinked(m_spellInfo->ID, SPELL_LINKED_TYPE_TRIGGERED);
    if (linkedSet.size() > 0)
    {
        for (SpellLinkedSet::const_iterator itr = linkedSet.begin(); itr != linkedSet.end(); ++itr)
        {
            AddTriggeredSpell(*itr);
        }
    }

    m_targets.updateTradeSlotItem();

    FillTargetMap();

    if (m_spellState == SPELL_STATE_FINISHED)
    {
        m_caster->DecreaseCastCounter();
        SetExecutedCurrently(false);
        return;
    }

    SendSpellCooldown();

    TakePower();
    TakeReagents();
    TakeAmmo();

    SendCastResult(castResult);
    SendSpellGo();

    InitializeDamageMultipliers();

    float speed = m_spellInfo->Speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->Speed : m_spellInfo->Speed;
    if (speed > 0.0f)
    {

        TakeCastItem();

        for (auto& enrolled : m_roster.Units())
        {
            HandleDelayedSpellLaunch(&enrolled);
        }

        m_immediateHandled = false;
        m_spellState = SPELL_STATE_DELAYED;
        SetDelayStart(0);
    }
    else
    {

        handle_immediate();
    }

    m_caster->DecreaseCastCounter();
    SetExecutedCurrently(false);
}

void Spell::handle_immediate()
{

    _handle_immediate_phase();

    if (Recipe().Starts() == cast::Start::Channelled && m_duration)
    {
        m_spellState = SPELL_STATE_CASTING;
        SendChannelStart(m_duration);
    }

    for (auto& enrolled : m_roster.Units())
    {
        DoAllEffectOnTarget(&enrolled);
    }

    for (auto& enrolled : m_roster.Objects())
    {
        DoAllEffectOnTarget(&enrolled);
    }

    _handle_finish_phase();

    TakeCastItem();

    if (m_spellState != SPELL_STATE_CASTING)
    {
        finish(true);
    }
}

uint64 Spell::handle_delayed(uint64 t_offset)
{
    uint64 next_time = 0;

    if (!m_immediateHandled)
    {
        _handle_immediate_phase();
        m_immediateHandled = true;
    }

    for (auto& enrolled : m_roster.Units())
    {
        if (!enrolled.served)
        {
            if (enrolled.arrivesInMs <= t_offset)
            {
                DoAllEffectOnTarget(&enrolled);
            }
            else if (next_time == 0 || enrolled.arrivesInMs < next_time)
            {
                next_time = enrolled.arrivesInMs;
            }
        }
    }

    for (auto& enrolled : m_roster.Objects())
    {
        if (!enrolled.served)
        {
            if (enrolled.arrivesInMs <= t_offset)
            {
                DoAllEffectOnTarget(&enrolled);
            }
            else if (next_time == 0 || enrolled.arrivesInMs < next_time)
            {
                next_time = enrolled.arrivesInMs;
            }
        }
    }

    if (next_time == 0)
    {

        _handle_finish_phase();

        finish(true);

        return 0;
    }
    else
    {

        return next_time;
    }
}

void Spell::_handle_immediate_phase()
{

    HandleThreatSpells();

    m_needSpellLog = IsNeedSendToClient();
    for (const auto& operation : Recipe().Does())
    {
        const SpellEffectIndex j = SpellEffectIndex(operation.slot);

        if (operation.verb == SPELL_EFFECT_SEND_EVENT && !m_roster.ServesSlot(operation.slot))
        {
            HandleEffects(nullptr, nullptr, nullptr, j);
            continue;
        }

        if (operation.verb == SPELL_EFFECT_SCHOOL_DAMAGE)
        {
            m_needSpellLog = false;
        }
    }

    m_diminishLevel = unit::Fade::Full;
    m_diminishGroup = DIMINISHING_NONE;

    for (auto& enrolled : m_roster.Items())
    {
        DoAllEffectOnTarget(&enrolled);
    }

    for (const auto& operation : Recipe().Does())
    {

        if (operation.verb == SPELL_EFFECT_PERSISTENT_AREA_AURA ||

            (operation.verb == SPELL_EFFECT_TRANS_DOOR && operation.targetA == TARGET_AREAEFFECT_GO_AROUND_DEST))
        {
            HandleEffects(nullptr, nullptr, nullptr, SpellEffectIndex(operation.slot));
        }
    }
}

void Spell::_handle_finish_phase()
{

    if (m_needSpellLog)
    {
        SendLogExecute();
    }

    if (m_caster->m_extraAttacks && m_spellInfo->HasSpellEffect(SPELL_EFFECT_ADD_EXTRA_ATTACKS))
    {
        switch (m_spellInfo->ID)
        {

            case 15494:
            case 18797:
            case 21919:
            case 20178:
                break;
            default:
                if (Unit* victim = m_caster->getVictim())
                {
                    m_caster->HandleProcExtraAttackFor(victim);
                }
                else
                {
                    m_caster->m_extraAttacks = 0;
                }
                break;
        }
    }
}

void Spell::SendSpellCooldown()
{
    if (!IsPlayer(m_caster))
    {
        return;
    }

    Player* _player = (Player*)m_caster;

    if (Recipe().Says().spentWhileActive || Recipe().Says().passive)
    {
        return;
    }

    _player->AddSpellAndCategoryCooldowns(m_spellInfo, m_CastItem ? m_CastItem->GetEntry() : 0, this);
}

void Spell::update(uint32 difftime)
{

    UpdatePointers();

    if (!(m_targets.getUnitTargetGuid() == 0) && !m_targets.getUnitTarget())
    {
        cancel();
        return;
    }

    if (Unit* target = m_targets.getUnitTarget())
    {
        if (!target->IsVisibleForOrDetect(m_caster, m_caster, true) || target->HasAuraType(SPELL_AURA_FEIGN_DEATH))
        {
            if (m_caster->GetTargetGuid() == target->GetObjectGuid())
            {
                m_caster->SetTargetGuid(0);
            }
            cancel();
            return;
        }
    }

    if (m_targets.getUnitTarget() && (m_targets.getUnitTarget() != m_caster) && IsSingleTargetSpell(m_spellInfo) &&
        !IsNextMeleeSwingSpell() && !IsAutoRepeat() && !m_IsTriggeredSpell)
    {
        if (!HasLineOfSight(*m_caster, *m_targets.getUnitTarget()))
        {
            cancel();
            return;
        }
    }

    if ( (IsPlayer(m_caster) || IsCreature(m_caster)) && m_timer != 0 &&
        (m_castPositionX != m_caster->Where().X() || m_castPositionY != m_caster->Where().Y() || m_castPositionZ != m_caster->Where().Z()) &&
        (Recipe().At(EFFECT_INDEX_0).verb != SPELL_EFFECT_STUCK || !m_caster->m_movementInfo.HasMovementFlag(MOVEFLAG_FALLINGFAR)))
    {

        if (m_spellState == SPELL_STATE_CASTING)
        {
            cancel();
        }

        else if (!IsNextMeleeSwingSpell() && !IsAutoRepeat() && !m_IsTriggeredSpell && (m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT))
        {
            cancel();
        }
    }

    switch (m_spellState)
    {
        case SPELL_STATE_PREPARING:
        {
            if (m_timer)
            {
                if (difftime >= m_timer)
                {
                    m_timer = 0;
                }
                else
                {
                    m_timer -= difftime;
                }
            }

            if (m_timer == 0 && !IsNextMeleeSwingSpell() && !IsAutoRepeat())
            {
                cast();
            }
            break;
        }
        case SPELL_STATE_CASTING:
        {
            if (m_timer > 0)
            {
                if (IsPlayer(m_caster) ||IsCreature(m_caster))
                {

                    if (m_caster->m_movementInfo.HasMovementFlag(MOVEFLAG_FALLING))
                    {
                        cancel();
                    }

                    if (m_caster->hasUnitState(UNIT_STAT_CAN_NOT_REACT))
                    {

                        if (!Recipe().Says().channels && !Recipe().Says().survivesIncapacity)
                        {
                            cancel();
                        }
                    }

                    if (m_spellInfo->ChannelInterruptFlags & CHANNEL_FLAG_TURNING && m_castOrientation != m_caster->Where().Facing())
                    {
                        if (IsPlayer(m_caster))
                        {
                            if (static_cast<Player*>(m_caster)->GetMover()->GetObjectGuid() == m_caster->GetObjectGuid())
                            {
                                cancel();
                            }
                        }
                        else
                        {
                            cancel();
                        }
                    }
                }

                if (!IsAliveUnitPresentInTargetList())
                {
                    SendChannelUpdate(0);
                    finish();
                }

                if (difftime >= m_timer)
                {
                    m_timer = 0;
                }
                else
                {
                    m_timer -= difftime;
                }
            }

            if (m_timer == 0)
            {
                SendChannelUpdate(0);

                if (!IsAutoRepeat() && !IsNextMeleeSwingSpell())
                {
                    if (Player* p = m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
                    {
                        for (const auto& enrolled : m_roster.Units())
                        {
                            if (!(GuidHigh(enrolled.guid) == HIGHGUID_UNIT))
                            {
                                continue;
                            }

                            Unit* unit = m_caster->GetObjectGuid() == enrolled.guid ? m_caster : ObjectLookup::GetUnit(*m_caster, enrolled.guid);
                            if (unit == nullptr)
                            {
                                continue;
                            }

                            p->RewardPlayerAndGroupAtCast(unit, m_spellInfo->ID);
                        }

                        for (const auto& enrolled : m_roster.Objects())
                        {
                            GameObject* go = m_caster->GetMap()->GetGameObject(enrolled.guid);
                            if (!go)
                            {
                                continue;
                            }

                            p->RewardPlayerAndGroupAtCast(go, m_spellInfo->ID);
                        }
                    }
                }

                finish();
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

void Spell::finish(bool ok)
{
    if (!m_caster)
    {
        return;
    }

    if (m_spellState == SPELL_STATE_FINISHED)
    {
        return;
    }

    if (Player* modOwner = m_caster->GetSpellModOwner())
    {
        if (ok || m_spellState != SPELL_STATE_PREPARING)
        {
            modOwner->SpellMods().Spent(this);
        }
        else
        {
            modOwner->SpellMods().Restore(this);
        }
    }

    m_spellState = SPELL_STATE_FINISHED;

    if (!ok)
    {
        return;
    }

    const auto targetTriggers = m_caster->GetAurasByType(SPELL_AURA_ADD_TARGET_TRIGGER);
    for (auto* aura : targetTriggers)
    {
        if (!aura->isAffectedOnSpell(m_spellInfo))
        {
            continue;
        }
        for (const auto& enrolled : m_roster.Units())
        {
            if (enrolled.verdict == SPELL_MISS_NONE)
            {

                Unit* unit = m_caster->GetObjectGuid() == enrolled.guid ? m_caster : ObjectLookup::GetUnit(*m_caster, enrolled.guid);
                if (unit && unit->IsAlive())
                {
                    SpellEntry const* auraSpellInfo = aura->GetSpellProto();
                    SpellEffectIndex auraSpellIdx = aura->GetEffIndex();

                    int32 auraBasePoints = aura->GetBasePoints();
                    int32 chance = m_caster->CalculateSpellDamage(unit, cast::RecipeOf(*auraSpellInfo), cast::RecipeOf(*auraSpellInfo).At(static_cast<uint8>(auraSpellIdx)), &auraBasePoints);
                    if (roll_chance_i(chance))
                    {
                        m_caster->CastSpell(unit, auraSpellInfo->EffectTriggerSpell[auraSpellIdx], true, nullptr, aura);
                    }
                }
            }
        }
    }

    if (m_healthLeech)
    {
        m_caster->DealHeal(m_caster, uint32(m_healthLeech), m_spellInfo);
    }

    if (IsMeleeAttackResetSpell())
    {
        m_caster->resetAttackTimer(BASE_ATTACK);
        if (m_caster->haveOffhandWeapon())
        {
            m_caster->resetAttackTimer(OFF_ATTACK);
        }
    }

    if (IsPlayer(m_caster) && NeedsComboPoints(m_spellInfo))
    {

        bool needDrop = true;
        if (!Recipe().IsPositive())
        {
            for (const auto& enrolled : m_roster.Units())
            {
                if (enrolled.verdict != SPELL_MISS_NONE && enrolled.guid != m_caster->GetObjectGuid())
                {
                    needDrop = false;
                    break;
                }
            }
        }
        if (needDrop)
        {
            ((Player*)m_caster)->ClearComboPoints();
        }
    }

    if (!m_TriggerSpells.empty())
    {
        CastTriggerSpells();
    }

    if (Recipe().Says().stopsAttack)
    {
        m_caster->AttackStop();
    }

}

void Spell::TakeAmmo()
{
    if (Recipe().Swings() == RANGED_ATTACK &&IsPlayer(m_caster))
    {
        Item* pItem = ((Player*)m_caster)->GetWeaponForAttack(RANGED_ATTACK, true, false);

        if (!pItem || pItem->GetProto()->SubClass == ITEM_SUBCLASS_WEAPON_WAND)
        {
            return;
        }

        if (pItem->GetProto()->InventoryType == INVTYPE_THROWN)
        {
            if (pItem->GetMaxStackCount() == 1)
            {

                ((Player*)m_caster)->DurabilityPointLossForEquipSlot(EQUIPMENT_SLOT_RANGED);
            }
            else
            {

                uint32 count = 1;
                ((Player*)m_caster)->DestroyItemCount(pItem, count, true);
            }
        }
        else if (uint32 ammo = ((Player*)m_caster)->GetUInt32Value(PLAYER_AMMO_ID))
        {
            ((Player*)m_caster)->DestroyItemCount(ammo, 1, true);
        }
    }
}
