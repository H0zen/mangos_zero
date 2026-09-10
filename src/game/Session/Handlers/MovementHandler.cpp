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

#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include <ctime>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "MovementAnswers.h"
#include "OpcodeTable.h"
#include "Log.h"
#include "Player.h"
#include "Fleet.h"
#include "MapCoords.h"
#include "MapFoundry.h"
#include "MapRoster.h"
#include "Transports.h"
#include "TransportMap.h"
#include <cmath>
#include "BattleGround/BattleGround.h"
#include "WaypointMovementGenerator.h"
#include "MapPersistentStateMgr.h"
#include "ObjectMgr.h"

#define MOVEMENT_PACKET_TIME_DELAY 300

void movement::MoveWorldportAck(WorldSession& session, WorldPacket& )
{
    DEBUG_LOG("WORLD: got MSG_MOVE_WORLDPORT_ACK.");
    session.HandleMoveWorldportAckOpcode();
}

void WorldSession::HandleMoveWorldportAckOpcode()
{

    if (!GetPlayer()->IsBeingTeleportedFar())
    {
        return;
    }

    Geometry::Placement const old_loc = GetPlayer()->Where();

    Geometry::Placement& loc = GetPlayer()->GetTeleportDest();

    if (!MapCoords::Valid(loc.MapId(), loc.X(), loc.Y(), loc.Z(), loc.Facing()))
    {
        sLog.outError("WorldSession::HandleMoveWorldportAckOpcode: %s was teleported far to a not valid location "
            "(map:%u, x:%f, y:%f, z:%f) We port him to his homebind instead..",
            GetPlayer()->GetGuidStr().c_str(), loc.MapId(), loc.X(), loc.Y(), loc.Z());

        GetPlayer()->SetSemaphoreTeleportFar(false);

        GetPlayer()->TeleportToHomebind();
        return;
    }

    MapEntry const* mEntry = sMapStore.LookupEntry(loc.MapId());

    Map* map = nullptr;

    if (mEntry->IsBattleGround())
    {
        if (GetPlayer()->Battle().Id())
        {
            map = sMapRoster.Find(loc.MapId(), GetPlayer()->Battle().Id());
        }

        if (!map)
        {
            DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s was teleported far to nonexisten battleground instance "
                " (map:%u, x:%f, y:%f, z:%f) Trying to port him to his previous place..",
                GetPlayer()->GetGuidStr().c_str(), loc.MapId(), loc.X(), loc.Y(), loc.Z());

            GetPlayer()->SetSemaphoreTeleportFar(false);

            if (!GetPlayer()->TeleportTo(old_loc))
            {
                DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s can not be ported to his previous place, teleporting him to his homebind place...",
                    GetPlayer()->GetGuidStr().c_str());
                GetPlayer()->TeleportToHomebind();
            }
            return;
        }
    }

    InstanceTemplate const* mInstance = ObjectMgr::GetInstanceTemplate(loc.MapId());

    if (!GetPlayer()->Binds().StillWelcome() && !mInstance)
    {
        GetPlayer()->Binds().StillWelcome(true);
    }

    GetPlayer()->SetSemaphoreTeleportFar(false);

    if (!map)
    {
        map = sMapFoundry.OpenFor(*GetPlayer(), loc.MapId());
    }

    GetPlayer()->SetMap(map);
    GetPlayer()->Place().MoveTo(loc.X(), loc.Y(), loc.Z(), loc.Facing());

    GetPlayer()->m_movementInfo.ChangePosition(loc.X(), loc.Y(),
                                               loc.Z(), loc.Facing());

    GetPlayer()->m_clientGUIDs.clear();
    GetPlayer()->m_clientPlatforms.clear();

    GetPlayer()->SendInitialPacketsBeforeAddToMap();

    if (!GetPlayer()->BoardingMap()->Add(GetPlayer()))
    {
        DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s was teleported far but couldn't be added to map "
            " (map:%u, x:%f, y:%f, z:%f) Trying to port him to his previous place..",
            GetPlayer()->GetGuidStr().c_str(), loc.MapId(), loc.X(), loc.Y(), loc.Z());

        if (!GetPlayer()->TeleportTo(old_loc))
        {
            DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s can not be ported to his previous place, teleporting him to his homebind place...",
                GetPlayer()->GetGuidStr().c_str());
            GetPlayer()->TeleportToHomebind();
        }
        return;
    }

    if (_player->Battle().InOne())
    {

        if (!mEntry->IsBattleGround())
        {

            _player->Battle().In(0, BATTLEGROUND_TYPE_NONE);

            _player->Battle().Side(TEAM_NONE);
        }

        else if (BattleGround* bg = _player->Battle().Ground())
        {
            if (_player->Queues().CalledToInstance(_player->Battle().Id()))
            {
                bg->AddPlayer(_player);
            }
        }
    }

    GetPlayer()->SendInitialPacketsAfterAddToMap();

    if (GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
    {
        if (!_player->Battle().InOne())
        {

            FlightPathMovementGenerator* flight = (FlightPathMovementGenerator*)(GetPlayer()->GetMotionMaster()->top());
            flight->Reset(*GetPlayer());
            return;
        }

        GetPlayer()->GetMotionMaster()->MovementExpired();
        GetPlayer()->m_taxi.ClearTaxiDestinations();
    }

    if (mEntry->IsRaid() && mInstance)
    {
        if (time_t timeReset = sMapPersistentStateMgr.GetScheduler().GetResetTimeFor(mEntry->MapID))
        {
            uint32 timeleft = uint32(timeReset - time(nullptr));
            GetPlayer()->SendInstanceResetWarning(mEntry->MapID, timeleft);
        }
    }

    if (!mEntry->IsMountAllowed())
    {
        _player->RemoveAurasOfType(SPELL_AURA_MOUNTED);
    }

    if (GetPlayer()->pvpInfo.inHostileArea)
    {
        GetPlayer()->CastSpell(GetPlayer(), 2479, true);
    }

    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    GetPlayer()->ProcessDelayedOperations();
}

