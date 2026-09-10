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

#include "CharacterRows.h"
#include <cmath>
#include <string>
#include "Chat.h"
#include "ObjectMgr.h"
#include "World.h"
#include "MapCoords.h"
#include "CellImpl.h"
#include "Transports.h"
#include "TransportMap.h"
#include "Utilities/MathDefines.h"

#ifdef _DEBUG_VMAPS
#endif

enum CreatureLinkType
{
    CREATURE_LINK_RAW = -1,
    CREATURE_LINK_GUID = 0,
    CREATURE_LINK_ENTRY = 1,
};

static char const* const creatureKeys[] =
{
        "Hcreature",
        "Hcreature_entry",
    nullptr
};

enum GameobjectLinkType
{
    GAMEOBJECT_LINK_RAW = -1,
    GAMEOBJECT_LINK_GUID = 0,
    GAMEOBJECT_LINK_ENTRY = 1,
};

static char const* const gameobjectKeys[] =
{
        "Hgameobject",
        "Hgameobject_entry",
    nullptr
};

static char const* const areatriggerKeys[] =
{
        "Hareatrigger",
        "Hareatrigger_target",
    nullptr
};

bool ChatHandler::HandleGoHelper(Player* player, uint32 mapid, float x, float y, float const zPtr, float const ortPtr)
{

    const bool aboard = Transport::IsVesselMapId(mapid);

    float z;
    float ort = aboard ? ortPtr : player->Where().Facing();
    z = zPtr;
    if (!aboard && zPtr > 0.0f)
    {
        z = zPtr;

        if (ortPtr > 0.0f)
        {
            ort = ortPtr;
        }

        if (!MapCoords::Valid(mapid, x, y, z, ort))
        {
            PSendSysMessage(LANG_INVALID_TARGET_COORD, x, y, mapid);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else if (!aboard)
    {

        if (!MapCoords::Valid(mapid, x, y))
        {
            PSendSysMessage(LANG_INVALID_TARGET_COORD, x, y, mapid);
            SetSentErrorMessage(true);
            return false;
        }

    }

    if (player->IsTaxiFlying())
    {
        player->GetMotionMaster()->MovementExpired();
        player->m_taxi.ClearTaxiDestinations();
    }

    else
    {
        player->SaveRecallPosition();
    }

    if (!player->TeleportTo(mapid, x, y, z, ort) && aboard)
    {
        SendSysMessage("That vessel is between two maps right now. You have not been moved; "
                       "try again once she has arrived.");
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandleSummonCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid = 0;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
    {
        return false;
    }

    Player* player = m_session->GetPlayer();
    if (target == player || target_guid == player->GetObjectGuid())
    {
        PSendSysMessage(LANG_CANT_TELEPORT_SELF);
        SetSentErrorMessage(true);
        return false;
    }

    if (target)
    {
        std::string nameLink = playerLink(target_name);

        if (HasLowerSecurity(target))
        {
            return false;
        }

        if (target->IsBeingTeleported())
        {
            PSendSysMessage(LANG_IS_TELEPORTED, nameLink.c_str());
            SetSentErrorMessage(true);
            return false;
        }

        Map* pMap = player->GetMap();

        if (pMap->IsBattleGround())
        {

            if (!target->isGameMaster())
            {
                PSendSysMessage(LANG_CANNOT_GO_TO_BG_GM, nameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }

            else if (target->Battle().Id() && player->Battle().Id() != target->Battle().Id())
            {
                PSendSysMessage(LANG_CANNOT_GO_TO_BG_FROM_BG, nameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }

            target->Battle().In(player->Battle().Id(), player->Battle().Kind());

            if (!target->GetMap()->IsBattleGround())
            {
                target->Battle().RecordTheWayBack();
            }
        }
        else if (pMap->IsDungeon())
        {
            Map* cMap = target->GetMap();
            if (cMap->Instanceable() && cMap->GetInstanceId() != pMap->GetInstanceId())
            {

                PSendSysMessage(LANG_CANNOT_SUMMON_TO_INST, nameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }

            if (!player->GetGroup() || !target->GetGroup() ||
                (target->GetGroup()->GetLeaderGuid() != player->GetObjectGuid()) ||
                (player->GetGroup()->GetLeaderGuid() != player->GetObjectGuid()))

            {
                PSendSysMessage(LANG_CANNOT_SUMMON_TO_INST, nameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }
        }

        PSendSysMessage(LANG_SUMMONING, nameLink.c_str(), "");
        if (needReportToTarget(target))
        {
            ChatHandler(target).PSendSysMessage(LANG_SUMMONED_BY, playerLink(player->GetName()).c_str());
        }

        if (target->IsTaxiFlying())
        {
            target->GetMotionMaster()->MovementExpired();
            target->m_taxi.ClearTaxiDestinations();
        }

        else
        {
            target->SaveRecallPosition();
        }

        float x, y, z;
        ClosePointNear(*player, x, y, z, target->Where().Extent());
        target->TeleportTo(player->GetMapId(), x, y, z, target->Where().Facing());
    }
    else
    {

        if (HasLowerSecurity(nullptr, target_guid))
        {
            return false;
        }

        std::string nameLink = playerLink(target_name);

        PSendSysMessage(LANG_SUMMONING, nameLink.c_str(), GetMangosString(LANG_OFFLINE));

        CharacterRows::SetPlaceOf(target_guid, player->GetMapId(),
            player->Where().X(),
            player->Where().Y(),
            player->Where().Z(),
            player->Where().Facing(),
            player->GetTerrain()->GetZoneId(player->Where().X(), player->Where().Y(), player->Where().Z()));
    }

    return true;
}

bool ChatHandler::HandleAppearCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid = 0;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
    {
        return false;
    }

    Player* _player = m_session->GetPlayer();
    if (target == _player || target_guid == _player->GetObjectGuid())
    {
        SendSysMessage(LANG_CANT_TELEPORT_SELF);
        SetSentErrorMessage(true);
        return false;
    }

    if (target)
    {

        if (HasLowerSecurity(target))
        {
            return false;
        }

        std::string chrNameLink = playerLink(target_name);

        Map* cMap = target->GetMap();
        if (cMap->IsBattleGround())
        {

            if (!_player->isGameMaster())
            {
                PSendSysMessage(LANG_CANNOT_GO_TO_BG_GM, chrNameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }

            else if (_player->Battle().Id() && _player->Battle().Id() != target->Battle().Id())
            {
                PSendSysMessage(LANG_CANNOT_GO_TO_BG_FROM_BG, chrNameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }

            _player->Battle().In(target->Battle().Id(), target->Battle().Kind());

            if (!_player->GetMap()->IsBattleGround())
            {
                _player->Battle().RecordTheWayBack();
            }
        }
        else if (cMap->IsDungeon())
        {

            if (_player->GetGroup())
            {

                if (_player->GetGroup() != target->GetGroup())
                {
                    PSendSysMessage(LANG_CANNOT_GO_TO_INST_PARTY, chrNameLink.c_str());
                    SetSentErrorMessage(true);
                    return false;
                }
            }
            else
            {

                if (!_player->isGameMaster())
                {
                    PSendSysMessage(LANG_CANNOT_GO_TO_INST_GM, chrNameLink.c_str());
                    SetSentErrorMessage(true);
                    return false;
                }
            }

            DungeonHold* pBind = _player->Binds().To(target->GetMapId());
            if (!pBind)
            {
                Group* group = _player->GetGroup();

                DungeonHold* gBind = group ? group->Binds().To(target->GetMapId()) : nullptr;

                if (!gBind)
                {
                    DungeonPersistentState* save = ((DungeonMap*)target->GetMap())->GetPersistanceState();

                    if (group && group->IsLeader(_player->GetObjectGuid()))
                    {
                        group->Binds().BindTo(save, !save->CanReset());
                    }
                    else
                    {
                        _player->Binds().BindTo(save, !save->CanReset());
                    }
                }
            }
        }

        PSendSysMessage(LANG_APPEARING_AT, chrNameLink.c_str());
        if (needReportToTarget(target))
        {
            ChatHandler(target).PSendSysMessage(LANG_APPEARING_TO, GetNameLink().c_str());
        }

        if (_player->IsTaxiFlying())
        {
            _player->GetMotionMaster()->MovementExpired();
            _player->m_taxi.ClearTaxiDestinations();
        }

        else
        {
            _player->SaveRecallPosition();
        }

        float x, y, z;
        ContactPointNear(*target, _player, x, y, z);

        _player->TeleportTo(target->GetMapId(), x, y, z, _player->Where().BearingTo(target->Where()), TELE_TO_GM_MODE);
    }
    else
    {

        if (HasLowerSecurity(nullptr, target_guid))
        {
            return false;
        }

        std::string nameLink = playerLink(target_name);

        PSendSysMessage(LANG_APPEARING_AT, nameLink.c_str());

        float x, y, z, o;
        uint32 map;
        bool in_flight;
        if (!CharacterRows::PlaceOf(target_guid, map, x, y, z, o, in_flight))
        {
            return false;
        }

        return HandleGoHelper(_player, map, x, y, z);
    }

    return true;
}

bool ChatHandler::HandleGroupgoCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
    {
        return false;
    }

    if (HasLowerSecurity(target))
    {
        return false;
    }

    Group* grp = target->GetGroup();

    std::string nameLink = GetNameLink(target);

    if (!grp)
    {
        PSendSysMessage(LANG_NOT_IN_GROUP, nameLink.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    Player* player = m_session->GetPlayer();
    Map* gmMap = player->GetMap();
    bool to_instance = gmMap->Instanceable();

    if (to_instance &&
        (!player->GetGroup() || (grp->GetLeaderGuid() != player->GetObjectGuid()) ||
        (player->GetGroup()->GetLeaderGuid() != player->GetObjectGuid())))

    {
        SendSysMessage(LANG_CANNOT_SUMMON_TO_INST);
        SetSentErrorMessage(true);
        return false;
    }

    for (GroupReference* itr = grp->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pl = itr->getSource();

        if (!pl || pl == m_session->GetPlayer() || !pl->GetSession())
        {
            continue;
        }

        if (HasLowerSecurity(pl))
        {
            return false;
        }

        std::string plNameLink = GetNameLink(pl);

        if (pl->IsBeingTeleported() == true)
        {
            PSendSysMessage(LANG_IS_TELEPORTED, plNameLink.c_str());
            SetSentErrorMessage(true);
            return false;
        }

        if (to_instance)
        {
            Map* plMap = pl->GetMap();

            if (plMap->Instanceable() && plMap->GetInstanceId() != gmMap->GetInstanceId())
            {

                PSendSysMessage(LANG_CANNOT_SUMMON_TO_INST, plNameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }
        }

        PSendSysMessage(LANG_SUMMONING, plNameLink.c_str(), "");
        if (needReportToTarget(pl))
        {
            ChatHandler(pl).PSendSysMessage(LANG_SUMMONED_BY, nameLink.c_str());
        }

        if (pl->IsTaxiFlying())
        {
            pl->GetMotionMaster()->MovementExpired();
            pl->m_taxi.ClearTaxiDestinations();
        }

        else
        {
            pl->SaveRecallPosition();
        }

        float x, y, z;
        ClosePointNear(*m_session->GetPlayer(), x, y, z, pl->Where().Extent());
        pl->TeleportTo(m_session->GetPlayer()->GetMapId(), x, y, z, pl->Where().Facing());
    }

    return true;
}

bool ChatHandler::HandleRecallCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
    {
        return false;
    }

    if (HasLowerSecurity(target))
    {
        return false;
    }

    if (target->IsBeingTeleported())
    {
        PSendSysMessage(LANG_IS_TELEPORTED, GetNameLink(target).c_str());
        SetSentErrorMessage(true);
        return false;
    }

    Geometry::Placement const& back = target->m_recall;
    return HandleGoHelper(target, back.MapId(), back.X(), back.Y(), back.Z(), back.Facing());
}

void ChatHandler::ReportTransportPosition(Occupant* obj)
{
    TransportMap* hull = obj->GetMap() ? obj->GetMap()->AsTransport() : nullptr;
    Transport* vessel = hull ? hull->Vessel() : nullptr;

    if (!vessel)
    {
        return;
    }

    PSendSysMessage("--- TRANSPORT %u (%s), map %u ---", vessel->GetEntry(), vessel->GetName(),
                    hull->GetId());

    PSendSysMessage("Vessel pose: X:%.3f Y:%.3f Z:%.3f O:%.3f  [waypoint estimate -- names "
                    "the grid to search, decides nothing]",
                    vessel->Where().X(), vessel->Where().Y(),
                    vessel->Where().Z(), vessel->Where().Facing());

    PSendSysMessage("Aboard at: X:%.3f Y:%.3f Z:%.3f O:%.3f  [this map's own coordinates]",
                    obj->Where().X(), obj->Where().Y(), obj->Where().Z(), obj->Where().Facing());

    if (!hull->IsCommissioned())
    {
        SendSysMessage("Deck mesh: NONE BAKED for this vessel. She carries nobody.");
        return;
    }

    const auto deckZ = hull->SurfaceAt(obj->Where().X(), obj->Where().Y(), obj->Where().Z(),
                                       3.0f, 10.0f);

    if (!deckZ)
    {
        PSendSysMessage("Deck mesh: NO FLOOR under (%.3f, %.3f). You are over the side.",
                        obj->Where().X(), obj->Where().Y());
        return;
    }

    PSendSysMessage("Deck mesh: Z:%.3f  (you are %+.3f above it), hull radius %.1f",
                    *deckZ, obj->Where().Z() - *deckZ, hull->HullRadius());

    const float PROBE = 1.0f;
    const float lo = obj->Where().Facing();
    const auto aheadZ = hull->SurfaceAt(obj->Where().X() + PROBE * cos(lo),
                                        obj->Where().Y() + PROBE * sin(lo),
                                        *deckZ, 3.0f, 10.0f);

    if (aheadZ)
    {
        const float pitch = atan2(*aheadZ - *deckZ, PROBE);
        PSendSysMessage("Deck slope along facing O:%.3f -> pitch %.3f rad (%.1f deg)",
                        lo, pitch, pitch * 180.0f / M_PI_F);
    }
    else
    {
        PSendSysMessage("Deck slope: edge of the deck %.1f yd ahead along O:%.3f", PROBE, lo);
    }
}

bool ChatHandler::HandleGPSCommand(char* args)
{
    Occupant* obj = nullptr;
    if (*args)
    {
        if (ObjectGuid guid = ExtractGuidFromLink(&args))
        {
            obj = (Occupant*)m_session->GetPlayer()->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
        }

        if (!obj)
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        obj = getSelectedUnit();

        if (!obj)
        {
            SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            SetSentErrorMessage(true);
            return false;
        }
    }
    CellPair cell_val = MaNGOS::ComputeCellPair(obj->Where().X(), obj->Where().Y());
    Cell cell(cell_val);

    uint32 zone_id, area_id;
    obj->GetTerrain()->GetZoneAndAreaId(zone_id, area_id, obj->Where().X(), obj->Where().Y(), obj->Where().Z());

    MapEntry const* mapEntry = sMapStore.LookupEntry(obj->GetMapId());
    AreaTableEntry const* zoneEntry = GetAreaEntryByAreaID(zone_id);
    AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(area_id);

    float zone_x = obj->Where().X();
    float zone_y = obj->Where().Y();

    if (!Map2ZoneCoordinates(zone_x, zone_y, zone_id))
    {
        zone_x = 0;
        zone_y = 0;
    }

    Map const* map = obj->GetMap();
    float ground_z = map->GetHeight(obj->Where().X(), obj->Where().Y(), MAX_HEIGHT);
    float floor_z = map->GetHeight(obj->Where().X(), obj->Where().Y(), obj->Where().Z());

    GridPair p = MaNGOS::ComputeGridPair(obj->Where().X(), obj->Where().Y());

    int gx = 63 - p.x_coord;
    int gy = 63 - p.y_coord;

    uint32 have_map = TerrainInfo::ExistTile(obj->GetMapId(), gx, gy) ? 1 : 0;
    uint32 have_vmap = have_map;

    TerrainInfo const* terrain = obj->GetMap()->GetTerrain();

    if (have_vmap)
    {
        if (terrain->IsOutdoors(obj->Where().X(), obj->Where().Y(), obj->Where().Z()))
        {
            PSendSysMessage("You are OUTdoor");
        }
        else
        {
            PSendSysMessage("You are INdoor");
        }
    }
    else
    {
        PSendSysMessage("no VMAP available for area info");
    }

    PSendSysMessage(LANG_MAP_POSITION,
        obj->GetMapId(), (mapEntry ? mapEntry->MapName_lang[GetSessionDbcLocale()] : "<unknown>"),
        zone_id, (zoneEntry ? zoneEntry->AreaName_lang[GetSessionDbcLocale()] : "<unknown>"),
        area_id, (areaEntry ? areaEntry->AreaName_lang[GetSessionDbcLocale()] : "<unknown>"),
        obj->Where().X(), obj->Where().Y(), obj->Where().Z(), obj->Where().Facing(),
        cell.GridX(), cell.GridY(), cell.CellX(), cell.CellY(), obj->GetInstanceId(),
        zone_x, zone_y, ground_z, floor_z, have_map, have_vmap);

    ReportTransportPosition(obj);

    DEBUG_LOG("Player %s GPS call for %s '%s' (%s: %u):",
        m_session ? GetNameLink().c_str() : GetMangosString(LANG_CONSOLE_COMMAND), (IsPlayer(obj) ? "player" : "creature"), obj->GetName(), (IsPlayer(obj) ? "GUID" : "Entry"), (IsPlayer(obj) ? obj->GetGUIDLow() : obj->GetEntry()));

    DEBUG_LOG(GetMangosString(LANG_MAP_POSITION),
        obj->GetMapId(), (mapEntry ? mapEntry->MapName_lang[sWorld.GetDefaultDbcLocale()] : "<unknown>"),
        zone_id, (zoneEntry ? zoneEntry->AreaName_lang[sWorld.GetDefaultDbcLocale()] : "<unknown>"),
        area_id, (areaEntry ? areaEntry->AreaName_lang[sWorld.GetDefaultDbcLocale()] : "<unknown>"),
        obj->Where().X(), obj->Where().Y(), obj->Where().Z(), obj->Where().Facing(),
        cell.GridX(), cell.GridY(), cell.CellX(), cell.CellY(), obj->GetInstanceId(),
        zone_x, zone_y, ground_z, floor_z, have_map, have_vmap);

    GridMapLiquidData liquid_status;
    GridMapLiquidStatus res = terrain->getLiquidStatus(obj->Where().X(), obj->Where().Y(), obj->Where().Z(), MAP_ALL_LIQUIDS, &liquid_status);
    if (res)
    {
        PSendSysMessage(LANG_LIQUID_STATUS, liquid_status.level, liquid_status.depth_level, liquid_status.type_flags, res);
    }

#ifdef _DEBUG_VMAPS

    const world::terrain::Column column = obj->GetTerrain()->ColumnAt(
        obj->Where().X(), obj->Where().Y(), obj->Where().Z() + 50.0f, obj->Where().Z() - 500.0f);

    if (column.Empty())
    {
        PSendSysMessage("Column: nothing here -- no terrain, no model, no liquid.");
    }
    else
    {
        PSendSysMessage("Column at (%.2f, %.2f), %zu surface(s), top first:",
                        obj->Where().X(), obj->Where().Y(), column.Surfaces().size());
        for (auto it = column.Surfaces().rbegin(); it != column.Surfaces().rend(); ++it)
        {
            const char* kind = "terrain";
            switch (it->kind)
            {
                case world::terrain::SurfaceKind::Static: kind = "model";  break;
                case world::terrain::SurfaceKind::Live:   kind = "live";   break;
                case world::terrain::SurfaceKind::Liquid: kind = "liquid"; break;
                default: break;
            }
            PSendSysMessage("  z=%10.3f  %-7s  %s", it->z, kind,
                            it->kind == world::terrain::SurfaceKind::Liquid
                                ? (it->deep ? "deep" : "shallow") : "");
        }
    }
#endif

    return true;
}

bool ChatHandler::HandleGetDistanceCommand(char* args)
{
    Occupant* obj = nullptr;

    if (*args)
    {
        if (ObjectGuid guid = ExtractGuidFromLink(&args))
        {
            obj = (Occupant*)m_session->GetPlayer()->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
        }

        if (!obj)
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        obj = getSelectedUnit();

        if (!obj)
        {
            SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            SetSentErrorMessage(true);
            return false;
        }
    }

    Player* player = m_session->GetPlayer();

    float dx, dy, dz;
    dx = player->Where().X() - obj->Where().X();
    dy = player->Where().Y() - obj->Where().Y();
    dz = player->Where().Z() - obj->Where().Z();

    PSendSysMessage(LANG_DISTANCE, player->Where().DistanceTo(obj->Where()), player->Where().DistanceTo(obj->Where(), false), sqrt(dx * dx + dy * dy + dz * dz));

    return true;
}

bool ChatHandler::HandleNearGraveCommand(char* args)
{
    Team g_team;

    size_t argslen = strlen(args);

    if (!*args)
    {
        g_team = TEAM_BOTH_ALLOWED;
    }
    else if (strncmp(args, "horde", argslen) == 0)
    {
        g_team = HORDE;
    }
    else if (strncmp(args, "alliance", argslen) == 0)
    {
        g_team = ALLIANCE;
    }
    else
    {
        return false;
    }

    Player* player = m_session->GetPlayer();
    uint32 zone_id = player->GetTerrain()->GetZoneId(player->Where().X(), player->Where().Y(), player->Where().Z());

    WorldSafeLocsEntry const* graveyard = sObjectMgr.GetClosestGraveYard(player->Where().X(), player->Where().Y(), player->Where().Z(), player->GetMapId(), g_team);

    if (graveyard)
    {
        uint32 g_id = graveyard->ID;

        GraveYardData const* data = sObjectMgr.FindGraveYardData(g_id, zone_id);
        if (!data)
        {
            PSendSysMessage(LANG_COMMAND_GRAVEYARDERROR, g_id);
            SetSentErrorMessage(true);
            return false;
        }

        std::string team_name;

        if (data->team == TEAM_BOTH_ALLOWED)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ANY);
        }
        else if (data->team == HORDE)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_HORDE);
        }
        else if (data->team == ALLIANCE)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ALLIANCE);
        }
        else
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_NOTEAM);
        }

        PSendSysMessage(LANG_COMMAND_GRAVEYARDNEAREST, g_id, team_name.c_str(), zone_id);
    }
    else
    {
        std::string team_name;

        if (g_team == TEAM_BOTH_ALLOWED)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ANY);
        }
        else if (g_team == HORDE)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_HORDE);
        }
        else if (g_team == ALLIANCE)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ALLIANCE);
        }

        if (g_team == TEAM_BOTH_ALLOWED)
        {
            PSendSysMessage(LANG_COMMAND_ZONENOGRAVEYARDS, zone_id);
        }
        else
        {
            PSendSysMessage(LANG_COMMAND_ZONENOGRAFACTION, zone_id, team_name.c_str());
        }
    }

    return true;
}

