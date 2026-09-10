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
#include "Common/TimeConstants.h"
#include <algorithm>
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

void Spell::EffectResurrectNew(const cast::Operation& operation)
{
    if (!unitTarget || unitTarget->IsAlive())
    {
        return;
    }

    if (!IsPlayer(unitTarget))
    {
        return;
    }

    if (!unitTarget->IsInWorld())
    {
        return;
    }

    Player* pTarget = ((Player*)unitTarget);

    if (pTarget->isRessurectRequested())
    {
        return;
    }

    uint32 health = damage;
    uint32 mana = operation.miscValue;
    pTarget->setResurrectRequestData(m_caster->GetObjectGuid(), m_caster->GetMapId(), m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z(), health, mana);
    SendResurrectRequest(pTarget);
}

void Spell::EffectInstaKill(const cast::Operation& )
{
    if (!unitTarget || !unitTarget->IsAlive())
    {
        return;
    }

    if (m_spellInfo->ID == 18788 &&IsCreature(unitTarget))
    {
        uint32 entry = unitTarget->GetEntry();
        uint32 spellID;
        switch (entry)
        {
            case   416: spellID = 18789; break;
            case   417: spellID = 18792; break;
            case  1860: spellID = 18790; break;
            case  1863: spellID = 18791; break;
            default:
                sLog.outError("EffectInstaKill: Unhandled creature entry (%u) case.", entry);
                return;
        }

        m_caster->CastSpell(m_caster, spellID, true);
    }

    if (m_caster == unitTarget)
    {
        finish();
        WorldPacket data(SMSG_SPELLINSTAKILLLOG, (8 + 4));
        data << m_caster->GetObjectGuid();
        data << uint32(m_spellInfo->ID);
        Deliver(Audience::Around(*m_caster).AndSubject(), &data);
    }

    m_caster->DealDamage(unitTarget, unitTarget->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
}

void Spell::EffectEnvironmentalDMG(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    uint32 absorb = 0;
    uint32 resist = 0;

    damage = m_spellInfo->CalculateSimpleValue(eff_idx);

    m_caster->CalculateDamageAbsorbAndResist(m_caster, GetSpellSchoolMask(m_spellInfo), SPELL_DIRECT_DAMAGE, damage, &absorb, &resist);

    m_caster->SendSpellNonMeleeDamageLog(m_caster, m_spellInfo->ID, damage, GetSpellSchoolMask(m_spellInfo), absorb, resist, false, 0, false);
    if (IsPlayer(m_caster))
    {
        ((Player*)m_caster)->Dangers().Harm(DAMAGE_FIRE, damage);
    }
}

void Spell::EffectSchoolDMG(const cast::Operation& operation)
{
    const SpellEffectIndex effect_idx = SpellEffectIndex(operation.slot);

    if (unitTarget && unitTarget->IsAlive())
    {
        switch (m_spellInfo->SpellClassSet)
        {
            case SPELLFAMILY_GENERIC:
            {
                switch (m_spellInfo->ID)
                {

                    case 24340: case 26558: case 28884:
                    case 26789:
                    {
                        uint32 count = 0;
                        for (const auto& enrolled : m_roster.Units())
                        {
                            if (enrolled.slots & (1 << effect_idx))
                            {
                                ++count;
                            }
                        }

                        damage /= count;
                        break;
                    }

                    case 25599:
                    {
                        damage = unitTarget->GetHealth() / 2;
                        if (damage < 200)
                        {
                            damage = 200;
                        }
                        break;
                    }

                    case 20467:    case 20963:    case 20964:    case 20965:    case 20966:
                    {
                        if (!unitTarget->hasUnitState(UNIT_STAT_STUNNED) &&IsPlayer(m_caster))
                        {
                            damage /= 2;
                        }
                        break;
                    }
                }
                break;
            }

            case SPELLFAMILY_MAGE:
                break;
            case SPELLFAMILY_WARRIOR:
            {

                if (m_spellInfo->SpellIconID == 38 && m_spellInfo->SpellClassMask & UI64LIT(0x2000000))
                {
                    damage = uint32(damage * (m_caster->GetTotalAttackPowerValue(BASE_ATTACK)) / 100);
                }

                else if (m_spellInfo->SpellClassMask & UI64LIT(0x100000000))
                {
                    damage += int32(m_caster->Sheet().ShieldBlock());
                }
                break;
            }
            case SPELLFAMILY_WARLOCK:
            {

                if (m_spellInfo->SpellClassMask & UI64LIT(0x0000000000000200))
                {

                    const auto mPeriodic = unitTarget->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
                    for (auto* aura : mPeriodic)
                    {
                        if (aura->GetCasterGuid() == m_caster->GetObjectGuid() &&

                            aura->GetSpellProto()->IsFitToFamily(SPELLFAMILY_WARLOCK, UI64LIT(0x0000000000000004)))
                        {
                            unitTarget->RemoveAurasCastBy(aura->GetId(), m_caster->GetObjectGuid());
                            break;
                        }
                    }
                }
                break;
            }
            case SPELLFAMILY_DRUID:
            {

                if ((m_spellInfo->SpellClassMask & UI64LIT(0x000800000)) && m_spellInfo->SpellVisualID == 6587)
                {

                    float multiple = m_caster->GetTotalAttackPowerValue(BASE_ATTACK) / 630 + operation.chainAmplitude;
                    damage += int32(m_caster->GetPower(POWER_ENERGY) * multiple);
                    m_caster->SetPower(POWER_ENERGY, 0);
                }
                break;
            }
            case SPELLFAMILY_ROGUE:
            {

                if ((m_spellInfo->SpellClassMask & UI64LIT(0x00020000)) &&IsPlayer(m_caster))
                {
                    if (uint32 combo = ((Player*)m_caster)->GetComboPoints())
                    {
                        damage += int32(m_caster->GetTotalAttackPowerValue(BASE_ATTACK) * combo * 0.03f);
                    }
                }
                break;
            }
            case SPELLFAMILY_HUNTER:
                break;
            case SPELLFAMILY_PALADIN:
                break;
        }

        if (damage >= 0)
        {
            m_damage += damage;
        }
    }
}

void Spell::EffectTriggerSpell(const cast::Operation& operation)
{

    if (!unitTarget)
    {
        if (gameObjTarget || itemTarget)
        {
            sLog.outError("Spell::EffectTriggerSpell (Spell: %u): Unsupported non-unit case!", m_spellInfo->ID);
        }
        return;
    }

    uint32 triggered_spell_id = operation.triggerSpell;

    switch (triggered_spell_id)
    {

        case 16630:
            if (urand(0, 100) < 67)
            {
                m_caster->CastSpell(unitTarget, triggered_spell_id, true);
            }
            return;

        case 16631:
            if (urand(0, 100) < 34)
            {
                m_caster->CastSpell(unitTarget, triggered_spell_id, true);
            }
            return;

        case 18461:
        {
            unitTarget->RemoveAurasOfType(SPELL_AURA_MOD_ROOT);
            unitTarget->RemoveAurasOfType(SPELL_AURA_MOD_DECREASE_SPEED);
            unitTarget->RemoveAurasOfType(SPELL_AURA_MOD_STALKED);

            if (!IsPlayer(unitTarget))
            {
                return;
            }

            uint32 spellId = 0;
            const PlayerSpellMap& sp_list = ((Player*)unitTarget)->GetSpellMap();
            for (PlayerSpellMap::const_iterator itr = sp_list.begin(); itr != sp_list.end(); ++itr)
            {

                if (!itr->second.active || itr->second.disabled || itr->second.state == PLAYERSPELL_REMOVED)
                {
                    continue;
                }

                SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);
                if (!spellInfo)
                {
                    continue;
                }

                if (spellInfo->IsFitToFamily(SPELLFAMILY_ROGUE, UI64LIT(0x0000000000400000)))
                {
                    spellId = spellInfo->ID;
                    break;
                }
            }

            if (!spellId)
            {
                return;
            }

            if (((Player*)unitTarget)->HasSpellCooldown(spellId))
            {
                ((Player*)unitTarget)->RemoveSpellCooldown(spellId);
            }

            m_caster->CastSpell(unitTarget, spellId, true);
            return;
        }

        case 23209:
            if (urand(0, 100) < 55)
            {
                m_caster->CastSpell(unitTarget, triggered_spell_id, true);
            }
            return;

        case 23253:
            if (urand(0, 100) < 35)
            {
                m_caster->CastSpell(unitTarget, triggered_spell_id, true);
            }
            return;

        case 23770:

            return;

        case 29284:
            m_caster->CastSpell(unitTarget, 24575, true, m_CastItem, nullptr, m_originalCasterGUID);
            return;

        case 29286:
            m_caster->CastSpell(unitTarget, 26464, true, m_CastItem, nullptr, m_originalCasterGUID);
            return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(triggered_spell_id);
    if (!spellInfo)
    {
        sLog.outError("EffectTriggerSpell of spell %u: triggering unknown spell id %i", m_spellInfo->ID, triggered_spell_id);
        return;
    }

    Unit* caster = m_caster;

    if (spellInfo->EquippedItemClass >= 0 &&IsPlayer(m_caster))
    {

        if (spellInfo->AttributesExC & SPELL_ATTR_EX3_MAIN_HAND)
        {
            Item* item = ((Player*)m_caster)->GetWeaponForAttack(BASE_ATTACK, true, false);

            if (!item)
            {
                return;
            }

            if (!item->IsFitToSpellRequirements(spellInfo))
            {
                return;
            }
        }

        if (spellInfo->AttributesExC & SPELL_ATTR_EX3_REQ_OFFHAND)
        {
            Item* item = ((Player*)m_caster)->GetWeaponForAttack(OFF_ATTACK, true, false);

            if (!item)
            {
                return;
            }

            if (!item->IsFitToSpellRequirements(spellInfo))
            {
                return;
            }
        }
    }
    else
    {

        caster = IsSpellWithCasterSourceTargetsOnly(spellInfo) ? unitTarget : m_caster;
    }

    caster->CastSpell(unitTarget, spellInfo, true, m_CastItem, nullptr, m_originalCasterGUID, m_spellInfo);
}

void Spell::EffectTriggerMissileSpell(const cast::Operation& operation)
{
    const SpellEffectIndex effect_idx = SpellEffectIndex(operation.slot);

    uint32 triggered_spell_id = operation.triggerSpell;

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(triggered_spell_id);

    if (!spellInfo)
    {
        if (unitTarget)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell ScriptStart spellid %u in EffectTriggerMissileSpell", m_spellInfo->ID);
            m_caster->GetMap()->Scripts().Start(DBS_ON_SPELL, m_spellInfo->ID, m_caster, unitTarget);
        }
        else
        {
            sLog.outError("EffectTriggerMissileSpell of spell %u (eff: %u): triggering unknown spell id %u",
                m_spellInfo->ID, effect_idx, triggered_spell_id);
        }
        return;
    }

    if (m_CastItem)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: cast Item spellId - %i", spellInfo->ID);
    }

    m_caster->CastSpell(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, spellInfo, true, m_CastItem, nullptr, m_originalCasterGUID, m_spellInfo);
}

