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
#include <vector>
#include <cstdlib>
#include "Occupant.h"
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
#include "Transports.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "Movement/Spline/packet_builder.h"
#include "Movement/Spline/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"

void Object::SendForcedObjectUpdate()
{
    if (!m_inWorld || !m_objectUpdated)
    {
        return;
    }

    UpdateDataMapType update_players;

    BuildUpdateData(update_players);
    RemoveFromClientUpdateList();

    WorldPacket packet;
    for (UpdateDataMapType::iterator iter = update_players.begin(); iter != update_players.end(); ++iter)
    {
        iter->second.BuildPacket(&packet);
        iter->first->GetSession()->SendPacket(&packet);
        packet.clear();
    }
}

void Object::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (!target)
    {
        return;
    }

    uint8  updatetype   = UPDATETYPE_CREATE_OBJECT;
    uint8 updateFlags  = m_updateFlag;

    if (target == this)
    {
        updateFlags |= UPDATEFLAG_SELF;
    }

    if (m_isNewObject)
    {
        switch (GuidHigh(GetObjectGuid()))
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

    if ((GuidHigh(GetObjectGuid()) == HIGHGUID_MO_TRANSPORT))
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

void Object::SendCreateUpdateToPlayer(Player* player)
{

    UpdateData upd;
    WorldPacket packet;

    BuildCreateUpdateBlockForPlayer(&upd, player);
    upd.BuildPacket(&packet);
    player->GetSession()->SendPacket(&packet);
}

void Object::BuildValuesUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    ByteBuffer& buf = data->GetBuffer();

    buf << uint8(UPDATETYPE_VALUES);
    buf << GetPackGUID();

    BuildValuesUpdate(UPDATETYPE_VALUES, &buf, target);

    data->AddUpdateBlock();
}

void Object::BuildOutOfRangeUpdateBlock(UpdateData* data) const
{
    data->AddOutOfRangeGUID(GetObjectGuid());
}

void Object::DestroyForPlayer(Player* target) const
{
    MANGOS_ASSERT(target);

    WorldPacket data(SMSG_DESTROY_OBJECT, 8);
    data << GetObjectGuid();
    target->GetSession()->SendPacket(&data);
}

