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

/**
 * @file Spell.cpp
 * @brief Spell casting and effect implementation
 *
 * This file implements the Spell class which handles spell casting:
 * - Spell validation and casting requirements
 * - Spell effect execution (damage, healing, summon, etc.)
 * - Spell targeting and area effects
 * - Spell cooldowns and resource costs
 * - Spell interruption and pushback
 * - Spell aura application
 * - Spell hit/miss calculations
 *
 * Spells are the primary combat mechanic in WoW, encompassing
 * abilities, talents, and item effects.
 *
 * @see Spell for the spell class
 * @see SpellAura for spell auras
 * @see SpellMgr for spell management
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

/**
 * @brief Checks whether a target matches the spell's creature type restrictions.
 *
 * @param target The target being validated.
 * @return True if the target type is allowed; otherwise, false.
 */
bool Spell::CheckTargetCreatureType(Unit* target) const
{
    uint32 spellCreatureTargetMask = m_spellInfo->TargetCreatureType;

    // Curse of Doom : not find another way to fix spell target check :/
    if (m_spellInfo->ID == 603)                             // in 1.12 "Curse of doom" have only 1 rank.
    {
        // not allow cast at player
        if (target->IsPlayer())
        {
            return false;
        }

        spellCreatureTargetMask = 0x7FF;
    }

    // Dismiss Pet and Taming Lesson skipped
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

/**
 * @brief Gets the current spell container slot used by this spell.
 *
 * @return The current spell container type.
 */
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

/**
 * @brief Validates whether a candidate target is acceptable for a specific effect.
 *
 * @param target The target being checked.
 * @param eff The effect index being validated.
 * @return True if the target is valid for the effect; otherwise, false.
 */
bool Spell::CheckTarget(Unit* target, const cast::Operation& operation)
{
    const SpellEffectIndex eff = SpellEffectIndex(operation.slot);

    // Check targets for creature type mask and remove not appropriate (skip explicit self target case, maybe need other explicit targets)
    if (operation.targetA != TARGET_SELF)
    {
        if (!CheckTargetCreatureType(target))
        {
            return false;
        }
    }

    // Check targets for not_selectable unit flag and remove
    // A player can cast spells on his pet (or other controlled unit) though in any state
    if (target != m_caster && target->GetCharmerOrOwnerGuid() != m_caster->GetObjectGuid())
    {
        // any unattackable target skipped
        if (target->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
        {
            return false;
        }

        // unselectable targets skipped in all cases except TARGET_SCRIPT targeting
        // in case TARGET_SCRIPT target selected by server always and can't be cheated
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

    // Check player targets and remove if in GM mode or GM invisibility (for not self casting case)
    if (target != m_caster && target->IsPlayer())
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

    // Check targets for LOS visibility (except spells without range limitations )
    if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->ID, nullptr, SPELL_DISABLE_LOS))
    {
        switch (operation.verb)
        {
            case SPELL_EFFECT_SUMMON_PLAYER:                    // from anywhere
                break;
            case SPELL_EFFECT_DUMMY:
                if (m_spellInfo->ID != 20577)                   // Cannibalize
                {
                    break;
                }
                // fall through
            case SPELL_EFFECT_RESURRECT_NEW:
                // player far away, maybe his corpse near?
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

                // all ok by some way or another, skip normal check
                break;
            default:                                            // normal case
            {
                // Get GO cast coordinates if original caster -> GO
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

    if (!target->IsPlayer() && Recipe().Says().playersOnly &&
        operation.targetA != TARGET_SCRIPT && operation.targetA != TARGET_SELF)
    {
        return false;
    }

    return true;
}

/**
 * @brief Asks whether the target the caster picked can be cast on.
 *
 * The one target named with the cast, as against the many a target list
 * later collects: line of sight, facing, level, creature type, whether he is
 * duelling, in a vehicle, already charmed, and the handful of spells that
 * name their own conditions.
 *
 * @return The reason the cast is refused, or SPELL_CAST_OK when there is no
 *         chosen target or it is a fair one.
 */
SpellCastResult Spell::CheckTheTargetChosen(bool strict)
{
    if (Unit* target = m_targets.getUnitTarget())
    {
        //Soothe animal
        bool foundSootheAnimal = true; //will be set to false in the default case
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
        //Perhaps this should be done for all spells?
        if (foundSootheAnimal)
        {
            if (target->getLevel() > m_spellInfo->MaxTargetLevel)
            {
                return SPELL_FAILED_HIGHLEVEL;
            }
        }

        // Swiftmend
        if (m_spellInfo->ID == 18562)                       // future versions have special aura state for this
        {
            if (!target->GetAura(SPELL_AURA_PERIODIC_HEAL, SPELLFAMILY_DRUID, UI64LIT(0x50)))
            {
                return SPELL_FAILED_TARGET_AURASTATE;
            }
        }

        // Tame Beast trigger
        if (m_spellInfo->ID == 1515)
        {
            SpellCastResult castResult = CanTameUnit(false);
            if (castResult != SPELL_CAST_OK)
            {
                return castResult;
            }
        }

        // give error message when applying lower hot rank to higher hot rank on target
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

        if (!(m_spellInfo->SpellClassSet == SPELLFAMILY_WARRIOR && m_spellInfo->SpellClassMask & UI64LIT(0x100000000))) // the Shield Slam does not depend on its dispel effect
        {
            // Fill possible dispel list
            bool isDispell = false;
            bool isEmpty = true;

            // As of Patch 1.10.0, dispel effects now check if there is something to dispel first
            for (const auto& operation : Recipe().Does())
            {
                // Dispell Magic
                switch (operation.verb)
                {
                    case SPELL_EFFECT_DISPEL:
                    {
                        // It is a dispell spell
                        isDispell = true;

                        // Create dispel mask by dispel type
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

                                    // do not remove positive auras if friendly target
                                    //               negative auras if non-friendly target
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

            // Ok if exist some buffs for dispel try dispel it
            if (isDispell && isEmpty)
            {
                return SPELL_FAILED_NOTHING_TO_DISPEL;
            }
        }

        if (!m_IsTriggeredSpell && IsDeathOnlySpell(m_spellInfo) && target->IsAlive())
        {
            return SPELL_FAILED_TARGET_NOT_DEAD;
        }

        // totem immunity for channeled spells(needs to be before spell cast)
        // spell attribs for player channeled spells
        if (Recipe().Says().channelTracksTarget &&
            target->IsCreature() &&
            ((Creature*)target)->IsTotem())
        {
            return SPELL_FAILED_IMMUNE;
        }

        // Power Infusion: As of patch 1.10, this is no longer usable if the target
        // has Arcane Power aura from mage.
        if (m_spellInfo->ID == 10060)    // 10060 = Power Infusion
        {
            if (target->HasAura(12042))    // 12042 = Arcane Power
            {
                return SPELL_FAILED_MORE_POWERFUL_SPELL_ACTIVE;
            }
        }

        bool non_caster_target = target != m_caster && !IsSpellWithCasterSourceTargetsOnly(m_spellInfo);

        if (non_caster_target)
        {
            // Not allow casting on flying player
            if (target->IsTaxiFlying())
            {
                return SPELL_FAILED_BAD_TARGETS;
            }

            if (!m_IsTriggeredSpell && !DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->ID, nullptr, SPELL_DISABLE_LOS) && !LineOfSightExemptions::Has(m_spellInfo->ID) && !HasLineOfSight(*m_caster, *target))
            {
                return SPELL_FAILED_LINE_OF_SIGHT;
            }

            // auto selection spell rank implemented in WorldSession::HandleCastSpellOpcode
            // this case can be triggered if rank not found (too low-level target for first rank)
            if (m_caster->IsPlayer() && !m_CastItem && !m_IsTriggeredSpell)
            {
                // spell expected to be auto-downranking in cast handle, so must be same
                if (m_spellInfo != sSpellMgr.SelectAuraRankForLevel(m_spellInfo, target->getLevel()))
                {
                    return SPELL_FAILED_LOWLEVEL;
                }
            }

            if (strict && Recipe().Says().playersOnly && !target->IsPlayer() && !IsAreaOfEffectSpell(m_spellInfo))
            {
                return SPELL_FAILED_BAD_TARGETS;
            }
        }
        else if (m_caster == target)
        {
            if (m_caster->IsPlayer() && m_caster->IsInWorld())
            {
                // Additional check for some spells
                // If 0 spell effect empty - client not send target data (need use selection)
                // TODO: check it on next client version
                if (m_targets.m_targetMask == TARGET_FLAG_SELF &&
                    Recipe().At(EFFECT_INDEX_1).targetA == TARGET_CHAIN_DAMAGE)
                {
                    target = m_caster->GetMap()->GetUnit(((Player*)m_caster)->GetSelectionGuid());
                    if (!target)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    // Arcane Missile self cast forbidden
                    if (m_spellInfo->SpellClassSet == SPELLFAMILY_MAGE &&
                        m_spellInfo->SpellClassMask & UI64LIT(0x00000800) &&
                        m_caster == target)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    // Gnomish Death Ray self cast forbidden
                    if (m_spellInfo->ID == 13278 && m_caster == target)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    m_targets.setUnitTarget(target);
                }
            }

            // Some special spells with non-caster only mode

            // Fire Shield
            if (m_spellInfo->SpellClassSet == SPELLFAMILY_WARLOCK &&
                m_spellInfo->SpellIconID == 16)
            {
                return SPELL_FAILED_BAD_TARGETS;
            }
        }

        // check pet presents
        for (const auto& operation : Recipe().Does())
        {
            if (operation.targetA == TARGET_PET)
            {
                Pet* pet = m_caster->GetPet();
                if (!pet)
                {
                    if (m_triggeredByAuraSpell)             // not report pet not existence for triggered spells
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

        // check creature type
        // ignore self casts (including area casts when caster selected as target)
        if (non_caster_target)
        {
            if (!CheckTargetCreatureType(target))
            {
                if (target->IsPlayer())
                {
                    return SPELL_FAILED_TARGET_IS_PLAYER;
                }
                else
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
            }

            // simple cases
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
            // check target for pet/charmed casts (not self targeted), self targeted cast used for area effects and etc
            if (!explicit_target_mode && m_caster->IsCreature() && m_caster->GetCharmerOrOwnerGuid())
            {
                // check correctness positive/negative cast target (pet cast real check and cheating check)
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

        // Must be behind the target.
        if (m_spellInfo->AttributesExB == SPELL_ATTR_EX2_FACING_TARGETS_BACK && (Recipe().Says().needsFacing && target->Where().HasInArc(m_caster->Where(), M_PI_F)))
        {
            SendInterrupted(SPELL_FAILED_NOT_BEHIND);
            return SPELL_FAILED_NOT_BEHIND;
        }

        // Target must be facing you.
        if ((m_spellInfo->Attributes == (SPELL_ATTR_ABILITY | SPELL_ATTR_NOT_SHAPESHIFT | SPELL_ATTR_DONT_AFFECT_SHEATH_STATE | SPELL_ATTR_STOP_ATTACK_TARGET)) && !target->Where().HasInArc(m_caster->Where(), M_PI_F))
        {
            SendInterrupted(SPELL_FAILED_NOT_INFRONT);
            return SPELL_FAILED_NOT_INFRONT;
        }

        // check if target is in combat
        if (non_caster_target && Recipe().Says().needsTargetOutOfCombat && target->IsInCombat())
        {
            return SPELL_FAILED_TARGET_AFFECTING_COMBAT;
        }

        // check if target is affected by Spirit of Redemption (Aura: 27827)
        if (target->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        {
            return SPELL_FAILED_BAD_TARGETS;
        }

    }

    return SPELL_CAST_OK;
}
