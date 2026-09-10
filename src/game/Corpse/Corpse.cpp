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

#include "Utilities/Errors.h"
#include <sstream>
#include "Corpse.h"
#include "Player.h"
#include "CorpseManager.h"
#include "ObjectGuid.h"
#include "Database/DatabaseEnv.h"
#include "World.h"
#include "ObjectMgr.h"

Corpse::Corpse(CorpseType type) : Occupant(),
    loot(this),
    lootRecipient(nullptr),
    lootForBody(false)
{
    m_objectTypeId = TYPEID_CORPSE;
    m_updateFlag = (UPDATEFLAG_TRANSPORT | UPDATEFLAG_ALL | UPDATEFLAG_HAS_POSITION);

    m_type = type;

    m_time = time(nullptr);
}

Corpse::~Corpse()
{
}

void Corpse::AddToWorld()
{

    if (!IsInWorld())
    {
        sCorpseManager.AddObject(this);
    }

    Object::AddToWorld();
}

void Corpse::RemoveFromWorld()
{

    if (IsInWorld())
    {
        sCorpseManager.RemoveObject(this);
    }

    Object::RemoveFromWorld();
}

bool Corpse::Create(uint32 guidlow)
{
    Object::_Create(guidlow, 0, HIGHGUID_CORPSE);
    return true;
}

bool Corpse::Create(uint32 guidlow, Player* owner)
{
    MANGOS_ASSERT(owner);

    Occupant::_Create(guidlow, HIGHGUID_CORPSE);
    Place().MoveTo(owner->Where().X(), owner->Where().Y(), owner->Where().Z(), owner->Where().Facing());

    SetMap(owner->GetMap());

    if (!IsPlaceable(*this))
    {
        sLog.outError("Corpse (guidlow %d, owner %s) not created. Suggested coordinates isn't valid (X: %f Y: %f)",
            guidlow, owner->GetName(), owner->Where().X(), owner->Where().Y());
        return false;
    }

    SetObjectScale(DEFAULT_OBJECT_SCALE);
    SetFloatValue(CORPSE_FIELD_POS_X, Where().X());
    SetFloatValue(CORPSE_FIELD_POS_Y, Where().Y());
    SetFloatValue(CORPSE_FIELD_POS_Z, Where().Z());
    SetFloatValue(CORPSE_FIELD_FACING, Where().Facing());
    SetGuidValue(CORPSE_FIELD_OWNER, owner->GetObjectGuid());

    m_grid = MaNGOS::ComputeGridPair(Where().X(), Where().Y());

    return true;
}

void Corpse::DeleteBonesFromWorld()
{
    MANGOS_ASSERT(GetType() == CORPSE_BONES);
    Corpse* corpse = GetMap()->GetCorpse(GetObjectGuid());

    if (!corpse)
    {
        sLog.outError("Bones %u not found in world.", GetGUIDLow());
        return;
    }

    AddObjectToRemoveList();
}

bool Corpse::OpenableBy(Player const& who) const
{
    return InReach(*this, who, INTERACTION_DISTANCE);
}

bool Corpse::IsVisibleForInState(Player const* u, Occupant const* viewPoint, bool inVisibleList) const
{
    return IsInWorld() && u->IsInWorld() && SeenWithin(*this, *viewPoint, GetMap()->GetVisibilityDistance() + (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f), false);
}

namespace
{

    constexpr time_t BONES_LIE_FOR = 60 * MINUTE;
    constexpr time_t BODY_LIES_FOR = 3 * DAY;
}

bool Corpse::IsExpired(time_t now) const
{
    const time_t lies = m_type == CORPSE_BONES ? BONES_LIE_FOR : BODY_LIES_FOR;

    return m_time + lies < now;
}
