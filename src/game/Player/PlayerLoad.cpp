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
#include <sstream>
#include <string>
#include <map>
#include <list>
#include "Database/SqlOperations.h"
#include <cstdlib>
#include "Player.h"
#include "Reclaim.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "CinematicFlyover.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "Fleet.h"
#include "MapCoords.h"
#include "MapFoundry.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "CorpseManager.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include <cmath>
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
#include "LFGMgr.h"
#include "DisableMgr.h"
#include "Corpse.h"
#include "Cast/Recipe/RecipeBook.h"

void Player::_LoadBGData(QueryResult* result)
{
    if (!result)
    {
        return;
    }

    Field* fields = result->Fetch();

    Battle().FromRow(fields[0].GetUInt32(),
                     Team(fields[1].GetUInt32()),
                     Geometry::Placement::Somewhere(
                         fields[6].GetUInt32(),
                         Geometry::Vector3(fields[2].GetFloat(),
                                           fields[3].GetFloat(),
                                           fields[4].GetFloat()),
                         fields[5].GetFloat()));

    delete result;
}

bool Player::LoadFromDB(ObjectGuid guid, SqlQueryHolder* holder)
{

    QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADFROM);

    if (!result)
    {
        sLog.outError("%s not found in table `characters`, can't load. ", GuidString(guid).c_str());
        return false;
    }

    Field* fields = result->Fetch();

    uint32 dbAccountId = fields[1].GetUInt32();

    if (dbAccountId != GetSession()->GetAccountId())
    {
        sLog.outError("%s loading from wrong account (is: %u, should be: %u)",
            GuidString(guid).c_str(), GetSession()->GetAccountId(), dbAccountId);
        delete result;
        return false;
    }

    Object::_Create(GuidCounter(guid), 0, HIGHGUID_PLAYER);
    m_inventory.Saves().Belongs(GetObjectGuid());

    m_name = fields[2].GetCppString();

    if (ObjectMgr::CheckPlayerName(m_name) != CHAR_NAME_SUCCESS ||
        (GetSession()->GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(m_name)))
    {
        delete result;
        CharacterDatabase.PExecute("UPDATE `characters` SET `at_login` = `at_login` | '%u' WHERE `guid` ='%u'",
            uint32(AT_LOGIN_RENAME), GuidCounter(guid));
        return false;
    }

    SetGuidValue(OBJECT_FIELD_GUID, guid);

    SetRace(fields[3].GetUInt8());
    SetClass(fields[4].GetUInt8());

    uint8 gender = fields[5].GetUInt8() & 0x01;
    SetGender(gender);

    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());
    if (!info)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "Player (GUID: %u) has wrong race/class (%u/%u), can't be loaded.",
            GuidCounter(guid), getRace(), getClass());
        return false;
    }

    SetUInt32Value(UNIT_FIELD_LEVEL, fields[6].GetUInt8());
    SetUInt32Value(PLAYER_XP, fields[7].GetUInt32());

    LoadFields(fields[51].GetString(), PLAYER_EXPLORED_ZONES_1, PLAYER_EXPLORED_ZONES_SIZE);

    InitDisplayIds();

    uint32 money = fields[8].GetUInt32();
    if (money > MAX_MONEY_AMOUNT)
    {
        money = MAX_MONEY_AMOUNT;
    }
    SetMoney(money);

    SetUInt32Value(PLAYER_BYTES, fields[9].GetUInt32());
    SetUInt32Value(PLAYER_BYTES_2, fields[10].GetUInt32());

    Drinking().Amount(fields[44].GetUInt16());

    SetDrunkAndGender(Drinking().Amount(), gender);

    SetAllPlayerFlags(fields[11].GetUInt32());
    SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, fields[43].GetInt32());

    SetUInt32Value(PLAYER_AMMO_ID, fields[53].GetUInt32());

    SetActionBars(fields[54].GetUInt8());

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        delete m_inventory.Own(slot);
        m_inventory.Own(slot, nullptr);
    }

    DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "Load Basic value of player %s is: ", m_name.c_str());
    outDebugStatsValues();

    setFactionForRace(getRace());
    SetCharm(nullptr);

    if (!_LoadHomeBind(holder->GetResult(PLAYER_LOGIN_QUERY_LOADHOMEBIND)))
    {
        return false;
    }

    InitPrimaryProfessions();

    uint32 transGUID = fields[30].GetUInt32();
    Place().MoveTo(fields[12].GetFloat(), fields[13].GetFloat(), fields[14].GetFloat(), fields[16].GetFloat());
    SetLocationMapId(fields[15].GetUInt32());

    m_movementInfo.ChangePosition(Where().X(), Where().Y(), Where().Z(),
                                  Where().Facing());

    _LoadGroup(holder->GetResult(PLAYER_LOGIN_QUERY_LOADGROUP));

    HonorRankInfo highest = m_honor.HighestRank();
    highest.rank = fields[38].GetUInt32();
    m_honor.HighestRank(MaNGOS::Honor::CalculateRankInfo(highest));
    m_honor.LastWeekPlace(fields[39].GetUInt32());
    m_honor.Stored(fields[40].GetFloat());
    m_honor.Kills(fields[41].GetUInt32(), false);
    m_honor.Kills(fields[42].GetUInt32(), true);

    _LoadHonorCP(holder->GetResult(PLAYER_LOGIN_QUERY_LOADHONORCP));

    m_binds.Load(holder->GetResult(PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES));

    if (!IsPlaceable(*this))
    {
        sLog.outError("%s have invalid coordinates (X: %f Y: %f Z: %f O: %f). Teleport to default race/class locations.",
            GuidString(guid).c_str(), Where().X(), Where().Y(), Where().Z(), Where().Facing());
        RelocateToHomebind();

        transGUID = 0;

        m_movementInfo.ClearTransportData();
    }

    _LoadBGData(holder->GetResult(PLAYER_LOGIN_QUERY_LOADBGDATA));

    if (Battle().InOne())
    {
        BattleGround* currentBg = sBattleGroundMgr.GetBattleGround(Battle().Id(), BATTLEGROUND_TYPE_NONE);

        bool player_at_bg = currentBg && currentBg->IsPlayerInBattleGround(GetObjectGuid());

        if (player_at_bg && currentBg->GetStatus() != STATUS_WAIT_LEAVE)
        {
            BattleGroundQueueTypeId bgQueueTypeId = sBattleGroundMgr.BGQueueTypeId(currentBg->GetTypeID());
            Queues().Take(bgQueueTypeId);

            Battle().KindIsKnown(currentBg->GetTypeID());

            currentBg->EventPlayerLoggedIn(this);
            currentBg->AddOrSetPlayerToCorrectBgGroup(this, GetObjectGuid(), Battle().SideAsSet());

            Queues().CalledTo(bgQueueTypeId, currentBg->GetInstanceID());
        }
        else
        {

            if (player_at_bg)
            {
                currentBg->RemovePlayerAtLeave(GetObjectGuid(), false, true);
            }

            const Geometry::Placement& _loc = Battle().CameFrom();
            SetLocationMapId(_loc.MapId());
            Place().MoveTo(_loc.X(), _loc.Y(), _loc.Z(), _loc.Facing());

            Battle().In(0, BATTLEGROUND_TYPE_NONE);

            _SaveBGData();
        }
    }
    else
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(GetMapId());

        if (!mapEntry || mapEntry->IsBattleGround())
        {
            const Geometry::Placement& _loc = Battle().CameFrom();
            SetLocationMapId(_loc.MapId());
            Place().MoveTo(_loc.X(), _loc.Y(), _loc.Z(), _loc.Facing());

            Battle().In(0, BATTLEGROUND_TYPE_NONE);

            _SaveBGData();
        }
    }

    if (transGUID != 0)
    {
        m_movementInfo.SetTransportData(MakeGuid(HIGHGUID_MO_TRANSPORT, transGUID), fields[26].GetFloat(), fields[27].GetFloat(), fields[28].GetFloat(), fields[29].GetFloat(), 0);

        Position const* transportPosition = m_movementInfo.GetTransportPos();

        if (!MaNGOS::IsValidMapCoord(Where().X() + transportPosition->x, Where().Y() + transportPosition->y,
            Where().Z() + transportPosition->z, Where().Facing() + transportPosition->o) ||

            std::fabs(transportPosition->x) > MAX_DECK_EXTENT ||
            std::fabs(transportPosition->y) > MAX_DECK_EXTENT ||
            std::fabs(transportPosition->z) > MAX_DECK_EXTENT)
        {
            sLog.outError("%s have invalid transport coordinates (X: %f Y: %f Z: %f O: %f). Teleport to default race/class locations.",
                GuidString(guid).c_str(), Where().X() + transportPosition->x, Where().Y() + transportPosition->y,
                Where().Z() + transportPosition->z, Where().Facing() + transportPosition->o);

            RelocateToHomebind();

            m_movementInfo.ClearTransportData();

            transGUID = 0;
        }
    }

    if (transGUID != 0)
    {
        if (Transport* aboard = sFleet.ByLowGuid(transGUID))
        {
            m_transport = aboard;

            SetLocationMapId(m_transport->GetMapId());
            Place().MoveTo(m_transport->Where().X(), m_transport->Where().Y(),
                           m_transport->Where().Z(), m_transport->Where().Facing());
        }

        if (!m_transport)
        {
            sLog.outError("%s have problems with transport guid (%u). Teleport to default race/class locations.",
                GuidString(guid).c_str(), transGUID);

            RelocateToHomebind();

            m_movementInfo.ClearTransportData();

            transGUID = 0;
        }
    }

    DungeonPersistentState* state = Binds().CopyForHimOrHisGroup(GetMapId());

    SetMap(sMapFoundry.OpenFor(*this, GetMapId()));

    if (GetInstanceId() && !state)
    {
        AreaTrigger const* at = sObjectMgr.GetMapEntranceTrigger(GetMapId());
        if (at)
        {
            Place().MoveTo(at->target_X, at->target_Y, at->target_Z, at->target_Orientation);

            if (Group* group = GetGroup())
            {
                uint32 zoneId = sTerrainMgr.GetZoneId(GetMapId(), at->target_X, at->target_Y, at->target_Z);

                std::ostringstream guidList;
                bool first = true;
                for (Group::MemberSlotList::const_iterator itr = group->GetMemberSlots().begin(); itr != group->GetMemberSlots().end(); ++itr)
                {
                    if (itr->guid != GetObjectGuid())
                    {
                        if (!first)
                        {
                            guidList << ',';
                        }
                        guidList << GuidCounter(itr->guid);
                        first = false;
                    }
                }

                if (!first)
                {
                    CharacterDatabase.PExecute(
                            "UPDATE `characters` SET `position_x`='%f',`position_y`='%f',`position_z`='%f',"
                            "`orientation`='%f',`map`='%u',`zone`='%u',`trans_x`='0',`trans_y`='0',`trans_z`='0',"
                            "`transguid`='0',`taxi_path`='' WHERE `guid` IN (%s) AND `map`='%u'",
                        at->target_X, at->target_Y, at->target_Z, at->target_Orientation,
                        GetMapId(), zoneId,
                        guidList.str().c_str(), GetMapId());
                }
            }
        }
        else
        {
            sLog.outError("Player %s(GUID: %u) logged in to a reset instance (map: %u) and there is no area-trigger leading to this map. Thus he can't be ported back to the entrance. This _might_ be an exploit attempt.", GetName(), GetGUIDLow(), GetMapId());
        }
    }

    SaveRecallPosition();

    time_t now = time(nullptr);
    time_t logoutTime = time_t(fields[22].GetUInt64());

    uint32 time_diff = uint32(now - logoutTime);

    float soberFactor;
    if (time_diff > 15 * MINUTE)
    {
        soberFactor = 0;
    }
    else
    {
        soberFactor = 1 - time_diff / (15.0f * MINUTE);
    }
    uint16 newDrunkenValue = uint16(soberFactor * Drinking().Amount());
    Drinking().Amount(newDrunkenValue);

    m_cinematic = fields[18].GetUInt32();
    Played().Total(fields[19].GetUInt32());
    Played().AtThisLevel(fields[20].GetUInt32());

    m_resetTalentsCost = fields[24].GetUInt32();
    m_resetTalentsTime = time_t(fields[25].GetUInt64());

    uint32 old_safe_flags = GetPlayerFlags() & (PLAYER_FLAGS_HIDE_CLOAK | PLAYER_FLAGS_HIDE_HELM);

    if (HasPlayerFlag(PLAYER_FLAGS_GM))
    {
        SetAllPlayerFlags(0 | old_safe_flags);
    }

    m_taxi.LoadTaxiMask(fields[17].GetString());

    uint32 extraflags = fields[31].GetUInt32();

    m_petMgr.LoadStableSlotsFromField(fields[32].GetUInt32());

    m_atLoginFlags = fields[33].GetUInt32();

    m_deathExpireTime = (time_t)fields[36].GetUInt64();
    if (m_deathExpireTime > now + reclaim::RUNGS * reclaim::FORGETS_AFTER)
    {
        m_deathExpireTime = now + reclaim::RUNGS * reclaim::FORGETS_AFTER - 1;
    }

    std::string taxi_nodes = fields[37].GetCppString();

    SetChannelObjectGuid(0);
    SetUInt32Value(UNIT_CHANNEL_SPELL, 0);

    SetCharm(nullptr);
    SetPet(nullptr);
    SetTargetGuid(0);
    SetCharmerGuid(0);
    SetOwnerGuid(0);
    SetCreatorGuid(0);

    SetGuidValue(PLAYER_FARSIGHT, 0);
    SetUInt32Value(PLAYER_TRACK_CREATURES, 0);
    SetUInt32Value(PLAYER_TRACK_RESOURCES, 0);

    RemoveAllAuras();

    ClearInCombat();

    SetDuelArbiterGuid(0);
    SetUInt32Value(PLAYER_DUEL_TEAM, 0);

    InitStatsForLevel();

    Resting().Bonus(fields[21].GetFloat());

    if (time_diff > 0)
    {
        Resting().Bonus(Resting().Bonus() + Resting().Over(time_diff, true, (fields[23].GetInt32() > 0)));
    }

    _LoadSkills(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSKILLS));

    _LoadMails(holder->GetResult(PLAYER_LOGIN_QUERY_LOADMAILS));
    _LoadMailedItems(holder->GetResult(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS));
    Post().Recount();

    _LoadAuras(holder->GetResult(PLAYER_LOGIN_QUERY_LOADAURAS), time_diff);

    if (HasPlayerFlag(PLAYER_FLAGS_GHOST))
    {
        m_deathState = DEAD;
    }

    _LoadSpells(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSPELLS));

    InitTalentForLevel();
    learnDefaultSpells();

    _LoadQuestStatus(holder->GetResult(PLAYER_LOGIN_QUERY_LOADQUESTSTATUS));

    m_reputationMgr.LoadFromDB(holder->GetResult(PLAYER_LOGIN_QUERY_LOADREPUTATION));

    _LoadInventory(holder->GetResult(PLAYER_LOGIN_QUERY_LOADINVENTORY), time_diff);
    _LoadItemLoot(holder->GetResult(PLAYER_LOGIN_QUERY_LOADITEMLOOT));

    m_inventory.RunClocks(time_diff, true);

    _LoadActions(holder->GetResult(PLAYER_LOGIN_QUERY_LOADACTIONS));

    m_social = sSocialMgr.LoadFromDB(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSOCIALLIST), GetObjectGuid());

    if (!m_taxi.LoadTaxiDestinationsFromString(taxi_nodes, GetTeam()))
    {

        TaxiNodesEntry const* nodeEntry = nullptr;
        if (uint32 node_id = m_taxi.GetTaxiSource())
        {
            nodeEntry = sTaxiNodesStore.LookupEntry(node_id);
        }

        if (!nodeEntry)
        {
            sLog.outError("Character %u have wrong data in taxi destination list, teleport to homebind.", GetGUIDLow());
            RelocateToHomebind();
        }
        else
        {
            sLog.outError("Character %u have too short taxi destination list, teleport to original node.", GetGUIDLow());
            SetLocationMapId(nodeEntry->map_id);
            Place().MoveTo(nodeEntry->x, nodeEntry->y, nodeEntry->z, 0.0f);
        }

        SetMap(sMapFoundry.OpenFor(*this, GetMapId()));
        SaveRecallPosition();

        m_taxi.ClearTaxiDestinations();
    }

    if (uint32 node_id = m_taxi.GetTaxiSource())
    {

        TaxiNodesEntry const* nodeEntry = sTaxiNodesStore.LookupEntry(node_id);
        MANGOS_ASSERT(nodeEntry);
        m_recall = Geometry::Placement::Somewhere(nodeEntry->map_id,
                                                  Geometry::Vector3(nodeEntry->x, nodeEntry->y, nodeEntry->z));

    }

    SetFallInformation(0, Where().Z());

    m_movementInfo.ChangePosition(Where().X(), Where().Y(), Where().Z(),
                                  Where().Facing());

    _LoadSpellCooldowns(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS));

    if (!IsAlive())
    {
        RemoveAllAurasOnDeath();
    }

    Tallied().Ready(true);
    Sheet().Everything();

    uint32 savedhealth = fields[45].GetUInt32();
    SetHealth(savedhealth > GetMaxHealth() ? GetMaxHealth() : savedhealth);
    for (uint32 i = 0; i < MAX_POWERS; ++i)
    {
        uint32 savedpower = fields[46 + i].GetUInt32();
        SetPower(Powers(i), savedpower > GetMaxPower(Powers(i)) ? GetMaxPower(Powers(i)) : savedpower);
    }

    uint32 createdDate = fields[55].GetUInt32();
    SetCreatedDate(createdDate);

    DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "The value of player %s after load item and aura is: ", m_name.c_str());
    outDebugStatsValues();

    delete result;

    if (GetSession()->GetSecurity() > SEC_PLAYER)
    {
        switch (sWorld.getConfig(CONFIG_UINT32_GM_LOGIN_STATE))
        {
            default:
            case 0:                      break;
            case 1: SetGameMaster(true); break;
            case 2:
                if (extraflags & PLAYER_EXTRA_GM_ON)
                {
                    SetGameMaster(true);
                }
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_VISIBLE_STATE))
        {
            default:
            case 0: SetGMVisible(false); break;
            case 1:                      break;
            case 2:
                if (extraflags & PLAYER_EXTRA_GM_INVISIBLE)
                {
                    SetGMVisible(false);
                }
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_ACCEPT_TICKETS))
        {
            default:
            case 0:                        break;
            case 1: SetAcceptTicket(true); break;
            case 2:
                if (extraflags & PLAYER_EXTRA_GM_ACCEPT_TICKETS)
                {
                    SetAcceptTicket(true);
                }
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_CHAT))
        {
            default:
            case 0:                  break;
            case 1: SetGMChat(true); break;
            case 2:
                if (extraflags & PLAYER_EXTRA_GM_CHAT)
                {
                    SetGMChat(true);
                }
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_WISPERING_TO))
        {
            default:
            case 0:                          break;
            case 1: SetAcceptWhispers(true); break;
            case 2:
                if (extraflags & PLAYER_EXTRA_ACCEPT_WHISPERS)
                {
                    SetAcceptWhispers(true);
                }
                break;
        }
    }

    return true;
}