void movement::MoveTeleportAck(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("MSG_MOVE_TELEPORT_ACK");

    ObjectGuid guid = 0;

    recv_data >> guid;

    uint32 counter, time;
    recv_data >> counter >> time;
    DEBUG_LOG("Guid: %s", GuidString(guid).c_str());
    DEBUG_LOG("Counter %u, time %u", counter, time / IN_MILLISECONDS);

    Unit* mover = who.GetMover();
    Player* plMover =IsPlayer(mover) ? (Player*)mover : nullptr;

    if (!plMover || !plMover->IsBeingTeleportedNear())
    {
        return;
    }

    if (guid != plMover->GetObjectGuid())
    {
        return;
    }

    plMover->SetSemaphoreTeleportNear(false);

    uint32 old_zone = plMover->GetTerrain()->GetZoneId(plMover->Where().X(), plMover->Where().Y(), plMover->Where().Z());

    Geometry::Placement const& dest = plMover->GetTeleportDest();

    plMover->SetPosition(dest.X(), dest.Y(), dest.Z(), dest.Facing(), true);

    uint32 newzone, newarea;
    plMover->GetTerrain()->GetZoneAndAreaId(newzone, newarea, plMover->Where().X(), plMover->Where().Y(), plMover->Where().Z());
    plMover->UpdateZone(newzone, newarea);

    if (old_zone != newzone)
    {

        if (plMover->pvpInfo.inHostileArea)
        {
            plMover->CastSpell(plMover, 2479, true);
        }
    }

    who.ResummonPetTemporaryUnSummonedIfAny();

    who.ProcessDelayedOperations();
}

