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
#include <vector>
#include "Reaction.h"
#include "Utterance.h"
#include "Summoning.h"
#include "Utilities/MathDefines.h"
#include "ScriptMgr.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "Mail.h"
#include "WaypointManager.h"
#include "WaypointMovementGenerator.h"
#include "Policies/Singleton.h"
#include "ProgressBar.h"
#include "World.h"
#include "SQLStorages.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "CorpseManager.h"
#include "LFGMgr.h"
#ifdef ENABLE_SD3
#include "system/ScriptDevMgr.h"
#endif

bool ScriptAction::GetScriptCommandObject(const ObjectGuid guid, bool includeItem, Object*& resultObject)
{
    resultObject = nullptr;

    if (!guid)
    {
        return true;
    }

    switch (GuidHigh(guid))
    {
        case HIGHGUID_UNIT:
            resultObject = m_map->GetCreature(guid);
            break;
        case HIGHGUID_PET:
            resultObject = m_map->GetPet(guid);
            break;
        case HIGHGUID_PLAYER:
            resultObject = m_map->GetPlayer(guid);
            break;
        case HIGHGUID_GAMEOBJECT:
            resultObject = m_map->GetGameObject(guid);
            break;
        case HIGHGUID_CORPSE:
            resultObject = sCorpseManager.Find(guid);
            break;
        case HIGHGUID_ITEM:
        {
            if (includeItem)
            {
                if (Player* player = m_map->GetPlayer(m_ownerGuid))
                {
                    resultObject = player->GetItemByGuid(guid);
                }
                break;
            }

        }
        default:
            sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u with unsupported guid %s, skipping", m_type, m_script->id, m_script->command, GuidString(guid).c_str());
            return false;
    }

    if (resultObject && !resultObject->IsInWorld())
    {
        resultObject = nullptr;
    }

    return true;
}

bool ScriptAction::GetScriptProcessTargets(Occupant* pOrigSource, Occupant* pOrigTarget, Occupant*& pFinalSource, Occupant*& pFinalTarget)
{
    Occupant* pBuddy = nullptr;

    if (m_script->buddyEntry)
    {
        if (m_script->data_flags & SCRIPT_FLAG_BUDDY_BY_GUID)
        {
            if (m_script->IsCreatureBuddy())
            {
                CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(m_script->buddyEntry);

                if (cinfo != nullptr)
                {
                    pBuddy = m_map->GetCreature(cinfo->GetObjectGuid(m_script->searchRadiusOrGuid));

                    if (pBuddy && !((Creature*)pBuddy)->IsAlive())
                    {
                        sLog.outError(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has buddy %u by guid %u but buddy is dead, skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                        return false;
                    }
                }
                else
                {
                    sLog.outError(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has no buddy %u by guid %u, skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                    return false;
                }
            }
            else
            {

                pBuddy = m_map->GetGameObject(MakeGuid(HIGHGUID_GAMEOBJECT, m_script->buddyEntry, m_script->searchRadiusOrGuid));
            }

            if (!pBuddy)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has buddy %u by guid %u not loaded in map %u (data-flags %u), skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid, m_map->GetId(), m_script->data_flags);
                return false;
            }
        }
        else
        {
            if (!pOrigSource && !pOrigTarget)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u called without buddy %u, but no source for search available, skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry);
                return false;
            }

            Occupant* pSearcher = pOrigSource ? pOrigSource : pOrigTarget;
            if (pOrigSource && pOrigTarget &&IsPlayer(pOrigSource) && !IsPlayer(pOrigTarget))
            {
                pSearcher = pOrigTarget;
            }

            if (m_script->IsCreatureBuddy())
            {
                Creature* pCreatureBuddy = nullptr;

                if (m_script->data_flags & SCRIPT_FLAG_BUDDY_IS_DESPAWNED)
                {
                    MaNGOS::AllCreaturesOfEntryInRangeCheck u_check(pSearcher, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                    MaNGOS::CreatureLastSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(pCreatureBuddy, u_check);
                    Cell::VisitGridObjects(pSearcher, searcher, m_script->searchRadiusOrGuid);
                }
                else
                {
                    MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*pSearcher, m_script->buddyEntry, true, false, m_script->searchRadiusOrGuid, true);
                    MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(pCreatureBuddy, u_check);

                    if (m_script->data_flags & SCRIPT_FLAG_BUDDY_IS_PET)
                    {
                        Cell::VisitWorldObjects(pSearcher, searcher, m_script->searchRadiusOrGuid);
                    }
                    else
                    {
                        Cell::VisitGridObjects(pSearcher, searcher, m_script->searchRadiusOrGuid);
                    }
                }

                pBuddy = pCreatureBuddy;

                if (!pBuddy && pSearcher->GetEntry() == m_script->buddyEntry)
                {
                    sLog.outErrorDb(" DB-SCRIPTS: WARNING: Process table `db_scripts [type = %d]` id %u, command %u has no OTHER buddy %u found - maybe you need to update the script?", m_type, m_script->id, m_script->command, m_script->buddyEntry);
                    pBuddy = pSearcher;
                }
            }
            else
            {
                GameObject* pGOBuddy = nullptr;

                MaNGOS::NearestGameObjectEntryInObjectRangeCheck u_check(*pSearcher, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> searcher(pGOBuddy, u_check);

                Cell::VisitGridObjects(pSearcher, searcher, m_script->searchRadiusOrGuid);
                pBuddy = pGOBuddy;
            }

            if (!pBuddy)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has buddy %u not found in range %u of searcher %s (data-flags %u), skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid, pSearcher->GetGuidStr().c_str(), m_script->data_flags);
                return false;
            }
        }
    }

    if (m_script->data_flags & SCRIPT_FLAG_BUDDY_AS_TARGET)
    {
        pFinalSource = pOrigSource;
        pFinalTarget = pBuddy;
    }
    else
    {
        pFinalSource = pBuddy ? pBuddy : pOrigSource;
        pFinalTarget = pOrigTarget;
    }

    if (m_script->data_flags & SCRIPT_FLAG_REVERSE_DIRECTION)
    {
        std::swap(pFinalSource, pFinalTarget);
    }

    if (m_script->data_flags & SCRIPT_FLAG_SOURCE_TARGETS_SELF)
    {
        pFinalTarget = pFinalSource;
    }

    return true;
}

