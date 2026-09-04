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

/**
 * @file Object.cpp
 * @brief Base implementation for all game objects
 *
 * This file implements the Object class, which is the base class for all
 * entities in the game world. It provides:
 * - Update field management (synchronized with clients)
 * - Object GUID handling
 * - Update data building for network transmission
 * - Object visibility and spawning
 * - Type identification
 *
 * The Object class uses an array of uint32 values (update fields) that
 * mirror the client's object state. Changes to these values are sent to
 * players who can see the object.
 */



#include "Utilities/Errors.h"
#include <string>
#include <vector>
#include <cstdlib>
#include "Object.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "Creature.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "UpdateData.h"
#include "FieldTable.h"
#include "Util.h"
#include "MapManager.h"
#include "Transports.h"
#include "TransportMap.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "movement/packet_builder.h"
#include "movement/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"

/**
 * @brief Force immediate update transmission to all viewers
 *
 * Sends all pending update changes immediately rather than waiting
 * for the next update tick. This is used for urgent updates that
 * must be visible immediately (e.g., combat state changes).
 *
 * The method builds update data for all nearby players and sends
 * it immediately, then removes the object from the pending update list.
 */
void Object::SendForcedObjectUpdate()
{
    if (!m_inWorld || !m_objectUpdated)
    {
        return;
    }

    UpdateDataMapType update_players;

    BuildUpdateData(update_players);
    RemoveFromClientUpdateList();

    WorldPacket packet;                                     // here we allocate a std::vector with a size of 0x10000
    for (UpdateDataMapType::iterator iter = update_players.begin(); iter != update_players.end(); ++iter)
    {
        iter->second.BuildPacket(&packet);
        iter->first->GetSession()->SendPacket(&packet);
        packet.clear();                                     // clean the string
    }
}

/**
 * @brief Build create update block for player
 * @param data Update data buffer
 * @param target Target player
 *
 * Builds the update packet data needed to create this object
 * for the specified player. Includes movement data and
 * all update field values.
 */
void Object::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (!target)
    {
        return;
    }

    uint8  updatetype   = UPDATETYPE_CREATE_OBJECT;
    uint8 updateFlags  = m_updateFlag;

    /** lower flag1 **/
    if (target == this)                                     // building packet for yourself
    {
        updateFlags |= UPDATEFLAG_SELF;
    }

    if (m_isNewObject)
    {
        switch (GetObjectGuid().GetHigh())
        {
            case HighGuid::HIGHGUID_DYNAMICOBJECT:
            case HighGuid::HIGHGUID_CORPSE:
            case HighGuid::HIGHGUID_PLAYER:
            case HighGuid::HIGHGUID_UNIT:
            case HighGuid::HIGHGUID_GAMEOBJECT:
                updatetype = UPDATETYPE_CREATE_OBJECT2;
                break;
            default:
                break;
        }
    }

    // DEBUG_LOG("BuildCreateUpdate: update-type: %u, object-type: %u got updateFlags: %X", updatetype, m_objectTypeId, updateFlags);

    // A passenger block names its hull by guid and carries (0,0,0) for a world
    // position, so the client can only place it once it knows that hull. Marking
    // here means every packet carrying a hull says so, whichever path built it.
    if (GetObjectGuid().IsMOTransport())
    {
        data->MarkTransport();
    }

    ByteBuffer& buf = data->GetBuffer();
    buf << uint8(updatetype);
    buf << GetPackGUID();
    buf << uint8(m_objectTypeId);

    BuildMovementUpdate(&buf, updateFlags);

    BuildValuesUpdate(updatetype, &buf, target);
    data->AddUpdateBlock();
}

/**
 * @brief Send create update to player
 * @param player Target player
 *
 * Sends the create update packet to the specified player,
 * causing the object to appear in their game world.
 */
void Object::SendCreateUpdateToPlayer(Player* player)
{
    // send create update to player
    UpdateData upd;
    WorldPacket packet;

    BuildCreateUpdateBlockForPlayer(&upd, player);
    upd.BuildPacket(&packet);
    player->GetSession()->SendPacket(&packet);
}

/**
 * @brief Build values update block for player
 * @param data Update data buffer
 * @param target Target player
 *
 * Builds the update packet data for changed field values
 * to send to the specified player.
 */
void Object::BuildValuesUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    ByteBuffer& buf = data->GetBuffer();

    buf << uint8(UPDATETYPE_VALUES);
    buf << GetPackGUID();

    BuildValuesUpdate(UPDATETYPE_VALUES, &buf, target);

    data->AddUpdateBlock();
}

/**
 * @brief Build out of range update block
 * @param data Update data buffer
 *
 * Adds this object's GUID to the out-of-range list,
 * indicating it should be removed from the client's view.
 */
void Object::BuildOutOfRangeUpdateBlock(UpdateData* data) const
{
    data->AddOutOfRangeGUID(GetObjectGuid());
}

/**
 * @brief Destroy object for player
 * @param target Target player
 *
 * Sends a destroy packet to the specified player,
 * removing this object from their game world.
 */
