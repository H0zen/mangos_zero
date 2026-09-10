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

#include "FollowerReference.h"
#include "IntentMovementGenerator.h"
#include "Unit.h"

class TargetedMovementGeneratorBase
{
    public:
        explicit TargetedMovementGeneratorBase(Unit& target) { i_target.link(&target, this); }

        void stopFollowing() {}

    protected:
        FollowerReference i_target;
};

class TargetedMovementGenerator : public IntentMovementGenerator,
                                  public TargetedMovementGeneratorBase
{
    public:
        Unit* GetTarget() const { return i_target.getTarget(); }

    protected:
        TargetedMovementGenerator(Unit& target, float offset, float angle)
            : TargetedMovementGeneratorBase(target), m_offset(offset), m_angle(angle) {}

        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) final;

        virtual void AddMoveState(Unit& owner) const = 0;
        virtual void ClearMoveState(Unit& owner) const = 0;

        virtual float TargetDistance(Unit& owner, bool forRangeCheck) const = 0;

        virtual bool LostTarget(Unit& ) const { return false; }

        virtual void ReachTarget(Unit& ) {}

        virtual bool EnableWalking(Unit& ) const { return false; }

        virtual uint32 RecheckIntervalMs() const { return 100; }

        void ResetTracking();

        float m_offset;
        float m_angle;

    private:

        Motion::Vector3 ComputeDestination(Unit& owner) const;

        bool RequiresNewPosition(Unit& owner, Motion::Vector3 const& spot) const;

        TimeTracker m_recheckTime{0};
        Motion::Vector3 m_dest;
        bool m_haveDest = false;
        bool m_targetReached = false;
};

class ChaseMovementGenerator final : public TargetedMovementGenerator
{
    public:
        ChaseMovementGenerator(Unit& target, float offset, float angle)
            : TargetedMovementGenerator(target, offset, angle) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return CHASE_MOTION_TYPE; }

    protected:
        void AddMoveState(Unit& owner) const override { owner.addUnitState(UNIT_STAT_CHASE_MOVE); }
        void ClearMoveState(Unit& owner) const override { owner.clearUnitState(UNIT_STAT_CHASE_MOVE); }

        float TargetDistance(Unit& owner, bool forRangeCheck) const override;
        bool LostTarget(Unit& owner) const override;
        void ReachTarget(Unit& owner) override;
};

class FollowMovementGenerator final : public TargetedMovementGenerator
{
    public:
        FollowMovementGenerator(Unit& target, float offset, float angle)
            : TargetedMovementGenerator(target, offset, angle) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return FOLLOW_MOTION_TYPE; }

    protected:
        void AddMoveState(Unit& owner) const override { owner.addUnitState(UNIT_STAT_FOLLOW_MOVE); }
        void ClearMoveState(Unit& owner) const override { owner.clearUnitState(UNIT_STAT_FOLLOW_MOVE); }

        float TargetDistance(Unit& owner, bool forRangeCheck) const override;
        bool EnableWalking(Unit& owner) const override;
        uint32 RecheckIntervalMs() const override { return 50; }

    private:

        void SyncSpeedWithMaster(Unit& owner) const;
};
