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
#include "Transports.h"
#include "TransportMap.h"
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

#define ZONE_UPDATE_INTERVAL (1*IN_MILLISECONDS)

void Player::CheckAreaExploreAndOutdoor()
{
    if (!IsAlive())
    {
        return;
    }

    if (IsTaxiFlying())
    {
        return;
    }

    uint32 anchorMap;
    float anchorX, anchorY, anchorZ;
    GetWorldAnchor(anchorMap, anchorX, anchorY, anchorZ);

    bool isOutdoor;
    uint16 areaFlag = AnchorTerrain()->GetAreaFlag(anchorX, anchorY, anchorZ, &isOutdoor);

    if (isOutdoor)
    {
        if (HasPlayerFlag(PLAYER_FLAGS_RESTING) && Resting().Kind() == REST_TYPE_IN_TAVERN)
        {
            AreaTriggerEntry const* at = sAreaTriggerStore.LookupEntry(Resting().InnTrigger());
            if (!at || !IsPointInAreaTriggerZone(at, GetMapId(), Where().X(), Where().Y(), Where().Z()))
            {

                Resting().Kind(REST_TYPE_NO);
            }
        }

        const PlayerSpellMap& sp_list = GetSpellMap();
        for (PlayerSpellMap::const_iterator itr = sp_list.begin(); itr != sp_list.end(); ++itr)
        {
            if (itr->second.state == PLAYERSPELL_REMOVED)
            {
                continue;
            }
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);
            if (!spellInfo || !IsNeedCastSpellAtOutdoor(spellInfo) || HasAura(itr->first))
            {
                continue;
            }

            ShapeshiftForm form = GetShapeshiftForm();
            if (!(spellInfo->ShapeshiftMask & (1 << (form - 1))))
            {
                continue;
            }
            if ((spellInfo->ShapeshiftMask || spellInfo->ShapeshiftExclude) && !IsNeedCastSpellAtFormApply(spellInfo, GetShapeshiftForm()))
            {
                continue;
            }
            CastSpell(this, itr->first, true, nullptr);
        }
    }
    else if (sWorld.getConfig(CONFIG_BOOL_VMAP_INDOOR_CHECK) && !isGameMaster())
    {
        RemoveAurasWithAttribute(SPELL_ATTR_OUTDOORS_ONLY);
    }

    if (areaFlag == 0xffff)
    {
        return;
    }
    int offset = areaFlag / 32;

    if (offset >= PLAYER_EXPLORED_ZONES_SIZE)
    {
        sLog.outError("Wrong area flag %u in map data for (X: %f Y: %f) point to field PLAYER_EXPLORED_ZONES_1 + %u ( %u must be < %u ).", areaFlag, Where().X(), Where().Y(), offset, offset, PLAYER_EXPLORED_ZONES_SIZE);
        return;
    }

    uint32 val = (uint32)(1 << (areaFlag % 32));
    uint32 currFields = GetExploredZones(offset);

    if (!(currFields & val))
    {
        SetExploredZones(offset, (uint32)(currFields | val));

        AreaTableEntry const* p = GetAreaEntryByAreaFlagAndMap(areaFlag, GetMapId());
        if (!p)
        {
            sLog.outError("PLAYER: Player %u discovered unknown area (x: %f y: %f map: %u", GetGUIDLow(), Where().X(), Where().Y(), GetMapId());
        }
        else if (p->ExplorationLevel > 0)
        {
            uint32 area = p->ID;
            if (getLevel() >= sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                SendExplorationExperience(area, 0);
            }
            else
            {
                int32 diff = int32(getLevel()) - p->ExplorationLevel;
                uint32 XP = 0;
                if (diff < -5)
                {
                    XP = uint32(sObjectMgr.GetBaseXP(getLevel() + 5) * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_EXPLORE));
                }
                else if (diff > 5)
                {
                    int32 exploration_percent = (100 - ((diff - 5) * 5));
                    if (exploration_percent > 100)
                    {
                        exploration_percent = 100;
                    }
                    else if (exploration_percent < 0)
                    {
                        exploration_percent = 0;
                    }

                    XP = uint32(sObjectMgr.GetBaseXP(p->ExplorationLevel) * exploration_percent / 100 * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_EXPLORE));
                }
                else
                {
                    XP = uint32(sObjectMgr.GetBaseXP(p->ExplorationLevel) * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_EXPLORE));
                }

                GiveXP(XP, nullptr);
                SendExplorationExperience(area, XP);
            }
            DETAIL_LOG("PLAYER: Player %u discovered a new area: %u", GetGUIDLow(), area);
        }
    }
}

void Player::UpdateArea(uint32 newArea)
{
    m_areaUpdateId    = newArea;

    AreaTableEntry const* area = GetAreaEntryByAreaID(newArea);

    if (area && (area->Flags & AREA_FLAG_ARENA))
    {
        if (!isGameMaster())
        {
            SetFFAPvP(true);
        }
    }
    else
    {

        if (IsFFAPvP() && !sWorld.IsFFAPvPRealm())
        {
            SetFFAPvP(false);
        }
    }

    UpdateAreaDependentAuras();
}

