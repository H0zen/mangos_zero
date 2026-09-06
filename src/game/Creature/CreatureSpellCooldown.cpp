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



#include "Creature.h"
#include "DBCStores.h"
#include "SpellMgr.h"
#include "LivingWorldAnchorPolicy.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include "ObjectGuid.h"
#include "SQLStorages.h"
#include "GossipDef.h"
#include "Player.h"
#include "GameEventMgr.h"
#include "PoolManager.h"
#include "Opcodes.h"
#include "Log.h"
#include "LootMgr.h"
#include "MapManager.h"
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
 * @brief Adds cooldown tracking for a creature spell and its category.
 *
 * @param spellid The spell identifier.
 */
void Unit::AddCreatureSpellCooldown(uint32 spellid)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellid);
    if (!spellInfo)
    {
        return;
    }

    time_t const now = time(nullptr);

    if (uint32 const cooldown = GetSpellRecoveryTime(spellInfo))
    {
        m_repertoire.ReadyAt(spellid, now + cooldown / IN_MILLISECONDS);
    }

    if (spellInfo->Category)
    {
        m_repertoire.CategoryUsedAt(spellInfo->Category, now);
    }
}

/**
 * @brief Checks whether a spell category cooldown is still active.
 *
 * @param spell_id The spell identifier.
 * @return true if the category cooldown is active; otherwise, false.
 */
bool Unit::HasCategoryCooldown(uint32 spell_id) const
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);
    if (!spellInfo)
    {
        return false;
    }

    return m_repertoire.CategoryDown(spellInfo->Category,
                                     spellInfo->CategoryRecoveryTime / IN_MILLISECONDS,
                                     time(nullptr));
}

/**
 * @brief Checks whether a spell or its category is currently on cooldown.
 *
 * @param spell_id The spell identifier.
 * @return true if a cooldown is active; otherwise, false.
 */
bool Unit::HasSpellCooldown(uint32 spell_id) const
{
    return m_repertoire.SpellDown(spell_id, time(nullptr)) || HasCategoryCooldown(spell_id);
}
