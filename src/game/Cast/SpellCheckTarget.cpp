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
#include "LineOfSightExemptions.h"
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
#include "Corpse.h"
#include "Cast/Recipe/RecipeBook.h"

bool Spell::CheckTargetCreatureType(Unit* target) const
{
    uint32 spellCreatureTargetMask = m_spellInfo->TargetCreatureType;

    if (m_spellInfo->ID == 603)
    {

        if (IsPlayer(target))
        {
            return false;
        }

        spellCreatureTargetMask = 0x7FF;
    }

    if (m_spellInfo->ID == 2641 || m_spellInfo->ID == 23356)
    {
        spellCreatureTargetMask =  0;
    }

    if (spellCreatureTargetMask)
    {
        uint32 TargetCreatureType = target->GetCreatureTypeMask();

        return !TargetCreatureType || (spellCreatureTargetMask & TargetCreatureType);
    }
    return true;
}

CurrentSpellTypes Spell::GetCurrentContainer()
{
    if (IsNextMeleeSwingSpell())
    {
        return (CURRENT_MELEE_SPELL);
    }
    else if (IsAutoRepeat())
    {
        return (CURRENT_AUTOREPEAT_SPELL);
    }
    else if (Recipe().Starts() == cast::Start::Channelled)
    {
        return (CURRENT_CHANNELED_SPELL);
    }
    else
    {
        return (CURRENT_GENERIC_SPELL);
    }
}

bool Spell::CheckTarget(Unit* target, const cast::Operation& operation)
{
    const SpellEffectIndex eff = SpellEffectIndex(operation.slot);

    if (operation.targetA != TARGET_SELF)
    {
        if (!CheckTargetCreatureType(target))
        {
            return false;
        }
    }

    if (target != m_caster && target->GetCharmerOrOwnerGuid() != m_caster->GetObjectGuid())
    {

        if (target->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
        {
            return false;
        }

        if ((!m_IsTriggeredSpell || target != m_targets.getUnitTarget()) &&
            target->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) &&
            operation.targetA != TARGET_SCRIPT &&
            operation.targetB != TARGET_SCRIPT &&
            operation.targetA != TARGET_AREAEFFECT_INSTANT &&
            operation.targetB != TARGET_AREAEFFECT_INSTANT &&
            operation.targetA != TARGET_AREAEFFECT_CUSTOM &&
            operation.targetB != TARGET_AREAEFFECT_CUSTOM &&
            operation.targetA != TARGET_NARROW_FRONTAL_CONE &&
            operation.targetB != TARGET_NARROW_FRONTAL_CONE)
        {
            return false;
        }
    }

    if (target != m_caster &&IsPlayer(target))
    {
        if (((Player*)target)->GetVisibility() == VISIBILITY_OFF)
        {
            return false;
        }

        if (((Player*)target)->isGameMaster() && !Recipe().IsPositive())
        {
            return false;
        }
    }

    if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->ID, nullptr, SPELL_DISABLE_LOS))
    {
        switch (operation.verb)
        {
            case SPELL_EFFECT_SUMMON_PLAYER:
                break;
            case SPELL_EFFECT_DUMMY:
                if (m_spellInfo->ID != 20577)
                {
                    break;
                }

            case SPELL_EFFECT_RESURRECT_NEW:

                if (target != m_caster && !HasLineOfSight(*target, *m_caster))
                {
                    if (!m_targets.getCorpseTargetGuid())
                    {
                        return false;
                    }

                    Corpse* corpse = m_caster->GetMap()->GetCorpse(m_targets.getCorpseTargetGuid());
                    if (!corpse)
                    {
                        return false;
                    }

                    if (target->GetObjectGuid() != corpse->GetOwnerGuid())
                    {
                        return false;
                    }

                    if (!HasLineOfSight(*corpse, *m_caster))
                    {
                        return false;
                    }
                }

                break;
            default:
            {

                if (target != m_caster)
                {
                    if (Occupant* caster = GetCastingObject())
                    {
                        if (!HasLineOfSight(*target, *caster))
                        {
                            return false;
                        }
                    }
                }
                break;
            }
        }
    }

    if (!IsPlayer(target) && Recipe().Says().playersOnly &&
        operation.targetA != TARGET_SCRIPT && operation.targetA != TARGET_SELF)
    {
        return false;
    }

    return true;
}

