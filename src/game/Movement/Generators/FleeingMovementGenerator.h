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

#include "IntentMovementGenerator.h"
#include "ObjectGuid.h"

#include <optional>

class FleeingMovementGenerator : public IntentMovementGenerator
{
    public:
        explicit FleeingMovementGenerator(ObjectGuid fright) : m_frightGuid(fright) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return FLEEING_MOTION_TYPE; }

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

        std::optional<Motion::Vector3> PickFleePoint(Unit& owner) const;

    private:
        ObjectGuid m_frightGuid = 0;

        TimeTracker m_restTime{0};
        Motion::Vector3 m_fleePoint;
        bool m_haveFleePoint = false;
};

class TimedFleeingMovementGenerator final : public FleeingMovementGenerator
{
    public:
        TimedFleeingMovementGenerator(ObjectGuid fright, uint32 time)
            : FleeingMovementGenerator(fright), m_totalFleeTime(time) {}

        MovementGeneratorType GetMovementGeneratorType() const override { return TIMED_FLEEING_MOTION_TYPE; }

        void Finalize(Unit& owner) override;

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

    private:
        TimeTracker m_totalFleeTime;
};
