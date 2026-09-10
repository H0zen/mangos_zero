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
#include "SharedDefines.h"

#include <vector>

namespace unit
{

    enum class Fade : uint8
    {
        Full = 0,
        Half = 1,
        Quarter = 2,
        Immune = 3,
    };

    class Diminishing
    {
        public:

            struct Entry
            {
                DiminishingGroup group = DIMINISHING_NONE;
                uint16 held = 0;
                uint32 releasedAt = 0;
                uint8 hits = 0;
            };

            Fade FadeOf(DiminishingGroup group, uint32 now);

            void RecordHit(DiminishingGroup group, uint32 now);

            void Hold(DiminishingGroup group);

            void Release(DiminishingGroup group, uint32 now);

            void Clear() { m_entries.clear(); }
            bool Empty() const { return m_entries.empty(); }

            static int32 Shorten(int32 duration, Fade fade);

            static constexpr uint32 RESET_WINDOW_MS = 15 * 1000;

        private:

            Entry* Find(DiminishingGroup group);

            std::vector<Entry> m_entries;
    };
}