void Player::UpdateZone(uint32 newZone, uint32 newArea, bool sendInitialWorldStates)
{

    AreaTableEntry const* zone = GetAreaEntryByAreaID(newZone);
    if (!zone)
    {
        return;
    }

    if (m_zoneUpdateId != newZone)
    {

        sOutdoorPvPMgr.HandlePlayerLeaveZone(this, m_zoneUpdateId);
        sOutdoorPvPMgr.HandlePlayerEnterZone(this, newZone);

        if (sendInitialWorldStates)
        {
            SendInitWorldStates(newZone);
        }

        if (sWorld.getConfig(CONFIG_BOOL_WEATHER))
        {
            Weather* wth = GetMap()->GetWeatherSystem()->FindOrCreateWeather(newZone);
            wth->SendWeatherUpdateToPlayer(this);
        }
    }

    m_zoneUpdateId    = newZone;
    m_zoneUpdateTimer = ZONE_UPDATE_INTERVAL;

    UpdateArea(newArea);

    switch (zone->FactionGroupMask)
    {
        case AREATEAM_ALLY:
            pvpInfo.inHostileArea = GetTeam() != ALLIANCE && (sWorld.IsPvPRealm() || zone->Flags & AREA_FLAG_CAPITAL);
            break;
        case AREATEAM_HORDE:
            pvpInfo.inHostileArea = GetTeam() != HORDE && (sWorld.IsPvPRealm() || zone->Flags & AREA_FLAG_CAPITAL);
            break;
        case AREATEAM_NONE:

            pvpInfo.inHostileArea = sWorld.IsPvPRealm() || Battle().InOne();
            break;
        default:
            pvpInfo.inHostileArea = false;
            break;
    }

    if (pvpInfo.inHostileArea)
    {
        if (!IsPvP() || pvpInfo.endTimer != 0)
        {
            UpdatePvP(true, true);
        }
    }
    else
    {
        if (IsPvP() && !HasPlayerFlag(PLAYER_FLAGS_IN_PVP) && pvpInfo.endTimer == 0)
        {
            pvpInfo.endTimer = time(0);
        }
    }

    if (zone->Flags & AREA_FLAG_CAPITAL)
    {
        Resting().Kind(REST_TYPE_IN_CITY);
    }
    else if (HasPlayerFlag(PLAYER_FLAGS_RESTING) && Resting().Kind() != REST_TYPE_IN_TAVERN)
    {

        Resting().Kind(REST_TYPE_NO);
    }

    if (IsAlive())
    {
        DestroyZoneLimitedItem(true, newZone);
    }

    if (!IsTaxiFlying())
    {
        UpdateLocalChannels(newZone);
    }

    if (GetGroup())
    {
        SetGroupUpdateFlag(GROUP_UPDATE_FLAG_ZONE);
    }

    UpdateZoneDependentAuras();
}

void Player::UpdateHomebindTime(uint32 time)
{

    if (Binds().StillWelcome() || isGameMaster())
    {
        if (Home().Countdown())
        {

            WorldPacket data(SMSG_RAID_GROUP_ONLY, 4 + 4);
            data << uint32(0);
            data << uint32(ERR_RAID_GROUP_REQUIRED);
            GetSession()->SendPacket(&data);
        }

        Home().Countdown(0);
    }
    else if (Home().Countdown() > 0)
    {
        if (time >= Home().Countdown())
        {

            TeleportTo(Home().MapId(), Home().X(), Home().Y(), Home().Z(), Where().Facing());
        }
        else
        {
            Home().Countdown(Home().Countdown() - time);
        }
    }
    else
    {

        Home().Countdown(60000);

        WorldPacket data(SMSG_RAID_GROUP_ONLY, 4 + 4);
        data << uint32(Home().Countdown());
        data << uint32(ERR_RAID_GROUP_REQUIRED);
        GetSession()->SendPacket(&data);
        DEBUG_LOG("PLAYER: Player '%s' (GUID: %u) will be teleported to homebind in 60 seconds", GetName(), GetGUIDLow());
    }
}

void Player::UpdateZoneDependentAuras()
{

    SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(m_zoneUpdateId);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
    {
        itr->second->ApplyOrRemoveSpellIfCan(this, m_zoneUpdateId, 0, true);
    }
}

void Player::UpdateAreaDependentAuras()
{

    m_auras.RemoveWhere(
        [this](SpellAuraHolder* holder)
        {
            return sSpellMgr.GetSpellAllowedInLocationError(holder->GetSpellProto(), GetMapId(),
                                                            m_zoneUpdateId, m_areaUpdateId, this) != SPELL_CAST_OK;
        },
        [this](SpellAuraHolder* holder) { RemoveHolder(holder); });

    SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(m_areaUpdateId);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
    {
        itr->second->ApplyOrRemoveSpellIfCan(this, m_zoneUpdateId, m_areaUpdateId, true);
    }
}
