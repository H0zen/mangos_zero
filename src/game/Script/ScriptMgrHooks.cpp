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

#include "ScriptMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "Player.h"
#include "Policies/Singleton.h"
#include "Log.h"
#include "ProgressBar.h"
#include "ObjectMgr.h"
#include "WaypointManager.h"
#include "World.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "SQLStorages.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "WaypointMovementGenerator.h"
#include "Mail.h"
#ifdef ENABLE_SD3
#include "system/ScriptDevMgr.h"
#endif
#include "LFGMgr.h"

CreatureAI* ScriptMgr::GetCreatureAI(Creature* pCreature)
{

#ifdef ENABLE_SD3
    return SD3::GetCreatureAI(pCreature);
#else
    return nullptr;
#endif
}

GameObjectAI* ScriptMgr::GetGameObjectAI(GameObject* pGo)
{

#ifdef ENABLE_SD3
    return SD3::GetGameObjectAI(pGo);
#else
    return nullptr;
#endif
}

InstanceData* ScriptMgr::CreateInstanceData(Map* pMap)
{
#ifdef ENABLE_SD3
    return SD3::CreateInstanceData(pMap);
#else
    return nullptr;
#endif
}

bool ScriptMgr::OnGossipHello(Player* pPlayer, Creature* pCreature)
{

#ifdef ENABLE_SD3
    return SD3::GossipHello(pPlayer, pCreature);
#else
    return false;
#endif
}

bool ScriptMgr::OnGossipHello(Player* pPlayer, GameObject* pGameObject)
{

#ifdef ENABLE_SD3
    return SD3::GOGossipHello(pPlayer, pGameObject);
#else
    return false;
#endif
}

bool ScriptMgr::OnGossipHello(Player* pPlayer, Item* pItem)
{

#ifdef ENABLE_SD3
    return SD3::ItemGossipHello(pPlayer, pItem);
#else
    return false;
#endif
}

bool ScriptMgr::OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 sender, uint32 action, const char* code)
{

#ifdef ENABLE_SD3
    if (code)
    {
        return SD3::GossipSelectWithCode(pPlayer, pCreature, sender, action, code);
    }
    else
    {
        return SD3::GossipSelect(pPlayer, pCreature, sender, action);
    }
#else
    return false;
#endif
}

bool ScriptMgr::OnGossipSelect(Player* pPlayer, GameObject* pGameObject, uint32 sender, uint32 action, const char* code)
{

#ifdef ENABLE_SD3
    if (code)
    {
        return SD3::GOGossipSelectWithCode(pPlayer, pGameObject, sender, action, code);
    }
    else
    {
        return SD3::GOGossipSelect(pPlayer, pGameObject, sender, action);
    }
#else
    return false;
#endif
}

bool ScriptMgr::OnGossipSelect(Player* pPlayer, Item* pItem, uint32 sender, uint32 action, const char* code)
{

#ifdef ENABLE_SD3
    if (code)
    {
        return SD3::ItemGossipSelectWithCode(pPlayer, pItem, sender, action, code);
    }
    else
    {
        return SD3::ItemGossipSelect(pPlayer, pItem, sender, action);
    }
#else
    return false;
#endif
}

bool ScriptMgr::OnQuestAccept(Player* pPlayer, Creature* pCreature, Quest const* pQuest)
{

#ifdef ENABLE_SD3
    return SD3::QuestAccept(pPlayer, pCreature, pQuest);
#else
    return false;
#endif
}

bool ScriptMgr::OnQuestAccept(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest)
{

#ifdef ENABLE_SD3
    return SD3::GOQuestAccept(pPlayer, pGameObject, pQuest);
#else
    return false;
#endif
}

bool ScriptMgr::OnQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest)
{

#ifdef ENABLE_SD3
    return SD3::ItemQuestAccept(pPlayer, pItem, pQuest);
#else
    return false;
#endif
}

bool ScriptMgr::OnQuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest, uint32 reward)
{

#ifdef ENABLE_SD3
    return SD3::QuestRewarded(pPlayer, pCreature, pQuest);
#else
    return false;
#endif
}

bool ScriptMgr::OnQuestRewarded(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest, uint32 reward)
{

#ifdef ENABLE_SD3
    return SD3::GOQuestRewarded(pPlayer, pGameObject, pQuest);
#else
    return false;
#endif
}

uint32 ScriptMgr::GetDialogStatus(Player* pPlayer, Creature* pCreature)
{

#ifdef ENABLE_SD3
    return SD3::GetNPCDialogStatus(pPlayer, pCreature);
#else
    return DIALOG_STATUS_UNDEFINED;
#endif
}

uint32 ScriptMgr::GetDialogStatus(Player* pPlayer, GameObject* pGameObject)
{

#ifdef ENABLE_SD3
    return SD3::GetGODialogStatus(pPlayer, pGameObject);
#else
    return DIALOG_STATUS_UNDEFINED;
#endif
}

bool ScriptMgr::OnGameObjectUse(Player* pPlayer, GameObject* pGameObject)
{

#ifdef ENABLE_SD3
    return SD3::GOUse(pPlayer, pGameObject);
#else
    return false;
#endif
}

bool ScriptMgr::OnGameObjectUse(Unit* pUnit, GameObject* pGameObject)
{

#ifdef ENABLE_SD3
    return SD3::GOUse(pUnit, pGameObject);
#else
    return false;
#endif
}

bool ScriptMgr::OnItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets)
{

#ifdef ENABLE_SD3
    return SD3::ItemUse(pPlayer, pItem, targets);
#else
    return false;
#endif
}

bool ScriptMgr::OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry)
{

#ifdef ENABLE_SD3
    return SD3::AreaTrigger(pPlayer, atEntry);
#else
    return false;
#endif
}

bool ScriptMgr::OnNpcSpellClick(Player* pPlayer, Creature* pClickedCreature, uint32 spellId)
{
#ifdef ENABLE_SD3
    return SD3::NpcSpellClick(pPlayer, pClickedCreature, spellId);
#else
    return false;
#endif
}

bool ScriptMgr::OnProcessEvent(uint32 eventId, Object* pSource, Object* pTarget, bool isStart)
{
#ifdef ENABLE_SD3
    return SD3::ProcessEvent(eventId, pSource, pTarget, isStart);
#else
    return false;
#endif
}

bool ScriptMgr::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid)
{

#ifdef ENABLE_SD3
    return SD3::EffectDummyUnit(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}

bool ScriptMgr::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, GameObject* pTarget, ObjectGuid originalCasterGuid)
{

#ifdef ENABLE_SD3
    return SD3::EffectDummyGameObject(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}

bool ScriptMgr::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Item* pTarget, ObjectGuid originalCasterGuid)
{

#ifdef ENABLE_SD3
    return SD3::EffectDummyItem(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}

bool ScriptMgr::OnEffectScriptEffect(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid)
{
#ifdef ENABLE_SD3
    return SD3::EffectScriptEffectUnit(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}

bool ScriptMgr::OnAuraDummy(Aura const* pAura, bool apply)
{
#ifdef ENABLE_SD3
    return SD3::AuraDummy(pAura, apply);
#else
    return false;
#endif
}
