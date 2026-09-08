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



#include <cmath>
#include "Reaction.h"
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

/**
 * @brief Builds the spell target lists for each active effect.
 */
void Spell::FillTargetMap()
{
    // TODO: ADD the correct target FILLS!!!!!!

    UnitList tmpUnitLists[MAX_EFFECT_INDEX];                // Stores the temporary Target Lists for each effect
    uint8 effToIndex[MAX_EFFECT_INDEX] = {0, 1, 2};         // Helper array, to link to another tmpUnitList, if the targets for both effects match
    for (const auto& operation : Recipe().Does())
    {
        const uint8 i = operation.slot;

        // targets for TARGET_SCRIPT_COORDINATES (A) and TARGET_SCRIPT
        // for TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT (A) all is checked in Spell::CheckCast and in Spell::CheckItem
        // filled in Spell::CheckCast call
        if (operation.targetA == TARGET_SCRIPT_COORDINATES ||
            operation.targetA == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
            (operation.targetA == TARGET_SCRIPT && operation.targetB != TARGET_SELF) ||
            (operation.targetB == TARGET_SCRIPT && operation.targetA != TARGET_SELF))
        {
            continue;
        }

        // TODO: find a way so this is not needed?
        // for area auras always add caster as target (needed for totems for example)
        if (IsAreaAuraEffect(operation.verb))
        {
            EnrolUnit(m_caster, SpellEffectIndex(i));
        }

        // no double fill for same targets
        for (const auto& earlier : Recipe().Does())
        {
            if (earlier.slot >= i)
            {
                break;
            }

            // Check if same target, but handle i.e. AreaAuras different
            if (operation.targetA == earlier.targetA && operation.targetB == earlier.targetB &&
                !IsAreaAuraEffect(operation.verb) && !IsAreaAuraEffect(earlier.verb))
                // Add further conditions here if required
            {
                effToIndex[i] = earlier.slot;               // effect i has same targeting list as effect j
                break;
            }
        }

        if (effToIndex[i] == i)                             // New target combination
        {
            // TargetA/TargetB dependent from each other, we not switch to full support this dependences
            // but need it support in some know cases
            switch (operation.targetA)
            {
                case TARGET_NONE:
                    switch (operation.targetB)
                    {
                        case TARGET_NONE:
                            if (m_caster->GetObjectGuid().IsPet())
                            {
                                SetTargetMap(operation, TARGET_SELF, tmpUnitLists[i /*==effToIndex[i]*/]);
                            }
                            else
                            {
                                SetTargetMap(operation, TARGET_EFFECT_SELECT, tmpUnitLists[i /*==effToIndex[i]*/]);
                            }
                            break;
                        default:
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_SELF:
                    switch (operation.targetB)
                    {
                        case TARGET_NONE:                   // Fill Target based on A only
                            // Arcane Missiles have strange targeting for auras
                            // Gnomish Death Ray triggered 13280
                            if ((m_spellInfo->SpellClassSet == SPELLFAMILY_MAGE && m_spellInfo->SpellClassMask & UI64LIT(0x00000800)) ||
                                (m_spellInfo->ID == 13280))
                            {
                                if (m_caster->IsPlayer())
                                {
                                    if (Unit* target = ObjectLookup::GetUnit(*m_caster, ((Player*)m_caster)->GetSelectionGuid()))
                                    {
                                        if (!IsFriendly(*m_caster, *target))
                                        {
                                            tmpUnitLists[i /*==effToIndex[i]*/].push_back(target);
                                        }
                                    }
                                }
                            }
                            else
                            {
                                SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            }
                            break;
                        case TARGET_EFFECT_SELECT:
                        case TARGET_SCRIPT:                 // B-target only used with CheckCast here
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        case TARGET_AREAEFFECT_INSTANT:     // use B case that not dependent from from A in fact
                            if ((m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) == 0)
                            {
                                m_targets.setDestination(m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z());
                            }
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        default:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_EFFECT_SELECT:
                    switch (operation.targetB)
                    {
                        case TARGET_NONE:
                        case TARGET_EFFECT_SELECT:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        // dest point setup required
                        case TARGET_AREAEFFECT_INSTANT:
                        case TARGET_AREAEFFECT_CUSTOM:
                        case TARGET_ALL_ENEMY_IN_AREA:
                        case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
                        case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
                        case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
                        case TARGET_AREAEFFECT_GO_AROUND_DEST:
                            // triggered spells get dest point from default target set, ignore it
                            if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) || m_IsTriggeredSpell)
                            {
                                if (Occupant* castObject = GetCastingObject())
                                {
                                    m_targets.setDestination(castObject->Where().X(), castObject->Where().Y(), castObject->Where().Z());
                                }
                            }
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        // target pre-selection required
                        case TARGET_INNKEEPER_COORDINATES:
                        case TARGET_TABLE_X_Y_Z_COORDINATES:
                        case TARGET_CASTER_COORDINATES:
                        case TARGET_SCRIPT_COORDINATES:
                        case TARGET_CURRENT_ENEMY_COORDINATES:
                        case TARGET_DUELVSPLAYER_COORDINATES:
                            // need some target for processing
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        default:
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_CASTER_COORDINATES:
                    switch (operation.targetB)
                    {
                        case TARGET_ALL_ENEMY_IN_AREA:
                            // Note: this hack with search required until GO casting not implemented
                            // environment damage spells already have around enemies targeting but this not help in case nonexistent GO casting support
                            // currently each enemy selected explicitly and self cast damage
                            if (operation.verb == SPELL_EFFECT_ENVIRONMENTAL_DAMAGE)
                            {
                                if (m_targets.getUnitTarget())
                                {
                                    tmpUnitLists[i /*==effToIndex[i]*/].push_back(m_targets.getUnitTarget());
                                }
                            }
                            else
                            {
                                SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                                SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            }
                            break;
                        case TARGET_NONE:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            tmpUnitLists[i /*==effToIndex[i]*/].push_back(m_caster);
                            break;
                        default:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_TABLE_X_Y_Z_COORDINATES:
                    switch (operation.targetB)
                    {
                        case TARGET_NONE:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);

                            // need some target for processing
                            SetTargetMap(operation, TARGET_EFFECT_SELECT, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        case TARGET_AREAEFFECT_INSTANT:     // All 17/7 pairs used for dest teleportation, A processed in effect code
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        default:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_DUELVSPLAYER_COORDINATES:
                    switch (operation.targetB)
                    {
                        case TARGET_NONE:
                        case TARGET_EFFECT_SELECT:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            if (Unit* currentTarget = m_targets.getUnitTarget())
                            {
                                tmpUnitLists[i /*==effToIndex[i]*/].push_back(currentTarget);
                            }
                            break;
                        default:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_SCRIPT:
                    switch (operation.targetB)
                    {
                        case TARGET_SELF:
                            // Fill target based on B only, A is only used with CheckCast here.
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    switch (operation.targetB)
                    {
                        case TARGET_NONE:
                        case TARGET_EFFECT_SELECT:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        case TARGET_SCRIPT_COORDINATES:     // B case filled in CheckCast but we need fill unit list base at A case
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        default:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
            }
        }

        if (m_caster->IsPlayer())
        {
            Player* me = (Player*)m_caster;
            for (UnitList::const_iterator itr = tmpUnitLists[effToIndex[i]].begin(); itr != tmpUnitLists[effToIndex[i]].end(); ++itr)
            {
                Player* targetOwner = (*itr)->GetCharmerOrOwnerPlayerOrPlayerItself();
                if (targetOwner && targetOwner != me && targetOwner->IsPvP() && !me->Duelling().With(targetOwner))
                {
                    me->UpdatePvP(true);
                    me->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
                    break;
                }
            }
        }

        for (UnitList::iterator itr = tmpUnitLists[effToIndex[i]].begin(); itr != tmpUnitLists[effToIndex[i]].end();)
        {
            if (!CheckTarget(*itr, operation))
            {
                itr = tmpUnitLists[effToIndex[i]].erase(itr);
                continue;
            }
            else
            {
                ++itr;
            }
        }

        for (UnitList::const_iterator iunit = tmpUnitLists[effToIndex[i]].begin(); iunit != tmpUnitLists[effToIndex[i]].end(); ++iunit)
        {
            EnrolUnit((*iunit), SpellEffectIndex(i));
        }
    }
}

/**
 * @brief Prepares proc-trigger metadata for the current spell cast.
 */
void Spell::prepareDataForTriggerSystem()
{
    //==========================================================================================
    // Now fill data for trigger system, need know:
    // an spell trigger another or not ( m_canTrigger )
    // Create base triggers flags for Attacker and Victim ( m_procAttacker and  m_procVictim)
    //==========================================================================================
    // Fill flag can spell trigger or not
    // TODO: possible exist spell attribute for this
    m_canTrigger = false;

    if (m_CastItem)
    {
        m_canTrigger = false;                                // Do not trigger from item cast spell
    }
    else if (!m_IsTriggeredSpell)
    {
        m_canTrigger = true;                                 // Normal cast - can trigger
    }
    else if (!m_triggeredByAuraSpell)
    {
        m_canTrigger = true;                                 // Triggered from SPELL_EFFECT_TRIGGER_SPELL - can trigger
    }

    if (!m_canTrigger)                                      // Exceptions (some periodic triggers)
    {
        switch (m_spellInfo->SpellClassSet)
        {
            case SPELLFAMILY_MAGE:
                // Arcane Missiles / Blizzard triggers need do it
                if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000200080)))
                {
                    m_canTrigger = true;
                }
                break;
            case SPELLFAMILY_WARLOCK:
                // For Hellfire Effect / Rain of Fire / Seed of Corruption triggers need do it
                if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000800000000060)))
                {
                    m_canTrigger = true;
                }
                break;
            case SPELLFAMILY_HUNTER:
                // Hunter Explosive Trap Effect/Immolation Trap Effect/Frost Trap Aura/Snake Trap Effect
                if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000200000000014)))
                {
                    m_canTrigger = true;
                }
                break;
            case SPELLFAMILY_PALADIN:
                // For Holy Shock triggers need do it
                if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0001000000200000)))
                {
                    m_canTrigger = true;
                }
                break;
            default:
                break;
        }
    }

    // Get data for type of attack and fill base info for trigger
    switch (m_spellInfo->DefenseType)
    {
        case SPELL_DAMAGE_CLASS_MELEE:
            m_procAttacker = PROC_FLAG_SUCCESSFUL_MELEE_SPELL_HIT;
            if (m_attackType == OFF_ATTACK)
            {
                m_procAttacker |= PROC_FLAG_SUCCESSFUL_OFFHAND_HIT;
            }
            m_procVictim   = PROC_FLAG_TAKEN_MELEE_SPELL_HIT;
            break;
        case SPELL_DAMAGE_CLASS_RANGED:
            // Auto attack
            if (Recipe().Says().autoRepeats)
            {
                m_procAttacker = PROC_FLAG_SUCCESSFUL_RANGED_HIT;
                m_procVictim   = PROC_FLAG_TAKEN_RANGED_HIT;
            }
            else // Ranged spell attack
            {
                m_procAttacker = PROC_FLAG_SUCCESSFUL_RANGED_SPELL_HIT;
                m_procVictim   = PROC_FLAG_TAKEN_RANGED_SPELL_HIT;
            }
            break;
        default:
            if (Recipe().IsPositive())           // Check for positive spell
            {
                m_procAttacker = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL;
                m_procVictim   = PROC_FLAG_TAKEN_POSITIVE_SPELL;
            }
            else if (Recipe().Says().autoRepeats)   // Wands auto attack
            {
                m_procAttacker = PROC_FLAG_SUCCESSFUL_RANGED_HIT;
                m_procVictim   = PROC_FLAG_TAKEN_RANGED_HIT;
            }
            else                                           // Negative spell
            {
                m_procAttacker = PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;
                m_procVictim   = PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT;
            }
            break;
    }

    // some negative spells have positive effects to another or same targets
    // avoid triggering negative hit for only positive targets
    m_negativeEffectMask = 0x0;
    for (const auto& operation : Recipe().Does())
    {
        if (!operation.positive)
        {
            m_negativeEffectMask |= (1 << operation.slot);
        }
    }

    // Hunter traps spells (for Entrapment trigger)
    // Gives your Immolation Trap, Frost Trap, Explosive Trap, and Snake Trap ....
    if (m_spellInfo->SpellClassSet == SPELLFAMILY_HUNTER && m_spellInfo->SpellClassMask & UI64LIT(0x000020000000001C))
    {
        m_procAttacker |= PROC_FLAG_ON_TRAP_ACTIVATION;
    }
}

