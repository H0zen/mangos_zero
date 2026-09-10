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
#include <vector>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "TaxiAnswers.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Path.h"
#include "WaypointMovementGenerator.h"

void taxi::TaxiNodeStatusQuery(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_TAXINODE_STATUS_QUERY");

    ObjectGuid guid = 0;

    recv_data >> guid;
    who.GetSession()->SendTaxiStatus(guid);
}

void WorldSession::SendTaxiStatus(ObjectGuid guid)
{

    Creature* unit = _player->GetMap()->GetCreature(guid);
    if (!unit)
    {
        DEBUG_LOG("WorldSession::SendTaxiStatus - %s not found or you can't interact with it.", GuidString(guid).c_str());
        return;
    }

    uint32 curloc = sObjectMgr.GetNearestTaxiNode(unit->Where().X(), unit->Where().Y(), unit->Where().Z(), unit->GetMapId(), _player->GetTeam());

    if (curloc == 0)
    {
        return;
    }

    DEBUG_LOG("WORLD: current location %u ", curloc);

    WorldPacket data(SMSG_TAXINODE_STATUS, 9);
    data << static_cast<ObjectGuid>(guid);
    data << uint8(_player->m_taxi.IsTaximaskNodeKnown(curloc) ? 1 : 0);
    SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_TAXINODE_STATUS");
}

void taxi::TaxiQueryAvailableNodes(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_TAXIQUERYAVAILABLENODES");

    ObjectGuid guid = 0;
    recv_data >> guid;

    Creature* unit = who.GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTaxiQueryAvailableNodes - %s not found or you can't interact with him.", GuidString(guid).c_str());
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    if (who.GetSession()->SendLearnNewTaxiNode(unit))
    {
        return;
    }

    who.GetSession()->SendTaxiMenu(unit);
}

void WorldSession::SendTaxiMenu(Creature* unit)
{

    uint32 curloc = sObjectMgr.GetNearestTaxiNode(unit->Where().X(), unit->Where().Y(), unit->Where().Z(), unit->GetMapId(), _player->GetTeam());

    if (curloc == 0)
    {
        return;
    }

    DEBUG_LOG("WORLD: CMSG_TAXINODE_STATUS_QUERY %u ", curloc);

    WorldPacket data(SMSG_SHOWTAXINODES, (4 + 8 + 4 + 8 * 4));
    data << uint32(1);
    data << unit->GetObjectGuid();
    data << uint32(curloc);
    _player->m_taxi.AppendTaximaskTo(data, _player->IsTaxiCheater());
    SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_SHOWTAXINODES");
}

