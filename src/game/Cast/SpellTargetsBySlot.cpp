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
 * @file SpellTargetsBySlot.cpp
 * @brief The targets a slot picks for itself.
 *
 * A row can say that a slot's target is whatever the slot's own verb
 * implies, and then the answer depends on the verb, and for a good many
 * spells on the spell. This is that answer.
 */

#include <algorithm>
#include <iterator>
#include <list>
#include "Reaction.h"
#include "Utilities/MathDefines.h"
#include "Spell.h"
#include "Cast/Targets/Trim.h"
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
#include "PlayerRegistry.h"
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
 * @brief Finds the nearest thing a corpse search accepts.
 *
 * @tparam T The corpse search predicate type.
 * @return The first match, or null when there is none within range.
 */
template<typename T>
Occupant* Spell::FindCorpseUsing()
{
    const float max_range = Recipe().Takes().rangeMax;

    Occupant* result = nullptr;

    T u_check(m_caster, max_range);
    MaNGOS::OccupantSearcher<T> searcher(result, u_check);

    Cell::VisitGridObjects(m_caster, searcher, max_range);

    if (!result)
    {
        Cell::VisitWorldObjects(m_caster, searcher, max_range);
    }

    return result;
}

/**
 * @brief Picks the targets a slot's own verb implies.
 *
 * @param operation      The slot being filled.
 * @param targetUnitMap  The list the picked units are added to.
 */
