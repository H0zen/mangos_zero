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

#include "Unit/Auras/Diminishing.h"

namespace unit
{
    namespace
    {
        constexpr uint8 MAX_HITS = 3;   ///< Fade::Immune

        /// Wrap-safe elapsed time, for a clock that is a rolling millisecond
        /// counter rather than a date.
        uint32 Since(uint32 then, uint32 now)
        {
            return now >= then ? now - then : (0xFFFFFFFFu - then) + now;
        }
    }

    Diminishing::Entry* Diminishing::Find(DiminishingGroup group)
    {
        for (Entry& entry : m_entries)
        {
            if (entry.group == group)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    Fade Diminishing::FadeOf(DiminishingGroup group, uint32 now)
    {
        Entry* entry = Find(group);
        if (!entry || entry->hits == 0)
        {
            return Fade::Full;
        }

        // Nothing of this group is on the unit and it has been quiet long
        // enough: the history is spent and the next one lands in full.
        if (entry->held == 0 && entry->releasedAt != 0 &&
            Since(entry->releasedAt, now) > RESET_WINDOW_MS)
        {
            entry->hits = 0;
            return Fade::Full;
        }

        return static_cast<Fade>(entry->hits);
    }

    void Diminishing::RecordHit(DiminishingGroup group, uint32 now)
    {
        if (Entry* entry = Find(group))
        {
            if (entry->hits < MAX_HITS)
            {
                ++entry->hits;
            }
            return;
        }

        Entry entry;
        entry.group = group;
        entry.releasedAt = now;
        entry.hits = 1;
        m_entries.push_back(entry);
    }

    void Diminishing::Hold(DiminishingGroup group)
    {
        if (Entry* entry = Find(group))
        {
            ++entry->held;
        }
    }

    void Diminishing::Release(DiminishingGroup group, uint32 now)
    {
        Entry* entry = Find(group);
        if (!entry || entry->held == 0)
        {
            return;
        }

        --entry->held;
        if (entry->held == 0)
        {
            // The quiet window starts when the last one comes off, not when the
            // first one landed.
            entry->releasedAt = now;
        }
    }

    int32 Diminishing::Shorten(int32 duration, Fade fade)
    {
        if (duration < 0)
        {
            return duration;
        }

        switch (fade)
        {
            case Fade::Half:    return duration / 2;
            case Fade::Quarter: return duration / 4;
            case Fade::Immune:  return 0;
            case Fade::Full:    break;
        }
        return duration;
    }
}