/**
 * @brief Writes a unit on the roster for one recipe slot.
 *
 * @param pVictim The unit target.
 * @param effIndex The effect index being applied.
 */
void Spell::EnrolUnit(Unit* pVictim, SpellEffectIndex effIndex)
{
    if (Recipe().Does().AtSlot(static_cast<uint8>(effIndex)) == nullptr)
    {
        return;
    }

    // Check for effect immune skip if immuned
    bool immuned = pVictim->IsImmuneToSpellEffect(m_spellInfo, effIndex, pVictim == m_caster);

    ObjectGuid targetGUID = pVictim->GetObjectGuid();

    // a unit already on the roster only gains the slot
    if (cast::UnitTarget* enrolled = m_roster.FindUnit(targetGUID))
    {
        if (!immuned)
        {
            enrolled->slots |= 1 << effIndex;
        }
        return;
    }

    cast::UnitTarget target;
    target.guid = targetGUID;
    target.slots = immuned ? 0 : (1 << effIndex);

    // the verdict is reckoned now, not when the spell arrives
    target.verdict = m_caster->SpellHitResult(pVictim, m_spellInfo, m_canReflect);

    // spell fly from visual cast object
    Occupant* affectiveObject = GetAffectiveCasterObject();

    // Spell have speed (possible inherited from triggering spell) - need calculate incoming time
    float speed = m_spellInfo->Speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->Speed : m_spellInfo->Speed;
    if (speed > 0.0f && affectiveObject && (pVictim != affectiveObject || (m_targets.m_targetMask & (TARGET_FLAG_SOURCE_LOCATION | TARGET_FLAG_DEST_LOCATION))))
    {
        // calculate spell incoming interval
        float dist;                                         // distance to impact
        if (pVictim == affectiveObject)                     // Calculate dist to destination target also for self-cast spells
        {
            if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
            {
                dist = affectiveObject->Where().DistanceTo(Geometry::Vector3(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ));
            }
            else                                            // Must have Source Target
            {
                dist = affectiveObject->Where().DistanceTo(Geometry::Vector3(m_targets.m_srcX, m_targets.m_srcY, m_targets.m_srcZ));
            }
        }
        else                                                // normal unit target, take distance
        {
            dist = affectiveObject->Where().DistanceTo(Geometry::Vector3(pVictim->Where().X(), pVictim->Where().Y(), pVictim->Where().Z()));
        }

        if (dist < 5.0f)
        {
            dist = 5.0f;
        }
        target.arrivesInMs = static_cast<uint64>(floor(dist / speed * 1000.0f));
    }

    // If target reflect spell back to caster
    if (target.verdict == SPELL_MISS_REFLECT)
    {
        // Calculate reflected spell result on caster
        target.reflectedVerdict =  m_caster->SpellHitResult(m_caster, m_spellInfo, m_canReflect);

        if (target.reflectedVerdict == SPELL_MISS_REFLECT)     // Impossible reflect again, so simply deflect spell
        {
            target.reflectedVerdict = SPELL_MISS_PARRY;
        }

        // Increase time interval for reflected spells by 1.5
        target.arrivesInMs += target.arrivesInMs >> 1;
    }
    else
    {
        target.reflectedVerdict = SPELL_MISS_NONE;
    }

    m_roster.Enrol(target);
}

