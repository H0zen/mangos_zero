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



#include <algorithm>
#include "Creature.h"
#include "World.h"
#include "ObjectMgr.h"
#include "LivingWorldAnchorPolicy.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "ScriptMgr.h"
#include "ObjectGuid.h"
#include "SQLStorages.h"
#include "SpellMgr.h"
#include "GossipDef.h"
#include "Player.h"
#include "GameEventMgr.h"
#include "PoolManager.h"
#include "Opcodes.h"
#include "Log.h"
#include "LootMgr.h"
#include "CreatureAI.h"
#include "CreatureAISelector.h"
#include "InstanceData.h"
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGroundMgr.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Spell.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Movement/Spline/MoveSplineInit.h"
#include "CreatureLinkingMgr.h"
#include "DisableMgr.h"
#include "MovementGenerator.h"
#include "Policies/Singleton.h"

/**
 * @brief Selects the creature level and recalculates level-dependent stats.
 *
 * @param forcedLevel Optional forced level override.
 */
void Creature::SelectLevel(uint32 forcedLevel /*= USE_DEFAULT_DATABASE_LEVEL*/)
{
    CreatureInfo const* cinfo = GetCreatureInfo();
    if (!cinfo)
    {
        return;
    }

    uint32 rank = IsPet() ? 0 : cinfo->Rank;                // TODO :: IsPet probably not needed here

    uint32 const minlevel = cinfo->MinLevel;
    uint32 const maxlevel = cinfo->MaxLevel;

    stats::RankRates const rates = RatesFor(rank);

    const uint32 level = stats::CreatureLevel(minlevel, maxlevel,
                                              forcedLevel == USE_DEFAULT_DATABASE_LEVEL ? 0 : forcedLevel,
                                              minlevel == maxlevel ? minlevel : urand(minlevel, maxlevel));

    SetLevel(level);

    //////////////////////////////////////////////////////////////////////////
    // Calculate level dependent stats
    //////////////////////////////////////////////////////////////////////////

    stats::Vitals made;

    // TODO: Remove cinfo->ArmorMultiplier test workaround to disable classlevelstats when DB is ready
    CreatureClassLvlStats const* cCLS = sObjectMgr.GetCreatureClassLvlStats(level, cinfo->UnitClass);
    if (cinfo->ArmorMultiplier > 0 && cCLS)
    {
        made = stats::VitalsFromTable(cCLS->BaseHealth, cCLS->BaseMana,
                                      cinfo->HealthMultiplier, cinfo->PowerMultiplier);
    }
    else if (forcedLevel == USE_DEFAULT_DATABASE_LEVEL || (forcedLevel >= minlevel && forcedLevel <= maxlevel))
    {
        made = stats::VitalsFromBand(cinfo->MaxLevelHealth, cinfo->MinLevelHealth,
                                     cinfo->MaxLevelMana, cinfo->MinLevelMana,
                                     level, minlevel, maxlevel);
    }
    else
    {
        sLog.outError("Creature::SelectLevel> Error trying to set level(%u) for creature %s without enough data to do it!", level, GetGuidStr().c_str());
        // probably wrong
        made.health = (cinfo->MaxLevelHealth / cinfo->MaxLevel) * level;
        made.mana = (cinfo->MaxLevelMana / cinfo->MaxLevel) * level;
    }

    const uint32 health = stats::ScaledHealth(made.health, rates.health);
    const uint32 mana = made.mana;

    //////////////////////////////////////////////////////////////////////////
    // Set values
    //////////////////////////////////////////////////////////////////////////

    // health
    SetCreateHealth(health);
    SetMaxHealth(health);
    SetHealth(health);

    Tallied().Value(UNIT_MOD_HEALTH, BASE_VALUE, float(health));

    // all power types
    for (int i = POWER_MANA; i <= POWER_HAPPINESS; ++i)
    {
        uint32 maxValue;

        switch (i)
        {
            case POWER_MANA:        maxValue = mana; break;
            case POWER_RAGE:        maxValue = 0; break;
            case POWER_FOCUS:       maxValue = POWER_FOCUS_DEFAULT; break;
            case POWER_ENERGY:      maxValue = POWER_ENERGY_DEFAULT * cinfo->PowerMultiplier; break;
            case POWER_HAPPINESS:   maxValue = POWER_HAPPINESS_DEFAULT; break;
        }

        uint32 value = maxValue;

        // For non regenerating powers set 0
        if ((i == POWER_ENERGY || i == POWER_MANA) && !IsRegeneratingPower())
        {
            value = 0;
        }

        // Mana requires an extra field to be set
        if (i == POWER_MANA)
        {
            SetCreateMana(value);
        }

        SetMaxPower(Powers(i), maxValue);
        SetPower(Powers(i), value);
        Tallied().Value(UnitMods(UNIT_MOD_POWER_START + i), BASE_VALUE, float(value));
    }

    // damage
    float damagemod = rates.damage;

    SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, cinfo->MinMeleeDmg * damagemod);
    SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, cinfo->MaxMeleeDmg * damagemod);

    SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, cinfo->MinMeleeDmg * damagemod);
    SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, cinfo->MaxMeleeDmg * damagemod);

    SetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, cinfo->MinRangedDmg * damagemod);
    SetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, cinfo->MaxRangedDmg * damagemod);

    Tallied().Value(UNIT_MOD_ATTACK_POWER, BASE_VALUE, cinfo->MeleeAttackPower * damagemod);
}

/**
 * @brief What this server multiplies a rank's numbers by.
 *
 * The three rates are read together because they are one statement about how hard a rank is
 * meant to be, and because this is the only place in the level arithmetic that a
 * configuration is read at all: everything below the call is a function of the values.
 */
stats::RankRates Creature::RatesFor(int32 rank)
{
    stats::RankRates rates;

    switch (rank)
    {
        case CREATURE_ELITE_NORMAL:
            rates.health = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_NORMAL_HP);
            rates.damage = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_NORMAL_DAMAGE);
            rates.spellDamage = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_NORMAL_SPELLDAMAGE);
            break;
        case CREATURE_ELITE_RAREELITE:
            rates.health = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_HP);
            rates.damage = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_DAMAGE);
            rates.spellDamage = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_SPELLDAMAGE);
            break;
        case CREATURE_ELITE_WORLDBOSS:
            rates.health = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_HP);
            rates.damage = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_DAMAGE);
            rates.spellDamage = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_SPELLDAMAGE);
            break;
        case CREATURE_ELITE_RARE:
            rates.health = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_HP);
            rates.damage = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_DAMAGE);
            rates.spellDamage = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_SPELLDAMAGE);
            break;
        // An elite, and anything whose row names a rank this build does not know.
        case CREATURE_ELITE_ELITE:
        default:
            rates.health = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_HP);
            rates.damage = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_DAMAGE);
            rates.spellDamage = sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE);
            break;
    }

    return rates;
}
