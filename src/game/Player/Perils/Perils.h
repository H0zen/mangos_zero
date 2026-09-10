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

enum EnvironmentalDamageType
{
    DAMAGE_EXHAUSTED            = 0,
    DAMAGE_DROWNING             = 1,
    DAMAGE_FALL                 = 2,
    DAMAGE_LAVA                 = 3,
    DAMAGE_SLIME                = 4,
    DAMAGE_FIRE                 = 5,
    DAMAGE_FALL_TO_VOID         = 6
};

class Map;
class Player;
struct LiquidTypeEntry;

enum PlayerUnderwaterState
{
    UNDERWATER_NONE = 0x00,
    UNDERWATER_INWATER = 0x01,
    UNDERWATER_INLAVA = 0x02,
    UNDERWATER_INSLIME = 0x04,
    UNDERWATER_INDARKWATER = 0x08,

    UNDERWATER_EXIST_TIMERS = 0x10
};

enum MirrorTimerType
{
    FATIGUE_TIMER               = 0,
    BREATH_TIMER                = 1,
    FIRE_TIMER                  = 2
};

#define MAX_TIMERS              3
#define DISABLED_MIRROR_TIMER   -1

class Perils
{
    public:

        explicit Perils(Player& who);

        void Look(Map* where, float x, float y, float z);

        void Redraw();

        void Run(uint32 elapsed);

        int32 Longest(MirrorTimerType which) const;

        void Stop(MirrorTimerType which);

        bool InWater() const { return m_inWater; }

        void InWater(bool apply);

        bool Drowning() const;

        LiquidTypeEntry const* Liquid() const { return m_liquid; }

        uint32 Harm(EnvironmentalDamageType type, uint32 damage);

        void Clear();

    private:

        void RunBar(MirrorTimerType which, uint8 standingIn, uint32 elapsed);

        void Emptied(MirrorTimerType which);

        void RunFire(uint32 elapsed);

        void Tell(MirrorTimerType which, uint32 most, uint32 left, int32 rate);

        Player& m_owner;

        int32 m_left[MAX_TIMERS];
        uint8 m_standingIn;
        uint8 m_drawn;
        bool m_inWater;
        LiquidTypeEntry const* m_liquid;
};