/**
 * @brief Resolves and adds a unit target by guid for a spell effect.
 *
 * @param unitGuid The unit guid to resolve.
 * @param effIndex The effect index being applied.
 */
void Spell::EnrolUnit(ObjectGuid unitGuid, SpellEffectIndex effIndex)
{
    if (Unit* unit = m_caster->GetObjectGuid() == unitGuid ? m_caster : ObjectLookup::GetUnit(*m_caster, unitGuid))
    {
        EnrolUnit(unit, effIndex);
    }
}

/**
 * @brief Adds a game object target entry for a spell effect.
 *
 * @param pVictim The game object target.
 * @param effIndex The effect index being applied.
 */
void Spell::EnrolObject(GameObject* pVictim, SpellEffectIndex effIndex)
{
    if (Recipe().Does().AtSlot(static_cast<uint8>(effIndex)) == nullptr)
    {
        return;
    }

    ObjectGuid targetGUID = pVictim->GetObjectGuid();

    // a gameobject already on the roster only gains the slot
    if (cast::ObjectTarget* enrolled = m_roster.FindObject(targetGUID))
    {
        enrolled->slots |= 1 << effIndex;
        return;
    }

    cast::ObjectTarget target;
    target.guid = targetGUID;
    target.slots = (1 << effIndex);

    // spell fly from visual cast object
    Occupant* affectiveObject = GetAffectiveCasterObject();

    // Spell can have speed - need calculate incoming time
    float speed = m_spellInfo->Speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->Speed : m_spellInfo->Speed;
    if (speed > 0.0f && affectiveObject && pVictim != affectiveObject)
    {
        // calculate spell incoming interval
        float dist = affectiveObject->Where().DistanceTo(Geometry::Vector3(pVictim->Where().X(), pVictim->Where().Y(), pVictim->Where().Z()));
        if (dist < 5.0f)
        {
            dist = 5.0f;
        }
        target.arrivesInMs = static_cast<uint64>(floor(dist / speed * 1000.0f));
    }

    m_roster.Enrol(target);
}

/**
 * @brief Resolves and adds a game object target by guid for a spell effect.
 *
 * @param goGuid The game object guid to resolve.
 * @param effIndex The effect index being applied.
 */
void Spell::EnrolObject(ObjectGuid goGuid, SpellEffectIndex effIndex)
{
    if (GameObject* go = m_caster->GetMap()->GetGameObject(goGuid))
    {
        EnrolObject(go, effIndex);
    }
}

/**
 * @brief Adds an item target entry for a spell effect.
 *
 * @param pitem The item target.
 * @param effIndex The effect index being applied.
 */
void Spell::EnrolItem(Item* pitem, SpellEffectIndex effIndex)
{
    if (Recipe().Does().AtSlot(static_cast<uint8>(effIndex)) == nullptr)
    {
        return;
    }

    // an item already on the roster only gains the slot
    if (cast::ItemTarget* enrolled = m_roster.FindItem(pitem))
    {
        enrolled->slots |= 1 << effIndex;
        return;
    }

    cast::ItemTarget target;
    target.item = pitem;
    target.slots = (1 << effIndex);
    m_roster.Enrol(target);
}
