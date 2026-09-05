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

#pragma once

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "Object.h"

#include <cmath>

class ByteBuffer;

/**
 * How a unit is moving, and the block that says so on the wire.
 *
 * MovementInfo is read off and written to a packet whole; the flags are what the
 * client and the server agree the unit is doing. Nothing here knows anything
 * about the unit itself.
 */

/**
 * These flags denote the different kinds of movement you can do. You can have many at the
 * same time as this is used as a bitmask.
 * \todo [-ZERO] Need check and update used in most movement packets (send and received)
 * \see MovementInfo
 */
enum MovementFlags
{
    // Byte 1 (Resets on Movement Key Press)
    MOVEFLAG_NONE             = 0x00000000,
    MOVEFLAG_FORWARD          = 0x00000001,
    MOVEFLAG_BACKWARD         = 0x00000002,
    MOVEFLAG_STRAFE_LEFT      = 0x00000004,
    MOVEFLAG_STRAFE_RIGHT     = 0x00000008,
    MOVEFLAG_TURN_LEFT        = 0x00000010,
    MOVEFLAG_TURN_RIGHT       = 0x00000020,
    MOVEFLAG_PITCH_UP         = 0x00000040,
    MOVEFLAG_PITCH_DOWN       = 0x00000080,

    // Byte 2 (Resets on Situation Change)
    MOVEFLAG_WALK_MODE        = 0x00000100,               // Walking

    MOVEFLAG_LEVITATING       = 0x00000400,
    MOVEFLAG_FLYING           = 0x00000800,               // [-ZERO] is it really need and correct value
    MOVEFLAG_FALLING          = 0x00002000,
    MOVEFLAG_FALLINGFAR       = 0x00004000,
    MOVEFLAG_SWIMMING         = 0x00200000,               // appears with fly flag also
    MOVEFLAG_SPLINE_ENABLED   = 0x00400000,
    MOVEFLAG_CAN_FLY          = 0x00800000,               // [-ZERO] is it really need and correct value
    MOVEFLAG_FLYING_OLD       = 0x01000000,               // [-ZERO] is it really need and correct value

    MOVEFLAG_ONTRANSPORT      = 0x02000000,               // Used for flying on some creatures
    MOVEFLAG_SPLINE_ELEVATION = 0x04000000,               // used for flight paths
    MOVEFLAG_ROOT             = 0x08000000,               // used for flight paths
    MOVEFLAG_WATERWALKING     = 0x10000000,               // prevent unit from falling through water
    MOVEFLAG_SAFE_FALL        = 0x20000000,               // active rogue safe fall spell (passive)
    MOVEFLAG_HOVER            = 0x40000000
};

// flags that use in movement check for example at spell casting
MovementFlags const movementFlagsMask = MovementFlags(
    MOVEFLAG_FORWARD | MOVEFLAG_BACKWARD | MOVEFLAG_STRAFE_LEFT | MOVEFLAG_STRAFE_RIGHT |
    MOVEFLAG_PITCH_UP | MOVEFLAG_PITCH_DOWN | MOVEFLAG_ROOT |
    MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR | MOVEFLAG_SPLINE_ELEVATION
    );

MovementFlags const movementOrTurningFlagsMask = MovementFlags(
    movementFlagsMask | MOVEFLAG_TURN_LEFT | MOVEFLAG_TURN_RIGHT
    );

class MovementInfo
{
    public:
        MovementInfo() : moveFlags(MOVEFLAG_NONE), time(0),
            t_time(0), s_pitch(0.0f), fallTime(0), u_unk1(0.0f) {}

        // Read/Write methods
        void Read(ByteBuffer& data);
        void Write(ByteBuffer& data) const;

        // Movement flags manipulations
        void AddMovementFlag(MovementFlags f) { moveFlags |= f; }
        void RemoveMovementFlag(MovementFlags f) { moveFlags &= ~f; }
        bool HasMovementFlag(MovementFlags f) const { return moveFlags & f; }
        MovementFlags GetMovementFlags() const { return MovementFlags(moveFlags); }
        void SetMovementFlags(MovementFlags f) { moveFlags = f; }

        // Position manipulations
        Position const* GetPos() const { return &pos; }
        void SetTransportData(ObjectGuid guid, float x, float y, float z, float o, uint32 time)
        {
            t_guid = guid;
            t_pos.x = x;
            t_pos.y = y;
            t_pos.z = z;
            t_pos.o = o;
            t_time = time;
        }
        void ClearTransportData()
        {
            t_guid = ObjectGuid();
            t_pos.x = 0.0f;
            t_pos.y = 0.0f;
            t_pos.z = 0.0f;
            t_pos.o = 0.0f;
            t_time = 0;
        }
        ObjectGuid const& GetTransportGuid() const { return t_guid; }
        Position const* GetTransportPos() const { return &t_pos; }
        uint32 GetTime()
        {
            return time;
        }

        uint32 GetTransportTime() const { return t_time; }
        uint32 GetFallTime() const { return fallTime; }
        void ChangeOrientation(float o) { pos.o = o; }
        void ChangePosition(float x, float y, float z, float o) { pos.x = x; pos.y = y; pos.z = z; pos.o = o; }
        void UpdateTime(uint32 _time) { time = _time; }

        struct JumpInfo
        {
            JumpInfo() : velocity(0.f), sinAngle(0.f), cosAngle(0.f), xyspeed(0.f) {}
            float   velocity, sinAngle, cosAngle, xyspeed;
        };

        JumpInfo const& GetJumpInfo() const { return jump; }
        void SetJumpInfo(float vel, float sinA, float cosA, float xyspd)
        {
            jump.velocity = vel; jump.sinAngle = sinA; jump.cosAngle = cosA; jump.xyspeed = xyspd;
        }
        void SetFallTime(uint32 t) { fallTime = t; }
    private:
        // common
        uint32   moveFlags;             // see enum MovementFlags
        uint32   time;
        Position pos;
        ObjectGuid t_guid;              // transport
        Position t_pos;
        uint32   t_time;
        float    s_pitch;               // swimming and unknown

        uint32   fallTime;              // last fall time
        JumpInfo jump;                  // jumping
        float    u_unk1;                // spline
};
