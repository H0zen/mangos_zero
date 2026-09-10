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
#include "MotionFrame.h"
#include "MovementIntent.h"

#include <memory>

class Unit;

class MotionDriver
{
    public:

        Motion::MoveStatus BeginTick(Unit& owner);

        bool Apply(Unit& owner, Motion::MoveIntent const& intent);

        void OnSpeedChanged() { m_speedChanged = true; }

        void ResetLeg();

        bool Reachable() const { return !m_query || m_query->Reachable(); }

    private:

        bool ReconcileMove(Unit& owner, Motion::MoveIntent const& intent);

        void ReconcileHold(Unit& owner, Motion::MoveIntent const& intent);

        bool LayLeg(Unit& owner, Motion::MoveIntent const& intent);

        Motion::IPathQuery* Query(Unit const& owner);

        std::unique_ptr<Motion::IPathQuery> m_query;
        Motion::FrameKind m_queryFrame = Motion::FrameKind::World;

        Motion::Vector3 m_legGoal;
        bool m_haveLeg = false;

        bool m_blocked = false;
        bool m_speedChanged = false;
        bool m_wasTraveling = false;
};