SpellCastResult Spell::CheckTheTargetChosen(bool strict)
{
    if (Unit* target = m_targets.getUnitTarget())
    {

        bool foundSootheAnimal = true;
        switch (m_spellInfo->ID)
        {
            case 9901:
            case 8955:
            case 2908:
                break;
            default:
                foundSootheAnimal = false;
                break;
        }

        if (foundSootheAnimal)
        {
            if (target->getLevel() > m_spellInfo->MaxTargetLevel)
            {
                return SPELL_FAILED_HIGHLEVEL;
            }
        }

        if (m_spellInfo->ID == 18562)
        {
            if (!target->GetAura(SPELL_AURA_PERIODIC_HEAL, SPELLFAMILY_DRUID, UI64LIT(0x50)))
            {
                return SPELL_FAILED_TARGET_AURASTATE;
            }
        }

        if (m_spellInfo->ID == 1515)
        {
            SpellCastResult castResult = CanTameUnit(false);
            if (castResult != SPELL_CAST_OK)
            {
                return castResult;
            }
        }

        if (!m_spellInfo->HasSpellEffect(SPELL_EFFECT_HEAL) && IsSpellHaveAura(m_spellInfo, SPELL_AURA_PERIODIC_HEAL))
        {
            const auto mPeriodicHeal = target->GetAurasByType(SPELL_AURA_PERIODIC_HEAL);
            for (auto* aura : mPeriodicHeal)
            {
                if (aura->GetSpellProto()->SpellClassSet == m_spellInfo->SpellClassSet)
                {
                    if (m_spellInfo->IsFitToFamilyMask(aura->GetSpellProto()->SpellClassMask))
                    {
                        if (CompareAuraRanks(m_spellInfo->ID, aura->GetSpellProto()->ID) < 0)
                        {
                            return SPELL_FAILED_MORE_POWERFUL_SPELL_ACTIVE;
                        }
                    }
                }
            }
        }

        if (!(m_spellInfo->SpellClassSet == SPELLFAMILY_WARRIOR && m_spellInfo->SpellClassMask & UI64LIT(0x100000000)))
        {

            bool isDispell = false;
            bool isEmpty = true;

            for (const auto& operation : Recipe().Does())
            {

                switch (operation.verb)
                {
                    case SPELL_EFFECT_DISPEL:
                    {

                        isDispell = true;

                        uint32 dispel_type = operation.miscValue;
                        uint32 dispelMask = GetDispellMask(DispelType(dispel_type));
                        Unit::SpellAuraHolderMap const& auras = target->GetSpellAuraHolderMap();
                        for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
                        {
                            SpellAuraHolder* holder = itr->second;
                            uint32 disp = (1 << holder->GetSpellProto()->DispelType);
                            if (disp & dispelMask)
                            {
                                if (holder->GetSpellProto()->DispelType == DISPEL_MAGIC)
                                {
                                    bool positive = true;
                                    if (!holder->IsPositive())
                                    {
                                        positive = false;
                                    }
                                    else
                                    {
                                        positive = (holder->GetSpellProto()->AttributesEx & SPELL_ATTR_EX_CANT_BE_REFLECTED) == 0;
                                    }

                                    if (positive == IsFriendly(*target, *m_caster))
                                    {
                                        continue;
                                    }
                                }

                                isEmpty = false;
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            if (isDispell && isEmpty)
            {
                return SPELL_FAILED_NOTHING_TO_DISPEL;
            }
        }

        if (!m_IsTriggeredSpell && IsDeathOnlySpell(m_spellInfo) && target->IsAlive())
        {
            return SPELL_FAILED_TARGET_NOT_DEAD;
        }

        if (Recipe().Says().channelTracksTarget &&IsCreature(target) &&
            ((Creature*)target)->IsTotem())
        {
            return SPELL_FAILED_IMMUNE;
        }

        if (m_spellInfo->ID == 10060)
        {
            if (target->HasAura(12042))
            {
                return SPELL_FAILED_MORE_POWERFUL_SPELL_ACTIVE;
            }
        }

        bool non_caster_target = target != m_caster && !IsSpellWithCasterSourceTargetsOnly(m_spellInfo);

        if (non_caster_target)
        {

            if (target->IsTaxiFlying())
            {
                return SPELL_FAILED_BAD_TARGETS;
            }

            if (!m_IsTriggeredSpell && !DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->ID, nullptr, SPELL_DISABLE_LOS) && !LineOfSightExemptions::Has(m_spellInfo->ID) && !HasLineOfSight(*m_caster, *target))
            {
                return SPELL_FAILED_LINE_OF_SIGHT;
            }

            if (IsPlayer(m_caster) && !m_CastItem && !m_IsTriggeredSpell)
            {

                if (m_spellInfo != sSpellMgr.SelectAuraRankForLevel(m_spellInfo, target->getLevel()))
                {
                    return SPELL_FAILED_LOWLEVEL;
                }
            }

            if (strict && Recipe().Says().playersOnly && !IsPlayer(target) && !IsAreaOfEffectSpell(m_spellInfo))
            {
                return SPELL_FAILED_BAD_TARGETS;
            }
        }
        else if (m_caster == target)
        {
            if (IsPlayer(m_caster) && m_caster->IsInWorld())
            {

                if (m_targets.m_targetMask == TARGET_FLAG_SELF &&
                    Recipe().At(EFFECT_INDEX_1).targetA == TARGET_CHAIN_DAMAGE)
                {
                    target = m_caster->GetMap()->GetUnit(((Player*)m_caster)->GetSelectionGuid());
                    if (!target)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    if (m_spellInfo->SpellClassSet == SPELLFAMILY_MAGE &&
                        m_spellInfo->SpellClassMask & UI64LIT(0x00000800) &&
                        m_caster == target)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    if (m_spellInfo->ID == 13278 && m_caster == target)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    m_targets.setUnitTarget(target);
                }
            }

            if (m_spellInfo->SpellClassSet == SPELLFAMILY_WARLOCK &&
                m_spellInfo->SpellIconID == 16)
            {
                return SPELL_FAILED_BAD_TARGETS;
            }
        }

        for (const auto& operation : Recipe().Does())
        {
            if (operation.targetA == TARGET_PET)
            {
                Pet* pet = m_caster->GetPet();
                if (!pet)
                {
                    if (m_triggeredByAuraSpell)
                    {
                        return SPELL_FAILED_DONT_REPORT;
                    }
                    else
                    {
                        return SPELL_FAILED_NO_PET;
                    }
                }
                else if (!pet->IsAlive())
                {
                    return SPELL_FAILED_TARGETS_DEAD;
                }
                break;
            }
        }

        if (non_caster_target)
        {
            if (!CheckTargetCreatureType(target))
            {
                if (IsPlayer(target))
                {
                    return SPELL_FAILED_TARGET_IS_PLAYER;
                }
                else
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
            }

            bool explicit_target_mode = false;
            bool target_hostile = false;
            bool target_hostile_checked = false;
            bool target_friendly = false;
            bool target_friendly_checked = false;
            for (const auto& operation : Recipe().Does())
            {
                if (IsExplicitPositiveTarget(operation.targetA))
                {
                    if (!target_hostile_checked)
                    {
                        target_hostile_checked = true;
                        target_hostile = IsHostile(*m_caster, *target);
                    }

                    if (target_hostile)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    explicit_target_mode = true;
                }
                else if (IsExplicitNegativeTarget(operation.targetA))
                {
                    if (!target_friendly_checked)
                    {
                        target_friendly_checked = true;
                        target_friendly = IsFriendly(*m_caster, *target);
                    }

                    if (target_friendly)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    explicit_target_mode = true;
                }
            }

            if (!explicit_target_mode &&IsCreature(m_caster) && m_caster->GetCharmerOrOwnerGuid())
            {

                if (Recipe().IsPositive())
                {
                    if (!target_hostile_checked)
                    {
                        target_hostile = IsHostile(*m_caster, *target);
                    }

                    if (target_hostile)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }
                else
                {
                    if (!target_friendly_checked)
                    {
                        target_friendly = IsFriendly(*m_caster, *target);
                    }

                    if (target_friendly)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }
            }
        }

        if (Recipe().IsPositive())
        {
            if (target->IsImmuneToSpell(m_spellInfo, target == m_caster))
            {
                return SPELL_FAILED_TARGET_AURASTATE;
            }
        }

        if (m_spellInfo->AttributesExB == SPELL_ATTR_EX2_FACING_TARGETS_BACK && (Recipe().Says().needsFacing && target->Where().HasInArc(m_caster->Where(), M_PI_F)))
        {
            SendInterrupted(SPELL_FAILED_NOT_BEHIND);
            return SPELL_FAILED_NOT_BEHIND;
        }

        if ((m_spellInfo->Attributes == (SPELL_ATTR_ABILITY | SPELL_ATTR_NOT_SHAPESHIFT | SPELL_ATTR_DONT_AFFECT_SHEATH_STATE | SPELL_ATTR_STOP_ATTACK_TARGET)) && !target->Where().HasInArc(m_caster->Where(), M_PI_F))
        {
            SendInterrupted(SPELL_FAILED_NOT_INFRONT);
            return SPELL_FAILED_NOT_INFRONT;
        }

        if (non_caster_target && Recipe().Says().needsTargetOutOfCombat && target->IsInCombat())
        {
            return SPELL_FAILED_TARGET_AFFECTING_COMBAT;
        }

        if (target->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        {
            return SPELL_FAILED_BAD_TARGETS;
        }

    }

    return SPELL_CAST_OK;
}
