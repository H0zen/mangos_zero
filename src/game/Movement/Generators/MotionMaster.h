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
#include <stack>
#include <vector>
#include <sstream>

class MovementGenerator;
class Unit;

#define VISUAL_WAYPOINT 1

enum MovementGeneratorType
{
    IDLE_MOTION_TYPE = 0,
    RANDOM_MOTION_TYPE = 1,
    WAYPOINT_MOTION_TYPE = 2,
    MAX_DB_MOTION_TYPE = 3,

    CONFUSED_MOTION_TYPE = 4,
    CHASE_MOTION_TYPE = 5,
    HOME_MOTION_TYPE = 6,
    FLIGHT_MOTION_TYPE = 7,
    POINT_MOTION_TYPE = 8,
    FLEEING_MOTION_TYPE = 9,
    DISTRACT_MOTION_TYPE = 10,
    ASSISTANCE_MOTION_TYPE = 11,
    ASSISTANCE_DISTRACT_MOTION_TYPE = 12,
    TIMED_FLEEING_MOTION_TYPE = 13,
    FOLLOW_MOTION_TYPE = 14,
    EFFECT_MOTION_TYPE = 15,

    EXTERNAL_WAYPOINT_MOVE = 256,
    EXTERNAL_WAYPOINT_MOVE_START = 512,
    EXTERNAL_WAYPOINT_FINISHED_LAST = 1024
};

enum MMCleanFlag
{
    MMCF_NONE = 0,
    MMCF_UPDATE = 1,
    MMCF_RESET = 2
};

class MotionMaster : private std::stack<MovementGenerator*>
{
    private:
        typedef std::stack<MovementGenerator*> Impl;
        typedef std::vector<MovementGenerator*> ExpireList;

    public:

        explicit MotionMaster(Unit* unit) : m_owner(unit), m_expList(nullptr), m_cleanFlag(MMCF_NONE) {}

        ~MotionMaster();

        void Initialize();

        MovementGenerator const* GetCurrent() const { return top(); }

        using Impl::top;
        using Impl::empty;

        typedef Impl::container_type::const_iterator const_iterator;
        const_iterator begin() const { return Impl::c.begin(); }
        const_iterator end() const { return Impl::c.end(); }

        void UpdateMotion(uint32 diff);

        void Clear(bool reset = true, bool all = false)
        {
            if (m_cleanFlag & MMCF_UPDATE)
            {
                DelayedClean(reset, all);
            }
            else
            {
                DirectClean(reset, all);
            }
        }

        void MovementExpired(bool reset = true)
        {
            if (m_cleanFlag & MMCF_UPDATE)
            {
                DelayedExpire(reset);
            }
            else
            {
                DirectExpire(reset);
            }
        }

        void MoveIdle();

        void MoveRandomAroundPoint(float x, float y, float z, float radius, float verticalZ = 0.0f);

        void MoveTargetedHome();

        void MoveFollow(Unit* target, float dist, float angle);

        void MoveChase(Unit* target, float dist = 0.0f, float angle = 0.0f);

        void MoveConfused();

        void MoveFleeing(Unit* enemy, uint32 timeLimit = 0);

        void MovePoint(uint32 id, float x, float y, float z, bool generatePath = true);
        void MovePointRouted(uint32 id, float x, float y, float z);

        void MoveSeekAssistance(float x, float y, float z);

        void MoveSeekAssistanceDistract(uint32 timer);

        void MoveWaypoint(int32 id = 0, uint32 source = 0, uint32 initialDelay = 0, uint32 overwriteEntry = 0);

        void MoveTaxiFlight(uint32 path, uint32 pathnode);

        void MoveDistract(uint32 timeLimit);

        void MoveFall();

        void MoveFlyOrLand(uint32 id, float x, float y, float z, bool liftOff);

        MovementGeneratorType GetCurrentMovementGeneratorType() const;
        bool IsCurrentLegRouted() const;

        void PropagateSpeedChange();

        bool SetNextWaypoint(uint32 pointId);

        uint32 getLastReachedWaypoint() const;

        void GetWaypointPathInformation(std::ostringstream& oss) const;

        bool GetDestination(float& x, float& y, float& z);

    private:

        void Mutate(MovementGenerator* m);

        void DirectClean(bool reset, bool all);

        void DelayedClean(bool reset, bool all);

        void DirectExpire(bool reset);

        void DelayedExpire(bool reset);

        Unit*       m_owner;
        ExpireList* m_expList;
        uint8       m_cleanFlag;
};
