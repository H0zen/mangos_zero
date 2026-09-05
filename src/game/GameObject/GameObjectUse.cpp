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


#include <cmath>
#include "Utterance.h"
#include "Summoning.h"
#include "Utilities/Errors.h"
#include <sstream>
#include "Utilities/MathDefines.h"
#include "GameObject.h"
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "PoolManager.h"
#include "SpellMgr.h"
#include "Spell.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "LootMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "InstanceData.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Util.h"
#include "ScriptMgr.h"
#include "GameObjectModel.h"
#include "CreatureAISelector.h"
#include "SQLStorages.h"
#include "GameObjectAI.h"
#include "Geometry/Quat.h"
#include "AnimatedTraps.h"

/**
 * @brief Someone has used this object.
 *
 * What that comes to depends entirely on what kind of object it is, and each kind
 * answers for itself below. All that is common is what happens before -- the use
 * cooldown the template may impose, and the scripts that get a say -- and what may
 * happen after, which is one spell, cast at whoever used it.
 *
 * @param user The unit using the object.
 */
void GameObject::Use(Unit* user)
{
    // user must be provided
    MANGOS_ASSERT(user || PrintEntryError("GameObject::Use (without user)"));

    // traps and goobers are the only ones the template gives a use cooldown to
    if (uint32 cooldown = GetGOInfo()->GetCooldown())
    {
        if (m_usableAt > sWorld.GetGameTime())
        {
            return;
        }

        m_usableAt = sWorld.GetGameTime() + cooldown;
    }

    bool const scriptReturnValue = user->GetTypeId() == TYPEID_PLAYER && sScriptMgr.OnGameObjectUse((Player*)user, this);

    if (!scriptReturnValue)
    {
        GetMap()->ScriptsStart(DBS_ON_GOT_USE, GetEntry(), user, this);
    }

    // WHAT KIND IT IS WAS DECIDED WHEN ITS TEMPLATE WAS FIXED TO IT, not here. A door
    // does not ask what it is every time somebody opens it.
    GameObjectBehaviour::Casting const cast = m_behaviour->UsedBy(user, scriptReturnValue);

    if (!cast.spellId)
    {
        return;
    }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(cast.spellId);
    if (!spellInfo)
    {
        sLog.outError("WORLD: unknown spell id %u at use action for gameobject (Entry: %u GoType: %u )", cast.spellId, GetEntry(), GetGoType());
        return;
    }

    Spell* spell = new Spell(cast.caster, spellInfo, cast.triggered, GetObjectGuid());

    // spell target is user of GO
    SpellCastTargets targets;
    targets.setUnitTarget(user);

    spell->prepare(&targets);
}