bool Player::isAllowedToLoot(Creature* creature)
{

    if (!creature->HasDynFlag(UNIT_DYNFLAG_TAPPED) || !creature->Taking().EnoughPlayerDamage())
    {
        return false;
    }

    if (Player* recipient = creature->Claim().Entitled())
    {

        if (Group* plr_group = recipient->GetGroup())
        {

            if (Group* my_group = GetGroup())
            {

                if (plr_group != my_group)
                {
                    return false;
                }
            }
            else
            {
                return false;
            }

            if (creature->GetKilledTime() < plr_group->GetMemberSlotJoinedTime(GetObjectGuid()))
            {
                return false;
            }

            switch (plr_group->GetLootMethod())
            {

                case MASTER_LOOT:
                case FREE_FOR_ALL:
                    return true;

                case GROUP_LOOT:
                case ROUND_ROBIN:
                case NEED_BEFORE_GREED:
                {
                    uint32 loot_id = creature->GetCreatureInfo()->LootId;

                    bool hasLoot = loot_id;

                    bool hasSharedLoot = LootTemplates_Creature.HaveSharedQuestLootForPlayer(loot_id, this);

                    bool hasStartingQuestLoot = LootTemplates_Creature.HaveStartingQuestLootForPlayer(loot_id,  this);

                    if (!hasLoot)
                    {
                        return false;
                    }

                    else if (creature->Taking().Opened())
                    {
                        return true;
                    }

                    else if (creature->Taking().AssignedTo() == GetGUIDLow())
                    {
                        return true;
                    }

                    else if (creature->Taking().AssignedTo() != 0 && !hasSharedLoot && !hasStartingQuestLoot)
                    {
                        return false;
                    }

                    Player* final_looter = recipient;

                    Group::MemberSlotList slots = plr_group->GetMemberSlots();

                    for (Group::MemberSlotList::iterator itr = slots.begin(); itr != slots.end(); ++itr)
                    {

                        if (Player* grp_plr = sObjectMgr.GetPlayer(itr->guid))
                        {

                            if (!grp_plr->IsInWorld())
                            {
                                continue;
                            }

                            if (!grp_plr->Where().WithinDist(creature->Where(), sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
                            {
                                continue;
                            }

                            if (final_looter->lastTimeLooted > grp_plr->lastTimeLooted)
                            {
                                final_looter = grp_plr;
                            }
                        }
                    }

                    final_looter->lastTimeLooted = time(nullptr);

                    creature->Taking().AssignedTo(final_looter->GetGUIDLow());
                    final_looter->GetGroup()->SetLooterGuid(final_looter->GetObjectGuid());

                    return (final_looter->GetGUIDLow() == GetGUIDLow() || hasSharedLoot || hasStartingQuestLoot);
                }
                default:

                    return false;
            }
        }

        else if (recipient == this)
        {
            return true;
        }
    }
    else

    {
        return !creature->Claim().IsClaimed();
    }

    return false;
}

void Player::_LoadActions(QueryResult* result)
{
    m_actionButtons.clear();

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint8 button = fields[0].GetUInt8();
            uint32 action = fields[1].GetUInt32();
            uint8 type = fields[2].GetUInt8();

            if (ActionButton* ab = addActionButton(button, action, type))
            {
                ab->uState = ACTIONBUTTON_UNCHANGED;
            }
            else
            {
                sLog.outError("  ...at loading, and will deleted in DB also");

                m_actionButtons[button].uState = ACTIONBUTTON_DELETED;
            }
        }
        while (result->NextRow());

        delete result;
    }
}

