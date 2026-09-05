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

class Map;
class Player;
struct LiquidTypeEntry;

/**
 * What he is standing in. Internal values, never sent to the client.
 */
enum PlayerUnderwaterState
{
    UNDERWATER_NONE = 0x00,
    UNDERWATER_INWATER = 0x01,
    UNDERWATER_INLAVA = 0x02,
    UNDERWATER_INSLIME = 0x04,
    UNDERWATER_INDARKWATER = 0x08,

    UNDERWATER_EXIST_TIMERS = 0x10
};

/// The bars the client draws over his portrait.
enum MirrorTimerType
{
    FATIGUE_TIMER               = 0,
    BREATH_TIMER                = 1,
    FIRE_TIMER                  = 2  // Probably a mistake. More likely to be FEIGN_DEATH_TIMER
};

#define MAX_TIMERS              3
#define DISABLED_MIRROR_TIMER   -1

/**
 * What the ground and the water are doing to a character.
 *
 * Three countdowns run against him: his breath under water, his strength in
 * dark water, and his skin in lava. The first two are drawn as bars on his
 * portrait, empty while he is in it and fill ten times as fast once he is out;
 * the third is not drawn at all, and only bites.
 *
 * A bar that runs out does not stop. It is wound back two seconds and takes its
 * toll again, so a character who stays under keeps losing health at that pace
 * until he leaves or dies. The two seconds are the beat of the harm, not a
 * grace.
 *
 * What he stands in is read from the ground beneath him whenever he moves, and
 * the liquid itself may carry a spell -- the slime of the Undercity does -- which
 * is put on him while he is in it and taken off when he leaves. Where a liquid
 * carries its own spell, the fire countdown stands down and lets the spell do
 * the work.
 */
class Perils
{
    public:

        explicit Perils(Player& who);

        /// Reads the ground under him and sets what he is standing in.
        void Look(Map* where, float x, float y, float z);

        /// Has the bars redrawn on the next run.
        void Redraw();

        /// Runs the three countdowns.
        void Run(uint32 elapsed);

        /// How long the given countdown lasts, or DISABLED_MIRROR_TIMER when it
        /// does not run for him at all.
        int32 Longest(MirrorTimerType which) const;

        /// Takes the bar off his portrait and forgets the countdown.
        void Stop(MirrorTimerType which);

        bool InWater() const { return m_inWater; }

        /// Says he has entered or left the water, and settles what follows from it.
        void InWater(bool apply);

        /// His breath has under two seconds left.
        bool Drowning() const;

        /// The liquid he was last standing in, which may carry a spell.
        LiquidTypeEntry const* Liquid() const { return m_liquid; }

        /// Everything stands down, and nothing is drawn.
        void Clear();

    private:

        /// One of the two bars: it empties while he is in it and fills once he is out.
        void RunBar(MirrorTimerType which, uint8 standingIn, uint32 elapsed);

        /// What an emptied bar costs him.
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