void Spell::PickWhatTheSlotImplies(const cast::Operation& operation, UnitList& targetUnitMap)
{
    const SpellEffectIndex effIndex = SpellEffectIndex(operation.slot);

        // add here custom effects that need default target.
        // FOR EVERY TARGET TYPE THERE IS A DIFFERENT FILL!!
        switch (operation.verb)
        {
            case SPELL_EFFECT_DUMMY:
            {
                switch (m_spellInfo->ID)
                {
                    case 20577:                         // Cannibalize
                    {
                        Occupant* result = FindCorpseUsing<MaNGOS::CannibalizeObjectCheck> ();

                        if (result)
                        {
                            switch (result->GetTypeId())
                            {
                                case TYPEID_UNIT:
                                case TYPEID_PLAYER:
                                    targetUnitMap.push_back((Unit*)result);
                                    break;
                                case TYPEID_CORPSE:
                                    m_targets.setCorpseTarget((Corpse*)result);
                                    if (Player* owner = sPlayerRegistry.Find(((Corpse*)result)->GetOwnerGuid()))
                                    {
                                        targetUnitMap.push_back(owner);
                                    }
                                    break;
                            }
                        }
                        else
                        {
                            // clear cooldown at fail
                            if (m_caster->IsPlayer())
                            {
                                ((Player*)m_caster)->RemoveSpellCooldown(m_spellInfo->ID, true);
                            }
                            SendCastResult(SPELL_FAILED_NO_EDIBLE_CORPSES);
                            finish(false);
                        }
                        break;
                    }
                    default:
                        if (m_targets.getUnitTarget())
                        {
                            targetUnitMap.push_back(m_targets.getUnitTarget());
                        }
                        break;
                }
                // Add AoE target-mask to self, if no target-dest provided already
                if ((m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) == 0)
                {
                    m_targets.setDestination(m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z());
                }
                break;
            }
            case SPELL_EFFECT_BIND:
            case SPELL_EFFECT_RESURRECT:
            case SPELL_EFFECT_PARRY:
            case SPELL_EFFECT_BLOCK:
            case SPELL_EFFECT_CREATE_ITEM:
            case SPELL_EFFECT_WEAPON:
            case SPELL_EFFECT_TRIGGER_SPELL:
            case SPELL_EFFECT_TRIGGER_MISSILE:
            case SPELL_EFFECT_LEARN_SPELL:
            case SPELL_EFFECT_SKILL_STEP:
            case SPELL_EFFECT_PROFICIENCY:
            case SPELL_EFFECT_SUMMON_POSSESSED:
            case SPELL_EFFECT_SUMMON_OBJECT_WILD:
            case SPELL_EFFECT_SELF_RESURRECT:
            case SPELL_EFFECT_REPUTATION:
            case SPELL_EFFECT_ADD_HONOR:
            case SPELL_EFFECT_SEND_TAXI:
                if (m_targets.getUnitTarget())
                {
                    targetUnitMap.push_back(m_targets.getUnitTarget());
                }
                // Triggered spells have additional spell targets - cast them even if no explicit unit target is given (required for spell 50516 for example)
                else if (operation.verb == SPELL_EFFECT_TRIGGER_SPELL)
                {
                    targetUnitMap.push_back(m_caster);
                }
                break;
            case SPELL_EFFECT_SUMMON_PLAYER:
                if (m_caster->IsPlayer() && ((Player*)m_caster)->GetSelectionGuid())
                {
                    if (Player* target = sObjectMgr.GetPlayer(((Player*)m_caster)->GetSelectionGuid()))
                    {
                        targetUnitMap.push_back(target);
                    }
                }
                break;
            case SPELL_EFFECT_RESURRECT_NEW:
                if (m_targets.getUnitTarget())
                {
                    targetUnitMap.push_back(m_targets.getUnitTarget());
                }
                if (m_targets.getCorpseTargetGuid())
                {
                    if (Corpse* corpse = m_caster->GetMap()->GetCorpse(m_targets.getCorpseTargetGuid()))
                    {
                        if (Player* owner = sPlayerRegistry.Find(corpse->GetOwnerGuid()))
                        {
                            targetUnitMap.push_back(owner);
                        }
                    }
                }
                break;
            case SPELL_EFFECT_TELEPORT_UNITS:
            case SPELL_EFFECT_SUMMON:
                /** [-ZERO]  if (m_spellInfo->EffectMiscValueB[effIndex] == SUMMON_TYPE_POSESSED ||
                 *              m_spellInfo->EffectMiscValueB[effIndex] == SUMMON_TYPE_POSESSED2)
                 *              {
                 *                  if (m_targets.getUnitTarget())
                 *                  {
                 *                      targetUnitMap.push_back(m_targets.getUnitTarget());
                 *                  }
                 *              }
                 *              else
                 */
                {
                    targetUnitMap.push_back(m_caster);
                }
                break;
            case SPELL_EFFECT_SUMMON_CHANGE_ITEM:
            case SPELL_EFFECT_SUMMON_WILD:
            case SPELL_EFFECT_SUMMON_GUARDIAN:
            case SPELL_EFFECT_TRANS_DOOR:
            case SPELL_EFFECT_ADD_FARSIGHT:
            case SPELL_EFFECT_STUCK:
            case SPELL_EFFECT_DESTROY_ALL_TOTEMS:
            case SPELL_EFFECT_SUMMON_DEMON:
            case SPELL_EFFECT_SKILL:
                targetUnitMap.push_back(m_caster);
                break;
            case SPELL_EFFECT_PERSISTENT_AREA_AURA:
                if (Unit* currentTarget = m_targets.getUnitTarget())
                {
                    m_targets.setDestination(currentTarget->Where().X(), currentTarget->Where().Y(), currentTarget->Where().Z());
                }
                break;
            case SPELL_EFFECT_LEARN_PET_SPELL:
                if (Pet* pet = m_caster->GetPet())
                {
                    targetUnitMap.push_back(pet);
                }
                break;
            case SPELL_EFFECT_ENCHANT_ITEM:
            case SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY:
            case SPELL_EFFECT_DISENCHANT:
            case SPELL_EFFECT_FEED_PET:
                if (m_targets.getItemTarget())
                {
                    EnrolItem(m_targets.getItemTarget(), effIndex);
                }
                break;
            case SPELL_EFFECT_APPLY_AURA:
                switch (operation.aura)
                {
                    case SPELL_AURA_ADD_FLAT_MODIFIER:  // some spell mods auras have 0 target modes instead expected TARGET_SELF(1) (and present for other ranks for same spell for example)
                    case SPELL_AURA_ADD_PCT_MODIFIER:
                        targetUnitMap.push_back(m_caster);
                        break;
                    default:                            // apply to target in other case
                        if (m_targets.getUnitTarget())
                        {
                            targetUnitMap.push_back(m_targets.getUnitTarget());
                        }
                        break;
                }
                break;
            case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
                // AreaAura
                if ((m_spellInfo->Attributes == (SPELL_ATTR_NOT_SHAPESHIFT | SPELL_ATTR_DONT_AFFECT_SHEATH_STATE | SPELL_ATTR_CASTABLE_WHILE_MOUNTED | SPELL_ATTR_CASTABLE_WHILE_SITTING)) || (m_spellInfo->Attributes == SPELL_ATTR_NOT_SHAPESHIFT))
                {
                    SetTargetMap(operation, TARGET_AREAEFFECT_PARTY, targetUnitMap);
                }
                break;
            case SPELL_EFFECT_SKIN_PLAYER_CORPSE:
                if (m_targets.getUnitTarget())
                {
                    targetUnitMap.push_back(m_targets.getUnitTarget());
                }
                else if (m_targets.getCorpseTargetGuid())
                {
                    if (Corpse* corpse = m_caster->GetMap()->GetCorpse(m_targets.getCorpseTargetGuid()))
                    {
                        if (Player* owner = sPlayerRegistry.Find(corpse->GetOwnerGuid()))
                        {
                            targetUnitMap.push_back(owner);
                        }
                    }
                }
                break;
            default:
                break;
        }
}