void Player::_LoadAuras(QueryResult* result, uint32 timediff)
{

    for (int i = UNIT_FIELD_AURA; i <= UNIT_FIELD_AURASTATE; ++i)
    {
        SetUInt32Value(i, 0);
    }

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            ObjectGuid caster_guid = static_cast<ObjectGuid>(fields[0].GetUInt64());
            uint32 item_lowguid = fields[1].GetUInt32();
            uint32 spellid = fields[2].GetUInt32();
            uint32 stackcount = fields[3].GetUInt32();
            uint32 remaincharges = fields[4].GetUInt32();
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = fields[i + 5].GetInt32();
                periodicTime[i] = fields[i + 8].GetUInt32();
            }

            int32 maxduration = fields[11].GetInt32();
            int32 remaintime = fields[12].GetInt32();
            uint32 effIndexMask = fields[13].GetUInt32();

            SpellEntry const* spellproto = sSpellStore.LookupEntry(spellid);
            if (!spellproto)
            {
                sLog.outError("Unknown spell (spellid %u), ignore.", spellid);
                continue;
            }

            if (remaintime != -1 && !cast::RecipeOf(*spellproto).IsPositive())
            {
                if (remaintime / IN_MILLISECONDS <= int32(timediff))
                {
                    continue;
                }

                remaintime -= timediff * IN_MILLISECONDS;
            }

            if (spellproto->ProcCharges == 0)
            {
                remaincharges = 0;
            }

            if (!spellproto->CumulativeAura)
            {
                stackcount = 1;
            }
            else if (spellproto->CumulativeAura < stackcount)
            {
                stackcount = spellproto->CumulativeAura;
            }
            else if (!stackcount)
            {
                stackcount = 1;
            }

            SpellAuraHolder* holder = CreateSpellAuraHolder(spellproto, this, nullptr);
            holder->SetLoadedState(caster_guid, MakeGuid(HIGHGUID_ITEM, item_lowguid), stackcount, remaincharges, maxduration, remaintime);

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if ((effIndexMask & (1 << i)) == 0)
                {
                    continue;
                }

                Aura* aura = CreateAura(spellproto, SpellEffectIndex(i), nullptr, holder, this);
                if (!damage[i])
                {
                    damage[i] = aura->GetModifier()->m_amount;
                }

                aura->SetLoadedState(damage[i], periodicTime[i]);
                holder->AddAura(aura, SpellEffectIndex(i));
            }

            if (!holder->IsEmptyHolder())
            {

                if (caster_guid != GetObjectGuid() && holder->GetTrackedAuraType() == TRACK_AURA_TYPE_SINGLE_TARGET)
                {
                    holder->SetTrackedAuraType(TRACK_AURA_TYPE_NOT_TRACKED);
                }

                AddSpellAuraHolder(holder);
                DETAIL_LOG("Added auras from spellid %u", spellproto->ID);
            }
            else
            {
                delete holder;
            }
        }
        while (result->NextRow());
        delete result;
    }

    if (getClass() == CLASS_WARRIOR && !HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
    {
        CastSpell(this, SPELL_ID_PASSIVE_BATTLE_STANCE, true);
    }
}