bool ChatHandler::HandleGoTaxinodeCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    uint32 nodeId;
    if (!ExtractUint32KeyFromLink(&args, "Htaxinode", nodeId))
    {
        return false;
    }

    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(nodeId);
    if (!node)
    {
        PSendSysMessage(LANG_COMMAND_GOTAXINODENOTFOUND, nodeId);
        SetSentErrorMessage(true);
        return false;
    }

    if (node->x == 0.0f && node->y == 0.0f && node->z == 0.0f)
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD, node->x, node->y, node->map_id);
        SetSentErrorMessage(true);
        return false;
    }

    return HandleGoHelper(_player, node->map_id, node->x, node->y, node->z);
}

bool ChatHandler::HandleGoCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* _player = m_session->GetPlayer();

    uint32 mapid;
    float x, y, z;

    if (ExtractFloat(&args, x))
    {
        if (!ExtractFloat(&args, y))
        {
            return false;
        }

        if (!ExtractFloat(&args, z))
        {
            return false;
        }

        if (!ExtractOptUInt32(&args, mapid, _player->GetMapId()))
        {
            return false;
        }
    }

    else if (!ExtractLocationFromLink(&args, mapid, x, y, z))
    {
        return false;
    }

    return HandleGoHelper(_player, mapid, x, y, z);
}

