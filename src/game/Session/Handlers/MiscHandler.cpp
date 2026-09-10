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

#include <zlib.h>
#include "Reaction.h"
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <string>
#include <ctime>
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "World.h"
#include "CinematicFlyover.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "UpdateData.h"
#include "LootMgr.h"
#include "Chat.h"
#include "ScriptMgr.h"
#include "PlayerRegistry.h"
#include "ObjectLookup.h"
#include "Occupant.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Pet.h"
#include "SocialMgr.h"
#include "Corpse.h"

void WorldSession::HandleRepopRequestOpcode(WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REPOP_REQUEST");

    if (GetPlayer()->IsAlive() || GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_GHOST))
    {
        return;
    }

    if (GetPlayer()->GetDeathState() == JUST_DIED)
    {
        DEBUG_LOG("HandleRepopRequestOpcode: got request after player %s(%d) was killed and before he was updated", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow());
        GetPlayer()->KillPlayer();
    }

    GetPlayer()->RemovePet(PET_SAVE_REAGENTS);
    GetPlayer()->BuildPlayerRepop();
    GetPlayer()->RepopAtGraveyard();
}

void WorldSession::HandleWhoOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_WHO");

    uint32 level_min, level_max, racemask, classmask, zones_count, str_count;
    uint32 zoneids[10];
    std::string player_name, guild_name;

    recv_data >> level_min;
    recv_data >> level_max;
    recv_data >> player_name;

    recv_data >> guild_name;

    recv_data >> racemask;
    recv_data >> classmask;
    recv_data >> zones_count;

    if (zones_count > 10)
    {
        return;
    }

    for (uint32 i = 0; i < zones_count; ++i)
    {
        uint32 temp;
        recv_data >> temp;
        zoneids[i] = temp;
        DEBUG_LOG("Zone %u: %u", i, zoneids[i]);
    }

    recv_data >> str_count;

    if (str_count > 4)
    {
        return;
    }

    DEBUG_LOG("Minlvl %u, maxlvl %u, name %s, guild %s, racemask %u, classmask %u, zones %u, strings %u", level_min, level_max, player_name.c_str(), guild_name.c_str(), racemask, classmask, zones_count, str_count);

    std::wstring str[4];
    for (uint32 i = 0; i < str_count; ++i)
    {
        std::string temp;
        recv_data >> temp;

        if (!Utf8toWStr(temp, str[i]))
        {
            continue;
        }

        wstrToLower(str[i]);

        DEBUG_LOG("String %u: %s", i, temp.c_str());
    }

    std::wstring wplayer_name;
    std::wstring wguild_name;
    if (!(Utf8toWStr(player_name, wplayer_name) && Utf8toWStr(guild_name, wguild_name)))
    {
        return;
    }
    wstrToLower(wplayer_name);
    wstrToLower(wguild_name);

    if (level_max >= MAX_LEVEL)
    {
        level_max = STRONG_MAX_LEVEL;
    }

    Team team = _player->GetTeam();
    AccountTypes security = GetSecurity();
    bool allowTwoSideWhoList = sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST);
    AccountTypes gmLevelInWhoList = (AccountTypes)sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST);

    const uint32 zone = _player->GetCachedZoneId();
    const bool notInBattleground = !((zone == 2597) || (zone == 3277) || (zone == 3358));

    uint32 matchcount = 0;
    uint32 displaycount = 0;

    WorldPacket data(SMSG_WHO, 50);
    data << uint32(matchcount);
    data << uint32(displaycount);

    sPlayerRegistry.ForEach([&](Player* pl)
    {
        if (security == SEC_PLAYER)
        {

            if (pl->GetTeam() != team && !allowTwoSideWhoList)
            {
                return;
            }

            if (pl->GetSession()->GetSecurity() > gmLevelInWhoList)
            {
                return;
            }
        }

        if (!pl->IsInWorld())
        {
            return;
        }

        if (!pl->IsVisibleGloballyFor(_player))
        {
            return;
        }

        uint32 lvl = pl->getLevel();
        if (lvl < level_min || lvl > level_max)
        {
            return;
        }

        uint32 class_ = pl->getClass();
        if (!(classmask & (1 << class_)))
        {
            return;
        }

        uint32 race = pl->getRace();
        if (!(racemask & (1 << race)))
        {
            return;
        }

        uint32 pzoneid = pl->GetTerrain()->GetZoneId(pl->Where().X(), pl->Where().Y(), pl->Where().Z());

        bool z_show = true;
        for (uint32 i = 0; i < zones_count; ++i)
        {
            if (zoneids[i] == pzoneid)
            {

                z_show = (zone != pzoneid) || notInBattleground || (_player->GetInstanceId() == pl->GetInstanceId());
                break;
            }

            z_show = false;
        }
        if (!z_show)
        {
            return;
        }

        std::string pname = pl->GetName();
        std::wstring wpname;
        if (!Utf8toWStr(pname, wpname))
        {
            return;
        }
        wstrToLower(wpname);

        if (!(wplayer_name.empty() || wpname.find(wplayer_name) != std::wstring::npos))
        {
            return;
        }

        std::string gname = sGuildMgr.GetGuildNameById(pl->GetGuildId());
        std::wstring wgname;
        if (!Utf8toWStr(gname, wgname))
        {
            return;
        }
        wstrToLower(wgname);

        if (!(wguild_name.empty() || wgname.find(wguild_name) != std::wstring::npos))
        {
            return;
        }

        std::string aname;
        if (AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(pzoneid))
        {
            aname = areaEntry->AreaName_lang[GetSessionDbcLocale()];
        }

        bool s_show = true;
        for (uint32 i = 0; i < str_count; ++i)
        {
            if (!str[i].empty())
            {
                if (wgname.find(str[i]) != std::wstring::npos ||
                    wpname.find(str[i]) != std::wstring::npos ||
                    Utf8FitTo(aname, str[i]))
                {
                    s_show = true;
                    break;
                }
                s_show = false;
            }
        }
        if (!s_show)
        {
            return;
        }

        if (++matchcount > 49)
        {
            return;
        }

        ++displaycount;

        data << pname;
        data << gname;
        data << uint32(lvl);
        data << uint32(class_);
        data << uint32(race);
        data << uint32(pzoneid);
    });

    if (sWorld.getConfig(CONFIG_UINT32_MAX_WHOLIST_RETURNS) && matchcount > sWorld.getConfig(CONFIG_UINT32_MAX_WHOLIST_RETURNS))
    {
        matchcount = sWorld.getConfig(CONFIG_UINT32_MAX_WHOLIST_RETURNS);
    }

    data.put(0, displaycount);
    data.put(4, matchcount);

    SendPacket(&data);
    DEBUG_LOG("WORLD: Send SMSG_WHO Message");
}