bool ScriptAction::LogIfNotCreature(Occupant* pOccupant)
{
    if (!pOccupant || !IsCreature(pOccupant))
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-creature, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}

bool ScriptAction::LogIfNotUnit(Occupant* pOccupant)
{
    if (!pOccupant || !IsType(pOccupant, TYPEMASK_UNIT))
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-unit, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}

bool ScriptAction::LogIfNotGameObject(Occupant* pOccupant)
{
    if (!pOccupant || !IsGameObject(pOccupant))
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-gameobject, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}

bool ScriptAction::LogIfNotPlayer(Occupant* pOccupant)
{
    if (!pOccupant || !IsPlayer(pOccupant))
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-player, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}

Player* ScriptAction::GetPlayerTargetOrSourceAndLog(Occupant* pSource, Occupant* pTarget)
{
    if ((!pTarget || !IsPlayer(pTarget)) && (!pSource || !IsPlayer(pSource)))
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non player, skipping.", m_type, m_script->id, m_script->command);
        return nullptr;
    }

    return pTarget &&IsPlayer(pTarget) ? (Player*)pTarget : (Player*)pSource;
}

bool ScriptAction::HandleScriptStep()
{
    Occupant* pSource;
    Occupant* pTarget;
    Object* pSourceOrItem;

    {

        Object* source = nullptr;
        Object* target = nullptr;
        if (!GetScriptCommandObject(m_sourceGuid, true, source))
        {
            return false;
        }
        if (!GetScriptCommandObject(m_targetGuid, false, target))
        {
            return false;
        }

        DEBUG_FILTER_LOG(LOG_FILTER_DB_SCRIPTS, "DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u for source %s (%sin world), target %s (%sin world)", m_type, m_script->id, m_script->command, GuidString(m_sourceGuid).c_str(), source ? "" : "not ", GuidString(m_targetGuid).c_str(), target ? "" : "not ");

        pSource = source && IsType(source, TYPEMASK_PRESENCE) ? static_cast<Occupant*>(source) : nullptr;
        pTarget = target && IsType(target, TYPEMASK_PRESENCE) ? static_cast<Occupant*>(target) : nullptr;
        if (!GetScriptProcessTargets(pSource, pTarget, pSource, pTarget))
        {
            return false;
        }

        pSourceOrItem = pSource ? pSource : (source && IsType(source, TYPEMASK_ITEM) ? source : nullptr);
    }

    switch (m_script->command)
    {
        case SCRIPT_COMMAND_TALK:
        {
            if (!pSource)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u found no occupant as source, skipping.", m_type, m_script->id, m_script->command);
                break;
            }

            Unit* unitTarget = pTarget && IsType(pTarget, TYPEMASK_UNIT) ? static_cast<Unit*>(pTarget) : nullptr;
            int32 textId = m_script->textId[0];

            if (m_script->textId[1])
            {
                int i = 2;
                for (; i < MAX_TEXT_ID; ++i)
                {
                    if (!m_script->textId[i])
                    {
                        break;
                    }
                }

                textId = m_script->textId[urand(0, i - 1)];
            }

            if (!DoDisplayText(pSource, textId, unitTarget))
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, could not display text %i properly", m_type, m_script->id, textId);
            }
            break;
        }
        case SCRIPT_COMMAND_EMOTE:
        {
            if (LogIfNotUnit(pSource))
            {
                break;
            }

            std::vector<uint32> emotes;
            emotes.push_back(m_script->emote.emoteId);
            for (int i = 0; i < MAX_TEXT_ID; ++i)
            {
                if (!m_script->textId[i])
                {
                    break;
                }
                emotes.push_back(uint32(m_script->textId[i]));
            }

            ((Unit*)pSource)->HandleEmote(emotes[urand(0, emotes.size() - 1)]);
            break;
        }
        case SCRIPT_COMMAND_FIELD_SET:
            if (!pSourceOrItem)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for nullptr object.", m_type, m_script->id, m_script->command);
                break;
            }
            if (m_script->setField.fieldId <= OBJECT_FIELD_ENTRY || m_script->setField.fieldId >= pSourceOrItem->GetValuesCount())
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for wrong field %u (max count: %u) in %s.",
                    m_type, m_script->id, m_script->command, m_script->setField.fieldId, pSourceOrItem->GetValuesCount(), pSourceOrItem->GetGuidStr().c_str());
                break;
            }
            pSourceOrItem->SetUInt32Value(m_script->setField.fieldId, m_script->setField.fieldValue);
            break;
        case SCRIPT_COMMAND_MOVE_TO:
        {
            if (LogIfNotUnit(pSource))
            {
                break;
            }

            if ((m_script->x == 0.0f && m_script->y == 0.0f && m_script->z == 0.0f) ||

                ((Unit*)pSource)->Where().WithinDist(Geometry::Vector3(m_script->x, m_script->y, m_script->z), 0.01f - ((Unit*)pSource)->Where().Extent()))
            {
                ((Unit*)pSource)->SetFacingTo(m_script->o);
                break;
            }

            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                ((Unit*)pSource)->NearTeleportTo(m_script->x, m_script->y, m_script->z, m_script->o != 0.0f ? m_script->o : ((Unit*)pSource)->Where().Facing());
                break;
            }

            if (m_script->moveTo.travelSpeed)
            {
                ((Unit*)pSource)->MonsterMoveWithSpeed(m_script->x, m_script->y, m_script->z, m_script->moveTo.travelSpeed * 0.01f);
            }
            else
            {
                ((Unit*)pSource)->GetMotionMaster()->Clear();
                ((Unit*)pSource)->GetMotionMaster()->MovePoint(0, m_script->x, m_script->y, m_script->z);
            }
            break;
        }
        case SCRIPT_COMMAND_FLAG_SET:
            if (!pSourceOrItem)
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_SET (script id %u) call for nullptr object.", m_script->id);
                break;
            }
            if (m_script->setFlag.fieldId <= OBJECT_FIELD_ENTRY || m_script->setFlag.fieldId >= pSourceOrItem->GetValuesCount())
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_SET (script id %u) call for wrong field %u (max count: %u) in %s.",
                    m_script->id, m_script->setFlag.fieldId, pSourceOrItem->GetValuesCount(), pSourceOrItem->GetGuidStr().c_str());
                break;
            }
            pSourceOrItem->SetFlag(m_script->setFlag.fieldId, m_script->setFlag.fieldValue);
            break;
        case SCRIPT_COMMAND_FLAG_REMOVE:
            if (!pSourceOrItem)
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_REMOVE (script id %u) call for nullptr object.", m_script->id);
                break;
            }
            if (m_script->removeFlag.fieldId <= OBJECT_FIELD_ENTRY || m_script->removeFlag.fieldId >= pSourceOrItem->GetValuesCount())
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_REMOVE (script id %u) call for wrong field %u (max count: %u) in %s.",
                    m_script->id, m_script->removeFlag.fieldId, pSourceOrItem->GetValuesCount(), pSourceOrItem->GetGuidStr().c_str());
                break;
            }
            pSourceOrItem->RemoveFlag(m_script->removeFlag.fieldId, m_script->removeFlag.fieldValue);
            break;
        case SCRIPT_COMMAND_TELEPORT_TO:
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            pPlayer->TeleportTo(m_script->teleportTo.mapId, m_script->x, m_script->y, m_script->z, m_script->o);
            break;
        }
        case SCRIPT_COMMAND_QUEST_EXPLORED:
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            Occupant* pOccupant = nullptr;
            if (pSource && IsType(pSource, TYPEMASK_CREATURE_OR_GAMEOBJECT))
            {
                pOccupant = pSource;
            }
            else if (pTarget && IsType(pTarget, TYPEMASK_CREATURE_OR_GAMEOBJECT))
            {
                pOccupant = pTarget;
            }

            if (m_script->questExplored.distance != 0 && !pOccupant)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u called without source occupant, skipping.", m_type, m_script->id, m_script->command);
                break;
            }

            bool failQuest = false;

            if (pOccupant &&IsCreature(pOccupant) && !((Creature*)pOccupant)->IsAlive())
            {
                failQuest = true;
            }
            else if (m_script->questExplored.distance != 0 && !InReach(*pOccupant, *pPlayer, float(m_script->questExplored.distance)))
            {
                failQuest = true;
            }

            if (!failQuest)
            {
                pPlayer->Journal().Explored(m_script->questExplored.questId);
            }
            else
            {
                pPlayer->FailQuest(m_script->questExplored.questId);
            }

            break;
        }
        case SCRIPT_COMMAND_KILL_CREDIT:
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            uint32 creatureEntry = m_script->killCredit.creatureEntry;
            Occupant* pRewardSource = pSource &&IsCreature(pSource) ? pSource : (pTarget &&IsCreature(pTarget) ? pTarget : nullptr);

            if (!creatureEntry)
            {
                if (pRewardSource)
                {
                    creatureEntry =  pRewardSource->GetEntry();
                }
                else
                {
                    sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u called for dynamic killcredit without creature partner, skipping.", m_type, m_script->id, m_script->command);
                    break;
                }
            }

            if (m_script->killCredit.isGroupCredit)
            {
                Occupant* pSearcher = pRewardSource ? pRewardSource : (pSource ? pSource : pTarget);
                if (pSearcher != pRewardSource)
                {
                    sLog.outDebug(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, SCRIPT_COMMAND_KILL_CREDIT called for groupCredit without creature as searcher, script might need adjustment.", m_type, m_script->id);
                }
                pPlayer->RewardPlayerAndGroupAtEvent(creatureEntry, pSearcher);
            }
            else
            {
                pPlayer->Journal().KillCredited(creatureEntry, pRewardSource ? pRewardSource->GetObjectGuid() : 0);
            }

            break;
        }
        case SCRIPT_COMMAND_RESPAWN_GO:
        {
            GameObject* pGo;
            if (m_script->respawnGo.goGuid)
            {
                GameObjectData const* goData = sObjectMgr.GetGOData(m_script->respawnGo.goGuid);
                if (!goData)
                {
                    break;
                }

                pGo = m_map->GetGameObject(MakeGuid(HIGHGUID_GAMEOBJECT, goData->id, m_script->respawnGo.goGuid));
            }
            else
            {
                if (LogIfNotGameObject(pSource))
                {
                    break;
                }

                pGo = (GameObject*)pSource;
            }

            if (!pGo)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for gameobject(guid: %u, buddyEntry: %u).", m_type, m_script->id, m_script->command, m_script->respawnGo.goGuid, m_script->buddyEntry);
                break;
            }

            if (pGo->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE ||
                pGo->GetGoType() == GAMEOBJECT_TYPE_DOOR)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u can not be used with gameobject of type %u (guid: %u, buddyEntry: %u).", m_type, m_script->id, m_script->command, uint32(pGo->GetGoType()), m_script->respawnGo.goGuid, m_script->buddyEntry);
                break;
            }

            if (pGo->isSpawned())
            {
                break;
            }

            uint32 time_to_despawn = m_script->respawnGo.despawnDelay < 5 ? 5 : m_script->respawnGo.despawnDelay;

            pGo->SetLootState(GO_READY);
            pGo->SetRespawnTime(time_to_despawn);
            pGo->Refresh();
            break;
        }
        case SCRIPT_COMMAND_TEMP_SUMMON_CREATURE:
        {
            if (!pSource)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u found no occupant as source, skipping.", m_type, m_script->id, m_script->command);
                break;
            }

            float x = m_script->x;
            float y = m_script->y;
            float z = m_script->z;
            float o = m_script->o;

            Creature* pCreature = SummonCreature(*pSource, m_script->summonCreature.creatureEntry, x, y, z, o, m_script->summonCreature.despawnDelay ? TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN : TEMPSPAWN_DEAD_DESPAWN, m_script->summonCreature.despawnDelay, (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL) ? true : false, m_script->textId[0] != 0);
            if (!pCreature)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for creature (entry: %u).", m_type, m_script->id, m_script->command, m_script->summonCreature.creatureEntry);
                break;
            }

            break;
        }
        case SCRIPT_COMMAND_OPEN_DOOR:
        case SCRIPT_COMMAND_CLOSE_DOOR:
        {
            GameObject* pDoor;
            uint32 time_to_reset = m_script->changeDoor.resetDelay < 15 ? 15 : m_script->changeDoor.resetDelay;

            if (m_script->changeDoor.goGuid)
            {
                GameObjectData const* goData = sObjectMgr.GetGOData(m_script->changeDoor.goGuid);
                if (!goData)
                {
                    break;
                }

                pDoor = m_map->GetGameObject(MakeGuid(HIGHGUID_GAMEOBJECT, goData->id, m_script->changeDoor.goGuid));
            }
            else
            {
                if (LogIfNotGameObject(pSource))
                {
                    break;
                }

                pDoor = (GameObject*)pSource;
            }

            if (!pDoor)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for gameobject(guid: %u, buddyEntry: %u).", m_type, m_script->id, m_script->command, m_script->changeDoor.goGuid, m_script->buddyEntry);
                break;
            }

            if (pDoor->GetGoType() != GAMEOBJECT_TYPE_DOOR)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for non-door(GoType: %u).", m_type, m_script->id, m_script->command, pDoor->GetGoType());
                break;
            }

            if ((m_script->command == SCRIPT_COMMAND_OPEN_DOOR && pDoor->GetGoState() != GO_STATE_READY) ||
                (m_script->command == SCRIPT_COMMAND_CLOSE_DOOR && pDoor->GetGoState() == GO_STATE_READY))
            {
                break;
            }

            pDoor->UseDoorOrButton(time_to_reset);

            if (pTarget && IsType(pTarget, TYPEMASK_GAMEOBJECT) && ((GameObject*)pTarget)->GetGoType() == GAMEOBJECT_TYPE_BUTTON)
            {
                ((GameObject*)pTarget)->UseDoorOrButton(time_to_reset);
            }

            break;
        }
        case SCRIPT_COMMAND_ACTIVATE_OBJECT:
        {
            if (LogIfNotUnit(pSource))
            {
                break;
            }
            if (LogIfNotGameObject(pTarget))
            {
                break;
            }

            ((GameObject*)pTarget)->Use((Unit*)pSource);
            break;
        }
        case SCRIPT_COMMAND_REMOVE_AURA:
        {
            if (LogIfNotUnit(pSource))
            {
                break;
            }

            ((Unit*)pSource)->RemoveAuras(m_script->removeAura.spellId);
            break;
        }
        case SCRIPT_COMMAND_CAST_SPELL:
        {
            if (LogIfNotUnit(pTarget))
            {
                break;
            }

            uint32 spell = m_script->castSpell.spellId;
            uint32 filledCount = 0;
            while (filledCount < MAX_TEXT_ID && m_script->textId[filledCount])
            {
                ++filledCount;
            }

            if (filledCount > 0)
            {
                if (uint32 randomField = urand(0, filledCount))
                {
                    spell = m_script->textId[randomField - 1];
                }
            }

            if (pSource &&IsGameObject(pSource))
            {
                ((Unit*)pTarget)->CastSpell(((Unit*)pTarget), spell, true, nullptr, nullptr, pSource->GetObjectGuid());
                {
                    break;
                }
            }

            if (LogIfNotUnit(pSource))
            {
                break;
            }
            ((Unit*)pSource)->CastSpell(((Unit*)pTarget), spell, (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL) != 0);

            break;
        }
        case SCRIPT_COMMAND_PLAY_SOUND:
        {
            if (!pSource)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u could not find proper source", m_type, m_script->id, m_script->command);
                break;
            }

            Player* pSoundTarget = nullptr;
            if (m_script->playSound.flags & 1)
            {
                pSoundTarget = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
                if (!pSoundTarget)
                {
                    break;
                }
            }

            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                PlaySound(*pSource, SoundKind::Music, m_script->playSound.soundId, pSoundTarget);
            }
            else
            {
                if (m_script->playSound.flags & 2)
                {
                    PlaySound(*pSource, SoundKind::AtObject, m_script->playSound.soundId, pSoundTarget);
                }
                else if (m_script->playSound.flags & (4 | 8))
                {
                    PlaySoundToMap(*m_map, m_script->playSound.soundId, (m_script->playSound.flags & 8) ? pSource->GetTerrain()->GetZoneId(pSource->Where().X(), pSource->Where().Y(), pSource->Where().Z()) : 0);
                }
                else
                {
                    PlaySound(*pSource, SoundKind::Flat, m_script->playSound.soundId, pSoundTarget);
                }
            }
            break;
        }
        case SCRIPT_COMMAND_CREATE_ITEM:
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            if (Item* pItem = pPlayer->StoreNewItemInInventorySlot(m_script->createItem.itemEntry, m_script->createItem.amount))
            {
                pPlayer->SendNewItem(pItem, m_script->createItem.amount, true, false);
            }

            break;
        }
        case SCRIPT_COMMAND_DESPAWN_SELF:
        {

            if (pTarget && !IsCreature(pTarget) && pSource &&IsCreature(pSource))
            {
                sLog.outErrorDb("DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u target must be creature, but (only) source is, use data_flags to fix", m_type, m_script->id, m_script->command);
                pTarget = pSource;
            }

            if (LogIfNotCreature(pTarget))
            {
                break;
            }

            ((Creature*)pTarget)->ForcedDespawn(m_script->despawn.despawnDelay);

            break;
        }
        case SCRIPT_COMMAND_PLAY_MOVIE:
        {
            break;
        }
        case SCRIPT_COMMAND_MOVEMENT:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            switch (m_script->movement.movementType)
            {
                case IDLE_MOTION_TYPE:
                    ((Creature*)pSource)->GetMotionMaster()->MoveIdle();
                    break;
                case RANDOM_MOTION_TYPE:
                    if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                    {
                        ((Creature*)pSource)->GetMotionMaster()->MoveRandomAroundPoint(pSource->Where().X(), pSource->Where().Y(), pSource->Where().Z(), float(m_script->movement.wanderDistance));
                    }
                    else
                    {
                        float respX, respY, respZ, respO, wander_distance;
                        Creature* pRespawnOwner = (Creature*)pSource;
                        respX = pRespawnOwner->Spawn().X();
                        respY = pRespawnOwner->Spawn().Y();
                        respZ = pRespawnOwner->Spawn().Z();
                        respO = pRespawnOwner->Spawn().Facing();
                        wander_distance = pRespawnOwner->GetRespawnRadius();
                        wander_distance = m_script->movement.wanderDistance ? m_script->movement.wanderDistance : wander_distance;
                        ((Creature*)pSource)->GetMotionMaster()->MoveRandomAroundPoint(respX, respY, respZ, wander_distance);
                    }
                    break;
                case WAYPOINT_MOTION_TYPE:
                    ((Creature*)pSource)->GetMotionMaster()->MoveWaypoint();
                    break;
            }

            break;
        }
        case SCRIPT_COMMAND_SET_ACTIVEOBJECT:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            ((Creature*)pSource)->SetActiveObjectState(m_script->activeObject.activate);
            break;
        }
        case SCRIPT_COMMAND_SET_FACTION:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            if (m_script->faction.factionId)
            {
                ((Creature*)pSource)->SetFactionTemporary(m_script->faction.factionId, m_script->faction.flags);
            }
            else
            {
                ((Creature*)pSource)->ClearTemporaryFaction();
            }

            break;
        }
        case SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            if (!m_script->morph.creatureOrModelEntry)
            {
                ((Creature*)pSource)->DeMorph();
            }
            else if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                ((Creature*)pSource)->SetDisplayId(m_script->morph.creatureOrModelEntry);
            }
            else
            {
                CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_script->morph.creatureOrModelEntry);
                uint32 display_id = Creature::ChooseDisplayId(ci);

                ((Creature*)pSource)->SetDisplayId(display_id);
            }

            break;
        }
        case SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            if (!m_script->mount.creatureOrModelEntry)
            {
                ((Creature*)pSource)->Unmount();
            }
            else if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                ((Creature*)pSource)->Mount(m_script->mount.creatureOrModelEntry);
            }
            else
            {
                CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_script->mount.creatureOrModelEntry);
                uint32 display_id = Creature::ChooseDisplayId(ci);

                ((Creature*)pSource)->Mount(display_id);
            }

            break;
        }
        case SCRIPT_COMMAND_SET_RUN:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            ((Creature*)pSource)->SetWalk(!m_script->run.run, true);

            break;
        }
        case SCRIPT_COMMAND_ATTACK_START:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }
            if (LogIfNotUnit(pTarget))
            {
                break;
            }

            Creature* pAttacker = static_cast<Creature*>(pSource);
            Unit* unitTarget = static_cast<Unit*>(pTarget);

            if (IsFriendly(*pAttacker, *unitTarget))
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u attacker is friendly to target, can not attack (Attacker: %s, Target: %s)", m_type, m_script->id, m_script->command, pAttacker->GetGuidStr().c_str(), unitTarget->GetGuidStr().c_str());
                break;
            }

            pAttacker->AI()->AttackStart(unitTarget);

            break;
        }
        case SCRIPT_COMMAND_GO_LOCK_STATE:
        {
            if (LogIfNotGameObject(pSource))
            {
                break;
            }

            GameObject* pGo = static_cast<GameObject*>(pSource);

            if (m_script->goLockState.lockState & 0x01)
            {
                pGo->SetGoFlag(GO_FLAG_LOCKED);
            }
            else if (m_script->goLockState.lockState & 0x02)
            {
                pGo->RemoveGoFlag(GO_FLAG_LOCKED);
            }

            if (m_script->goLockState.lockState & 0x04)
            {
                pGo->SetGoFlag(GO_FLAG_NO_INTERACT);
            }
            else if (m_script->goLockState.lockState & 0x08)
            {
                pGo->RemoveGoFlag(GO_FLAG_NO_INTERACT);
            }

            break;
        }
        case SCRIPT_COMMAND_STAND_STATE:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            ((Unit*)pSource)->SetStandState(m_script->standState.stand_state);
            break;
        }
        case SCRIPT_COMMAND_MODIFY_NPC_FLAGS:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            Creature* pCreature = static_cast<Creature*>(pSource);

            if (m_script->npcFlag.change_flag & 0x01)
            {
                pCreature->SetNpcFlag(m_script->npcFlag.flag);
            }

            else if (m_script->npcFlag.change_flag & 0x02)
            {
                pCreature->RemoveNpcFlag(m_script->npcFlag.flag);
            }

            else
            {
                pCreature->ApplyNpcFlag(m_script->npcFlag.flag,
                                        !pCreature->HasNpcFlag(m_script->npcFlag.flag));
            }

            break;
        }
        case SCRIPT_COMMAND_SEND_TAXI_PATH:
        {

            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            pPlayer->ActivateTaxiPathTo(m_script->sendTaxiPath.taxiPathId);
            break;
        }
        case SCRIPT_COMMAND_TERMINATE_SCRIPT:
        {
            bool result = false;
            if (m_script->terminateScript.npcEntry)
            {
                Occupant* pSearcher = pSource ? pSource : pTarget;
                if (!pSearcher)
                {
                    sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u called without source or target for npc search %u in range %u, skipping.",
                        m_type, m_script->id, m_script->command, m_script->terminateScript.npcEntry, m_script->terminateScript.searchDist);
                    break;
                }

                if (IsPlayer(pSearcher) && pTarget && !IsPlayer(pTarget))
                {
                    pSearcher = pTarget;
                }

                Creature* pCreatureBuddy = nullptr;
                MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*pSearcher, m_script->terminateScript.npcEntry, true, false, m_script->terminateScript.searchDist, true);
                MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(pCreatureBuddy, u_check);
                Cell::VisitGridObjects(pSearcher, searcher, m_script->terminateScript.searchDist);

                if (!(m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL) && !pCreatureBuddy)
                {
                    DEBUG_FILTER_LOG(LOG_FILTER_DB_SCRIPTS, "DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, terminate further steps of this script! (as searched other npc %u was not found alive)", m_type, m_script->id, m_script->terminateScript.npcEntry);
                    result = true;
                }
                else if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL && pCreatureBuddy)
                {
                    DEBUG_FILTER_LOG(LOG_FILTER_DB_SCRIPTS, "DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, terminate further steps of this script! (as searched other npc %u was found alive)", m_type, m_script->id, m_script->terminateScript.npcEntry);
                    result = true;
                }
            }
            else
            {
                result = true;
            }

            if (result)
            {
                if (m_script->textId[0] && !LogIfNotCreature(pSource))
                {
                    Creature* cSource = static_cast<Creature*>(pSource);
                    if (cSource->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
                    {
                        (static_cast<WaypointMovementGenerator* >(cSource->GetMotionMaster()->top()))->AddToWaypointPauseTime(m_script->textId[0]);
                    }
                }

                return true;
            }

            break;
        }
        case SCRIPT_COMMAND_PAUSE_WAYPOINTS:
        {
            if (LogIfNotCreature(pSource))
            {
                return false;
            }
            if (m_script->pauseWaypoint.doPause)
            {
                ((Creature*)pSource)->addUnitState(UNIT_STAT_WAYPOINT_PAUSED);
            }
            else
            {
                ((Creature*)pSource)->clearUnitState(UNIT_STAT_WAYPOINT_PAUSED);
            }
            break;
        }
        case SCRIPT_COMMAND_JOIN_LFG:
        {

            break;
        }
        case SCRIPT_COMMAND_TERMINATE_COND:
        {
            Player* player = nullptr;
            Occupant* second = pSource;

            if (pTarget &&IsPlayer(pTarget))
            {
                player = static_cast<Player*>(pTarget);
            }

            else if (pSource &&IsPlayer(pSource))
            {
                player = static_cast<Player*>(pSource);
                second = pTarget;
            }

            bool terminateResult;
            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                terminateResult = !sObjectMgr.IsPlayerMeetToCondition(m_script->terminateCond.conditionId, player, m_map, second, CONDITION_FROM_DBSCRIPTS);
            }
            else
            {
                terminateResult = sObjectMgr.IsPlayerMeetToCondition(m_script->terminateCond.conditionId, player, m_map, second, CONDITION_FROM_DBSCRIPTS);
            }

            if (terminateResult && m_script->terminateCond.failQuest && player)
            {
                if (Group* group = player->GetGroup())
                {
                    for (GroupReference* groupRef = group->GetFirstMember(); groupRef != nullptr; groupRef = groupRef->next())
                    {
                        Player* member = groupRef->getSource();
                        if (member->GetQuestStatus(m_script->terminateCond.failQuest) == QUEST_STATUS_INCOMPLETE)
                        {
                            member->FailQuest(m_script->terminateCond.failQuest);
                        }
                    }
                }
                else
                {
                    if (player->GetQuestStatus(m_script->terminateCond.failQuest) == QUEST_STATUS_INCOMPLETE)
                    {
                        player->FailQuest(m_script->terminateCond.failQuest);
                    }
                }
            }
            return terminateResult;
        }
        case SCRIPT_COMMAND_SEND_AI_EVENT_AROUND:
        {
            if (LogIfNotCreature(pSource))
            {
                return false;
            }
            if (LogIfNotUnit(pTarget))
            {
                break;
            }

            ((Creature*)pSource)->AI()->SendAIEventAround(AIEventType(m_script->sendAIEvent.eventType), (Unit*)pTarget, 0, float(m_script->sendAIEvent.radius));
            break;
        }
        case SCRIPT_COMMAND_TURN_TO:
        {
            if (LogIfNotUnit(pSource))
            {
                break;
            }

            ((Unit*)pSource)->SetFacingTo(pSource->Where().BearingTo(pTarget->Where()));
            break;
        }
        case SCRIPT_COMMAND_MOVE_DYNAMIC:
        {
            if (LogIfNotCreature(pSource))
            {
                return false;
            }
            if (LogIfNotUnit(pTarget))
            {
                return false;
            }

            float x, y, z;
            if (m_script->moveDynamic.maxDist == 0)
            {
                if (pTarget == pSource)
                {
                    sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, _MOVE_DYNAMIC called with maxDist == 0, but resultingSource == resultingTarget (== %s)", m_type, m_script->id, pSource->GetGuidStr().c_str());
                    break;
                }
                ContactPointNear(*pTarget, pSource, x, y, z);
            }
            else
            {
                float orientation;
                if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                {
                    orientation = pSource->Where().Facing() + m_script->o + 2 * M_PI_F;
                }
                else
                {
                    orientation = m_script->o;
                }

                const Geometry::Vector3 spot = RandomGroundPointNear(*pSource,
                    pTarget->Where().Pos(), m_script->moveDynamic.maxDist,
                    m_script->moveDynamic.minDist, (orientation == 0.0f ? nullptr : &orientation));
                x = spot.x;
                y = spot.y;
                z = spot.z;
                z = std::max(z, pTarget->Where().Z());
                ClampToAllowedZ(*pSource, x, y, z);
            }
            ((Creature*)pSource)->GetMotionMaster()->MovePoint(1, x, y, z);
            break;
        }
        case SCRIPT_COMMAND_SEND_MAIL:
        {
            if (LogIfNotPlayer(pTarget))
            {
                return false;
            }
            if (!m_script->sendMail.altSender && LogIfNotCreature(pSource))
            {
                return false;
            }

            MailSender sender;
            if (m_script->sendMail.altSender)
            {
                sender = MailSender(MAIL_CREATURE, m_script->sendMail.altSender);
            }
            else
            {
                sender = MailSender(pSource);
            }
            uint32 deliverDelay = m_script->textId[0] > 0 ? (uint32)m_script->textId[0] : 0;

            MailDraft(m_script->sendMail.mailTemplateId).SendMailTo(static_cast<Player*>(pTarget), sender, MAIL_CHECK_MASK_HAS_BODY, deliverDelay);
            break;
        }
        case SCRIPT_COMMAND_CHANGE_ENTRY:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            ((Creature*)pSource)->UpdateEntry(m_script->changeEntry.creatureEntry);
            break;
        }
        case SCRIPT_COMMAND_DESPAWN_GO:
        {

            uint32 goEntry;
            GameObject* pGo;
            if (!m_script->despawnGo.goGuid)
            {
                sLog.outErrorDb("Table `db_scripts [type = %d]` has no gameobject defined in SCRIPT_COMMAND_DESPAWN_GO for script id %u", m_type, m_script->id);
                break;
            }

            GameObjectData const* goData = sObjectMgr.GetGOData(m_script->despawnGo.goGuid);
            if (!goData)
            {
                sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid gameobject (GUID: %u) in SCRIPT_COMMAND_RESPAWN_GO for script id %u", m_type, m_script->despawnGo.goGuid, m_script->id);
                break;
            }

            pGo = m_map->GetGameObject(MakeGuid(HIGHGUID_GAMEOBJECT, goData->id, m_script->despawnGo.goGuid));

            pGo->SetRespawnTime(m_script->despawnGo.respawnTime);
            pGo->SetLootState(GO_JUST_DEACTIVATED);

            break;
        }
        case SCRIPT_COMMAND_RESPAWN:
        {
            if (LogIfNotCreature(pTarget))
            {
                break;
            }
            ((Creature*)pTarget)->Respawn();
            break;
        }
        case SCRIPT_COMMAND_SET_EQUIPMENT_SLOTS:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            Creature* pCSource = static_cast<Creature*>(pSource);

            if (m_script->setEquipment.resetDefault)
            {
                pCSource->LoadEquipment(pCSource->GetCreatureInfo()->EquipmentTemplateId, true);
                break;
            }

            if (m_script->textId[0] >= 0)
            {
                pCSource->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, m_script->textId[0]);
            }

            if (m_script->textId[1] >= 0)
            {
                pCSource->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, m_script->textId[1]);
            }

            if (m_script->textId[2] >= 0)
            {
                pCSource->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, m_script->textId[2]);
            }
            break;
        }
        case SCRIPT_COMMAND_RESET_GO:
        {
            if (LogIfNotGameObject(pTarget))
            {
                break;
            }

            GameObject* pGoTarget = static_cast<GameObject*>(pTarget);

            switch (pGoTarget->GetGoType())
            {
                case GAMEOBJECT_TYPE_DOOR:
                case GAMEOBJECT_TYPE_BUTTON:
                    pGoTarget->ResetDoorOrButton();
                    break;
                default:
                    sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for gameobject(buddyEntry: %u). Gameobject is not a door or button", m_type, m_script->id, m_script->command, m_script->buddyEntry);
                    break;
            }
            break;
        }
        case SCRIPT_COMMAND_UPDATE_TEMPLATE:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            Creature* pCre = static_cast<Creature*>(pSource);

            if (pCre->GetEntry() != m_script->updateTemplate.entry)
            {
                pCre->UpdateEntry(m_script->updateTemplate.entry, m_script->updateTemplate.faction ? HORDE : ALLIANCE);
            }
            else
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u: source creature already has the specified creature entry %u", m_type, m_script->id, m_script->command, m_script->updateTemplate.entry);
            }
            break;
        }
        case SCRIPT_COMMAND_XP_USER:
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            if (m_script->xpDisabled.flags)
            {
                pPlayer->SetPlayerFlag(PLAYER_FLAGS_XP_USER_DISABLED);
            }
            else
            {
                pPlayer->RemovePlayerFlag(PLAYER_FLAGS_XP_USER_DISABLED);
            }
            break;
        }

        case SCRIPT_COMMAND_SET_FLY:
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            Creature* pFlier = static_cast<Creature*>(pSource);

            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                pFlier->SetAlwaysStanding(m_script->fly.enable != 0);
            }

            pFlier->SetLevitate(m_script->fly.enable != 0);
            break;
        }
        default:
            sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u unknown command used.", m_type, m_script->id, m_script->command);
            break;
    }

    return false;
}
