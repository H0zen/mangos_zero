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

class LiftPath
{
    public:
        LiftPath() : m_frames(nullptr) {}
        explicit LiftPath(TransportAnimation const* frames) : m_frames(frames) {}

        static LiftPath Of(uint32 goEntry);

        bool IsEmpty() const { return m_frames == nullptr || m_frames->empty(); }

        uint32 Period() const;

        Geometry::Vector3 OffsetAt(uint32 phaseMs) const;

        uint32 SequenceAt(uint32 phaseMs) const;

    private:

        std::size_t FrameAt(uint32 phaseMs) const;

        TransportAnimation const* m_frames;
};