void WorldSession::HandleLogoutRequestOpcode(WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_LOGOUT_REQUEST, security %u", GetSecurity());

    if (ObjectGuid lootGuid = GetPlayer()->GetLootGuid())
    {
        DoLootRelease(lootGuid);
    }

    bool instantLogout = (GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_RESTING) || GetPlayer()->IsTaxiFlying() || GetSecurity() >= (AccountTypes)sWorld.getConfig(CONFIG_UINT32_INSTANT_LOGOUT));

    bool canLogoutInCombat = GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_RESTING);

    uint8 reason = 0;
    if (GetPlayer()->IsInCombat() && !canLogoutInCombat)
    {
        reason = 1;
    }
    else if (GetPlayer()->m_movementInfo.HasMovementFlag(MovementFlags(MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR)))
    {
        reason = 3;
    }
    else if (GetPlayer()->Duelling().Stands() || GetPlayer()->HasAura(SPELL_GM_FREEZE))
    {
        reason = 2;
    }

    WorldPacket data(SMSG_LOGOUT_RESPONSE, 1 + 4);
    data << uint32(reason);
    data << uint8(instantLogout);
    SendPacket(&data);

    if (reason)
    {
        LogoutRequest(time(0));
        return;
    }

    if (instantLogout)
    {
        LogoutPlayer(true);
        return;
    }

    if (GetPlayer()->CanFreeMove())
    {
        float height = GetPlayer()->GetMap()->GetHeight(GetPlayer()->Where().X(), GetPlayer()->Where().Y(), GetPlayer()->Where().Z());
        if ((GetPlayer()->Where().Z() < height + 0.1f) && !(GetPlayer()->IsInWater()))
        {
            GetPlayer()->SetStandState(UNIT_STAND_STATE_SIT);
        }

        GetPlayer()->SetRoot(true);
        GetPlayer()->SetUnitFlag(UNIT_FLAG_STUNNED);
    }

    LogoutRequest(time(nullptr));
}