void movement::MovementOpcodes(WorldSession& session, WorldPacket& recv_data)
{
    uint16 opcode = recv_data.GetOpcode();
    if (!sLog.HasLogFilter(LOG_FILTER_PLAYER_MOVES))
    {
        DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(opcode), opcode, opcode);
        recv_data.hexlike();
    }

    Unit* mover = session.GetPlayer()->GetMover();
    Player* plMover =IsPlayer(mover) ? (Player*)mover : nullptr;

    if (plMover && plMover->IsBeingTeleported())
    {
        recv_data.rpos(recv_data.wpos());
        return;
    }

    MovementInfo movementInfo;
    movementInfo.Read(recv_data);

    if (!Verify(*session.GetPlayer(), movementInfo))
    {
        return;
    }

    if (opcode == MSG_MOVE_FALL_LAND && plMover && !plMover->IsTaxiFlying())
    {
        plMover->HandleFall(movementInfo);
    }

    Relocate(*session.GetPlayer(), movementInfo);

    if (plMover)
    {
        plMover->UpdateFallInformationIfNeed(movementInfo, opcode);
    }

    WorldPacket data(opcode, uint16(recv_data.size() + 2));
    data << mover->GetPackGUID();
    movementInfo.Write(data);
    Deliver(Audience::Around(*mover).Except(session.GetPlayer()), &data);
}

void movement::ForceSpeedChangeAckOpcodes(Player& who, WorldPacket& recv_data)
{
    uint16 opcode = recv_data.GetOpcode();
    DEBUG_LOG("WORLD: Received %s (%u, 0x%X) opcode", LookupOpcodeName(recv_data.GetOpcode()), opcode, opcode);

    ObjectGuid guid = 0;
    MovementInfo movementInfo;
    float  newspeed;

    recv_data >> guid;
    recv_data >> Unused<uint32>();
    recv_data >> movementInfo;
    recv_data >> newspeed;

    if (who.GetObjectGuid() != guid)
    {
        return;
    }

    UnitMoveType move_type;
    UnitMoveType force_move_type;

    static char const* move_type_name[MAX_MOVE_TYPE] = {  "Walk", "Run", "RunBack", "Swim", "SwimBack", "TurnRate" };

    switch (opcode)
    {
        case CMSG_FORCE_WALK_SPEED_CHANGE_ACK:          move_type = MOVE_WALK;          force_move_type = MOVE_WALK;        break;
        case CMSG_FORCE_RUN_SPEED_CHANGE_ACK:           move_type = MOVE_RUN;           force_move_type = MOVE_RUN;         break;
        case CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK:      move_type = MOVE_RUN_BACK;      force_move_type = MOVE_RUN_BACK;    break;
        case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:          move_type = MOVE_SWIM;          force_move_type = MOVE_SWIM;        break;
        case CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:     move_type = MOVE_SWIM_BACK;     force_move_type = MOVE_SWIM_BACK;   break;
        case CMSG_FORCE_TURN_RATE_CHANGE_ACK:           move_type = MOVE_TURN_RATE;     force_move_type = MOVE_TURN_RATE;   break;
        default:
            sLog.outError("WorldSession::HandleForceSpeedChangeAck: Unknown move type opcode: %u", opcode);
            return;
    }

    if (who.m_forced_speed_changes[force_move_type] > 0)
    {
        --who.m_forced_speed_changes[force_move_type];
        if (who.m_forced_speed_changes[force_move_type] > 0)
        {
            return;
        }
    }

    if (!who.GetTransport() && fabs(who.Pacing().At(move_type) - newspeed) > 0.01f)
    {
        if (who.Pacing().At(move_type) > newspeed)
        {
            sLog.outError("%sSpeedChange player %s is NOT correct (must be %f instead %f), force set to correct value",
                move_type_name[move_type], who.GetName(), who.Pacing().At(move_type), newspeed);
            who.Pacing().SetRate(move_type, who.Pacing().RateOf(move_type), true);
        }
        else
        {
            BASIC_LOG("Player %s from account id %u kicked for incorrect speed (must be %f instead %f)",
                who.GetName(), who.GetSession()->GetAccountId(), who.Pacing().At(move_type), newspeed);
            who.GetSession()->KickPlayer();
        }
    }
}

void movement::SetActiveMover(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_ACTIVE_MOVER");

    ObjectGuid guid = 0;
    recv_data >> guid;

    if (who.GetMover()->GetObjectGuid() != guid)
    {
        sLog.outError("HandleSetActiveMoverOpcode: incorrect mover guid: mover is %s and should be %s",
            who.GetMover()->GetGuidStr().c_str(), GuidString(guid).c_str());
        return;
    }
}

