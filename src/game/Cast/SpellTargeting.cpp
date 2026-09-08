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
 * @brief Populates a unit target list for a specific implicit target mode.
 *
 * @param effIndex The effect index being processed.
 * @param targetMode The implicit target mode.
 * @param targetUnitMap The unit list being populated.
 */
void Spell::SetTargetMap(const cast::Operation& operation, uint32 targetMode, UnitList& targetUnitMap)
{
    const SpellEffectIndex effIndex = SpellEffectIndex(operation.slot);

    float radius;
    uint32 EffectChainTarget = operation.chainTargets;
    uint32 unMaxTargets = m_spellInfo->MaxTargets;  // Get spell max affected targets

    GetSpellRangeAndRadius(effIndex, radius, EffectChainTarget, unMaxTargets);

    std::list<GameObject*> tempTargetGOList;

    switch (targetMode)
    {
        case TARGET_TOTEM_EARTH:
        case TARGET_TOTEM_WATER:
        case TARGET_TOTEM_AIR:
        case TARGET_TOTEM_FIRE:
        {
            float angle = m_caster->Where().Facing();
            switch (targetMode)
            {
                case TARGET_TOTEM_FIRE:  angle += M_PI_F * 0.25f; break;            // front - left
                case TARGET_TOTEM_AIR:   angle += M_PI_F * 0.75f; break;            // back  - left
                case TARGET_TOTEM_WATER: angle += M_PI_F * 1.25f; break;            // back  - right
                case TARGET_TOTEM_EARTH: angle += M_PI_F * 1.75f; break;            // front - right
            }

            float x, y;
            float z = m_caster->Where().Z();
            // Do not search for a free spot. TODO: Should there be searched for a free spot. There was once a discussion that in case this space was impossible (LOS) m_caster's position should be used.
            // TODO Bring this back to memory and search for it!
            const Geometry::Vector3 near_ = PointNear(*m_caster, radius, angle);
            x = near_.x;
            y = near_.y;
            ClampToAllowedZ(*m_caster, x, y, z);
            m_targets.setDestination(x, y, z);

            // Add Summoner
            targetUnitMap.push_back(m_caster);
            break;
        }
        case TARGET_SELF:
            targetUnitMap.push_back(m_caster);
            break;
        case TARGET_RANDOM_ENEMY_CHAIN_IN_AREA:
        case TARGET_RANDOM_FRIEND_CHAIN_IN_AREA:
        case TARGET_RANDOM_UNIT_CHAIN_IN_AREA:
            PickARandomChainInTheArea(targetMode, targetUnitMap, radius, EffectChainTarget, unMaxTargets);
            break;
        case TARGET_PET:
        {
            Pet* tmpUnit = m_caster->GetPet();
            if (!tmpUnit)
            {
                break;
            }
            targetUnitMap.push_back(tmpUnit);
            break;
        }
        case TARGET_CHAIN_DAMAGE:
            PickTheChainFromTheVictim(operation, targetUnitMap, radius, EffectChainTarget, unMaxTargets);
            break;
        case TARGET_ALL_ENEMY_IN_AREA:
            FillAreaTargets(targetUnitMap, radius, cast::Around::Spot, cast::Side::HostileForArea);
            break;
        case TARGET_AREAEFFECT_INSTANT:
            PickTheAreaTheVerbWants(operation, targetUnitMap, radius);
            break;
        case TARGET_AREAEFFECT_CUSTOM:
            PickTheNamedCreaturesInTheArea(operation, targetUnitMap, radius);
            break;
        case TARGET_AREAEFFECT_GO_AROUND_SOURCE:
        case TARGET_AREAEFFECT_GO_AROUND_DEST:
            PickTheObjectsAroundTheSpot(effIndex, targetMode, tempTargetGOList, radius);
            break;
        case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
        {
            // targets the ground, not the units in the area
            switch (operation.verb)
            {
                case SPELL_EFFECT_PERSISTENT_AREA_AURA:
                    break;
                case SPELL_EFFECT_SUMMON:
                    targetUnitMap.push_back(m_caster);
                    break;
                default:
                    FillAreaTargets(targetUnitMap, radius, cast::Around::Spot, cast::Side::HostileForArea);
                    break;
            }
            break;
        }
        case TARGET_DUELVSPLAYER_COORDINATES:
        {
            if (Unit* currentTarget = m_targets.getUnitTarget())
            {
                m_targets.setDestination(currentTarget->Where().X(), currentTarget->Where().Y(), currentTarget->Where().Z());
            }
            break;
        }
        case TARGET_ALL_PARTY_AROUND_CASTER:
        case TARGET_ALL_PARTY_AROUND_CASTER_2:
        case TARGET_ALL_PARTY:
        {
            FillRaidOrPartyTargets(targetUnitMap, m_caster, radius, false, true, true);
            break;
        }
        case TARGET_ALL_RAID_AROUND_CASTER:
        {
            FillRaidOrPartyTargets(targetUnitMap, m_caster, radius, true, true, false);
            break;
        }
        case TARGET_SINGLE_FRIEND:
        case TARGET_SINGLE_FRIEND_2:
            if (m_targets.getUnitTarget())
            {
                targetUnitMap.push_back(m_targets.getUnitTarget());
            }
            break;
        case TARGET_CASTER_COORDINATES:
        {
            // Check original caster is GO - set its coordinates as src cast
            if (Occupant* caster = GetCastingObject())
            {
                m_targets.setSource(caster->Where().X(), caster->Where().Y(), caster->Where().Z());
            }
            break;
        }
        case TARGET_ALL_HOSTILE_UNITS_AROUND_CASTER:
            FillAreaTargets(targetUnitMap, radius, cast::Around::Caster, cast::Side::Hostile);
            break;
        case TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER:
            // selected friendly units (for casting objects) around casting object
            FillAreaTargets(targetUnitMap, radius, cast::Around::Caster, cast::Side::Friendly, GetCastingObject());
            break;
        case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
            FillAreaTargets(targetUnitMap, radius, cast::Around::Spot, cast::Side::Friendly);
            break;
        case TARGET_SINGLE_PARTY:
            PickTheOneGroupmate(targetUnitMap);
            break;
        case TARGET_GAMEOBJECT:
            if (m_targets.getGOTarget())
            {
                EnrolObject(m_targets.getGOTarget(), effIndex);
            }
            break;
        case TARGET_IN_FRONT_OF_CASTER:
        {
            cast::Around pushType = cast::Around::CasterInFront;
            switch (m_spellInfo->SpellVisualID)            // Some spell require a different target fill
            {
                case 3879: pushType = cast::Around::CasterBehind;     break;
                case 7441: pushType = cast::Around::CasterInFront15; break;
            }
            FillAreaTargets(targetUnitMap, radius, pushType, cast::Side::HostileForArea);
            break;
        }
        case TARGET_LARGE_FRONTAL_CONE:
            FillAreaTargets(targetUnitMap, radius, cast::Around::CasterInFront90, cast::Side::HostileForArea);
            break;
        case TARGET_NARROW_FRONTAL_CONE:
            PickTheConeThisSpellOpens(operation, targetUnitMap, radius);
            break;
        case TARGET_DUELVSPLAYER:
        {
            if (Unit* target = m_targets.getUnitTarget())
            {
                if (IsFriendly(*m_caster, *target))
                {
                    targetUnitMap.push_back(target);
                }
                else
                {
                    if (Unit* pUnitTarget = m_caster->SelectMagnetTarget(target, this, effIndex))
                    {
                        if (target != pUnitTarget)
                        {
                            m_targets.setUnitTarget(pUnitTarget);
                        }
                        targetUnitMap.push_back(pUnitTarget);
                    }
                }
            }
            break;
        }
        case TARGET_GAMEOBJECT_ITEM:
            if (m_targets.getGOTargetGuid())
            {
                EnrolObject(m_targets.getGOTarget(), effIndex);
            }
            else if (m_targets.getItemTarget())
            {
                EnrolItem(m_targets.getItemTarget(), effIndex);
            }
            break;
        case TARGET_MASTER:
            if (Unit* owner = m_caster->GetCharmerOrOwner())
            {
                targetUnitMap.push_back(owner);
            }
            break;
        case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
            // targets the ground, not the units in the area
            if (operation.verb != SPELL_EFFECT_PERSISTENT_AREA_AURA)
            {
                FillAreaTargets(targetUnitMap, radius, cast::Around::Spot, cast::Side::HostileForArea);
            }
            break;
        case TARGET_MINION:
            if (operation.verb != SPELL_EFFECT_DUEL)
            {
                targetUnitMap.push_back(m_caster);
            }
            break;
        case TARGET_AREAEFFECT_PARTY:
            PickThePartyAround(targetUnitMap, radius);
            break;
        case TARGET_SCRIPT:
        {
            if (m_targets.getUnitTarget())
            {
                targetUnitMap.push_back(m_targets.getUnitTarget());
            }
            if (m_targets.getItemTarget())
            {
                EnrolItem(m_targets.getItemTarget(), effIndex);
            }
            break;
        }
        case TARGET_SELF_FISHING:
            targetUnitMap.push_back(m_caster);
            break;
        case TARGET_CHAIN_HEAL:
            PickTheChainOfWounded(targetUnitMap, radius, EffectChainTarget, unMaxTargets);
            break;
        case TARGET_CURRENT_ENEMY_COORDINATES:
        {
            Unit* currentTarget = m_targets.getUnitTarget();
            if (currentTarget)
            {
                targetUnitMap.push_back(currentTarget);
                m_targets.setDestination(currentTarget->Where().X(), currentTarget->Where().Y(), currentTarget->Where().Z());
            }
            break;
        }
        case TARGET_AREAEFFECT_PARTY_AND_CLASS:
            PickThePartyOfTheTargetsClass(targetUnitMap, radius);
            break;
        case TARGET_TABLE_X_Y_Z_COORDINATES:
        {
            if (SpellTargetPosition const* st = sSpellMgr.GetSpellTargetPosition(m_spellInfo->ID))
            {
                m_targets.setDestination(st->target_X, st->target_Y, st->target_Z);
                // TODO - maybe use an (internal) value for the map for neat far teleport handling

                // far-teleport spells are handled in SpellEffect, elsewise report an error about an unexpected map (spells are always locally)
                if (st->target_mapId != m_caster->GetMapId() && operation.verb != SPELL_EFFECT_TELEPORT_UNITS)
                {
                    sLog.outError("SPELL: wrong map (%u instead %u) target coordinates for spell ID %u", st->target_mapId, m_caster->GetMapId(), m_spellInfo->ID);
                }
            }
            else
            {
                sLog.outError("SPELL: unknown target coordinates for spell ID %u", m_spellInfo->ID);
            }
            break;
        }
        case TARGET_DYNAMIC_OBJECT_FRONT:
        case TARGET_DYNAMIC_OBJECT_BEHIND:
        case TARGET_DYNAMIC_OBJECT_LEFT_SIDE:
        case TARGET_DYNAMIC_OBJECT_RIGHT_SIDE:
            PickTheSpotBesideTheCaster(operation, targetMode, targetUnitMap, radius);
            break;
        case TARGET_EFFECT_SELECT:
            PickWhatTheSlotImplies(operation, targetUnitMap);
            break;
        default:
            // sLog.outError( "SPELL: Unknown implicit target (%u) for spell ID %u", targetMode, m_spellInfo->Id );
            break;
    }

    if (targetMode != TARGET_SELF && Recipe().Says().cannotTargetSelf)
    {
        targetUnitMap.remove(m_caster);
    }

    cast::KeepAtMost(targetUnitMap, unMaxTargets, m_targets.getUnitTarget(), true);

    cast::KeepAtMost(tempTargetGOList, unMaxTargets, m_targets.getGOTarget(), false);
    for (auto found : tempTargetGOList)
    {
        EnrolObject(found, effIndex);
    }
}