bool ChatHandler::HandleGoXYCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    float x;
    if (!ExtractFloat(&args, x))
    {
        return false;
    }

    float y;
    if (!ExtractFloat(&args, y))
    {
        return false;
    }

    uint32 mapid;
    if (!ExtractOptUInt32(&args, mapid, _player->GetMapId()))
    {
        return false;
    }

    float z = 0.0f;
    if (MapCoords::Valid(mapid, x, y))
    {
        if (TerrainInfo const* terrain = sTerrainMgr.LoadTerrain(mapid))
        {
            float ground = terrain->GetWaterOrGroundLevel(x, y, MAX_HEIGHT);
            if (ground > INVALID_HEIGHT)
            {
                z = ground;
            }
        }
    }

    return HandleGoHelper(_player, mapid, x, y, z);
}

bool ChatHandler::HandleGoXYZCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    float x;
    if (!ExtractFloat(&args, x))
    {
        return false;
    }

    float y;
    if (!ExtractFloat(&args, y))
    {
        return false;
    }

    float z;
    if (!ExtractFloat(&args, z))
    {
        return false;
    }

    uint32 mapid;
    if (!ExtractOptUInt32(&args, mapid, _player->GetMapId()))
    {
        return false;
    }

    return HandleGoHelper(_player, mapid, x, y, z);
}

