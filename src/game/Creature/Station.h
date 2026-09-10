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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#pragma once

#include "Geometry/Placement.h"
#include "MotionMaster.h"
#include "Platform/Define.h"

class Station
{
    public:

        Geometry::Placement const& Where() const { return m_where; }
        void Where(Geometry::Placement const& pose) { m_where = pose; }

        void PlaceInFrameOf(Geometry::Placement const& frame, Geometry::Vector3 const& at, float facing)
        {
            m_where.EnterFrameOf(frame, at, facing);
        }

        float Radius() const { return m_radius; }
        void Radius(float yards) { m_radius = yards; }

        MovementGeneratorType Wander() const { return m_wander; }
        void Wander(MovementGeneratorType how) { m_wander = how; }

        Geometry::Vector3 const& Anchor() const { return m_anchor; }
        void Anchor(Geometry::Vector3 const& at) { m_anchor = at; }

    private:

        Geometry::Placement m_where;
        Geometry::Vector3 m_anchor;

        float m_radius = 5.0f;
        MovementGeneratorType m_wander = IDLE_MOTION_TYPE;
};