void Player::LoadCorpse()
{
    if (IsAlive())
    {
        sCorpseManager.ConvertCorpseForPlayer(GetObjectGuid());
    }
    else
    {
        if (Corpse* corpse = GetCorpse())
        {
            ShowReleaseTimer(corpse && !sMapStore.LookupEntry(corpse->GetMapId())->Instanceable());
        }
        else
        {

            ResurrectPlayer(0.5f);
        }
    }
}

void Player::_LoadInventory(QueryResult* result, uint32 timediff)
{

    std::map<uint32, Bag*> bagMap;

    uint32 zone = GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z());

    if (result)
    {
        std::list<Item*> problematicItems;

        m_inventory.Saves().Shut(true);
        do
        {
            Field* fields = result->Fetch();
            uint32 bag_guid  = fields[1].GetUInt32();
            uint8  slot      = fields[2].GetUInt8();
            uint32 item_lowguid = fields[3].GetUInt32();
            uint32 item_id   = fields[4].GetUInt32();

            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_id);

            if (!proto)
            {
                CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` = '%u'", item_lowguid);
                CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid` = '%u'", item_lowguid);
                sLog.outError("Player::_LoadInventory: Player %s has an unknown item (id: #%u) in inventory, deleted.", GetName(), item_id);
                continue;
            }

            Item* item = NewItemOrBag(proto);

            if (!item->LoadFromDB(item_lowguid, fields, GetObjectGuid()))
            {
                sLog.outError("Player::_LoadInventory: Player %s has broken item (id: #%u) in inventory, deleted.", GetName(), item_id);
                CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` = '%u'", item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();
                continue;
            }

            if (IsAlive() && item->IsLimitedToAnotherMapOrZone(GetMapId(), zone))
            {
                CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` = '%u'", item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();
                continue;
            }

            if (timediff > 15 * MINUTE && (item->GetProto()->Flags & ITEM_FLAG_CONJURED))
            {
                CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` = '%u'", item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();
                continue;
            }

            bool success = true;

            if (!bag_guid)
            {
                item->SetContainer(nullptr);
                item->SetSlot(slot);

                if (Inventory::IsCarried(INVENTORY_SLOT_BAG_0, slot))
                {
                    ItemPosCountVec dest;
                    if (CanStoreItem(INVENTORY_SLOT_BAG_0, slot, dest, item, false) == EQUIP_ERR_OK)
                    {
                        item = StoreItem(dest, item, true);
                    }
                    else
                    {
                        success = false;
                    }
                }
                else if (Inventory::IsWorn(INVENTORY_SLOT_BAG_0, slot))
                {
                    uint16 dest;
                    if (CanEquipItem(slot, dest, item, false, false) == EQUIP_ERR_OK)
                    {
                        QuickEquipItem(dest, item);
                    }
                    else
                    {
                        success = false;
                    }
                }
                else if (Inventory::IsBanked(INVENTORY_SLOT_BAG_0, slot))
                {
                    ItemPosCountVec dest;
                    if (CanBankItem(INVENTORY_SLOT_BAG_0, slot, dest, item, false, false) == EQUIP_ERR_OK)
                    {
                        item = BankItem(dest, item, true);
                    }
                    else
                    {
                        success = false;
                    }
                }

                if (success)
                {

                    if (item->IsBag() && Inventory::HoldsBag(item->GetPos()))
                    {
                        bagMap[item_lowguid] = (Bag*)item;
                    }
                }
            }

            else
            {
                item->SetSlot(NULL_SLOT);

                std::map<uint32, Bag*>::const_iterator itr = bagMap.find(bag_guid);
                if (itr != bagMap.end() && slot < itr->second->GetBagSize())
                {
                    ItemPosCountVec dest;
                    if (CanStoreItem(itr->second->GetSlot(), slot, dest, item, false) == EQUIP_ERR_OK)
                    {
                        item = StoreItem(dest, item, true);
                    }
                    else
                    {
                        success = false;
                    }
                }
                else
                {
                    success = false;
                }
            }

            if (success)
            {
                item->SetState(ITEM_UNCHANGED, this);

                if (item->GetContainer())
                {
                    item->GetContainer()->SetState(ITEM_UNCHANGED, this);
                }
            }
            else
            {
                sLog.outError("Player::_LoadInventory: Player %s has item (GUID: %u Entry: %u) can't be loaded to inventory (Bag GUID: %u Slot: %u) by some reason, will send by mail.", GetName(), item_lowguid, item_id, bag_guid, slot);
                CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` = '%u'", item_lowguid);
                problematicItems.push_back(item);
            }
        }
        while (result->NextRow());

        delete result;
        m_inventory.Saves().Shut(false);

        while (!problematicItems.empty())
        {
            std::string subject = "Item could not be loaded to inventory.";
            std::string content = GetSession()->GetMangosString(LANG_NOT_EQUIPPED_ITEM);

            MailDraft draft(subject,"");
            draft.SetSubjectAndBody(subject,content);
            for (int i = 0; !problematicItems.empty() && i < MAX_MAIL_ITEMS; ++i)
            {
                Item* item = problematicItems.front();
                problematicItems.pop_front();

                draft.AddItem(item);
            }

            draft.SendMailTo(this, MailSender(this, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
        }
    }

    _ApplyAllItemMods();
}

