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

#include <random>
#include "Platform/Define.h"
#include <vector>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "SpellAuras.h"
#include "Group.h"
#include "UpdateData.h"
#include "SharedDefines.h"
#include "Pet.h"
#include "GameObject.h"
#include "GossipDef.h"
#include "Creature.h"
#include "Totem.h"
#include "CreatureAI.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundWS.h"
#include "Language.h"
#include "SocialMgr.h"
#include "Util.h"
#include "TemporarySummon.h"
#include "ScriptMgr.h"
#include "Formulas.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Geometry/Vector3.h"

void Spell::EffectApplyAura(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!unitTarget)
    {
        return;
    }

    if (m_spellInfo->ID == 30918)
    {

        unitTarget->RemoveAurasAtMechanicImmunity(IMMUNE_TO_ROOT_AND_SNARE_MASK, 30918, true);
        return;
    }

    if ((!unitTarget->IsAlive() && !(IsDeathOnlySpell(m_spellInfo) || IsDeathPersistentSpell(m_spellInfo))) &&
        (!IsPlayer(unitTarget) || !((Player*)unitTarget)->GetSession()->PlayerLoading()))
    {
        return;
    }

    Unit* caster = GetAffectiveCaster();
    if (!caster)
    {

        if ((GuidHigh(m_originalCasterGUID) == HIGHGUID_GAMEOBJECT))
        {
            caster = unitTarget;
        }
        else
        {
            return;
        }
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell: Aura is: %u", operation.aura);

    Aura* aur = CreateAura(m_spellInfo, eff_idx, &m_currentBasePoints[eff_idx], m_spellAuraHolder, unitTarget, caster, m_CastItem);
    m_spellAuraHolder->AddAura(aur, eff_idx);
}

void Spell::EffectPowerDrain(const cast::Operation& operation)
{
    if (operation.miscValue < 0 || operation.miscValue >= MAX_POWERS)
    {
        return;
    }

    Powers drain_power = Powers(operation.miscValue);

    if (!unitTarget)
    {
        return;
    }
    if (!unitTarget->IsAlive())
    {
        return;
    }
    if (unitTarget->GetPowerType() != drain_power)
    {
        return;
    }
    if (damage < 0)
    {
        return;
    }

    int32 curPower = unitTarget->GetPower(drain_power);

    damage = m_caster->SpellDamageBonusDone(unitTarget, m_spellInfo, uint32(damage), SPELL_DIRECT_DAMAGE);
    damage = unitTarget->SpellDamageBonusTaken(m_caster, m_spellInfo, uint32(damage), SPELL_DIRECT_DAMAGE);

    int32 new_damage;
    if (curPower < damage)
    {
        new_damage = curPower;
    }
    else
    {
        new_damage = damage;
    }

    unitTarget->ModifyPower(drain_power, -new_damage);

    if (drain_power == POWER_MANA && m_caster != unitTarget)
    {
        float manaMultiplier = operation.amplitude;
        if (manaMultiplier == 0)
        {
            manaMultiplier = 1;
        }

        if (Player* modOwner = m_caster->GetSpellModOwner())
        {
            modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_MULTIPLE_VALUE, manaMultiplier);
        }

        int32 gain = int32(new_damage * manaMultiplier);

        m_caster->EnergizeBySpell(m_caster, m_spellInfo->ID, gain, POWER_MANA);
    }
}

void Spell::EffectSendEvent(const cast::Operation& operation)
{

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell ScriptStart %u for spellid %u in EffectSendEvent ", operation.miscValue, m_spellInfo->ID);

    StartEvents_Event(m_caster->GetMap(), operation.miscValue, m_caster, focusObject, true, m_caster);
}

void Spell::EffectPowerBurn(const cast::Operation& operation)
{
    if (operation.miscValue < 0 || operation.miscValue >= MAX_POWERS)
    {
        return;
    }

    Powers powertype = Powers(operation.miscValue);

    if (!unitTarget)
    {
        return;
    }
    if (!unitTarget->IsAlive())
    {
        return;
    }
    if (unitTarget->GetPowerType() != powertype)
    {
        return;
    }
    if (damage < 0)
    {
        return;
    }

    int32 curPower = int32(unitTarget->GetPower(powertype));

    int32 new_damage = (curPower < damage) ? curPower : damage;

    unitTarget->ModifyPower(powertype, -new_damage);
    float multiplier = operation.amplitude;

    if (Player* modOwner = m_caster->GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_MULTIPLE_VALUE, multiplier);
    }

    new_damage = int32(new_damage * multiplier);
    m_damage += new_damage;
}

