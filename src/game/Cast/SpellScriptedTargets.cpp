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

#include "Reaction.h"
#include "Utilities/MathDefines.h"
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
#include "CreatureRecord.h"
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
#include "LineOfSightExemptions.h"
#include "BattleGround/BattleGround.h"
#include "Util.h"
#include "Chat.h"
#include "TemporarySummon.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "Cast/Recipe/RecipeBook.h"

SpellCastResult Spell::EnrolScriptedTargets()
{

    if (m_roster.Units().empty())
    {
        for (const auto& operation : Recipe().Does())
        {
            const int j = operation.slot;

            if (operation.targetA == TARGET_SCRIPT ||
                operation.targetB == TARGET_SCRIPT ||
                operation.targetA == TARGET_SCRIPT_COORDINATES ||
                operation.targetB == TARGET_SCRIPT_COORDINATES ||
                operation.targetA == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
                operation.targetB == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
            {
                SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(m_spellInfo->ID);

                if (bounds.first == bounds.second)
                {
                    if (operation.targetA == TARGET_SCRIPT || operation.targetB == TARGET_SCRIPT)
                    {
                        sLog.outErrorDb("Spell entry %u, effect %i has EffectImplicitTargetA/EffectImplicitTargetB = TARGET_SCRIPT, but creature are not defined in `spell_script_target`", m_spellInfo->ID, j);
                    }

                    if (operation.targetA == TARGET_SCRIPT_COORDINATES || operation.targetB == TARGET_SCRIPT_COORDINATES)
                    {
                        sLog.outErrorDb("Spell entry %u, effect %i has EffectImplicitTargetA/EffectImplicitTargetB = TARGET_SCRIPT_COORDINATES, but gameobject or creature are not defined in `spell_script_target`", m_spellInfo->ID, j);
                    }

                    if (operation.targetA == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT || operation.targetB == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
                    {
                        sLog.outErrorDb("Spell entry %u, effect %i has EffectImplicitTargetA/EffectImplicitTargetB = TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT, but gameobject are not defined in `spell_script_target`", m_spellInfo->ID, j);
                    }
                }

                SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(m_spellInfo->RangeIndex);
                float range = GetSpellMaxRange(srange);

                if (!range)
                {
                    range = m_caster->GetMap()->IsDungeon() ? DEFAULT_VISIBILITY_INSTANCE : DEFAULT_VISIBILITY_DISTANCE;
                }

                Creature* targetExplicit = nullptr;
                Creature* creatureScriptTarget = nullptr;
                GameObject* goScriptTarget = nullptr;

                for (SQLMultiStorage::SQLMultiSIterator<SpellTargetEntry> i_spellST = bounds.first; i_spellST != bounds.second; ++i_spellST)
                {
                    if (i_spellST->CanNotHitWithSpellEffect(SpellEffectIndex(j)))
                    {
                        continue;
                    }

                    switch (i_spellST->type)
                    {
                        case SPELL_TARGET_TYPE_GAMEOBJECT:
                        {
                            GameObject* p_GameObject = nullptr;

                            if (i_spellST->targetEntry)
                            {
                                MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(*m_caster, i_spellST->targetEntry, range);
                                MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> checker(p_GameObject, go_check);
                                Cell::VisitGridObjects(m_caster, checker, range);

                                if (p_GameObject)
                                {

                                    creatureScriptTarget = nullptr;
                                    goScriptTarget = p_GameObject;
                                    range = go_check.GetLastRange();
                                }
                            }
                            else if (focusObject)
                            {
                                float frange = m_caster->Where().DistanceTo(focusObject->Where());
                                if (range >= frange)
                                {
                                    creatureScriptTarget = nullptr;
                                    goScriptTarget = focusObject;
                                    range = frange;
                                }
                            }
                            break;
                        }
                        case SPELL_TARGET_TYPE_CREATURE:
                        case SPELL_TARGET_TYPE_DEAD:
                        default:
                        {
                            Creature* p_Creature = nullptr;

                            if (Unit* pTarget = m_targets.getUnitTarget())
                            {
                                if (IsCreature(pTarget) && pTarget->GetEntry() == i_spellST->targetEntry)
                                {
                                    if (i_spellST->type == SPELL_TARGET_TYPE_DEAD && ((Creature*)pTarget)->IsCorpse())
                                    {

                                        if (InReach(*pTarget, *m_caster, GetSpellMaxRange(srange)))
                                        {
                                            targetExplicit = (Creature*)pTarget;
                                        }
                                    }
                                    else if (i_spellST->type == SPELL_TARGET_TYPE_CREATURE && pTarget->IsAlive())
                                    {

                                        if (InReach(*pTarget, *m_caster, GetSpellMaxRange(srange)))
                                        {
                                            targetExplicit = (Creature*)pTarget;
                                        }
                                    }
                                }
                            }

                            if (!targetExplicit)
                            {
                                MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*m_caster, i_spellST->targetEntry, i_spellST->type != SPELL_TARGET_TYPE_DEAD, i_spellST->type == SPELL_TARGET_TYPE_DEAD, range);
                                MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(p_Creature, u_check);

                                Cell::VisitAllObjects(m_caster, searcher, range);

                                range = u_check.GetLastRange();
                            }

                            if (targetExplicit)
                            {
                                creatureScriptTarget = targetExplicit;
                            }
                            else if (p_Creature)
                            {
                                creatureScriptTarget = p_Creature;
                            }

                            if (creatureScriptTarget)
                            {
                                goScriptTarget = nullptr;
                            }

                            break;
                        }
                    }
                }

                if (creatureScriptTarget)
                {

                    if (operation.targetA == TARGET_SCRIPT_COORDINATES ||
                        operation.targetB == TARGET_SCRIPT_COORDINATES)
                    {
                        m_targets.setDestination(creatureScriptTarget->Where().X(), creatureScriptTarget->Where().Y(), creatureScriptTarget->Where().Z());

                        if (operation.targetA == TARGET_SCRIPT_COORDINATES && operation.verb != SPELL_EFFECT_PERSISTENT_AREA_AURA)
                        {
                            EnrolUnit(creatureScriptTarget, SpellEffectIndex(j));
                        }
                    }

                    else
                    {
                        if (operation.targetA == TARGET_SCRIPT ||
                            operation.targetB == TARGET_SCRIPT)
                        {
                            EnrolUnit(creatureScriptTarget, SpellEffectIndex(j));
                        }
                    }
                }
                else if (goScriptTarget)
                {

                    if (operation.targetA == TARGET_SCRIPT_COORDINATES ||
                        operation.targetB == TARGET_SCRIPT_COORDINATES)
                    {
                        m_targets.setDestination(goScriptTarget->Where().X(), goScriptTarget->Where().Y(), goScriptTarget->Where().Z());

                        if (operation.targetA == TARGET_SCRIPT_COORDINATES && operation.verb != SPELL_EFFECT_PERSISTENT_AREA_AURA)
                        {
                            EnrolObject(goScriptTarget, SpellEffectIndex(j));
                        }
                    }

                    else
                    {
                        if (operation.targetA == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
                            operation.targetB == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
                        {
                            EnrolObject(goScriptTarget, SpellEffectIndex(j));
                        }
                    }
                }

                else
                {

                    if (operation.targetA != TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
                    {

                        if (m_triggeredByAuraSpell || m_IsTriggeredSpell)
                        {
                            return SPELL_FAILED_DONT_REPORT;
                        }
                        else
                        {
                            return SPELL_FAILED_BAD_TARGETS;
                        }
                    }
                }
            }
        }
    }

    return SPELL_CAST_OK;
}
