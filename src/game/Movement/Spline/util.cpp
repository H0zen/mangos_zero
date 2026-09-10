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

#include <string>
#include "MoveSplineFlag.h"
#include <math.h>

namespace Movement
{

    double gravity = 19.29110527038574;

    float terminalVelocity = 60.148003f;

    float terminalSavefallVelocity = 7.f;

    const float terminal_length = float(terminalVelocity * terminalVelocity) / (2.f * gravity);
    const float terminal_savefall_length = (terminalSavefallVelocity * terminalSavefallVelocity) / (2.f * gravity);
    const float terminalFallTime = float(terminalVelocity / gravity);

    float computeFallTime(float path_length, bool isSafeFall)
    {
        if (path_length < 0.f)
        {
            return 0.f;
        }

        float time;
        if (isSafeFall)
        {
            if (path_length >= terminal_savefall_length)
            {
                time = (path_length - terminal_savefall_length) / terminalSavefallVelocity + terminalSavefallVelocity / gravity;
            }
            else
            {
                time = sqrtf(2.f * path_length / gravity);
            }
        }
        else
        {
            if (path_length >= terminal_length)
            {
                time = (path_length - terminal_length) / terminalVelocity + terminalFallTime;
            }
            else
            {
                time = sqrtf(2.f * path_length / gravity);
            }
        }

        return time;
    }

    float computeFallElevation(float t_passed, bool isSafeFall, float start_velocity)
    {
        float termVel;
        float result;

        if (isSafeFall)
        {
            termVel = terminalSavefallVelocity;
        }
        else
        {
            termVel = terminalVelocity;
        }

        if (start_velocity > termVel)
        {
            start_velocity = termVel;
        }

        float terminal_time = terminalFallTime - start_velocity / gravity;

        if (t_passed > terminal_time)
        {
            result = terminalVelocity * (t_passed - terminal_time) +
                start_velocity * terminal_time + gravity * terminal_time * terminal_time * 0.5f;
        }
        else
        {
            result = t_passed * (start_velocity + t_passed * gravity * 0.5f);
        }

        return result;
    }

    float computeFallElevation(float t_passed)
    {
        float result;

        if (t_passed > terminalFallTime)
        {

            result = terminalVelocity * (t_passed - terminalFallTime) + terminal_length;
        }
        else
        {
            result = t_passed * t_passed * gravity * 0.5f;
        }

        return result;
    }

#define STR(x) #x

    const char* g_MovementFlag_names[] =
    {
        STR(Forward),
        STR(Backward),
        STR(Strafe_Left),
        STR(Strafe_Right),
        STR(Turn_Left),
        STR(Turn_Right),
        STR(Pitch_Up),
        STR(Pitch_Down),

        STR(Walk),
        STR(Ontransport),
        STR(Levitation),
        STR(Root),
        STR(Falling),
        STR(Fallingfar),
        STR(Pendingstop),
        STR(PendingSTRafestop),
        STR(Pendingforward),
        STR(Pendingbackward),
        STR(PendingSTRafeleft),
        STR(PendingSTRaferight),
        STR(Pendingroot),
        STR(Swimming),
        STR(Ascending),
        STR(Descending),
        STR(Can_Fly),
        STR(Flying),
        STR(Spline_Elevation),
        STR(Spline_Enabled),
        STR(Waterwalking),
        STR(Safe_Fall),
        STR(Hover),
        STR(Unknown13),
        STR(Unk1),
        STR(Unk2),
        STR(Unk3),
        STR(Fullspeedturning),
        STR(Fullspeedpitching),
        STR(Allow_Pitching),
        STR(Unk4),
        STR(Unk5),
        STR(Unk6),
        STR(Unk7),
        STR(Interp_Move),
        STR(Interp_Turning),
        STR(Interp_Pitching),
        STR(Unk8),
        STR(Unk9),
        STR(Unk10),
    };

    const char* g_SplineFlag_names[32] =
    {
        STR(Done),
        STR(Falling),
        STR(Unknown3),
        STR(Unknown4),
        STR(Unknown5),
        STR(Unknown6),
        STR(Unknown7),
        STR(Unknown8),
        STR(Runmode),
        STR(Flying),
        STR(No_Spline),
        STR(Unknown12),
        STR(Unknown13),
        STR(Unknown14),
        STR(Unknown15),
        STR(Unknown16),
        STR(Final_Point),
        STR(Final_Target),
        STR(Final_Angle),
        STR(Unknown19),
        STR(Cyclic),
        STR(Enter_Cycle),
        STR(Frozen),
        STR(Unknown23),
        STR(Unknown24),
        STR(Unknown25),
        STR(Unknown26),
        STR(Unknown27),
        STR(Unknown28),
        STR(Unknown29),
        STR(Unknown30),
        STR(Unknown31),
    };

    template<class Flags, int N>
        void print_flags(Flags t, const char * (&names)[N], std::string& str)
    {
        for (int i = 0; i < N; ++i)
        {
            if ((t & (Flags)(1 << i)) && names[i] != nullptr)
            {
                str.append(" ").append(names[i]);
            }
        }
    }

    std::string MoveSplineFlag::ToString() const
    {
        std::string str;
        print_flags(raw(), g_SplineFlag_names, str);
        return str;
    }
}
