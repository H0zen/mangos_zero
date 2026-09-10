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

void Spell::FillAreaTargets(UnitList& targetUnitMap, float radius, cast::Around where, cast::Side side, Occupant* originalCaster )
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

    const bool catchesEveryone = m_spellInfo->ID == 1509;

    cast::Catchment catchment(targetUnitMap, reach, side, origin, catchesEveryone, Recipe().Says().castOnDead);
    Cell::VisitAllObjects(centreX, centreY, m_caster->GetMap(), catchment, radius);
}

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
