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

/**
 * @brief Fills a target list with everyone the spell's area catches.
 *
 * @param targetUnitMap        The list the caught units are added to
 * @param radius               How far the area reaches from its centre
 * @param where                What the area is drawn around
 * @param side                 Which side of the caster may be caught
 * @param originalCaster       Whose side is asked; the affective caster when none is given
 */
void Spell::FillAreaTargets(UnitList& targetUnitMap, float radius, cast::Around where, cast::Side side, Occupant* originalCaster /*=nullptr*/)
{
    Occupant* origin = originalCaster != nullptr ? originalCaster : GetAffectiveCasterObject();
    Occupant* castingObject = GetCastingObject();

    if (origin == nullptr || castingObject == nullptr)
    {
        return;
    }

    cast::Reach reach;
    reach.where = where;
    reach.radius = radius;
    reach.from = castingObject;

    float centreX = castingObject->Where().X();
    float centreY = castingObject->Where().Y();

    if (where == cast::Around::Spot)
    {
        float x, y, z;
        if (m_targets.m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
        {
            m_targets.getSource(x, y, z);
        }
        else
        {
            m_targets.getDestination(x, y, z);
        }

        reach.at = Geometry::Vector3(x, y, z);
        centreX = x;
        centreY = y;
    }

    // the GM spell that reaches everyone, whatever they are and whose side they are on
    const bool catchesEveryone = m_spellInfo->ID == 1509;

    cast::Catchment catchment(targetUnitMap, reach, side, origin, catchesEveryone, Recipe().Says().castOnDead);
    Cell::VisitAllObjects(centreX, centreY, m_caster->GetMap(), catchment, radius);
}

/**
 * @brief Fills a target list with party or raid members around a reference unit.
 *
 * @param targetUnitMap The target list being populated.
 * @param member The reference member.
 * @param radius The search radius.
 * @param raid True to include the whole raid; false to limit to the subgroup.
 * @param withPets True to include pets.
 * @param withcaster True to include the caster when applicable.
 */
void Spell::FillRaidOrPartyTargets(UnitList& targetUnitMap, Unit* member, float radius, bool raid, bool withPets, bool withcaster)
{
    Player* pMember = member->GetCharmerOrOwnerPlayerOrPlayerItself();
    Group* pGroup = pMember ? pMember->GetGroup() : nullptr;

    if (pGroup)
    {
        uint8 subgroup = pMember->GetSubGroup();

        for (GroupReference* itr = pGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* Target = itr->getSource();

            // IsHostileTo check duel and controlled by enemy
            if (Target && (raid || subgroup == Target->GetSubGroup()) &&
                !IsHostile(*m_caster, *Target))
            {
                if ((Target == m_caster && withcaster) ||
                    (Target != m_caster && InReach(*m_caster, *Target, radius)))
                {
                    targetUnitMap.push_back(Target);
                }

                if (withPets)
                {
                    if (Pet* pet = Target->GetPet())
                    {
                        if ((pet == m_caster && withcaster) ||
                            (pet != m_caster && InReach(*m_caster, *pet, radius)))
                        {
                            targetUnitMap.push_back(pet);
                        }
                    }
                }
            }
        }
    }
    else
    {
        Unit* ownerOrSelf = pMember ? pMember : member->GetCharmerOrOwnerOrSelf();
        if ((ownerOrSelf == m_caster && withcaster) ||
            (ownerOrSelf != m_caster && InReach(*m_caster, *ownerOrSelf, radius)))
        {
            targetUnitMap.push_back(ownerOrSelf);
        }

        if (withPets)
        {
            if (Pet* pet = ownerOrSelf->GetPet())
            {
                if ((pet == m_caster && withcaster) ||
                    (pet != m_caster && InReach(*m_caster, *pet, radius)))
                {
                    targetUnitMap.push_back(pet);
                }
            }
        }
    }
}