void WorldSession::HandlePlayerLogoutOpcode(WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_PLAYER_LOGOUT Message");
}

void WorldSession::HandleLogoutCancelOpcode(WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_LOGOUT_CANCEL Message");

    if (!GetPlayer())
    {
        return;
    }

    LogoutRequest(0);

    WorldPacket data(SMSG_LOGOUT_CANCEL_ACK, 0);
    SendPacket(&data);

    if (GetPlayer()->CanFreeMove())
    {

        GetPlayer()->SetRoot(false);

        GetPlayer()->SetStandState(UNIT_STAND_STATE_STAND);

        GetPlayer()->RemoveUnitFlag(UNIT_FLAG_STUNNED);
    }

    DEBUG_LOG("WORLD: sent SMSG_LOGOUT_CANCEL_ACK Message");
}

void WorldSession::HandleTogglePvP(WorldPacket& recv_data)
{

    if (recv_data.size() == 1)
    {
        bool newPvPStatus;
        recv_data >> newPvPStatus;
        GetPlayer()->ApplyPlayerFlag(PLAYER_FLAGS_IN_PVP, newPvPStatus);
    }
    else
    {
        GetPlayer()->TogglePlayerFlag(PLAYER_FLAGS_IN_PVP);
    }

    if (GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_IN_PVP))
    {
        if (!GetPlayer()->IsPvP() || GetPlayer()->pvpInfo.endTimer != 0)
        {
            GetPlayer()->UpdatePvP(true, true);
        }
    }
    else
    {
        if (!GetPlayer()->pvpInfo.inHostileArea && GetPlayer()->IsPvP())
        {
            GetPlayer()->pvpInfo.endTimer = time(nullptr);
        }
    }
}

void WorldSession::HandleZoneUpdateOpcode(WorldPacket& recv_data)
{
    uint32 newZone;
    recv_data >> newZone;

    DETAIL_LOG("WORLD: Received opcode CMSG_ZONEUPDATE: newzone is %u", newZone);

    uint32 newzone, newarea;
    GetPlayer()->GetTerrain()->GetZoneAndAreaId(newzone, newarea, GetPlayer()->Where().X(), GetPlayer()->Where().Y(), GetPlayer()->Where().Z());
    GetPlayer()->UpdateZone(newzone, newarea);
}

void WorldSession::HandleSetTargetOpcode(WorldPacket& recv_data)
{

    ObjectGuid guid  = 0;
    recv_data >> guid;

    _player->SetTargetGuid(guid);

    Unit* unit = ObjectLookup::GetUnit(*_player, guid);
    if (!unit)
    {
        return;
    }

    if (FactionTemplateEntry const* factionTemplateEntry = sFactionTemplateStore.LookupEntry(unit->getFaction()))
    {
        _player->GetReputationMgr().SetVisible(factionTemplateEntry);
    }
}

void WorldSession::HandleSetSelectionOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    recv_data >> guid;

    _player->SetSelectionGuid(guid);

    if ((guid == 0))
    {
        _player->InterruptSpell(CURRENT_AUTOREPEAT_SPELL, false);
        return;
    }

    Unit* unit = ObjectLookup::GetUnit(*_player, guid);
    if (!unit)
    {
        return;
    }

    if (FactionTemplateEntry const* factionTemplateEntry = sFactionTemplateStore.LookupEntry(unit->getFaction()))
    {
        _player->GetReputationMgr().SetVisible(factionTemplateEntry);
    }
}

void WorldSession::HandleStandStateChangeOpcode(WorldPacket& recv_data)
{

    uint32 animstate;
    recv_data >> animstate;

    _player->SetStandState(animstate);
}

void WorldSession::HandleFriendListOpcode(WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_FRIEND_LIST");
    _player->GetSocial()->SendFriendList(_player);
}