void WorldSession::SendDoFlight(uint32 mountDisplayId, uint32 path, uint32 pathNode)
{

    if (_player->hasUnitState(UNIT_STAT_DIED))
    {
        _player->RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    while (_player->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
    {
        _player->GetMotionMaster()->MovementExpired(false);
    }

    if (mountDisplayId)
    {
        _player->Mount(mountDisplayId);
    }

    _player->GetMotionMaster()->MoveTaxiFlight(path, pathNode);
}

bool WorldSession::SendLearnNewTaxiNode(Creature* unit)
{

    uint32 curloc = sObjectMgr.GetNearestTaxiNode(unit->Where().X(), unit->Where().Y(), unit->Where().Z(), unit->GetMapId(), _player->GetTeam());

    if (curloc == 0)
    {
        return true;
    }

    if (_player->m_taxi.SetTaximaskNode(curloc))
    {
        WorldPacket msg(SMSG_NEW_TAXI_PATH, 0);
        SendPacket(&msg);

        WorldPacket update(SMSG_TAXINODE_STATUS, 9);
        update << static_cast<ObjectGuid>(unit->GetObjectGuid());
        update << uint8(1);
        SendPacket(&update);

        return true;
    }
    else
    {
        return false;
    }
}

void WorldSession::SendActivateTaxiReply(ActivateTaxiReply reply)
{
    WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
    data << uint32(reply);
    SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_ACTIVATETAXIREPLY");
}

void taxi::ActivateTaxiExpress(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXIEXPRESS");

    ObjectGuid guid = 0;
    uint32 node_count, _totalcost;

    recv_data >> guid >> _totalcost >> node_count;

    Creature* npc = who.GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!npc)
    {
        DEBUG_LOG("WORLD: HandleActivateTaxiExpressOpcode - %s not found or you can't interact with it.", GuidString(guid).c_str());
        return;
    }
    std::vector<uint32> nodes;

    for (uint32 i = 0; i < node_count; ++i)
    {
        uint32 node;
        recv_data >> node;

        if (!who.m_taxi.IsTaximaskNodeKnown(node) && !who.IsTaxiCheater())
        {
            who.GetSession()->SendActivateTaxiReply(ERR_TAXINOTVISITED);
            recv_data.rpos(recv_data.wpos());
            return;
        }
        nodes.push_back(node);
    }

    if (nodes.empty())
    {
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXIEXPRESS from %d to %d" , nodes.front(), nodes.back());

    who.ActivateTaxiPathTo(nodes, npc);
}

void taxi::MoveSplineDone(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_SPLINE_DONE");

    MovementInfo movementInfo;

    recv_data >> movementInfo;
    recv_data >> Unused<uint32>();
    recv_data >> Unused<uint32>();

    uint32 curDest = who.m_taxi.GetTaxiDestination();
    if (!curDest)
    {
        return;
    }

    TaxiNodesEntry const* curDestNode = sTaxiNodesStore.LookupEntry(curDest);

    if (curDestNode && curDestNode->map_id != who.GetMapId())
    {
        if (who.GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
        {

            FlightPathMovementGenerator* flight = (FlightPathMovementGenerator*)(who.GetMotionMaster()->top());

            flight->Interrupt(who);

            flight->SetCurrentNodeAfterTeleport();
            TaxiPathNodeEntry const& node = flight->GetPath()[flight->GetCurrentNode()];
            flight->SkipCurrentNode();

            who.TeleportTo(curDestNode->map_id, node.LocX, node.LocY, node.LocZ, who.Where().Facing());
        }
        return;
    }

    uint32 destinationnode = who.m_taxi.NextTaxiDestination();
    if (destinationnode > 0)
    {

        uint32 sourcenode = who.m_taxi.GetTaxiSource();

        if (who.IsTaxiCheater())
        {
            if (who.m_taxi.SetTaximaskNode(sourcenode))
            {
                WorldPacket data(SMSG_NEW_TAXI_PATH, 0);
                who.GetSession()->SendPacket(&data);
            }
        }

        DEBUG_LOG("WORLD: Taxi has to go from %u to %u", sourcenode, destinationnode);

        uint32 mountDisplayId = sObjectMgr.GetTaxiMountDisplayId(sourcenode, who.GetTeam());

        uint32 path, cost;
        sObjectMgr.GetTaxiPath(sourcenode, destinationnode, path, cost);

        if (path && mountDisplayId)
        {
            who.GetSession()->SendDoFlight(mountDisplayId, path, 1);
        }
        else
        {
            who.m_taxi.ClearTaxiDestinations();
        }
    }
    else
    {
        who.m_taxi.ClearTaxiDestinations();
    }
}

void taxi::ActivateTaxi(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXI");

    ObjectGuid guid = 0;
    std::vector<uint32> nodes;
    nodes.resize(2);

    recv_data >> guid >> nodes[0] >> nodes[1];
    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXI from %d to %d" , nodes[0], nodes[1]);
    Creature* npc = who.GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!npc)
    {
        DEBUG_LOG("WORLD: HandleActivateTaxiOpcode - %s not found or you can't interact with it.", GuidString(guid).c_str());
        return;
    }
    if (!who.IsTaxiCheater())
    {
        if (!who.m_taxi.IsTaximaskNodeKnown(nodes[0]) || !who.m_taxi.IsTaximaskNodeKnown(nodes[1]))
        {
            who.GetSession()->SendActivateTaxiReply(ERR_TAXINOTVISITED);
            return;
        }
    }
    who.ActivateTaxiPathTo(nodes, npc);
}
