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

// How often a unit has been controlled lately, and what that costs the next one.
//
// This is a component: it owns its state and answers about it, and it does not
// know what a Unit is. Everything it needs arrives as an argument, which is what
// makes it exercisable on values and what stops the next question about control
// effects from being answered by adding another method to Unit.
//
// The division of labour with the caller is deliberate. Whether diminishing
// applies at all -- this group against this kind of victim, from a friend or an
// enemy, reflected or not -- is a question the caster's side already has the
// answer to. This holds the history and states the penalty.

#include "Platform/Define.h"
#include "SharedDefines.h"

#include <vector>

namespace unit
{
    /// What a control effect of a given group is worth by now.
    enum class Fade : uint8
    {
        Full = 0,       ///< first application: the whole duration
        Half = 1,
        Quarter = 2,
        Immune = 3,     ///< nothing lands at all
    };

    class Diminishing
    {
        public:

            /// The history for one group.
            struct Entry
            {
                DiminishingGroup group = DIMINISHING_NONE;
                uint16 held = 0;        ///< auras of this group currently on the unit
                uint32 releasedAt = 0;  ///< when the last one came off
                uint8 hits = 0;         ///< how far the group has faded
            };

            /**
             * @brief How much of the next effect of this group survives.
             *
             * `now` arrives from the caller so the component has no clock of its
             * own. A group with nothing held that has been quiet for the reset
             * window forgets its history here, which is why this is not const.
             */
            Fade FadeOf(DiminishingGroup group, uint32 now);

            /// Records that an effect of this group landed.
            void RecordHit(DiminishingGroup group, uint32 now);

            /// An aura of this group went on the unit.
            void Hold(DiminishingGroup group);

            /// One came off; `now` starts the quiet window once none are left.
            void Release(DiminishingGroup group, uint32 now);

            void Clear() { m_entries.clear(); }
            bool Empty() const { return m_entries.empty(); }

            /**
             * @brief A duration, shortened by how faded the group is. Pure.
             *
             * A duration of -1 is permanent and is returned untouched.
             */
            static int32 Shorten(int32 duration, Fade fade);

            /// The quiet time after which a group forgets its history.
            static constexpr uint32 RESET_WINDOW_MS = 15 * 1000;

        private:

            Entry* Find(DiminishingGroup group);

            // A unit carries a handful of these at most, so a flat vector beats
            // any map: it is one cache line to walk and nothing to allocate per
            // group.
            std::vector<Entry> m_entries;
    };
}