void movement::MoveNotActiveMover(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_NOT_ACTIVE_MOVER");
    recv_data.hexlike();

    ObjectGuid old_mover_guid = 0;
    MovementInfo mi;

    recv_data >> old_mover_guid;
    recv_data >> mi;

    if (who.GetMover()->GetObjectGuid() == old_mover_guid)
    {
        if (who.GetObjectGuid() != old_mover_guid )
        {
            sLog.outError("HandleMoveNotActiveMover: incorrect mover guid: mover is %s and should be %s instead of %s",
                who.GetMover()->GetGuidStr().c_str(),
                who.GetGuidStr().c_str(),
                GuidString(old_mover_guid).c_str());
        }
        recv_data.rpos(recv_data.wpos());
        return;
    }

    who.m_movementInfo = mi;
}

void movement::MountSpecialAnim(Player& who, WorldPacket& )
{

    WorldPacket data(SMSG_MOUNTSPECIAL_ANIM, 8);
    data << who.GetObjectGuid();

    Deliver(Audience::Around(who), &data);
}

void movement::MoveKnockBackAck(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_MOVE_KNOCK_BACK_ACK");

    Unit* mover = session.GetPlayer()->GetMover();
    Player* plMover =IsPlayer(mover) ? (Player*)mover : nullptr;

    if (plMover && plMover->IsBeingTeleported())
    {
        recv_data.rpos(recv_data.wpos());
        return;
    }

    ObjectGuid guid = 0;
    MovementInfo movementInfo;

    recv_data >> guid;
    recv_data >> Unused<uint32>();
    recv_data >> movementInfo;

    if (!Verify(*session.GetPlayer(), movementInfo, guid))
    {
        return;
    }

    Relocate(*session.GetPlayer(), movementInfo);

    WorldPacket data(MSG_MOVE_KNOCK_BACK, recv_data.size() + 15);
    data << mover->GetObjectGuid();
    data << movementInfo;
    data << movementInfo.GetJumpInfo().sinAngle;
    data << movementInfo.GetJumpInfo().cosAngle;
    data << movementInfo.GetJumpInfo().xyspeed;
    data << movementInfo.GetJumpInfo().velocity;
    Deliver(Audience::Around(*mover).Except(session.GetPlayer()), &data);
}

void WorldSession::SendKnockBack(float angle, float horizontalSpeed, float verticalSpeed)
{
    float vsin = sin(angle);
    float vcos = cos(angle);

    WorldPacket data(SMSG_MOVE_KNOCK_BACK, 9 + 4 + 4 + 4 + 4 + 4);
    data << GetPlayer()->GetPackGUID();
    data << uint32(0);
    data << float(vcos);
    data << float(vsin);
    data << float(horizontalSpeed);
    data << float(-verticalSpeed);
    SendPacket(&data);
}

void movement::MoveHoverAck(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_MOVE_HOVER_ACK");

    MovementInfo movementInfo;

    recv_data >> Unused<uint64>();
    recv_data >> Unused<uint32>();
    recv_data >> movementInfo;
    recv_data >> Unused<uint32>();
}

void movement::MoveWaterWalkAck(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_MOVE_WATER_WALK_ACK");

    MovementInfo movementInfo;

    recv_data.read_skip<uint64>();
    recv_data.read_skip<uint32>();
    recv_data >> movementInfo;
    recv_data >> Unused<uint32>();
}

void movement::SummonResponse(Player& who, WorldPacket& recv_data)
{
    if (!who.IsAlive() || who.IsInCombat())
    {
        return;
    }

    ObjectGuid summonerGuid = 0;
    recv_data >> summonerGuid;

    who.SummonIfPossible(true);
}

void movement::MoveTimeSkipped(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    uint32 time_skipped;
    recv_data >> guid;
    recv_data >> time_skipped;
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_TIME_SKIPPED for %s, time_skipped: %u", GuidString(guid).c_str(), time_skipped);
}

bool movement::Verify(Player& who, MovementInfo const& movementInfo, ObjectGuid guid)
{

    if (guid != who.GetMover()->GetObjectGuid())
    {
        return false;
    }

    return Verify(who, movementInfo);
}

