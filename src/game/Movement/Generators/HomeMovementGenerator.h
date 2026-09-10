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

#include <array>

class HomeMovementGenerator final : public IntentMovementGenerator
{
    public:
        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return HOME_MOTION_TYPE; }

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

    private:

        bool AtHome(Unit const& owner) const;

        bool Resumable(Unit const& owner, Motion::MoveStatus const& status, uint32 diff);

        bool RefreshSpeedRates(Unit const& owner);

        Motion::Vector3 m_home;
        float m_facing = 0.0f;
        bool m_haveHome = false;
        bool m_arrived = false;

        Motion::Vector3 m_progressPosition;
        int32 m_pathIndex = 0;
        bool m_homeIntentIssued = false;
        bool m_expectHomeLeg = false;
        bool m_onHomeLeg = false;
        std::array<float, 6> m_speedRates{};
        uint32 m_stalled = 0;
};
