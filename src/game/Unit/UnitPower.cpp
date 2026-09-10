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

void Unit::SetPowerType(Powers new_powertype)
{

    SetPowerKind(new_powertype);

    if (IsPlayer(this))
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_POWER_TYPE);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (IsPlayer(owner)) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_POWER_TYPE);
            }
        }
    }

    if (IsPlayer(this) || (IsCreature(this) && ((Creature*)this)->IsPet()))
    {
        uint32 maxValue = GetCreatePowers(new_powertype);
        uint32 curValue = maxValue;

        if (new_powertype == POWER_RAGE)
        {
            curValue = 0;
        }

        if (new_powertype != POWER_MANA)
        {
            SetMaxPower(new_powertype, maxValue);
            SetPower(new_powertype, curValue);
        }
    }
}

void Unit::SetPower(Powers power, uint32 val)
{
    if (GetPower(power) == val)
    {
        return;
    }

    uint32 maxPower = GetMaxPower(power);
    if (maxPower < val)
    {
        val = maxPower;
    }

    SetStatInt32Value(UNIT_FIELD_POWER1 + power, val);

    if (IsPlayer(this))
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_CUR_POWER);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (IsPlayer(owner)) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_CUR_POWER);
            }
        }

        if (pet->getPetType() == HUNTER_PET && power == POWER_HAPPINESS)
        {
            pet->Sheet().Swing(BASE_ATTACK);
        }
    }
}

void Unit::SetMaxPower(Powers power, uint32 val)
{
    uint32 cur_power = GetPower(power);
    SetStatInt32Value(UNIT_FIELD_MAXPOWER1 + power, val);

    if (IsPlayer(this))
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_MAX_POWER);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (IsPlayer(owner)) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MAX_POWER);
            }
        }
    }

    if (val < cur_power)
    {
        SetPower(power, val);
    }
}

void Unit::ApplyPowerMod(Powers power, uint32 val, bool apply)
{
    ApplyModUInt32Value(UNIT_FIELD_POWER1 + power, val, apply);

    if (IsPlayer(this))
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_CUR_POWER);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (IsPlayer(owner)) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_CUR_POWER);
            }
        }
    }
}

void Unit::ApplyMaxPowerMod(Powers power, uint32 val, bool apply)
{
    ApplyModUInt32Value(UNIT_FIELD_MAXPOWER1 + power, val, apply);

    if (IsPlayer(this))
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_MAX_POWER);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (IsPlayer(owner)) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MAX_POWER);
            }
        }
    }
}

void Unit::ApplyAuraProcTriggerDamage(Aura* aura, bool apply)
{
    if (apply)
    {
        m_auraIndex.Add(SPELL_AURA_PROC_TRIGGER_DAMAGE, aura);
    }
    else
    {
        m_auraIndex.Remove(SPELL_AURA_PROC_TRIGGER_DAMAGE, aura);
    }
}

uint32 Unit::GetCreatePowers(Powers power) const
{
    switch (power)
    {
        case POWER_HEALTH:      return 0;
        case POWER_MANA:        return GetCreateMana();
        case POWER_RAGE:        return POWER_RAGE_DEFAULT;
        case POWER_FOCUS:       return (IsPlayer(this) || !((Creature const*)this)->IsPet() || ((Pet const*)this)->getPetType() != HUNTER_PET ? 0 : POWER_FOCUS_DEFAULT);
        case POWER_ENERGY:      return POWER_ENERGY_DEFAULT;
        case POWER_HAPPINESS:   return (IsPlayer(this) || !((Creature const*)this)->IsPet() || ((Pet const*)this)->getPetType() != HUNTER_PET ? 0 : POWER_HAPPINESS_DEFAULT);
        default: break;
    }

    return 0;
}