void Spell::EffectHeal(const cast::Operation& )
{
    if (unitTarget && unitTarget->IsAlive() && damage >= 0)
    {

        Unit* caster = GetAffectiveCaster();
        if (!caster)
        {
            return;
        }

        int32 addhealth = damage;

        if (m_spellInfo->ID == 18562)
        {
            const auto RejorRegr = unitTarget->GetAurasByType(SPELL_AURA_PERIODIC_HEAL);

            Aura* targetAura = nullptr;
            for (auto* heal : RejorRegr)
            {
                if (heal->GetSpellProto()->SpellClassSet == SPELLFAMILY_DRUID &&

                    (heal->GetSpellProto()->SpellClassMask & UI64LIT(0x0000000000000050)))
                {
                    if (!targetAura || heal->GetAuraDuration() < targetAura->GetAuraDuration())
                    {
                        targetAura = heal;
                    }
                }
            }

            if (!targetAura)
            {
                sLog.outError("Target (GUID: %u TypeId: %u) has aurastate AURA_STATE_SWIFTMEND but no matching aura.", unitTarget->GetGUIDLow(), unitTarget->GetTypeId());
                return;
            }
            int idx = 0;
            while (idx < 3)
            {
                if (targetAura->GetSpellProto()->EffectAura[idx] == SPELL_AURA_PERIODIC_HEAL)
                {
                    break;
                }
                idx++;
            }

            int32 tickheal = targetAura->GetModifier()->m_amount;
            int32 tickcount = GetSpellDuration(targetAura->GetSpellProto()) / targetAura->GetSpellProto()->EffectAuraPeriod[idx];
            if (targetAura->GetSpellProto()->SpellClassMask & UI64LIT(0x0000000000000040))
            {
                tickcount -= 1;
            }

            unitTarget->RemoveAuras(targetAura->GetId());

            addhealth += tickheal * tickcount;
        }

        addhealth = caster->SpellHealingBonusDone(unitTarget, m_spellInfo, addhealth, HEAL, 1, this);
        addhealth = unitTarget->SpellHealingBonusTaken(caster, m_spellInfo, addhealth, HEAL, 1, this);

        m_healing += addhealth;
    }
}

void Spell::EffectHealMechanical(const cast::Operation& )
{

    if (unitTarget && unitTarget->IsAlive() && damage >= 0)
    {

        Unit* caster = GetAffectiveCaster();
        if (!caster)
        {
            return;
        }

        uint32 addhealth = caster->SpellHealingBonusDone(unitTarget, m_spellInfo, damage, HEAL);
        addhealth = unitTarget->SpellHealingBonusTaken(caster, m_spellInfo, addhealth, HEAL);

        caster->DealHeal(unitTarget, addhealth, m_spellInfo);
    }
}

void Spell::EffectHealthLeech(const cast::Operation& operation)
{
    if (!unitTarget)
    {
        return;
    }
    if (!unitTarget->IsAlive())
    {
        return;
    }

    if (damage < 0)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "HealthLeech :%i", damage);

    uint32 curHealth = unitTarget->GetHealth();
    damage = m_caster->SpellNonMeleeDamageLog(unitTarget, m_spellInfo->ID, damage);
    if ((int32)curHealth < damage)
    {
        damage = curHealth;
    }

    float multiplier = operation.amplitude;

    if (Player* modOwner = m_caster->GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_MULTIPLE_VALUE, multiplier);
    }

    uint32 heal = uint32(damage * multiplier);
    if (m_caster->IsAlive())
    {
        heal = m_caster->SpellHealingBonusTaken(m_caster, m_spellInfo, heal, HEAL);

        m_caster->DealHeal(m_caster, heal, m_spellInfo);
    }
}

