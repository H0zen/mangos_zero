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

/**
 * Where a unit belongs, how far it may stray, and how it moves when nothing is
 * asking anything of it.
 *
 * The place is a whole pose -- a point and a facing -- in the frame it was put
 * in. A thing spawned on a deck belongs to a spot on that deck, and nothing
 * composes that with where the ship happens to be: home movement, wandering and
 * leashing all read this as it stands.
 *
 * The radius is how far it may wander from that place, and it does a second
 * duty: a radius of nothing with a wandering default means the row wanted it to
 * stand still, which is how the spawn is written back out.
 *
 * The anchor is different from the place. It is where it was standing when the
 * fight began, which is what it leashes back to -- so a thing pulled a hundred
 * yards from home leashes to where the pull started, not to home.
 *
 * A player has one of these and never uses it: he was not put anywhere and
 * nothing leashes him.
 */
class Station
{
    public:

        /// Where it belongs, as a pose in the frame it was put in.
        Geometry::Placement const& Where() const { return m_where; }
        void Where(Geometry::Placement const& pose) { m_where = pose; }

        /// Puts it at a point in the frame the given pose is in, so a thing on a
        /// deck belongs to a deck spot rather than to wherever the ship was.
        void PlaceInFrameOf(Geometry::Placement const& frame, Geometry::Vector3 const& at, float facing)
        {
            m_where.EnterFrameOf(frame, at, facing);
        }

        /// How far from there it may wander, in yards.
        float Radius() const { return m_radius; }
        void Radius(float yards) { m_radius = yards; }

        /// How it moves when nothing else is asking.
        MovementGeneratorType Wander() const { return m_wander; }
        void Wander(MovementGeneratorType how) { m_wander = how; }

        /// Where it was standing when the fight began, and what it leashes to.
        Geometry::Vector3 const& Anchor() const { return m_anchor; }
        void Anchor(Geometry::Vector3 const& at) { m_anchor = at; }

    private:

        Geometry::Placement m_where;
        Geometry::Vector3 m_anchor;

        float m_radius = 5.0f;
        MovementGeneratorType m_wander = IDLE_MOTION_TYPE;
};