void WorldSession::HandleBugOpcode(WorldPacket& recv_data)
{
    uint32 suggestion, contentlen, typelen;
    std::string content, type;

    recv_data >> suggestion >> contentlen >> content;

    recv_data >> typelen >> type;

    if (suggestion == 0)
    {
        DEBUG_LOG("WORLD: Received opcode CMSG_BUG [Bug Report]");
    }
    else
    {
        DEBUG_LOG("WORLD: Received opcode CMSG_BUG [Suggestion]");
    }

    DEBUG_LOG("%s", type.c_str());
    DEBUG_LOG("%s", content.c_str());

    CharacterDatabase.escape_string(type);
    CharacterDatabase.escape_string(content);
    CharacterDatabase.PExecute("INSERT INTO `bugreport` (`type`,`content`) VALUES('%s', '%s')", type.c_str(), content.c_str());
}

void WorldSession::HandleReclaimCorpseOpcode(WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_RECLAIM_CORPSE");

    ObjectGuid guid = 0;
    recv_data >> guid;

    if (GetPlayer()->IsAlive())
    {
        return;
    }

    if (!GetPlayer()->HasPlayerFlag(PLAYER_FLAGS_GHOST))
    {
        return;
    }

    Corpse* corpse = GetPlayer()->GetCorpse();

    if (!corpse)
    {
        return;
    }

    if (corpse->GetGhostTime() + GetPlayer()->GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP) > time(nullptr))
    {
        return;
    }

    if (!InReach(*corpse, *(GetPlayer()), CORPSE_RECLAIM_RADIUS, true))
    {
        return;
    }

    GetPlayer()->ResurrectPlayer(GetPlayer()->Battle().InOne() ? 1.0f : 0.5f);

    GetPlayer()->SpawnCorpseBones();
}

void WorldSession::HandleResurrectResponseOpcode(WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_RESURRECT_RESPONSE");

    ObjectGuid guid = 0;
    uint8 status;
    recv_data >> guid;
    recv_data >> status;

    if (GetPlayer()->IsAlive())
    {
        return;
    }

    if (status == 0)
    {
        GetPlayer()->clearResurrectRequestData();
        return;
    }

    if (!GetPlayer()->isRessurectRequestedBy(guid))
    {
        return;
    }

    GetPlayer()->ResurectUsingRequestData();
}

void WorldSession::HandleAreaTriggerOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_AREATRIGGER");

    uint32 Trigger_ID;

    recv_data >> Trigger_ID;
    DEBUG_LOG("Trigger ID: %u", Trigger_ID);
    Player* player = GetPlayer();

    if (player->IsTaxiFlying())
    {
        DEBUG_LOG("Player '%s' (GUID: %u) in flight, ignore Area Trigger ID: %u", player->GetName(), player->GetGUIDLow(), Trigger_ID);
        return;
    }

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
    if (!atEntry)
    {
        DEBUG_LOG("Player '%s' (GUID: %u) send unknown (by DBC) Area Trigger ID: %u", player->GetName(), player->GetGUIDLow(), Trigger_ID);
        return;
    }

    const float delta = 5.0f;

    if (!IsPointInAreaTriggerZone(atEntry, player->GetMapId(), player->Where().X(), player->Where().Y(), player->Where().Z(), delta))
    {
        DEBUG_LOG("Player '%s' (GUID: %u) too far, ignore Area Trigger ID: %u", player->GetName(), player->GetGUIDLow(), Trigger_ID);
        return;
    }

    if (sScriptMgr.OnAreaTrigger(player, atEntry))
    {
        return;
    }

    uint32 quest_id = sObjectMgr.GetQuestForAreaTrigger(Trigger_ID);
    if (quest_id && player->IsAlive() && player->IsActiveQuest(quest_id))
    {
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);
        if (pQuest)
        {
            if (player->GetQuestStatus(quest_id) == QUEST_STATUS_INCOMPLETE)
            {
                player->Journal().Explored(quest_id);
            }
        }
    }

    if (sObjectMgr.IsTavernAreaTrigger(Trigger_ID))
    {

        if (player->Resting().Kind() != REST_TYPE_IN_CITY)
        {
            player->Resting().Kind(REST_TYPE_IN_TAVERN, Trigger_ID);
        }
        return;
    }

    if (BattleGround* bg = player->Battle().Ground())
    {
        if (bg->HandleAreaTrigger(player, Trigger_ID))
        {
            return;
        }
    }
    else if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
    {
        if (outdoorPvP->HandleAreaTrigger(player, Trigger_ID))
        {
            return;
        }
    }

    AreaTrigger const* at = sObjectMgr.GetAreaTrigger(Trigger_ID);
    if (!at)
    {
        return;
    }

    MapEntry const* targetMapEntry = sMapStore.LookupEntry(at->target_mapId);
    if (!targetMapEntry)
    {
        return;
    }

    if (!player->IsAlive() && targetMapEntry->IsDungeon())
    {
        uint32 corpseMapId = 0;
        if (Corpse* corpse = player->GetCorpse())
        {
            corpseMapId = corpse->GetMapId();
        }

        uint32 instance_map = corpseMapId;
        do
        {

            if (instance_map == targetMapEntry->MapID)
            {
                break;
            }

            InstanceTemplate const* instance = ObjectMgr::GetInstanceTemplate(instance_map);
            instance_map = instance ? instance->parent : 0;
        }
        while (instance_map);

        if (!instance_map)
        {
            player->GetSession()->SendAreaTriggerMessage("You can not enter %s while in a ghost mode",
                targetMapEntry->MapName_lang[player->GetSession()->GetSessionDbcLocale()]);
            return;
        }

        if (at->target_mapId != corpseMapId)
        {
            if (AreaTrigger const* corpseAt = sObjectMgr.GetMapEntranceTrigger(corpseMapId))
            {
                at = corpseAt;
                targetMapEntry = sMapStore.LookupEntry(at->target_mapId);
                if (!targetMapEntry)
                {
                    return;
                }
            }
        }

        player->ResurrectPlayer(0.5f);
        player->SpawnCorpseBones();
    }

    uint32 miscRequirement = 0;
    AreaLockStatus lockStatus = player->GetAreaTriggerLockStatus(at, miscRequirement);
    if (lockStatus != AREA_LOCKSTATUS_OK)
    {
        player->SendTransferAbortedByLockStatus(targetMapEntry, at, lockStatus, miscRequirement);
        return;
    }

    player->TeleportTo(at->target_mapId, at->target_X, at->target_Y, at->target_Z, at->target_Orientation, TELE_TO_NOT_LEAVE_TRANSPORT, true);
}

