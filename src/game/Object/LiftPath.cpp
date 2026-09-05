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

#include "LiftPath.h"
#include "DBCStores.h"

#include <algorithm>

LiftPath LiftPath::Of(uint32 goEntry)
{
    auto const found = sTransportAnimationsByEntry.find(goEntry);
    return found == sTransportAnimationsByEntry.end() ? LiftPath() : LiftPath(&found->second);
}

uint32 LiftPath::Period() const
{
    return IsEmpty() ? 0 : m_frames->back()->TimeIndex;
}

std::size_t LiftPath::FrameAt(uint32 phaseMs) const
{
    // The frame in force is the last one whose moment has already passed.
    auto const after = std::upper_bound(m_frames->begin(), m_frames->end(), phaseMs,
                                        [](uint32 when, TransportAnimationEntry const* frame)
                                        { return when < frame->TimeIndex; });

    return after == m_frames->begin() ? 0 : std::size_t(after - m_frames->begin()) - 1;
}

Geometry::Vector3 LiftPath::OffsetAt(uint32 phaseMs) const
{
    if (IsEmpty())
    {
        return Geometry::Vector3();
    }

    uint32 const period = Period();
    if (period != 0)
    {
        phaseMs %= period;
    }

    std::size_t const at = FrameAt(phaseMs);
    TransportAnimationEntry const* from = (*m_frames)[at];
    Geometry::Vector3 const here(from->PosX, from->PosY, from->PosZ);

    // The last keyframe closes the loop at exactly the period, so a phase inside the
    // loop always has a keyframe after it to slide towards. A pair sharing one moment
    // has no span to slide along.
    if (at + 1 >= m_frames->size())
    {
        return here;
    }

    TransportAnimationEntry const* to = (*m_frames)[at + 1];
    if (to->TimeIndex <= from->TimeIndex)
    {
        return here;
    }

    Geometry::Vector3 const there(to->PosX, to->PosY, to->PosZ);
    float const t = float(phaseMs - from->TimeIndex) / float(to->TimeIndex - from->TimeIndex);

    return here + (there - here) * t;
}

uint32 LiftPath::SequenceAt(uint32 phaseMs) const
{
    if (IsEmpty())
    {
        return 0;
    }

    uint32 const period = Period();
    if (period != 0)
    {
        phaseMs %= period;
    }

    return (*m_frames)[FrameAt(phaseMs)]->SequenceID;
}