bool ChatHandler::HandleGoZoneXYCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    float x;
    if (!ExtractFloat(&args, x))
    {
        return false;
    }

    float y;
    if (!ExtractFloat(&args, y))
    {
        return false;
    }

    uint32 areaid;
    if (*args)
    {
        if (!ExtractUint32KeyFromLink(&args, "Harea", areaid))
        {
            return false;
        }
    }
    else
    {
        areaid = _player->GetTerrain()->GetZoneId(_player->Where().X(), _player->Where().Y(), _player->Where().Z());
    }

    AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(areaid);

    if (x < 0 || x > 100 || y < 0 || y > 100 || !areaEntry)
    {
        PSendSysMessage(LANG_INVALID_ZONE_COORD, x, y, areaid);
        SetSentErrorMessage(true);
        return false;
    }

    AreaTableEntry const* zoneEntry = areaEntry->ParentAreaID ? GetAreaEntryByAreaID(areaEntry->ParentAreaID) : areaEntry;

    MapEntry const* mapEntry = sMapStore.LookupEntry(zoneEntry->ContinentID);

    if (mapEntry->Instanceable())
    {
        PSendSysMessage(LANG_INVALID_ZONE_MAP, areaEntry->ID, areaEntry->AreaName_lang[GetSessionDbcLocale()],
            mapEntry->MapID, mapEntry->MapName_lang[GetSessionDbcLocale()]);
        SetSentErrorMessage(true);
        return false;
    }

    if (!Zone2MapCoordinates(x, y, zoneEntry->ID))
    {
        PSendSysMessage(LANG_INVALID_ZONE_MAP, areaEntry->ID, areaEntry->AreaName_lang[GetSessionDbcLocale()],
            mapEntry->MapID, mapEntry->MapName_lang[GetSessionDbcLocale()]);
        SetSentErrorMessage(true);
        return false;
    }

    return HandleGoHelper(_player, mapEntry->MapID, x, y);
}

