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

void Spell::FillTargetMap()
{

    UnitList tmpUnitLists[MAX_EFFECT_INDEX];
    uint8 effToIndex[MAX_EFFECT_INDEX] = {0, 1, 2};
    for (const auto& operation : Recipe().Does())
    {
        const uint8 i = operation.slot;

        if (operation.targetA == TARGET_SCRIPT_COORDINATES ||
            operation.targetA == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
            (operation.targetA == TARGET_SCRIPT && operation.targetB != TARGET_SELF) ||
            (operation.targetB == TARGET_SCRIPT && operation.targetA != TARGET_SELF))
        {
            continue;
        }

        if (IsAreaAuraEffect(operation.verb))
        {
            EnrolUnit(m_caster, SpellEffectIndex(i));
        }

        for (const auto& earlier : Recipe().Does())
        {
            if (earlier.slot >= i)
            {
                break;
            }

            if (operation.targetA == earlier.targetA && operation.targetB == earlier.targetB &&
                !IsAreaAuraEffect(operation.verb) && !IsAreaAuraEffect(earlier.verb))

            {
                effToIndex[i] = earlier.slot;
                break;
            }
        }

        if (effToIndex[i] == i)
        {

            switch (operation.targetA)
            {
                case TARGET_NONE:
                    switch (operation.targetB)
                    {
                        case TARGET_NONE:
                            if ((GuidHigh(m_caster->GetObjectGuid()) == HIGHGUID_PET))
                            {
                                SetTargetMap(operation, TARGET_SELF, tmpUnitLists[i ]);
                            }
                            else
                            {
                                SetTargetMap(operation, TARGET_EFFECT_SELECT, tmpUnitLists[i ]);
                            }
                            break;
                        default:
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            break;
                    }
                    break;
                case TARGET_SELF:
                    switch (operation.targetB)
                    {
                        case TARGET_NONE:

                            if ((m_spellInfo->SpellClassSet == SPELLFAMILY_MAGE && m_spellInfo->SpellClassMask & UI64LIT(0x00000800)) ||
                                (m_spellInfo->ID == 13280))
                            {
                                if (IsPlayer(m_caster))
                                {
                                    if (Unit* target = ObjectLookup::GetUnit(*m_caster, ((Player*)m_caster)->GetSelectionGuid()))
                                    {
                                        if (!IsFriendly(*m_caster, *target))
                                        {
                                            tmpUnitLists[i ].push_back(target);
                                        }
                                    }
                                }
                            }
                            else
                            {
                                SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            }
                            break;
                        case TARGET_EFFECT_SELECT:
                        case TARGET_SCRIPT:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            break;
                        case TARGET_AREAEFFECT_INSTANT:
                            if ((m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) == 0)
                            {
                                m_targets.setDestination(m_caster->Where().X(), m_caster->Where().Y(), m_caster->Where().Z());
                            }
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            break;
                        default:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            break;
                    }
                    break;
                case TARGET_EFFECT_SELECT:
                    switch (operation.targetB)
                    {
                        case TARGET_NONE:
                        case TARGET_EFFECT_SELECT:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            break;

                        case TARGET_AREAEFFECT_INSTANT:
                        case TARGET_AREAEFFECT_CUSTOM:
                        case TARGET_ALL_ENEMY_IN_AREA:
                        case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
                        case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
                        case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
                        case TARGET_AREAEFFECT_GO_AROUND_DEST:

                            if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) || m_IsTriggeredSpell)
                            {
                                if (Occupant* castObject = GetCastingObject())
                                {
                                    m_targets.setDestination(castObject->Where().X(), castObject->Where().Y(), castObject->Where().Z());
                                }
                            }
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            break;

                        case TARGET_INNKEEPER_COORDINATES:
                        case TARGET_TABLE_X_Y_Z_COORDINATES:
                        case TARGET_CASTER_COORDINATES:
                        case TARGET_SCRIPT_COORDINATES:
                        case TARGET_CURRENT_ENEMY_COORDINATES:
                        case TARGET_DUELVSPLAYER_COORDINATES:

                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            break;
                        default:
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            break;
                    }
                    break;
                case TARGET_CASTER_COORDINATES:
                    switch (operation.targetB)
                    {
                        case TARGET_ALL_ENEMY_IN_AREA:

                            if (operation.verb == SPELL_EFFECT_ENVIRONMENTAL_DAMAGE)
                            {
                                if (m_targets.getUnitTarget())
                                {
                                    tmpUnitLists[i ].push_back(m_targets.getUnitTarget());
                                }
                            }
                            else
                            {
                                SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                                SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            }
                            break;
                        case TARGET_NONE:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            tmpUnitLists[i ].push_back(m_caster);
                            break;
                        default:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            break;
                    }
                    break;
                case TARGET_TABLE_X_Y_Z_COORDINATES:
                    switch (operation.targetB)
                    {
                        case TARGET_NONE:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);

                            SetTargetMap(operation, TARGET_EFFECT_SELECT, tmpUnitLists[i ]);
                            break;
                        case TARGET_AREAEFFECT_INSTANT:
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            break;
                        default:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            break;
                    }
                    break;
                case TARGET_DUELVSPLAYER_COORDINATES:
                    switch (operation.targetB)
                    {
                        case TARGET_NONE:
                        case TARGET_EFFECT_SELECT:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            if (Unit* currentTarget = m_targets.getUnitTarget())
                            {
                                tmpUnitLists[i ].push_back(currentTarget);
                            }
                            break;
                        default:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            break;
                    }
                    break;
                case TARGET_SCRIPT:
                    switch (operation.targetB)
                    {
                        case TARGET_SELF:

                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
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
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            break;
                        case TARGET_SCRIPT_COORDINATES:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            break;
                        default:
                            SetTargetMap(operation, operation.targetA, tmpUnitLists[i ]);
                            SetTargetMap(operation, operation.targetB, tmpUnitLists[i ]);
                            break;
                    }
                    break;
            }
        }

        if (IsPlayer(m_caster))
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

