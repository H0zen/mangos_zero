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
#include <string>
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
#include "MapRoster.h"
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

void Player::SendTransferAbortedByLockStatus(MapEntry const* mapEntry, AreaTrigger const* at, AreaLockStatus lockStatus, uint32 miscRequirement)
{
    MANGOS_ASSERT(mapEntry);

    DEBUG_LOG("SendTransferAbortedByLockStatus: Called for %s on map %u, LockAreaStatus %u, miscRequirement %u)", GetGuidStr().c_str(), mapEntry->MapID, lockStatus, miscRequirement);

    if (at && at->failed_text_mangos_string_id > 0)
    {
        GetSession()->SendAreaTriggerMessage("%s", GetSession()->GetMangosString(at->failed_text_mangos_string_id));
        return;
    }

    switch (lockStatus)
    {
        case AREA_LOCKSTATUS_LEVEL_TOO_LOW:
            GetSession()->SendAreaTriggerMessage(GetSession()->GetMangosString(LANG_LEVEL_MINREQUIRED), miscRequirement);
            break;
        case AREA_LOCKSTATUS_LEVEL_TOO_HIGH:
            GetSession()->SendAreaTriggerMessage(GetSession()->GetMangosString(LANG_LEVEL_MAXREQUIRED), miscRequirement);
            break;
        case AREA_LOCKSTATUS_LEVEL_NOT_EQUAL:
            GetSession()->SendAreaTriggerMessage(GetSession()->GetMangosString(LANG_LEVEL_EQUALREQUIRED), miscRequirement);
            break;
        case AREA_LOCKSTATUS_ZONE_IN_COMBAT:
            GetSession()->SendTransferAborted(mapEntry->MapID, TRANSFER_ABORT_ZONE_IN_COMBAT);
            break;
        case AREA_LOCKSTATUS_INSTANCE_IS_FULL:
            GetSession()->SendTransferAborted(mapEntry->MapID, TRANSFER_ABORT_MAX_PLAYERS);
            break;
        case AREA_LOCKSTATUS_WRONG_TEAM:
            if (miscRequirement == 469)
            {
                GetSession()->SendAreaTriggerMessage("%s", GetSession()->GetMangosString(LANG_WRONG_TEAM_ALLIANCE));
            }
            else
            {
                GetSession()->SendAreaTriggerMessage("%s", GetSession()->GetMangosString(LANG_WRONG_TEAM_HORDE));
            }
            break;
        case AREA_LOCKSTATUS_QUEST_NOT_COMPLETED:
            if (mapEntry->IsContinent())
            {
                DEBUG_LOG("SendTransferAbortedByLockStatus: LockAreaStatus %u, do not teleport, no message sent (mapId %u)", lockStatus, mapEntry->MapID);
                break;
            }

            break;
        case AREA_LOCKSTATUS_MISSING_ITEM:
            if (sObjectMgr.GetMapEntranceTrigger(mapEntry->MapID))
            {
                GetSession()->SendAreaTriggerMessage(GetSession()->GetMangosString(LANG_REQUIRED_ITEM), sObjectMgr.GetItemPrototype(miscRequirement)->Name1);
            }
            break;
        case AREA_LOCKSTATUS_NOT_ALLOWED:
        case AREA_LOCKSTATUS_RAID_LOCKED:
        case AREA_LOCKSTATUS_UNKNOWN_ERROR:

            break;
        case AREA_LOCKSTATUS_PVP_RANK:
        {

            const std::string msg = "You cannot enter this zone";
            GetSession()->SendAreaTriggerMessage("%s", msg.c_str());
            break;
        }

        case AREA_LOCKSTATUS_OK:
            sLog.outError("SendTransferAbortedByLockStatus: LockAreaStatus AREA_LOCKSTATUS_OK received for %s (mapId %u)", GetGuidStr().c_str(), mapEntry->MapID);
            MANGOS_ASSERT(false);
            break;
        default:
            sLog.outError("SendTransfertAbortedByLockstatus: unhandled LockAreaStatus %u, when %s attempts to enter in map %u", lockStatus, GetGuidStr().c_str(), mapEntry->MapID);
            break;
    }
}

