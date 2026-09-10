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
#include "Unit.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "Spell.h"
#include "Group.h"
#include "SpellAuras.h"
#include "CreatureAI.h"
#include "TemporarySummon.h"
#include "Formulas.h"
#include "Pet.h"
#include "Util.h"
#include "Totem.h"
#include "BattleGround/BattleGround.h"
#include "InstanceData.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "MapPersistentStateMgr.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "MovementGenerator.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "Movement/Spline/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"
#include <math.h>
#include <stdarg.h>

bool Unit::IsVisibleForOrDetect(Unit const* u, Occupant const* viewPoint, bool detect, bool inVisibleList, bool is3dDistance) const
{
    if (!u || !CanBeSeen(*this, *u))
    {
        return false;
    }

    if (u == this)
    {
        return true;
    }

    bool at_same_transport =
        IsPlayer(this) &&IsPlayer(u) &&
        !((Player*)this)->GetSession()->PlayerLogout() && !((Player*)u)->GetSession()->PlayerLogout() &&
        !((Player*)this)->GetSession()->PlayerLoading() && !((Player*)u)->GetSession()->PlayerLoading() &&
        ((Player*)this)->GetTransport() && ((Player*)this)->GetTransport() == ((Player*)u)->GetTransport();

    if (!at_same_transport && (!IsInWorld() || !u->IsInWorld()))
    {
        return false;
    }

    if (m_Visibility == VISIBILITY_REMOVE_CORPSE)
    {
        return false;
    }

    Map& _map = *u->GetMap();

    if (IsPlayer(u))
    {

        if (!IsVisibleInGridForPlayer((Player*)u))
        {
            return false;
        }

        if (!u->IsAlive())
        {
            detect = false;
        }
    }
    else
    {

        if (!u->IsAlive() || !IsAlive())
        {
            return false;
        }
    }

    if (u->IsTaxiFlying())
    {

        if (!SeenWithin(*this, *viewPoint, World::GetMaxVisibleDistanceInFlight() + (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f), is3dDistance))
        {
            return false;
        }
    }
    else if (!at_same_transport)
    {

        float visibilityDistance = viewPoint->GetVisibilityDistanceOverride();
        if (visibilityDistance <= 0.0f)
        {
            visibilityDistance = _map.GetVisibilityDistance();
        }

        if (!SeenWithin(*this, *viewPoint, visibilityDistance + (inVisibleList ? World::GetVisibleUnitGreyDistance() : 0.0f), is3dDistance))
        {
            return false;
        }
    }

    if (GetCharmerOrOwnerGuid() == u->GetObjectGuid())
    {
        return true;
    }

    if (u->IsAlive() && IsAlive() && IsInvisibleForAlive() != u->IsInvisibleForAlive())
    {
        if (!IsPlayer(u) || !((Player*)u)->isGameMaster())
        {
            return false;
        }
    }

    if (m_Visibility == VISIBILITY_ON && u->m_invisibilityMask == 0)
    {
        return true;
    }

    if (IsPlayer(u) && ((Player*)u)->isGameMaster())
    {
        if (IsPlayer(this))
        {
            return ((Player*)this)->GetSession()->GetSecurity() <= ((Player*)u)->GetSession()->GetSecurity();
        }
        else
        {
            return true;
        }
    }

    if (m_Visibility == VISIBILITY_OFF)
    {
        return false;
    }

    if (IsPlayer(this) &&IsPlayer(u))
    {
        if (((Player*)this)->IsGroupVisibleFor(((Player*)u)) && IsFriendly(*u, *this))
        {
            return true;
        }
    }

    bool invisible = (m_invisibilityMask != 0 || u->m_invisibilityMask != 0);

    if (invisible &&

        ((m_invisibilityMask & u->m_invisibilityMask) != 0 ||

        u->CanDetectInvisibilityOf(this) ||

        CanDetectInvisibilityOf(u)))
    {
        invisible = false;
    }

    if (invisible || m_Visibility == VISIBILITY_GROUP_STEALTH)
    {
        if (IsHostile(*u, *this))
        {

            const auto auras = GetAurasByType(SPELL_AURA_MOD_STALKED);
            for (auto* aura : auras)
            {
                if (aura->GetCasterGuid() == u->GetObjectGuid())
                {
                    return true;
                }
            }
        }

        if (invisible)
        {
            return false;
        }
    }

    if (m_Visibility == VISIBILITY_GROUP_NO_DETECT)
    {
        return false;
    }

    if (m_Visibility != VISIBILITY_GROUP_STEALTH)
    {
        return true;
    }

    if (!detect)
    {
        return (IsPlayer(u)) ? ((Player*)u)->HaveAtClient(this) : false;
    }

    if (!getAttackers().empty())
    {
        return true;
    }

    if (Where().WithinDist(u->Where(), 0.24f))
    {
        return true;
    }

    if (u->hasUnitState(UNIT_STAT_STUNNED) && (u != this))
    {
        return false;
    }

    float visibleDistance = (IsPlayer(u)) ? MAX_PLAYER_STEALTH_DETECT_RANGE : ((Creature const*)u)->GetAttackDistance(this);

    bool IsInFront = InFrontPhased(*viewPoint, *this, visibleDistance, M_PI_F);
    if (!IsInFront)
    {
        return false;
    }

    visibleDistance = (10.5f - (GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH) / 100.0f)) /2;

    visibleDistance += int32(u->GetLevelForTarget(this)) - int32(GetLevelForTarget(u));

    int32 stealthMod = GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH_LEVEL);
    if (stealthMod < 0)
    {
        stealthMod = 0;
    }

    visibleDistance += (int32(u->GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH_DETECT)) - stealthMod) / 5.0f;
    visibleDistance = visibleDistance > MAX_PLAYER_STEALTH_DETECT_RANGE ? MAX_PLAYER_STEALTH_DETECT_RANGE : visibleDistance;

    if (visibleDistance <= 0 || !Where().WithinDist(viewPoint->Where(), visibleDistance))
    {
        return false;
    }

    float ox, oy, oz;
    ox = viewPoint->Where().X();
    oy = viewPoint->Where().Y();
    oz = viewPoint->Where().Z();
    return HasLineOfSight(*this, Geometry::Vector3(ox, oy, oz));
}

void Unit::UpdateVisibilityAndView()
{

    static const AuraType auratypes[] = {SPELL_AURA_BIND_SIGHT, SPELL_AURA_FAR_SIGHT, SPELL_AURA_NONE};
    for (AuraType const* type = &auratypes[0]; *type != SPELL_AURA_NONE; ++type)
    {
        for (auto* aura : GetAurasByType(*type))
        {
            const Unit* owner = aura->GetCaster();

            if (!owner || !IsVisibleForOrDetect(owner, this, false))
            {
                RemoveAura(aura);
            }
        }
    }

    GetViewPoint().Call_UpdateVisibilityForOwner();
    UpdateObjectVisibility();
    ScheduleAINotify(0);
    GetViewPoint().Event_ViewPointVisibilityChanged();
}

void Unit::SetVisibility(UnitVisibility x)
{
    m_Visibility = x;

    if (IsInWorld())
    {
        UpdateVisibilityAndView();
    }
}