void WorldSession::HandleUpdateAccountData(WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_UPDATE_ACCOUNT_DATA");
    recv_data.rpos(recv_data.wpos());

}

void WorldSession::HandleRequestAccountData(WorldPacket& )
{
    DETAIL_LOG("WORLD: Received opcode CMSG_REQUEST_ACCOUNT_DATA");

}

void WorldSession::HandleSetActionButtonOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_ACTION_BUTTON");
    uint8 button;
    uint32 packetData;
    recv_data >> button >> packetData;

    uint32 action = ACTION_BUTTON_ACTION(packetData);
    uint8  type   = ACTION_BUTTON_TYPE(packetData);

    DETAIL_LOG("BUTTON: %u ACTION: %u TYPE: %u", button, action, type);
    if (!packetData)
    {
        DETAIL_LOG("MISC: Remove action from button %u", button);
        GetPlayer()->removeActionButton(button);
    }
    else
    {
        switch (type)
        {
            case ACTION_BUTTON_MACRO:
            case ACTION_BUTTON_CMACRO:
                DETAIL_LOG("MISC: Added Macro %u into button %u", action, button);
                break;
            case ACTION_BUTTON_SPELL:
                DETAIL_LOG("MISC: Added Spell %u into button %u", action, button);
                break;
            case ACTION_BUTTON_ITEM:
                DETAIL_LOG("MISC: Added Item %u into button %u", action, button);
                break;
            default:
                sLog.outError("MISC: Unknown action button type %u for action %u into button %u", type, action, button);
                return;
        }
        GetPlayer()->addActionButton(button, action, type);
    }
}

void WorldSession::HandleCompleteCinematic(WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_COMPLETE_CINEMATIC");

    if (Player* player = GetPlayer())
    {
        if (CinematicFlyover* flyover = player->GetCinematicFlyover())
        {
            if (flyover->IsActive())
            {
                flyover->Stop();
            }
        }

        player->ReleaseLoginCinematicRoot();
    }
}

void WorldSession::HandleNextCinematicCamera(WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode CMSG_NEXT_CINEMATIC_CAMERA");

    if (Player* player = GetPlayer())
    {
        if (CinematicFlyover* flyover = player->GetCinematicFlyover())
        {
            flyover->Begin();
        }
    }
}