AreaLockStatus Player::GetAreaTriggerLockStatus(AreaTrigger const* at, uint32& miscRequirement)
{
    miscRequirement = 0;

    if (!at)
    {
        return AREA_LOCKSTATUS_UNKNOWN_ERROR;
    }

    MapEntry const* mapEntry = sMapStore.LookupEntry(at->target_mapId);
    if (!mapEntry)
    {
        return AREA_LOCKSTATUS_UNKNOWN_ERROR;
    }

    if (isGameMaster())
    {
        return AREA_LOCKSTATUS_OK;
    }

    if (mapEntry->IsRaid() && !sWorld.getConfig(CONFIG_BOOL_INSTANCE_IGNORE_RAID))
    {
        if (!GetGroup() || !GetGroup()->isRaidGroup())
        {
            return AREA_LOCKSTATUS_RAID_LOCKED;
        }
    }

    if (at->condition)
    {
        ConditionEntry fault;
        if (!sObjectMgr.IsPlayerMeetToCondition(at->condition, this, GetMap(),nullptr, CONDITION_AREA_TRIGGER, &fault))
        {
            switch (fault.type)
            {
                case CONDITION_LEVEL:
                {
                    if (sWorld.getConfig(CONFIG_BOOL_INSTANCE_IGNORE_LEVEL))
                    {
                        break;
                    }
                    else
                    {
                        miscRequirement = fault.param1;
                        switch (fault.param2)
                        {
                            case 0: { return AREA_LOCKSTATUS_LEVEL_NOT_EQUAL; }
                            case 1: { return AREA_LOCKSTATUS_LEVEL_TOO_LOW; }
                            case 2: { return AREA_LOCKSTATUS_LEVEL_TOO_HIGH; }
                        }
                    }
                }

                case CONDITION_ITEM:
                {
                    miscRequirement = fault.param1;
                    return AREA_LOCKSTATUS_MISSING_ITEM;
                }

                case CONDITION_QUESTREWARDED:
                {
                    miscRequirement = fault.param1;
                    return AREA_LOCKSTATUS_QUEST_NOT_COMPLETED;
                }

                case CONDITION_TEAM:
                {
                    miscRequirement = fault.param1;
                    return AREA_LOCKSTATUS_WRONG_TEAM;
                }

                case CONDITION_PVP_RANK:
                {
                    miscRequirement = fault.param1;
                    return AREA_LOCKSTATUS_PVP_RANK;
                }

                default:
                    return AREA_LOCKSTATUS_UNKNOWN_ERROR;
            }
        }
    }

    DungeonPersistentState* state = Binds().CopyForHimOrHisGroup(at->target_mapId);
    Map* map = sMapRoster.Find(at->target_mapId, state ? state->GetInstanceId() : 0);

    if (map && map->IsDungeon())
    {

        if (((DungeonMap*)map)->GetPlayersCountExceptGMs() >= ((DungeonMap*)map)->GetMaxPlayers())
        {
            return AREA_LOCKSTATUS_INSTANCE_IS_FULL;
        }

        if (map && map->GetInstanceData() && map->GetInstanceData()->IsEncounterInProgress())
        {
            return AREA_LOCKSTATUS_ZONE_IN_COMBAT;
        }

        DungeonHold* pBind = Binds().To(at->target_mapId);
        if (pBind && pBind->permanent && pBind->state != state)
        {
            return AREA_LOCKSTATUS_HAS_BIND;
        }
        if (pBind && pBind->permanent && pBind->state != map->GetPersistentState())
        {
            return AREA_LOCKSTATUS_HAS_BIND;
        }
    }

    return AREA_LOCKSTATUS_OK;
};