void Object::DestroyForPlayer(Player* target) const
{
    MANGOS_ASSERT(target);

    WorldPacket data(SMSG_DESTROY_OBJECT, 8);
    data << GetObjectGuid();
    target->GetSession()->SendPacket(&data);
}

/**
 * @brief Build movement update block
 * @param data Byte buffer to write to
 * @param updateFlags Update flags
 *
 * Builds the movement data portion of the update packet.
 * Includes position, orientation, movement flags, and speeds
 * for living objects, or just position for static objects.
 */
void Object::BuildMovementUpdate(ByteBuffer* data, uint8 updateFlags) const
{
    Unit const* unit = NULL;
    uint32 highGuid = 0;
    MovementFlags moveflags = MOVEFLAG_NONE;

    switch (m_objectTypeId)
    {
        case TYPEID_OBJECT:
        case TYPEID_ITEM:
        case TYPEID_CONTAINER:
        case TYPEID_GAMEOBJECT:
        case TYPEID_DYNAMICOBJECT:
        case TYPEID_CORPSE:
            highGuid = uint32(GetObjectGuid().GetHigh());
            break;

        case TYPEID_PLAYER:
            // TODO: this code must not be here
            if (static_cast<Player const*>(this)->GetTransport())
            {
                ((Unit*)this)->m_movementInfo.AddMovementFlag(MOVEFLAG_ONTRANSPORT);
            }
            else
            {
                ((Unit*)this)->m_movementInfo.RemoveMovementFlag(MOVEFLAG_ONTRANSPORT);
            }

        // fall through to TYPEID_UNIT -- unit must be set for UPDATEFLAG_LIVING
        case TYPEID_UNIT:
            unit = static_cast<Unit const*>(this);

            // A CREATURE has no client to speak for it, so it is derived here, at the
            // instant of writing, from the one thing that cannot fall out of step: the map
            // it is on. Its position on a deck map already IS the offset -- nothing is
            // composed.
            if (Map* on = unit->GetMap())
            {
                if (TransportMap* hull = on->AsTransport())
                {
                    if (Transport* vessel = hull->Vessel())
                    {
                        MovementInfo& aboard = const_cast<Unit*>(unit)->m_movementInfo;
                        aboard.AddMovementFlag(MOVEFLAG_ONTRANSPORT);
                        aboard.SetTransportData(vessel->GetObjectGuid(), unit->Where().X(),
                                                unit->Where().Y(), unit->Where().Z(),
                                                unit->Where().Facing(), 0);
                    }
                }
            }

            moveflags = unit->m_movementInfo.GetMovementFlags();
            break;

        default:
            break;
    }

    *data << uint8(updateFlags);

    if (updateFlags & UPDATEFLAG_LIVING)
    {
        MANGOS_ASSERT(unit);
        // Ask the spline, not the unit states.
        //
        // This used to test IsStopped(), which is !hasUnitState(UNIT_STAT_MOVING) — a
        // question about whether some generator has declared itself moving, not about
        // whether a spline is running. HomeMovementGenerator declared nothing, so every
        // healthy evade-return answered "stopped" and tripped this: 151 times in a single
        // evening, against creatures whose splines were perfectly live.
        //
        // Worse, it then STRIPPED the flags. A packet builder was reaching into unit
        // movement state and turning off a spline that was still running, so an observer
        // entering visibility mid-return was handed a creature with no movement to draw
        // while everyone already watching had been told it was moving.
        //
        // The real defect this was meant to catch is a flag that outlived its spline, so
        // that is what it now asks. It reports and does not touch anything; with the
        // reconciliation in Unit::UpdateSplineMovement it should never fire at all, which
        // is the point of leaving it here.
        if (unit->movespline->Finalized() && unit->m_movementInfo.HasMovementFlag(MOVEFLAG_SPLINE_ENABLED))
        {
            sLog.outError("%s has spline movement enabled but its spline is finalized!", GetGuidStr().c_str());
            std::string victimGuid = "none";
            if (Unit const* victim = unit->getVictim())
            {
                victimGuid = victim->GetGuidStr();
            }

            ObjectGuid const& targetGuid = unit->GetTargetGuid();
            std::string targetGuidString = targetGuid.IsEmpty() ? "none" : targetGuid.GetString();
            GridPair gridPair = MaNGOS::ComputeGridPair(unit->Where().X(), unit->Where().Y());
            CellPair cellPair = MaNGOS::ComputeCellPair(unit->Where().X(), unit->Where().Y());

            sLog.outError("[LivingWorld] spline-stall %s map=%u inst=%u pos=(%.2f,%.2f,%.2f o=%.2f) grid[%u,%u] cell[%u,%u] active-object=%s moveflags=0x%X movegen=%u in-combat=%s combat-timer=%u victim=%s target=%s",
                          unit->GetGuidStr().c_str(), unit->GetMapId(), unit->GetInstanceId(),
                          unit->Where().X(), unit->Where().Y(), unit->Where().Z(), unit->Where().Facing(),
                          gridPair.x_coord, gridPair.y_coord, cellPair.x_coord, cellPair.y_coord,
                          unit->IsActiveObject() ? "yes" : "no", uint32(moveflags),
                          uint32(const_cast<Unit*>(unit)->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                          unit->IsInCombat() ? "yes" : "no", unit->GetCombatTimer(),
                          victimGuid.c_str(), targetGuidString.c_str());
        }

        // A boarded unit has no world position worth sending: the client places it from
        // the vessel's own interpolated pose and the deck offset. A composed world
        // coordinate here is a guess the client would have to discard -- and when it does
        // not, the unit lands wherever the guess pointed.
        unit->WriteMovementInfo(*data);
        // Unit speeds
        *data << float(unit->GetSpeed(MOVE_WALK));
        *data << float(unit->GetSpeed(MOVE_RUN));
        *data << float(unit->GetSpeed(MOVE_RUN_BACK));
        *data << float(unit->GetSpeed(MOVE_SWIM));
        *data << float(unit->GetSpeed(MOVE_SWIM_BACK));
        *data << float(unit->GetSpeed(MOVE_TURN_RATE));

        if (unit->m_movementInfo.HasMovementFlag(MOVEFLAG_SPLINE_ENABLED))
        {
            Movement::PacketBuilder::WriteCreate(*unit->movespline, *data);
        }
    }
    else if (updateFlags & UPDATEFLAG_HAS_POSITION)
    {
        *data << ((WorldObject*)this)->Where().X();
        *data << ((WorldObject*)this)->Where().Y();
        *data << ((WorldObject*)this)->Where().Z();
        *data << ((WorldObject*)this)->Where().Facing();
    }

    if (updateFlags & UPDATEFLAG_HIGHGUID)
    {
        *data << highGuid;
    }

    if (updateFlags & UPDATEFLAG_ALL)
    {
        *data << (uint32)0x1;
    }

    if (updateFlags & UPDATEFLAG_FULLGUID)
    {
        if (unit && unit->getVictim())
        {
            *data << unit->getVictim()->GetPackGUID();
        }
        else
        {
            data->appendPackGUID(0);
        }
    }

    // 0x2
    if (updateFlags & UPDATEFLAG_TRANSPORT)
    {
        // THE PHASE, not the clock. The client does not take the modulo itself: it wants
        // how far along the route she is, and that is ours to compute. Hand it a raw wall
        // clock and the hull stops animating altogether -- a dead ship, with everyone
        // standing on her frozen too.
        if (isType(TYPEMASK_GAMEOBJECT)
            && ((GameObject*)this)->GetGoType() == GAMEOBJECT_TYPE_MO_TRANSPORT)
        {
            *data << uint32(((Transport*)this)->GetPathProgress());
        }
        else
        {
            *data << uint32(GameTime::GetGameTimeMS());       // ms time
        }
    }
}

/**
 * @brief Build the mask and the values for one observer
 *
 * The mask is the intersection of three things: what this class of object can
 * carry at all, what this observer is entitled to, and what is worth sending --
 * a value that is not zero on a create, a value that changed on an update.
 *
 * Nothing here knows what kind of object it is serializing. Fields whose value
 * depends on who is asking are rewritten by Fields::Project, and the handful
 * that vary that way go out even when the stored value stood still, because the
 * observer is what changed.
 */
void Object::BuildValuesUpdate(uint8 updatetype, ByteBuffer* data, Player* target) const
{
    if (!target)
    {
        return;
    }

    Fields::Table const& table = Fields::For(m_objectTypeId);
    MANGOS_ASSERT(table.count == m_valuesCount);

    uint32 admitted[Fields::MaxBlocks];
    Fields::MaskFor(table, Fields::AudienceFor(*this, *target), admitted);

    bool const creating = (updatetype != UPDATETYPE_VALUES);

    uint32 send[Fields::MaxBlocks] = { 0 };
    for (uint16 index = 0; index < table.count; ++index)
    {
        uint32 const bit = 1u << (index & 31);
        if (!(admitted[index >> 5] & bit))
        {
            continue;
        }

        bool wanted = creating ? (m_uint32Values[index] != 0) : m_changedValues[index];

        if (!wanted && Fields::AlwaysResend(m_objectTypeId, index))
        {
            wanted = !creating
                  || Fields::Project(*this, *target, index, m_uint32Values[index]) != 0;
        }

        if (wanted)
        {
            send[index >> 5] |= bit;
        }
    }

    *data << uint8(table.blocks);
    for (uint16 block = 0; block < table.blocks; ++block)
    {
        *data << send[block];
    }

    for (uint16 index = 0; index < table.count; ++index)
    {
        if (send[index >> 5] & (1u << (index & 31)))
        {
            *data << Fields::Project(*this, *target, index, m_uint32Values[index]);
        }
    }
}

/**
 * @brief Clear update mask
 * @param remove If true, remove from client update list
 *
 * Clears all changed value flags and optionally removes
 * the object from the pending update list.
 */
void Object::ClearUpdateMask(bool remove)
{
    if (m_uint32Values)
    {
        for (uint16 index = 0; index < m_valuesCount; ++index)
        {
            m_changedValues[index] = false;
        }
    }

    if (m_objectUpdated)
    {
        if (remove)
        {
            RemoveFromClientUpdateList();
        }
        m_objectUpdated = false;
    }
}