bool ChatHandler::HandleGoGridCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    float grid_x;
    if (!ExtractFloat(&args, grid_x))
    {
        return false;
    }

    float grid_y;
    if (!ExtractFloat(&args, grid_y))
    {
        return false;
    }

    uint32 mapid;
    if (!ExtractOptUInt32(&args, mapid, _player->GetMapId()))
    {
        return false;
    }

    float x = (grid_x - CENTER_GRID_ID + 0.5f) * SIZE_OF_GRIDS;
    float y = (grid_y - CENTER_GRID_ID + 0.5f) * SIZE_OF_GRIDS;

    return HandleGoHelper(_player, mapid, x, y);
}

bool ChatHandler::HandleGoCreatureCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* _player = m_session->GetPlayer();
    ObjectGuid targetMobGuid = 0;

    int crType;
    char* pParam1 = ExtractKeyFromLink(&args, creatureKeys, &crType);
    if (!pParam1)
    {
        return false;
    }

    if (crType == CREATURE_LINK_RAW && strcmp(pParam1, "id") == 0)
    {

        pParam1 = ExtractKeyFromLink(&args, "Hcreature_entry");
        if (!pParam1)
        {
            return false;
        }

        crType = CREATURE_LINK_ENTRY;
    }

    CreatureData const* data = nullptr;

    switch (crType)
    {
        case CREATURE_LINK_ENTRY:
        {
            uint32 tEntry;
            if (!ExtractUInt32(&pParam1, tEntry))
            {
                return false;
            }

            if (!tEntry)
            {
                return false;
            }

            if (!ObjectMgr::GetCreatureTemplate(tEntry))
            {
                SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }

            FindCreatureData worker(tEntry, m_session ? m_session->GetPlayer() : nullptr);

            sObjectMgr.DoCreatureData(worker);

            CreatureDataPair const* dataPair = worker.GetResult();
            if (!dataPair)
            {
                SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }

            data = &dataPair->second;
            targetMobGuid = data->GetObjectGuid(dataPair->first);
            break;
        }
        case CREATURE_LINK_GUID:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&pParam1, lowguid))
            {
                return false;
            }

            data = sObjectMgr.GetCreatureData(lowguid);

            if (!data)
            {
                SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }

            targetMobGuid = data->GetObjectGuid(lowguid);
            break;
        }
        case CREATURE_LINK_RAW:
        {
            uint32 lowguid;
            if (ExtractUInt32(&pParam1, lowguid))
            {
                data = sObjectMgr.GetCreatureData(lowguid);

                if (!data)
                {
                    SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }

                targetMobGuid = data->GetObjectGuid(lowguid);
            }

            else
            {
                std::string name = pParam1;
                WorldDatabase.escape_string(name);
                QueryResult* result = WorldDatabase.PQuery("SELECT `guid` FROM `creature`, `creature_template` WHERE `creature`.`id` = `creature_template`.`entry` AND `creature_template`.`name` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), name.c_str());
                if (!result)
                {
                    SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }

                FindCreatureData worker(0, m_session ? m_session->GetPlayer() : nullptr);

                do
                {
                    Field* fields = result->Fetch();
                    uint32 guid = fields[0].GetUInt32();

                    CreatureDataPair const* cr_data = sObjectMgr.GetCreatureDataPair(guid);
                    if (!cr_data)
                    {
                        continue;
                    }

                    worker(*cr_data);
                }
                while (result->NextRow());

                delete result;

                CreatureDataPair const* dataPair = worker.GetResult();
                if (!dataPair)
                {
                    SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }

                data = &dataPair->second;
                targetMobGuid = data->GetObjectGuid(dataPair->first);
            }
            break;
        }
    }

    Creature* targetMob = _player->GetMap()->GetAnyTypeCreature(targetMobGuid);
    if (targetMob)
    {
        HandleGoHelper(_player, targetMob->GetMapId(), targetMob->Where().X(), targetMob->Where().Y(), targetMob->Where().Z(), _player->Where().Facing());
    }
    else
    {

        HandleGoHelper(_player, data->mapid, data->posX, data->posY, data->posZ);

        PSendSysMessage(LANG_COMMAND_EXECUTE_GOCRE_ANOTHER_TIME, GuidCounter(targetMobGuid));
    }

    return true;
}

