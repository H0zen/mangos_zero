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
#include "MapManager.h"
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

bool stats::Apply(Unit& who, UnitMods group, UnitModifierType which, float amount, bool apply)
{
    if (group >= UNIT_MOD_END || which >= MODIFIER_TYPE_END)
    {
        sLog.outError("ERROR in stats::Apply(): nonexistent UnitMods or wrong UnitModifierType!");
        return false;
    }

    who.Tallied().Put(group, which, amount, apply);

    // Until the sheet is built there is nothing to tell.
    if (!who.Tallied().Ready())
    {
        return false;
    }

    switch (group)
    {
        case UNIT_MOD_STAT_STRENGTH:
        case UNIT_MOD_STAT_AGILITY:
        case UNIT_MOD_STAT_STAMINA:
        case UNIT_MOD_STAT_INTELLECT:
        case UNIT_MOD_STAT_SPIRIT:         who.Sheet().Stat(stats::StatOf(group));  break;

        case UNIT_MOD_ARMOR:               who.Sheet().Armour();     break;
        case UNIT_MOD_HEALTH:              who.Sheet().MaxHealth();  break;

        case UNIT_MOD_MANA:
        case UNIT_MOD_RAGE:
        case UNIT_MOD_FOCUS:
        case UNIT_MOD_ENERGY:
        case UNIT_MOD_HAPPINESS:           who.Sheet().MaxPower(stats::PowerOf(group)); break;

        case UNIT_MOD_RESISTANCE_HOLY:
        case UNIT_MOD_RESISTANCE_FIRE:
        case UNIT_MOD_RESISTANCE_NATURE:
        case UNIT_MOD_RESISTANCE_FROST:
        case UNIT_MOD_RESISTANCE_SHADOW:
        case UNIT_MOD_RESISTANCE_ARCANE:   who.Sheet().Resistance(stats::SchoolOf(group)); break;

        case UNIT_MOD_ATTACK_POWER:        who.Sheet().AttackPower(false);  break;
        case UNIT_MOD_ATTACK_POWER_RANGED: who.Sheet().AttackPower(true);   break;

        case UNIT_MOD_DAMAGE_MAINHAND:     who.Sheet().Swing(BASE_ATTACK);    break;
        case UNIT_MOD_DAMAGE_OFFHAND:      who.Sheet().Swing(OFF_ATTACK);     break;
        case UNIT_MOD_DAMAGE_RANGED:       who.Sheet().Swing(RANGED_ATTACK);  break;

        default:
            break;
    }

    return true;
}

/**
 * @brief Computes the effective attack power for an attack type.
 *
 * @param attType The attack type to evaluate.
 * @return The resulting attack power value.
 */
float Unit::GetTotalAttackPowerValue(WeaponAttackType attType) const
{
    bool const ranged = attType == RANGED_ATTACK;
    int32 const ap = GetAttackPowerBase(ranged) + GetAttackPowerBonus(ranged);
    if (ap < 0)
    {
        return 0.0f;
    }
    return ap * (1.0f + GetAttackPowerMultiplier(ranged));
}

/**
 * @brief Gets the stored weapon damage range value for an attack type.
 *
 * @param attType The attack type.
 * @param type The minimum or maximum range selector.
 * @return The stored damage range value.
 */
float Unit::GetWeaponDamageRange(WeaponAttackType attType , WeaponDamageRange type) const
{
    if (attType == OFF_ATTACK && !haveOffhandWeapon())
    {
        return 0.0f;
    }

    return m_weaponDamage[attType][type];
}