void WorldSession::HandleFeatherFallAck(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_FEATHER_FALL_ACK");

    recv_data.rpos(recv_data.wpos());
}

void WorldSession::HandleMoveUnRootAck(WorldPacket& recv_data)
{

    recv_data.rpos(recv_data.wpos());

}

void WorldSession::HandleMoveRootAck(WorldPacket& recv_data)
{

    recv_data.rpos(recv_data.wpos());

}

void WorldSession::HandleSetActionBarTogglesOpcode(WorldPacket& recv_data)
{
    uint8 ActionBar;

    recv_data >> ActionBar;

    if (!GetPlayer())
    {
        if (ActionBar != 0)
        {
            sLog.outError("WorldSession::HandleSetActionBarToggles in not logged state with value: %u, ignored", uint32(ActionBar));
        }
        return;
    }

    GetPlayer()->SetActionBars(ActionBar);
}

void WorldSession::HandlePlayedTime(WorldPacket& )
{
    WorldPacket data(SMSG_PLAYED_TIME, 4 + 4);
    data << uint32(_player->Played().Total());
    data << uint32(_player->Played().AtThisLevel());
    SendPacket(&data);
}

void WorldSession::HandleInspectOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    recv_data >> guid;
    DEBUG_LOG("Inspected guid is %s", GuidString(guid).c_str());

    Player* plr = sObjectMgr.GetPlayer(guid);
    if (plr && IsFriendly(*_player, *plr) && InReach(*_player, *plr, TRADE_DISTANCE, false))
    {
        _player->SetSelectionGuid(guid);

        WorldPacket data(SMSG_INSPECT, 8);
        data << static_cast<ObjectGuid>(guid);
        SendPacket(&data);
    }
    else
    {
        DEBUG_LOG("%s not found!", GuidString(guid).c_str());
    }

}

void WorldSession::HandleInspectHonorStatsOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    recv_data >> guid;

    Player* pl = sObjectMgr.GetPlayer(guid);
    if (pl && IsFriendly(*_player, *pl) && InReach(*_player, *pl, TRADE_DISTANCE, false))
    {
        WorldPacket data(MSG_INSPECT_HONOR_STATS, (8 + 1 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 1));
        data << guid;

        data << (uint8)pl->GetHonorBar();

        data << pl->GetUInt32Value(PLAYER_FIELD_SESSION_KILLS);

        data << pl->GetUInt32Value(PLAYER_FIELD_YESTERDAY_KILLS);

        data << pl->GetUInt32Value(PLAYER_FIELD_LAST_WEEK_KILLS);

        data << pl->GetUInt32Value(PLAYER_FIELD_THIS_WEEK_KILLS);

        data << pl->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS);

        data << pl->GetUInt32Value(PLAYER_FIELD_LIFETIME_DISHONORABLE_KILLS);

        data << pl->GetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION);

        data << pl->GetUInt32Value(PLAYER_FIELD_LAST_WEEK_CONTRIBUTION);

        data << pl->GetUInt32Value(PLAYER_FIELD_THIS_WEEK_CONTRIBUTION);

        data << pl->GetUInt32Value(PLAYER_FIELD_LAST_WEEK_RANK);
        data << (uint8)pl->GetHonorHighestRankInfo().visualRank;
        SendPacket(&data);
    }
    else
    {
        DEBUG_LOG("%s not found!", GuidString(guid).c_str());
    }
}

void WorldSession::HandleWorldTeleportOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_WORLD_TELEPORT from %s", GetPlayer()->GetGuidStr().c_str());

    uint32 time;
    uint32 mapid;
    float PositionX;
    float PositionY;
    float PositionZ;
    float Orientation;

    recv_data >> time;
    recv_data >> mapid;
    recv_data >> PositionX;
    recv_data >> PositionY;
    recv_data >> PositionZ;
    recv_data >> Orientation;

    if (GetPlayer()->IsTaxiFlying())
    {
        DEBUG_LOG("Player '%s' (GUID: %u) in flight, ignore worldport command.", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow());
        return;
    }

    DEBUG_LOG("Time %u sec, map=%u, x=%f, y=%f, z=%f, orient=%f", time / 1000, mapid, PositionX, PositionY, PositionZ, Orientation);

    if (GetSecurity() >= SEC_ADMINISTRATOR)
    {
        GetPlayer()->TeleportTo(mapid, PositionX, PositionY, PositionZ, Orientation);
    }
    else
    {
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
    }
}