bool ChatHandler::HandleGoObjectCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    int goType;
    char* pParam1 = ExtractKeyFromLink(&args, gameobjectKeys, &goType);
    if (!pParam1)
    {
        return false;
    }

    if (goType == GAMEOBJECT_LINK_RAW && strcmp(pParam1, "id") == 0)
    {

        pParam1 = ExtractKeyFromLink(&args, "Hgameobject_entry");
        if (!pParam1)
        {
            return false;
        }

        goType = GAMEOBJECT_LINK_ENTRY;
    }

    GameObjectData const* data = nullptr;

    switch (goType)
    {
        case CREATURE_LINK_ENTRY:
        {
            uint32 tEntry;
            if (!ExtractUInt32(&pParam1, tEntry))
            {
                return false;
            }

            if (!tEntry)
            {
                return false;
            }

            if (!ObjectMgr::GetGameObjectInfo(tEntry))
            {
                SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }

            FindGOData worker(tEntry, m_session ? m_session->GetPlayer() : nullptr);

            sObjectMgr.DoGOData(worker);

            GameObjectDataPair const* dataPair = worker.GetResult();

            if (!dataPair)
            {
                SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }

            data = &dataPair->second;
            break;
        }
        case GAMEOBJECT_LINK_GUID:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&pParam1, lowguid))
            {
                return false;
            }

            data = sObjectMgr.GetGOData(lowguid);
            if (!data)
            {
                SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }
            break;
        }
        case GAMEOBJECT_LINK_RAW:
        {
            uint32 lowguid;
            if (ExtractUInt32(&pParam1, lowguid))
            {

                data = sObjectMgr.GetGOData(lowguid);
                if (!data)
                {
                    SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }
            }
            else
            {
                std::string name = pParam1;
                WorldDatabase.escape_string(name);
                QueryResult* result = WorldDatabase.PQuery("SELECT `guid` FROM `gameobject`, `gameobject_template` WHERE `gameobject`.`id` = `gameobject_template`.`entry` AND `gameobject_template`.`name `" _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), name.c_str());
                if (!result)
                {
                    SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }

                FindGOData worker(0, m_session ? m_session->GetPlayer() : nullptr);

                do
                {
                    Field* fields = result->Fetch();
                    uint32 guid = fields[0].GetUInt32();

                    GameObjectDataPair const* go_data = sObjectMgr.GetGODataPair(guid);
                    if (!go_data)
                    {
                        continue;
                    }

                    worker(*go_data);
                }
                while (result->NextRow());

                delete result;

                GameObjectDataPair const* dataPair = worker.GetResult();
                if (!dataPair)
                {
                    SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }

                data = &dataPair->second;
            }
            break;
        }
    }

    return HandleGoHelper(_player, data->mapid, data->posX, data->posY, data->posZ);
}