void Spell::EffectTeleportUnits(const cast::Operation& operation)
{
    const SpellEffectIndex eff_idx = SpellEffectIndex(operation.slot);

    if (!unitTarget || unitTarget->IsTaxiFlying())
    {
        return;
    }

    uint32 targetType = operation.targetB;
    if (!targetType)
    {
        targetType = operation.targetA;
    }

    switch (targetType)
    {
        case TARGET_INNKEEPER_COORDINATES:
        {

            if (!IsPlayer(unitTarget))
            {
                return;
            }

            ((Player*)unitTarget)->TeleportToHomebind(unitTarget == m_caster ? TELE_TO_SPELL : 0);
            return;
        }
        case TARGET_AREAEFFECT_INSTANT:
        case TARGET_TABLE_X_Y_Z_COORDINATES:
        {
            SpellTargetPosition const* st = sSpellMgr.GetSpellTargetPosition(m_spellInfo->ID);
            if (!st)
            {
                sLog.outError("Spell::EffectTeleportUnits - unknown Teleport coordinates for spell ID %u", m_spellInfo->ID);
                return;
            }

            if (st->target_mapId == unitTarget->GetMapId())
            {
                unitTarget->NearTeleportTo(st->target_X, st->target_Y, st->target_Z, st->target_Orientation, unitTarget == m_caster);
            }
            else if (IsPlayer(unitTarget))
            {
                ((Player*)unitTarget)->TeleportTo(st->target_mapId, st->target_X, st->target_Y, st->target_Z, st->target_Orientation, unitTarget == m_caster ? TELE_TO_SPELL : 0);
            }
            break;
        }
        case TARGET_EFFECT_SELECT:
        {

            float x = unitTarget->Where().X();
            float y = unitTarget->Where().Y();
            float z = unitTarget->Where().Z();
            float orientation = m_caster->Where().Facing();

            m_caster->NearTeleportTo(x, y, z, orientation, unitTarget == m_caster);
            return;
        }
        default:
        {

            if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
            {
                sLog.outError("Spell::EffectTeleportUnits - unknown EffectImplicitTargetB[%u] = %u for spell ID %u", eff_idx, operation.targetB, m_spellInfo->ID);
                return;
            }

            float x = m_targets.m_destX;
            float y = m_targets.m_destY;
            float z = m_targets.m_destZ;
            float orientation = unitTarget->Where().Facing();

            unitTarget->NearTeleportTo(x, y, z, orientation, unitTarget == m_caster);
            return;
        }
    }

    switch (m_spellInfo->ID)
    {

        case 23442:
        {
            int32 r = irand(0, 119);
            if (r >= 70)
            {
                if (r < 100)
                {
                    m_caster->CastSpell(m_caster, 23445, true);
                }
                else
                {
                    m_caster->CastSpell(m_caster, 23449, true);
                }
            }
            return;
        }
    }
}