void WorldSession::HandleMoveSetRawPosition(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_SET_RAW_POSITION from %s", GetPlayer()->GetGuidStr().c_str());

    float PosX, PosY, PosZ, PosO;
    recv_data >> PosX >> PosY >> PosZ >> PosO;

    if (!GetPlayer()->IsInWorld() || GetPlayer()->IsTaxiFlying())
    {
        DEBUG_LOG("Player '%s' (GUID: %u) in a transfer, ignore setrawpos command.", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow());
        return;
    }

    if (GetSecurity() >= SEC_ADMINISTRATOR)
    {
        GetPlayer()->TeleportTo(GetPlayer()->GetMapId(), PosX, PosY, PosZ, PosO);
    }
    else
    {
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
    }
}

void WorldSession::HandleWhoisOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_WHOIS");
    std::string charname;
    recv_data >> charname;

    if (GetSecurity() < SEC_ADMINISTRATOR)
    {
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
        return;
    }

    if (charname.empty() || !normalizePlayerName(charname))
    {
        SendNotification(LANG_NEED_CHARACTER_NAME);
        return;
    }

    Player* plr = sObjectMgr.GetPlayer(charname.c_str());

    if (!plr)
    {
        SendNotification(LANG_PLAYER_NOT_EXIST_OR_OFFLINE, charname.c_str());
        return;
    }

    uint32 accid = plr->GetSession()->GetAccountId();

    QueryResult* result = LoginDatabase.PQuery("SELECT `username`,`email`,`last_ip` FROM `account` WHERE `id`=%u", accid);
    if (!result)
    {
        SendNotification(LANG_ACCOUNT_FOR_PLAYER_NOT_FOUND, charname.c_str());
        return;
    }

    Field* fields = result->Fetch();
    std::string acc = fields[0].GetCppString();
    if (acc.empty())
    {
        acc = "Unknown";
    }
    std::string email = fields[1].GetCppString();
    if (email.empty())
    {
        email = "Unknown";
    }
    std::string lastip = fields[2].GetCppString();
    if (lastip.empty())
    {
        lastip = "Unknown";
    }

    std::string msg = charname + "'s " + "account is " + acc + ", e-mail: " + email + ", last ip: " + lastip;

    WorldPacket data(SMSG_WHOIS, msg.size() + 1);
    data << msg;
    _player->GetSession()->SendPacket(&data);

    delete result;

    DEBUG_LOG("Received whois command from player %s for character %s", GetPlayer()->GetName(), charname.c_str());
}

void WorldSession::HandleFarSightOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_FAR_SIGHT");

    uint8 op;
    recv_data >> op;

    Occupant* obj = _player->GetMap()->GetOccupant(_player->GetFarSightGuid());
    if (!obj)
    {
        return;
    }

    switch (op)
    {
        case 0:
            DEBUG_LOG("Removed FarSight from %s", _player->GetGuidStr().c_str());
            _player->GetCamera().ResetView(false);
            break;
        case 1:
            DEBUG_LOG("Added FarSight %s to %s", GuidString(_player->GetFarSightGuid()).c_str(), _player->GetGuidStr().c_str());
            _player->GetCamera().SetView(obj, false);
            break;
    }
}

void WorldSession::HandleCancelMountAuraOpcode(WorldPacket& )
{
    DEBUG_LOG("WORLD: Received opcode  CMSG_CANCEL_MOUNT_AURA");

    if (!_player->IsMounted())
    {
        ChatHandler(this).SendSysMessage(LANG_CHAR_NON_MOUNTED);
        return;
    }

    if (_player->IsTaxiFlying())
    {
        ChatHandler(this).SendSysMessage(LANG_YOU_IN_FLIGHT);
        return;
    }

    _player->Unmount(_player->HasAuraType(SPELL_AURA_MOUNTED));
    _player->RemoveAurasOfType(SPELL_AURA_MOUNTED);
}

void WorldSession::HandleRequestPetInfoOpcode(WorldPacket& )
{

}

void WorldSession::HandleSetTaxiBenchmarkOpcode(WorldPacket& recv_data)
{
    uint8 mode;
    recv_data >> mode;

    DEBUG_LOG("Client used \"/timetest %d\" command", mode);
}