void Player::_LoadItemLoot(QueryResult* result)
{

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 item_guid   = fields[0].GetUInt32();

            Item* item = GetItemByGuid(MakeGuid(HIGHGUID_ITEM, item_guid));

            if (!item)
            {
                CharacterDatabase.PExecute("DELETE FROM `item_loot` WHERE `guid` = '%u'", item_guid);
                sLog.outError("Player::_LoadItemLoot: Player %s has loot for nonexistent item (GUID: %u) in `item_loot`, deleted.", GetName(), item_guid);
                continue;
            }

            item->LoadLootFromDB(fields);
        }
        while (result->NextRow());

        delete result;
    }
}

void Player::_LoadMailedItems(QueryResult* result)
{

    if (!result)
    {
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        uint32 mail_id       = fields[1].GetUInt32();
        uint32 item_guid_low = fields[2].GetUInt32();
        uint32 item_template = fields[3].GetUInt32();

        Mail* mail = Post().Find(mail_id);
        if (!mail)
        {
            continue;
        }
        mail->AddItem(item_guid_low, item_template);

        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_template);

        if (!proto)
        {
            sLog.outError("Player %u has unknown item_template (ProtoType) in mailed items(GUID: %u template: %u) in mail (%u), deleted.", GetGUIDLow(), item_guid_low, item_template, mail->messageID);
            CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `item_guid` = '%u'", item_guid_low);
            CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid` = '%u'", item_guid_low);
            continue;
        }

        Item* item = NewItemOrBag(proto);

        if (!item->LoadFromDB(item_guid_low, fields, GetObjectGuid()))
        {
            sLog.outError("Player::_LoadMailedItems - Item in mail (%u) doesn't exist !!!! - item guid: %u, deleted from mail", mail->messageID, item_guid_low);
            CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `item_guid` = '%u'", item_guid_low);
            item->FSetState(ITEM_REMOVED);
            item->SaveToDB();
            continue;
        }

        Post().Keep(item);
    }
    while (result->NextRow());

    delete result;
}

void Player::_LoadMails(QueryResult* result)
{
    Post().Letters().clear();

    if (!result)
    {
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        Mail* m = new Mail;
        m->messageID = fields[0].GetUInt32();
        m->messageType = fields[1].GetUInt8();
        m->sender = fields[2].GetUInt32();
        m->receiverGuid = MakeGuid(HIGHGUID_PLAYER, fields[3].GetUInt32());
        m->subject = fields[4].GetCppString();
        m->body = fields[5].GetCppString();
        m->expire_time = (time_t)fields[6].GetUInt64();
        m->deliver_time = (time_t)fields[7].GetUInt64();
        m->money = fields[8].GetUInt32();
        m->COD = fields[9].GetUInt32();
        m->checked = fields[10].GetUInt32();
        m->stationery = fields[11].GetUInt8();
        m->mailTemplateId = fields[12].GetInt16();
        m->has_items = fields[13].GetBool();

        if (m->mailTemplateId && !sMailTemplateStore.LookupEntry(m->mailTemplateId))
        {
            sLog.outError("Player::_LoadMail - Mail (%u) have nonexistent MailTemplateId (%u), remove at load", m->messageID, m->mailTemplateId);
            m->mailTemplateId = 0;
        }

        m->state = MAIL_STATE_UNCHANGED;

        Post().Letters().push_back(m);

        if (m->mailTemplateId && !m->has_items)
        {
            m->prepareTemplateItems(this);
        }
    }
    while (result->NextRow());
    delete result;
}

void Player::LoadPet()
{

    if (IsInWorld())
    {
        Pet* pet = new Pet;
        if (!pet->LoadPetFromDB(this, 0, 0, true))
        {
            delete pet;
        }
    }
}

void Player::_LoadQuestStatus(QueryResult* result)
{
    m_journal.Clear();

    uint32 slot = 0;

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 quest_id = fields[0].GetUInt32();

            Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);
            if (pQuest)
            {

                QuestStatusData& questStatusData = m_journal.Of(quest_id);

                uint32 qstatus = fields[1].GetUInt32();
                if (qstatus < MAX_QUEST_STATUS)
                {
                    questStatusData.m_status = QuestStatus(qstatus);
                }
                else
                {
                    questStatusData.m_status = QUEST_STATUS_NONE;
                    sLog.outError("Player %s have invalid quest %d status (%d), replaced by QUEST_STATUS_NONE(0).", GetName(), quest_id, qstatus);
                }

                questStatusData.m_rewarded = (fields[2].GetUInt8() > 0);
                questStatusData.m_explored = (fields[3].GetUInt8() > 0);

                time_t quest_time = time_t(fields[4].GetUInt64());

                if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED) && !GetQuestRewardStatus(quest_id) && questStatusData.m_status != QUEST_STATUS_NONE)
                {
                    AddTimedQuest(quest_id);

                    if (quest_time <= sWorld.GetGameTime())
                    {
                        questStatusData.m_timer = 1;
                    }
                    else
                    {
                        questStatusData.m_timer = uint32(quest_time - sWorld.GetGameTime()) * IN_MILLISECONDS;
                    }
                }
                else
                {
                    quest_time = 0;
                }

                questStatusData.m_creatureOrGOcount[0] = fields[5].GetUInt32();
                questStatusData.m_creatureOrGOcount[1] = fields[6].GetUInt32();
                questStatusData.m_creatureOrGOcount[2] = fields[7].GetUInt32();
                questStatusData.m_creatureOrGOcount[3] = fields[8].GetUInt32();
                questStatusData.m_itemcount[0] = fields[9].GetUInt32();
                questStatusData.m_itemcount[1] = fields[10].GetUInt32();
                questStatusData.m_itemcount[2] = fields[11].GetUInt32();
                questStatusData.m_itemcount[3] = fields[12].GetUInt32();

                questStatusData.uState = QUEST_UNCHANGED;

                if (slot < MAX_QUEST_LOG_SIZE &&
                    ((questStatusData.m_status == QUEST_STATUS_INCOMPLETE ||
                    questStatusData.m_status == QUEST_STATUS_COMPLETE ||
                    questStatusData.m_status == QUEST_STATUS_FAILED) &&
                    (!questStatusData.m_rewarded || pQuest->IsRepeatable())))
                {
                    SetQuestSlot(slot, quest_id, uint32(quest_time));

                    if (questStatusData.m_explored)
                    {
                        SetQuestSlotState(slot, QUEST_STATE_COMPLETE);
                    }

                    if (questStatusData.m_status == QUEST_STATUS_COMPLETE)
                    {
                        SetQuestSlotState(slot, QUEST_STATE_COMPLETE);
                    }

                    if (questStatusData.m_status == QUEST_STATUS_FAILED)
                    {
                        SetQuestSlotState(slot, QUEST_STATE_FAIL);
                    }

                    for (uint8 idx = 0; idx < QUEST_OBJECTIVES_COUNT; ++idx)
                    {
                        if (questStatusData.m_creatureOrGOcount[idx])
                        {
                            SetQuestSlotCounter(slot, idx, questStatusData.m_creatureOrGOcount[idx]);
                        }
                    }
                    ++slot;
                }

                if (questStatusData.m_rewarded)
                {

                    learnQuestRewardedSpells(pQuest);
                }

                DEBUG_LOG("Quest status is {%u} for quest {%u} for player (GUID: %u)", questStatusData.m_status, quest_id, GetGUIDLow());
            }
        }
        while (result->NextRow());

        delete result;
    }

    for (uint16 i = slot; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        SetQuestSlot(i, 0);
    }
}

void Player::_LoadSpells(QueryResult* result)
{

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 spell_id = fields[0].GetUInt32();

            addSpell(spell_id, fields[1].GetBool(), false, false, fields[2].GetBool());
        }
        while (result->NextRow());

        delete result;
    }
}

void Player::_LoadGroup(QueryResult* result)
{

    if (result)
    {
        uint32 groupId = (*result)[0].GetUInt32();
        delete result;

        if (Group* group = sObjectMgr.GetGroupById(groupId))
        {
            uint8 subgroup = group->GetMemberGroup(GetObjectGuid());
            SetGroup(group, subgroup);
        }
    }
}

void Player::ConvertInstancesToGroup(Player* player, Group* group, ObjectGuid player_guid)
{
    bool has_binds = false;
    bool has_solo = false;

    if (player)
    {
        player_guid = player->GetObjectGuid();
        if (!group)
        {
            group = player->GetGroup();
        }
    }

    MANGOS_ASSERT(player_guid);

    if (player)
    {
        DungeonHolds& held = player->Binds().All();
        for (DungeonHolds::iterator itr = held.begin(); itr != held.end();)
        {
            has_binds = true;

            if (group)
            {
                group->Binds().BindTo(itr->second.state, itr->second.permanent, true);
            }

            if (!itr->second.permanent)
            {

                player->Binds().Release(itr, true);
                has_solo = true;
            }
            else
            {
                ++itr;
            }
        }
    }

    uint32 player_lowguid = GuidCounter(player_guid);

    if (!player || !group || has_binds)
    {
        CharacterDatabase.PExecute("INSERT INTO `group_instance` SELECT `guid`, `instance`, `permanent` FROM `character_instance` WHERE `guid` = '%u'", player_lowguid);
    }

    if (!player || has_solo)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u' AND `permanent` = 0", player_lowguid);
    }
}

bool Player::_LoadHomeBind(QueryResult* result)
{
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());
    if (!info)
    {
        sLog.outError("Player have incorrect race/class pair. Can't be loaded.");
        return false;
    }

    bool ok = false;

    if (result)
    {
        Field* fields = result->Fetch();
        Home().SetTo(fields[0].GetUInt32(), fields[1].GetUInt16(),
                     fields[2].GetFloat(), fields[3].GetFloat(), fields[4].GetFloat());
        delete result;

        MapEntry const* bindMapEntry = sMapStore.LookupEntry(Home().MapId());

        if (MapCoords::Valid(Home().MapId(), Home().X(), Home().Y(), Home().Z()) &&
            !bindMapEntry->Instanceable())
        {
            ok = true;
        }
        else
        {
            CharacterDatabase.PExecute("DELETE FROM `character_homebind` WHERE `guid` = '%u'", GetGUIDLow());
        }
    }

    if (!ok)
    {
        Home().SetTo(info->mapId, uint16(info->areaId),
                     info->positionX, info->positionY, info->positionZ);

        CharacterDatabase.PExecute("INSERT INTO `character_homebind` (`guid`,`map`,`zone`,`position_x`,`position_y`,`position_z`) VALUES ('%u', '%u', '%u', '%f', '%f', '%f')", GetGUIDLow(), Home().MapId(), (uint32)Home().AreaId(), Home().X(), Home().Y(), Home().Z());
    }

    DEBUG_LOG("Setting player home position: mapid is: %u, zoneid is %u, X is %f, Y is %f, Z is %f",
        Home().MapId(), Home().AreaId(), Home().X(), Home().Y(), Home().Z());

    return true;
}