bool Spell::SetsOffProcs() const
{
    if (!m_CastItem && (!m_IsTriggeredSpell || !m_triggeredByAuraSpell))
    {
        return true;
    }

    return Recipe().ProcsThoughTriggered();
}

void Spell::EnrolUnit(Unit* pVictim, SpellEffectIndex effIndex)
{
    if (Recipe().Does().AtSlot(static_cast<uint8>(effIndex)) == nullptr)
    {
        return;
    }

    bool immuned = pVictim->IsImmuneToSpellEffect(m_spellInfo, effIndex, pVictim == m_caster);

    ObjectGuid targetGUID = pVictim->GetObjectGuid();

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

    target.verdict = m_caster->SpellHitResult(pVictim, m_spellInfo, m_canReflect);

    Occupant* affectiveObject = GetAffectiveCasterObject();

    float speed = m_spellInfo->Speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->Speed : m_spellInfo->Speed;
    if (speed > 0.0f && affectiveObject && (pVictim != affectiveObject || (m_targets.m_targetMask & (TARGET_FLAG_SOURCE_LOCATION | TARGET_FLAG_DEST_LOCATION))))
    {

        float dist;
        if (pVictim == affectiveObject)
        {
            if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
            {
                dist = affectiveObject->Where().DistanceTo(Geometry::Vector3(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ));
            }
            else
            {
                dist = affectiveObject->Where().DistanceTo(Geometry::Vector3(m_targets.m_srcX, m_targets.m_srcY, m_targets.m_srcZ));
            }
        }
        else
        {
            dist = affectiveObject->Where().DistanceTo(Geometry::Vector3(pVictim->Where().X(), pVictim->Where().Y(), pVictim->Where().Z()));
        }

        if (dist < 5.0f)
        {
            dist = 5.0f;
        }
        target.arrivesInMs = static_cast<uint64>(floor(dist / speed * 1000.0f));
    }

    if (target.verdict == SPELL_MISS_REFLECT)
    {

        target.reflectedVerdict =  m_caster->SpellHitResult(m_caster, m_spellInfo, m_canReflect);

        if (target.reflectedVerdict == SPELL_MISS_REFLECT)
        {
            target.reflectedVerdict = SPELL_MISS_PARRY;
        }

        target.arrivesInMs += target.arrivesInMs >> 1;
    }
    else
    {
        target.reflectedVerdict = SPELL_MISS_NONE;
    }

    m_roster.Enrol(target);
}

void Spell::EnrolUnit(ObjectGuid unitGuid, SpellEffectIndex effIndex)
{
    if (Unit* unit = m_caster->GetObjectGuid() == unitGuid ? m_caster : ObjectLookup::GetUnit(*m_caster, unitGuid))
    {
        EnrolUnit(unit, effIndex);
    }
}

void Spell::EnrolObject(GameObject* pVictim, SpellEffectIndex effIndex)
{
    if (Recipe().Does().AtSlot(static_cast<uint8>(effIndex)) == nullptr)
    {
        return;
    }

    ObjectGuid targetGUID = pVictim->GetObjectGuid();

    if (cast::ObjectTarget* enrolled = m_roster.FindObject(targetGUID))
    {
        enrolled->slots |= 1 << effIndex;
        return;
    }

    cast::ObjectTarget target;
    target.guid = targetGUID;
    target.slots = (1 << effIndex);

    Occupant* affectiveObject = GetAffectiveCasterObject();

    float speed = m_spellInfo->Speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->Speed : m_spellInfo->Speed;
    if (speed > 0.0f && affectiveObject && pVictim != affectiveObject)
    {

        float dist = affectiveObject->Where().DistanceTo(Geometry::Vector3(pVictim->Where().X(), pVictim->Where().Y(), pVictim->Where().Z()));
        if (dist < 5.0f)
        {
            dist = 5.0f;
        }
        target.arrivesInMs = static_cast<uint64>(floor(dist / speed * 1000.0f));
    }

    m_roster.Enrol(target);
}

void Spell::EnrolObject(ObjectGuid goGuid, SpellEffectIndex effIndex)
{
    if (GameObject* go = m_caster->GetMap()->GetGameObject(goGuid))
    {
        EnrolObject(go, effIndex);
    }
}

void Spell::EnrolItem(Item* pitem, SpellEffectIndex effIndex)
{
    if (Recipe().Does().AtSlot(static_cast<uint8>(effIndex)) == nullptr)
    {
        return;
    }

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