bool movement::Verify(Player& , MovementInfo const& movementInfo)
{
    if (!MaNGOS::IsValidMapCoord(movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z, movementInfo.GetPos()->o))
    {
        return false;
    }

    if (movementInfo.HasMovementFlag(MOVEFLAG_ONTRANSPORT))
    {

        if (movementInfo.GetTransportPos()->x > 50 || movementInfo.GetTransportPos()->y > 50 || movementInfo.GetTransportPos()->z > 100)
        {
            return false;
        }

        if (!MaNGOS::IsValidMapCoord(movementInfo.GetPos()->x + movementInfo.GetTransportPos()->x, movementInfo.GetPos()->y + movementInfo.GetTransportPos()->y,
            movementInfo.GetPos()->z + movementInfo.GetTransportPos()->z, movementInfo.GetPos()->o + movementInfo.GetTransportPos()->o))
        {
            return false;
        }
    }

    return true;
}

void movement::Relocate(Player& who, MovementInfo& movementInfo)
{

    movementInfo.UpdateTime(movementInfo.GetTime() + who.GetSession()->GetLatency());

    Unit* mover = who.GetMover();

    if (Player* plMover =IsPlayer(mover) ? (Player*)mover : nullptr)
    {

        plMover->m_movementInfo = movementInfo;

        if (movementInfo.HasMovementFlag(MOVEFLAG_ONTRANSPORT))
        {
            if (!plMover->GetTransport())
            {

                if (Transport* named = sFleet.ByGuid(movementInfo.GetTransportGuid()))
                {
                    plMover->SetTransport(named);

                    if (TransportMap* hull = named->AsMap())
                    {
                        hull->Embark(plMover);
                    }
                }
            }
        }
        else if (plMover->GetTransport())
        {

            if (TransportMap* hull = plMover->GetTransport()->AsMap())
            {
                Transport* vessel = plMover->GetTransport();

                const float reach = hull->HullRadius() + DECK_EDGE_MARGIN;

                const bool ashore = vessel->Where().WithinDist(
                    Geometry::Vector3(movementInfo.GetPos()->x, movementInfo.GetPos()->y,
                                      movementInfo.GetPos()->z), reach);

                if (ashore)
                {
                    hull->Disembark(plMover, movementInfo.GetPos()->x,
                                    movementInfo.GetPos()->y, movementInfo.GetPos()->z,
                                    movementInfo.GetPos()->o);
                }
                else
                {

                    hull->Disembark(plMover, vessel->Where().X(), vessel->Where().Y(),
                                    vessel->Where().Z(), vessel->Where().Facing());
                }
            }
            plMover->SetTransport(nullptr);
            movementInfo.ClearTransportData();
        }

        if (movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING) != plMover->IsInWater())
        {

            plMover->Dangers().InWater(!plMover->IsInWater() || plMover->GetMap()->GetTerrain()->IsUnderWater(movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z));
        }

        if (plMover->GetTransport() && plMover->GetMap()->AsTransport())
        {
            const Position* offset = movementInfo.GetTransportPos();
            plMover->SetPosition(offset->x, offset->y, offset->z, offset->o);
        }
        else
        {
            plMover->SetPosition(movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z, movementInfo.GetPos()->o);
        }
        plMover->m_movementInfo = movementInfo;

        plMover->UpdateLiftMinions();

        if (ObjectGuid lootGUID = plMover->GetLootGuid())
        {
            plMover->SendLootRelease(lootGUID);
        }

        if (movementInfo.GetPos()->z < -500.0f)
        {
            if (plMover->Battle().Ground() &&
                plMover->Battle().Ground()->HandlePlayerUnderMap(&who))
            {

            }
            else
            {

                if (plMover->IsAlive())
                {
                    plMover->Dangers().Harm(DAMAGE_FALL_TO_VOID, plMover->GetMaxHealth());

                    if (!plMover->IsAlive())
                    {

                        plMover->KillPlayer();
                        plMover->BuildPlayerRepop();
                    }
                }

                plMover->RepopAtGraveyard();

                plMover->ResurrectPlayer(0.5f);
                plMover->SpawnCorpseBones();
            }
        }
    }
    else
    {
        if (mover->IsInWorld())
        {
            mover->GetMap()->CreatureRelocation((Creature*)mover, movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z, movementInfo.GetPos()->o);
        }
    }
}