bool ChatHandler::HandleGoGraveyardCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    uint32 gyId;
    if (!ExtractUInt32(&args, gyId))
    {
        return false;
    }

    WorldSafeLocsEntry const* gy = sWorldSafeLocsStore.LookupEntry(gyId);
    if (!gy)
    {
        PSendSysMessage(LANG_COMMAND_GRAVEYARDNOEXIST, gyId);
        SetSentErrorMessage(true);
        return false;
    }

    return HandleGoHelper(_player, gy->map_id, gy->x, gy->y, gy->z);
}

bool ChatHandler::HandleGoTriggerCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    if (!*args)
    {
        return false;
    }

    char* atIdStr = ExtractKeyFromLink(&args, areatriggerKeys);
    if (!atIdStr)
    {
        return false;
    }

    uint32 atId;
    if (!ExtractUInt32(&atIdStr, atId))
    {
        return false;
    }

    if (!atId)
    {
        return false;
    }

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(atId);
    if (!atEntry)
    {
        PSendSysMessage(LANG_COMMAND_GOAREATRNOTFOUND, atId);
        SetSentErrorMessage(true);
        return false;
    }

    bool to_target = ExtractLiteralArg(&args, "target");
    if (!to_target && *args)
    {
        return false;
    }

    if (to_target)
    {
        AreaTrigger const* at = sObjectMgr.GetAreaTrigger(atId);
        if (!at)
        {
            PSendSysMessage(LANG_AREATRIGER_NOT_HAS_TARGET, atId);
            SetSentErrorMessage(true);
            return false;
        }

        return HandleGoHelper(_player, at->target_mapId, at->target_X, at->target_Y, at->target_Z);
    }
    else
    {
        return HandleGoHelper(_player, atEntry->mapid, atEntry->x, atEntry->y, atEntry->z);
    }
}

bool ChatHandler::HandleTeleDelCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string name = args;

    if (!sObjectMgr.DeleteGameTele(name))
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    SendSysMessage(LANG_COMMAND_TP_DELETED);
    return true;
}

