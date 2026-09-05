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



#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>

/**
 * @brief Applies environmental damage to the player.
 *
 * @param type The environmental damage type.
 * @param damage The incoming damage amount.
 * @return The final damage dealt after mitigation.
 */
uint32 Player::EnvironmentalDamage(EnvironmentalDamageType type, uint32 damage)
{
    if (!IsAlive() || isGameMaster())
    {
        return 0;
    }

    // Absorb and resist some environmental damage types
    uint32 absorb = 0;
    uint32 resist = 0;
    if (type == DAMAGE_LAVA)
    {
        if (this->IsImmuneToDamage(SPELL_SCHOOL_MASK_FIRE))
        {
            return 0;
        }

        CalculateDamageAbsorbAndResist(this, SPELL_SCHOOL_MASK_FIRE, DIRECT_DAMAGE, damage, &absorb, &resist);
    }
    else if (type == DAMAGE_SLIME)
    {
        if (this->IsImmuneToDamage(SPELL_SCHOOL_MASK_NATURE))
        {
            return 0;
        }

        CalculateDamageAbsorbAndResist(this, SPELL_SCHOOL_MASK_NATURE, DIRECT_DAMAGE, damage, &absorb, &resist);
    }

    damage -= absorb + resist;

    DealDamageMods(this, damage, &absorb);

    WorldPacket data(SMSG_ENVIRONMENTALDAMAGELOG, (21));
    data << GetObjectGuid();
    data << uint8(type != DAMAGE_FALL_TO_VOID ? type : DAMAGE_FALL);
    data << uint32(damage);
    data << uint32(absorb);
    data << uint32(resist);
    Broadcast(*this, &data, true);

    DamageEffectType damageType = SELF_DAMAGE;
    if (type == DAMAGE_FALL && getClass() == CLASS_ROGUE)
    {
        damageType = SELF_DAMAGE_ROGUE_FALL;
    }

    uint32 final_damage = DealDamage(this, damage, nullptr, damageType, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);

    if (type == DAMAGE_FALL && !IsAlive()) // DealDamage does not apply item durability loss at self-damage
    {
        DEBUG_LOG("We fell to death, losing 10 percent durability");
        DurabilityLossAll(0.10f, false);
        // Durability lost message
        WorldPacket data2(SMSG_DURABILITY_DAMAGE_DEATH, 0);
        GetSession()->SendPacket(&data2);
    }

    return final_damage;
}

/// The player sobers by 256 every 10 seconds
void Player::HandleSobering()
{
    m_drunkTimer = 0;

    // Decrease the drunk value by 256, or set to 0 if less than 256
    uint32 drunk = (m_drunk <= 256) ? 0 : (m_drunk - 256);
    SetDrunkValue(drunk);
}

/**
 * @brief Converts a drunk value to the corresponding drunken state.
 *
 * @param value The raw drunk value.
 * @return The resulting drunken state.
 */
DrunkenState Player::GetDrunkenstateByValue(uint16 value)
{
    // Determine the drunken state based on the drunk value
    if (value >= 23000)
    {
        return DRUNKEN_SMASHED;
    }
    if (value >= 12800)
    {
        return DRUNKEN_DRUNK;
    }
    if (value & 0xFFFE)
    {
        return DRUNKEN_TIPSY;
    }
    return DRUNKEN_SOBER;
}

/**
 * @brief Sets the player's drunk value and updates related visibility effects.
 *
 * @param newDrunkenValue The new drunk value.
 * @param itemId Unused source item identifier.
 */
void Player::SetDrunkValue(uint16 newDrunkenValue, uint32 /*itemId*/)
{
    // Get the old drunken state
    uint32 oldDrunkenState = Player::GetDrunkenstateByValue(m_drunk);

    // Set the new drunk value
    m_drunk = newDrunkenValue;
    SetDrunkAndGender(m_drunk, getGender());

    // Get the new drunken state
    uint32 newDrunkenState = Player::GetDrunkenstateByValue(m_drunk);

    // Special drunk invisibility detection
    if (newDrunkenState >= DRUNKEN_DRUNK)
    {
        m_detectInvisibilityMask |= (1 << 6);
    }
    else
    {
        m_detectInvisibilityMask &= ~(1 << 6);
    }
}
