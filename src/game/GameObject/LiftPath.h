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

#include "DBCStructure.h"
#include "Geometry/Vector3.h"

#include <cstddef>

/**
 * The loop a lift or a tram runs, read out of TransportAnimation.dbc.
 *
 * A handful of keyframes, each saying where the platform stands at that
 * millisecond of the loop as an offset from the gameobject's anchor. The last
 * keyframe's time is the length of the whole loop, and between two of them the
 * platform slides in a straight line -- no spline, unlike a vessel's route.
 *
 * This is the client's own algorithm. The server needs it because a rider sends
 * its own true position and a pet does not: the pet is placed by us, so we are
 * the ones who have to know where the platform is.
 */
class LiftPath
{
    public:
        LiftPath() : m_frames(nullptr) {}
        explicit LiftPath(TransportAnimation const* frames) : m_frames(frames) {}

        /// The keyframes the data gives this gameobject entry, if it gives any.
        static LiftPath Of(uint32 goEntry);

        bool IsEmpty() const { return m_frames == nullptr || m_frames->empty(); }

        /// One full loop, in milliseconds; 0 when there is nothing to loop over.
        uint32 Period() const;

        /// Where the platform stands, relative to the anchor, that far into the loop.
        Geometry::Vector3 OffsetAt(uint32 phaseMs) const;

        /// The model animation it is playing there.
        uint32 SequenceAt(uint32 phaseMs) const;

    private:
        /// Index of the keyframe in force at that moment; only valid on a non-empty path.
        std::size_t FrameAt(uint32 phaseMs) const;

        TransportAnimation const* m_frames;
};