void Spell::DoCreateItem(SpellEffectIndex eff_idx, uint32 itemtype)
{
    if (!unitTarget || !IsPlayer(unitTarget))
    {
        return;
    }

    Player* player = (Player*)unitTarget;

    uint32 newitemid = itemtype;
    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(newitemid);
    if (!pProto)
    {
        player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    uint32 bgType = 0;
    switch (m_spellInfo->ID)
    {
        case SPELL_AV_MARK_WINNER:
        case SPELL_AV_MARK_LOSER:
            bgType = BATTLEGROUND_AV;
            break;
        case SPELL_WS_MARK_WINNER:
        case SPELL_WS_MARK_LOSER:
            bgType = BATTLEGROUND_WS;
            break;
        case SPELL_AB_MARK_WINNER:
        case SPELL_AB_MARK_LOSER:
            bgType = BATTLEGROUND_AB;
            break;
        default:
            break;
    }

    uint32 num_to_add = damage;

    if (num_to_add < 1)
    {
        num_to_add = 1;
    }
    if (num_to_add > pProto->Stackable)
    {
        num_to_add = pProto->Stackable;
    }

    int items_count = 1;

    num_to_add *= items_count;

    ItemPosCountVec dest;
    uint32 no_space = 0;
    InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, newitemid, num_to_add, &no_space);
    if (msg != EQUIP_ERR_OK)
    {

        if (msg == EQUIP_ERR_INVENTORY_FULL || msg == EQUIP_ERR_CANT_CARRY_MORE_OF_THIS)
        {
            num_to_add -= no_space;
        }
        else
        {

            player->SendEquipError(msg, nullptr, nullptr, newitemid);
            return;
        }
    }

    if (num_to_add)
    {

        Item* pItem = player->StoreNewItem(dest, newitemid, true, Item::GenerateItemRandomPropertyId(newitemid));

        if (!pItem)
        {
            player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
            return;
        }

        if (pItem->GetProto()->Class != ITEM_CLASS_CONSUMABLE && pItem->GetProto()->Class != ITEM_CLASS_QUEST)
        {
            pItem->SetCreatorGuid(player->GetObjectGuid());
        }

        player->SendNewItem(pItem, num_to_add, true, !bgType);

        if (!bgType)
        {
            player->UpdateCraftSkill(m_spellInfo->ID);
        }
    }

    if (no_space > 0 && bgType)
    {
        if (BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(BattleGroundTypeId(bgType)))
        {
            bg->SendRewardMarkByMail(player, newitemid, no_space);
        }
    }
}

void Spell::EffectCreateItem(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    switch (m_spellInfo->ID)
    {
        case SPELL_FILLING_EMPTY_JAR__CURSED_OOZE:
        case SPELL_FILLING_EMPTY_JAR__TAINTED_OOZE:
        {
            if (IsCreature(unitTarget))
            {
                Creature* creature = static_cast<Creature*>(unitTarget);
                if (creature->IsDead() && (creature->GetEntry() == CREATURE_TAINTED_OOZE || creature->GetEntry() == CREATURE_CURSED_OOZE))
                {
                    creature->ForcedDespawn();
                }
            }

            break;
        }
        case SPELL_FILLING_EMPTY_JAR__PURE_OOZE:
        {
            if (IsCreature(unitTarget))
            {
                Creature* creature = static_cast<Creature*>(unitTarget);
                if (creature->IsDead() &&
                    (creature->GetEntry() == CREATURE_MUCULENT_OOZE ||
                    creature->GetEntry() == CREATURE_PRIMAL_OOZE ||
                    creature->GetEntry() == CREATURE_GLUTINOUS_OOZE
                    ))
                {
                    creature->ForcedDespawn();
                }
            }

            break;
        }
    }
    DoCreateItem(eff_idx, operation.itemType);
}

void Spell::EffectPersistentAA(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    Unit* pCaster = GetAffectiveCaster();

    if (!pCaster)
    {
        pCaster = m_caster;
    }

    float radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(operation.radiusIndex));

    if (Player* modOwner = pCaster->GetSpellModOwner())
    {
        modOwner->SpellMods().Apply(m_spellInfo->ID, SPELLMOD_RADIUS, radius);
    }

    DynamicObject* dynObj = new DynamicObject;
    if (!dynObj->Create(pCaster->GetMap()->GenerateLocalLowGuid(HIGHGUID_DYNAMICOBJECT), pCaster, m_spellInfo->ID,
        eff_idx, m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, m_duration, radius, DYNAMIC_OBJECT_AREA_SPELL))
    {
        delete dynObj;
        return;
    }

    pCaster->Conjured().AddArea(dynObj);
    pCaster->GetMap()->Add(dynObj);
}

void Spell::EffectEnergize(const cast::Operation& operation)
{
    if (!unitTarget)
    {
        return;
    }
    if (!unitTarget->IsAlive())
    {
        return;
    }

    if (operation.miscValue < 0 || operation.miscValue >= MAX_POWERS)
    {
        return;
    }

    Powers power = Powers(operation.miscValue);

    int level_multiplier = 0;
    int level_diff = 0;
    switch (m_spellInfo->ID)
    {
        case 9512:
            level_diff = m_caster->getLevel() - 60;
            level_multiplier = 2;
            break;
        case 24571:
            level_diff = m_caster->getLevel() - 60;
            level_multiplier = 10;
            break;
        case 24532:
            level_diff = m_caster->getLevel() - 60;
            level_multiplier = 4;
            break;
        default:
            break;
    }

    if (level_diff > 0)
    {
        damage -= level_multiplier * level_diff;
    }

    if (damage < 0)
    {
        return;
    }

    if (unitTarget->GetMaxPower(power) == 0)
    {
        return;
    }

    m_caster->EnergizeBySpell(unitTarget, m_spellInfo->ID, damage, power);
}