bool ChatHandler::HandleTeleAddCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* player = m_session->GetPlayer();
    if (!player)
    {
        return false;
    }

    std::string name = args;

    if (sObjectMgr.GetGameTele(name))
    {
        SendSysMessage(LANG_COMMAND_TP_ALREADYEXIST);
        SetSentErrorMessage(true);
        return false;
    }

    TransportMap* hull = player->GetMap() ? player->GetMap()->AsTransport() : nullptr;
    Transport* vessel = hull ? hull->Vessel() : nullptr;

    GameTele tele;
    tele.position_x = player->Where().X();
    tele.position_y = player->Where().Y();
    tele.position_z = player->Where().Z();
    tele.orientation = player->Where().Facing();
    tele.mapId = player->GetMapId();
    tele.name = name;

    if (sObjectMgr.AddGameTele(tele))
    {
        SendSysMessage(LANG_COMMAND_TP_ADDED);

        if (vessel)
        {
            PSendSysMessage("Aboard %s (transport %u, map %u): X:%.3f Y:%.3f Z:%.3f O:%.3f "
                            "[the deck's own coordinates -- .tele %s puts you back on her]",
                            vessel->GetName(), vessel->GetEntry(), tele.mapId,
                            tele.position_x, tele.position_y, tele.position_z,
                            tele.orientation, tele.name.c_str());
        }
    }
    else
    {
        SendSysMessage(LANG_COMMAND_TP_ADDEDERR);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandleTeleNameCommand(char* args)
{
    char* nameStr = ExtractOptNotLastArg(&args);

    Player* target;
    ObjectGuid target_guid = 0;
    std::string target_name;
    char* locationArgs = args;
    if (!ExtractPlayerTarget(&nameStr, &target, &target_guid, &target_name))
    {
        return false;
    }

    if (args && args[0] == '@')
    {
        char* destPlayerName = args + 1;
        if (!destPlayerName || !*destPlayerName)
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }

        Player* destPlayer = sObjectMgr.GetPlayer(destPlayerName);
        if (!destPlayer)
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }

        std::string chrNameLink = playerLink(target_name);
        std::string destNameLink = playerLink(destPlayer->GetName());
        if (target)
        {
            if (HasLowerSecurity(target))
            {
                return false;
            }
            if (target->IsBeingTeleported())
            {
                PSendSysMessage(LANG_IS_TELEPORTED, chrNameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }
            if (target == destPlayer)
            {
                SendSysMessage(LANG_CANT_TELEPORT_SELF);
                SetSentErrorMessage(true);
                return false;
            }

            PSendSysMessage(LANG_TELEPORTING_TO, chrNameLink.c_str(), "", destNameLink.c_str());
            if (needReportToTarget(target))
            {
                ChatHandler(target).PSendSysMessage(LANG_TELEPORTED_TO_BY, GetNameLink().c_str());
            }

            float dx, dy, dz;
            ContactPointNear(*destPlayer, target, dx, dy, dz);
            return HandleGoHelper(target, destPlayer->GetMapId(), dx, dy, dz, target->Where().BearingTo(destPlayer->Where()));
        }
        else
        {

            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }

    uint32 mapId;
    float x, y, z, o = 0.0f;
    char* mapStr = ExtractLiteralArg(&args);
    if (mapStr && ExtractUInt32(&mapStr, mapId))
    {
        char* xStr = ExtractLiteralArg(&args);
        char* yStr = ExtractLiteralArg(&args);
        char* zStr = ExtractLiteralArg(&args);

        if (xStr && yStr && zStr &&
            ExtractFloat(&xStr, x) && ExtractFloat(&yStr, y) && ExtractFloat(&zStr, z))
        {
            char* oStr = ExtractLiteralArg(&args);
            if (oStr)
            {
                ExtractFloat(&oStr, o);
            }

            MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
            if (!mapEntry)
            {
                PSendSysMessage("Map %u does not exist.", mapId);
                SetSentErrorMessage(true);
                return false;
            }

            std::string chrNameLink = playerLink(target_name);

            if (target)
            {

                if (HasLowerSecurity(target))
                {
                    return false;
                }

                if (target->IsBeingTeleported())
                {
                    PSendSysMessage(LANG_IS_TELEPORTED, chrNameLink.c_str());
                    SetSentErrorMessage(true);
                    return false;
                }

                PSendSysMessage("Teleporting %s to map %u (%s) at coordinates %.2f, %.2f, %.2f",
                    chrNameLink.c_str(), mapId, mapEntry->MapName_lang[GetSessionDbcLocale()], x, y, z);

                if (needReportToTarget(target))
                {
                    ChatHandler(target).PSendSysMessage(LANG_TELEPORTED_TO_BY, GetNameLink().c_str());
                }

                return HandleGoHelper(target, mapId, x, y, z, o);
            }
            else
            {

                if (HasLowerSecurity(nullptr, target_guid))
                {
                    return false;
                }

                PSendSysMessage("Teleporting %s %s to map %u at coordinates %.2f, %.2f, %.2f",
                    chrNameLink.c_str(), GetMangosString(LANG_OFFLINE), mapId, x, y, z);

                CharacterRows::SetPlaceOf(target_guid, mapId, x, y, z, o,
                    sTerrainMgr.GetZoneId(mapId, x, y, z));
                return true;
            }
        }
    }
    args = locationArgs;

    GameTele const* tele = ExtractGameTeleFromLink(&args);
    if (!tele)
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    if (target)
    {

        if (HasLowerSecurity(target))
        {
            return false;
        }

        std::string chrNameLink = playerLink(target_name);

        if (target->IsBeingTeleported() == true)
        {
            PSendSysMessage(LANG_IS_TELEPORTED, chrNameLink.c_str());
            SetSentErrorMessage(true);
            return false;
        }

        PSendSysMessage(LANG_TELEPORTING_TO, chrNameLink.c_str(), "", tele->name.c_str());
        if (needReportToTarget(target))
        {
            ChatHandler(target).PSendSysMessage(LANG_TELEPORTED_TO_BY, GetNameLink().c_str());
        }

        return HandleGoHelper(target, tele->mapId, tele->position_x, tele->position_y, tele->position_z, tele->orientation);
    }
    else
    {

        if (HasLowerSecurity(nullptr, target_guid))
        {
            return false;
        }

        std::string nameLink = playerLink(target_name);

        PSendSysMessage(LANG_TELEPORTING_TO, nameLink.c_str(), GetMangosString(LANG_OFFLINE), tele->name.c_str());
        CharacterRows::SetPlaceOf(target_guid, tele->mapId,
            tele->position_x, tele->position_y, tele->position_z, tele->orientation,
            sTerrainMgr.GetZoneId(tele->mapId, tele->position_x, tele->position_y, tele->position_z));
    }

    return true;
}

bool ChatHandler::HandleTeleCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* _player = m_session->GetPlayer();

    GameTele const* tele = ExtractGameTeleFromLink(&args);

    if (!tele)
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    return HandleGoHelper(_player, tele->mapId, tele->position_x, tele->position_y, tele->position_z, tele->orientation);
}

bool ChatHandler::HandleTeleGroupCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (HasLowerSecurity(player))
    {
        return false;
    }

    GameTele const* tele = ExtractGameTeleFromLink(&args);
    if (!tele)
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    std::string nameLink = GetNameLink(player);

    Group* grp = player->GetGroup();
    if (!grp)
    {
        PSendSysMessage(LANG_NOT_IN_GROUP, nameLink.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    for (GroupReference* itr = grp->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* pl = itr->getSource();

        if (!pl || !pl->GetSession())
        {
            continue;
        }

        if (HasLowerSecurity(pl))
        {
            return false;
        }

        std::string plNameLink = GetNameLink(pl);

        if (pl->IsBeingTeleported())
        {
            PSendSysMessage(LANG_IS_TELEPORTED, plNameLink.c_str());
            continue;
        }

        PSendSysMessage(LANG_TELEPORTING_TO, plNameLink.c_str(), "", tele->name.c_str());
        if (needReportToTarget(pl))
        {
            ChatHandler(pl).PSendSysMessage(LANG_TELEPORTED_TO_BY, nameLink.c_str());
        }

        if (pl->IsTaxiFlying())
        {
            pl->GetMotionMaster()->MovementExpired();
            pl->m_taxi.ClearTaxiDestinations();
        }

        else
        {
            pl->SaveRecallPosition();
        }

        pl->TeleportTo(tele->mapId, tele->position_x, tele->position_y, tele->position_z, tele->orientation);
    }

    return true;
}
