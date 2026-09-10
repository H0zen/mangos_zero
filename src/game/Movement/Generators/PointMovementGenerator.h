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

class PointMovementGenerator : public IntentMovementGenerator
{
    public:
        PointMovementGenerator(uint32 id, float x, float y, float z, bool generatePath)
            : m_id(id), m_dest(x, y, z), m_generatePath(generatePath) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return POINT_MOTION_TYPE; }

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

        virtual uint32 LegFlags() const
        {
            return m_generatePath ? Motion::MOVE_NONE : Motion::MOVE_STRAIGHT;
        }

        void MovementInform(Unit& owner) const;

        uint32 m_id;
        Motion::Vector3 m_dest;
        bool m_generatePath;
};

class AssistanceMovementGenerator final : public PointMovementGenerator
{
    public:
        AssistanceMovementGenerator(float x, float y, float z)
            : PointMovementGenerator(0, x, y, z, true) {}

        MovementGeneratorType GetMovementGeneratorType() const override { return ASSISTANCE_MOTION_TYPE; }

        void Finalize(Unit& owner) override;

    protected:
        uint32 LegFlags() const override { return Motion::MOVE_WALK; }
};

class RoutedPointMovementGenerator final : public PointMovementGenerator
{
    public:
        RoutedPointMovementGenerator(uint32 id, float x, float y, float z)
            : PointMovementGenerator(id, x, y, z, true), m_arrived(false) {}

        void Initialize(Unit& owner) override
        {
            m_arrived = false;
            PointMovementGenerator::Initialize(owner);
        }

        void Finalize(Unit& owner) override;

        bool IsRoutedLeg() const override { return true; }

    protected:
        uint32 LegFlags() const override { return Motion::MOVE_REQUIRE_ROUTE; }

        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

    private:
        bool m_arrived;
};

class FlyOrLandMovementGenerator final : public PointMovementGenerator
{
    public:

        FlyOrLandMovementGenerator(uint32 id, float x, float y, float z, bool )
            : PointMovementGenerator(id, x, y, z, false) {}

    protected:
        uint32 LegFlags() const override
        {
            return Motion::MOVE_FLY | Motion::MOVE_STRAIGHT;
        }
};

class EffectMovementGenerator final : public IntentMovementGenerator
{
    public:
        explicit EffectMovementGenerator(uint32 id) : m_id(id) {}

        void Initialize(Unit&) override {}
        void Interrupt(Unit&) override {}
        void Reset(Unit&) override {}
        void Finalize(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return EFFECT_MOTION_TYPE; }

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

    private:
        uint32 m_id;
};