void Object::BuildMovementUpdate(ByteBuffer* data, uint8 updateFlags) const
{
    Unit const* unit = nullptr;
    uint32 highGuid = 0;

    switch (m_objectTypeId)
    {
        case TYPEID_OBJECT:
        case TYPEID_ITEM:
        case TYPEID_CONTAINER:
        case TYPEID_GAMEOBJECT:
        case TYPEID_DYNAMICOBJECT:
        case TYPEID_CORPSE:
            highGuid = uint32(GuidHigh(GetObjectGuid()));
            break;

        case TYPEID_PLAYER:
        case TYPEID_UNIT:
            unit = static_cast<Unit const*>(this);
            break;

        default:
            break;
    }

    *data << uint8(updateFlags);

    if (updateFlags & UPDATEFLAG_LIVING)
    {
        MANGOS_ASSERT(unit);

        if (unit->movespline->Finalized() && unit->m_movementInfo.HasMovementFlag(MOVEFLAG_SPLINE_ENABLED))
        {
            sLog.outError("%s has spline movement enabled but its spline is finalized!", GetGuidStr().c_str());
            std::string victimGuid = "none";
            if (Unit const* victim = unit->getVictim())
            {
                victimGuid = victim->GetGuidStr();
            }

            ObjectGuid const& targetGuid = unit->GetTargetGuid();
            std::string targetGuidString = (targetGuid == 0) ? "none" : GuidString(targetGuid);
            GridPair gridPair = MaNGOS::ComputeGridPair(unit->Where().X(), unit->Where().Y());
            CellPair cellPair = MaNGOS::ComputeCellPair(unit->Where().X(), unit->Where().Y());

            sLog.outError("[LivingWorld] spline-stall %s map=%u inst=%u pos=(%.2f,%.2f,%.2f o=%.2f) grid[%u,%u] cell[%u,%u] active-object=%s moveflags=0x%X movegen=%u in-combat=%s combat-timer=%u victim=%s target=%s",
                          unit->GetGuidStr().c_str(), unit->GetMapId(), unit->GetInstanceId(),
                          unit->Where().X(), unit->Where().Y(), unit->Where().Z(), unit->Where().Facing(),
                          gridPair.x_coord, gridPair.y_coord, cellPair.x_coord, cellPair.y_coord,
                          unit->IsActiveObject() ? "yes" : "no",
                          uint32(unit->m_movementInfo.GetMovementFlags()),
                          uint32(const_cast<Unit*>(unit)->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                          unit->IsInCombat() ? "yes" : "no", unit->GetCombatTimer(),
                          victimGuid.c_str(), targetGuidString.c_str());
        }

        unit->WriteMovementInfo(*data);

        *data << float(unit->Pacing().At(MOVE_WALK));
        *data << float(unit->Pacing().At(MOVE_RUN));
        *data << float(unit->Pacing().At(MOVE_RUN_BACK));
        *data << float(unit->Pacing().At(MOVE_SWIM));
        *data << float(unit->Pacing().At(MOVE_SWIM_BACK));
        *data << float(unit->Pacing().At(MOVE_TURN_RATE));

        if (unit->m_movementInfo.HasMovementFlag(MOVEFLAG_SPLINE_ENABLED))
        {
            Movement::PacketBuilder::WriteCreate(*unit->movespline, *data);
        }
    }
    else if (updateFlags & UPDATEFLAG_HAS_POSITION)
    {
        *data << ((Occupant*)this)->Where().X();
        *data << ((Occupant*)this)->Where().Y();
        *data << ((Occupant*)this)->Where().Z();
        *data << ((Occupant*)this)->Where().Facing();
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

    if (updateFlags & UPDATEFLAG_TRANSPORT)
    {

        if (IsType(this, TYPEMASK_GAMEOBJECT)
            && ((GameObject*)this)->GetGoType() == GAMEOBJECT_TYPE_MO_TRANSPORT)
        {
            *data << uint32(((Transport*)this)->GetPathProgress());
        }
        else if (IsType(this, TYPEMASK_GAMEOBJECT) && ((GameObject*)this)->IsLift())
        {
            *data << uint32(((GameObject*)this)->LiftPhase());
        }
        else
        {
            *data << uint32(GameTime::GetGameTimeMS());
        }
    }
}

void Object::BuildValuesUpdate(uint8 updatetype, ByteBuffer* data, Player* target) const
{
    if (!target)
    {
        return;
    }

    Fields::Table const& table = Fields::For(m_objectTypeId);
    uint32 admitted[Fields::MaxBlocks];
    Fields::MaskFor(table, Fields::AudienceFor(*this, *target), admitted);

    uint32 const* const outside = Fields::OutsideMask(m_objectTypeId);

    uint32 send[Fields::MaxBlocks];

    if (updatetype == UPDATETYPE_VALUES)
    {
        for (uint16 block = 0; block < table.blocks; ++block)
        {
            send[block] = m_mirror.Dirty()[block] & admitted[block];
        }
    }
    else
    {
        uint32 asked[Fields::MaxBlocks];
        uint32 stored[Fields::MaxBlocks];

        for (uint16 block = 0; block < table.blocks; ++block)
        {
            asked[block] = outside[block] & admitted[block];
            stored[block] = admitted[block] & ~asked[block];
            send[block] = 0;
        }

        Fields::ForEachSet(stored, table.blocks, [&](uint16 index)
        {
            if (m_mirror.Read(index) != 0)
            {
                send[index >> 5] |= 1u << (index & 31);
            }
        });

        Fields::ForEachSet(asked, table.blocks, [&](uint16 index)
        {
            if (Fields::Project(*this, *target, index, m_mirror.Read(index)) != 0)
            {
                send[index >> 5] |= 1u << (index & 31);
            }
        });
    }

    *data << uint8(table.blocks);
    for (uint16 block = 0; block < table.blocks; ++block)
    {
        *data << send[block];
    }

    Fields::ForEachSet(send, table.blocks, [&](uint16 index)
    {
        *data << Fields::Project(*this, *target, index, m_mirror.Read(index));
    });
}

void Object::ClearUpdateMask(bool remove)
{
    m_mirror.Settle();

    if (m_objectUpdated)
    {
        if (remove)
        {
            RemoveFromClientUpdateList();
        }
        m_objectUpdated = false;
    }
}
