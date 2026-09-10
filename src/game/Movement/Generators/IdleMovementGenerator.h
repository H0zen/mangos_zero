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

#include "MovementGenerator.h"

class IdleMovementGenerator : public MovementGenerator
{
    public:

        void Initialize(Unit&) override {}

        void Finalize(Unit&) override {}

        void Interrupt(Unit&) override {}

        void Reset(Unit&) override;

        bool Update(Unit&, uint32) override { return true; }

        MovementGeneratorType GetMovementGeneratorType() const override { return IDLE_MOTION_TYPE; }
};

extern IdleMovementGenerator si_idleMovement;

class DistractMovementGenerator : public MovementGenerator
{
    public:

        explicit DistractMovementGenerator(uint32 timer) : m_timer(timer) {}

        void Initialize(Unit& owner) override;

        void Finalize(Unit& owner) override;

        void Interrupt(Unit&) override;

        void Reset(Unit&) override;

        bool Update(Unit& owner, uint32 diff) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return DISTRACT_MOTION_TYPE; }

    private:
        uint32 m_timer;
};

class AssistanceDistractMovementGenerator : public DistractMovementGenerator
{
    public:

        AssistanceDistractMovementGenerator(uint32 timer) :
            DistractMovementGenerator(timer) {}

        MovementGeneratorType GetMovementGeneratorType() const override { return ASSISTANCE_DISTRACT_MOTION_TYPE; }

        void Finalize(Unit& unit) override;
};
