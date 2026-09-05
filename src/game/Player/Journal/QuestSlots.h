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
#include "UpdateFields.h"

/// The three words a quest takes up in the log.
enum QuestSlotOffsets
{
    QUEST_ID_OFFSET             = 0,
    QUEST_COUNT_STATE_OFFSET    = 1,
    QUEST_TIME_OFFSET           = 2
};

#define MAX_QUEST_OFFSET 3

/// What the state byte says about a quest the character is carrying.
enum QuestSlotStateMask
{
    QUEST_STATE_NONE            = 0x0000,
    QUEST_STATE_COMPLETE        = 0x0001,
    QUEST_STATE_FAIL            = 0x0002
};

/**
 * How a quest sits in the log the client draws.
 *
 * A character carries twenty quests at once, each taking three words. The first
 * is the quest's own number and is the only one his party can see; the other two
 * are his alone.
 *
 * The middle word is four counters and a state packed together: six bits apiece
 * for the four things a quest can ask him to collect or kill, and the top byte
 * for whether it is finished or failed. Six bits is why no objective can ask for
 * more than sixty-three of anything.
 *
 * The last word is when a timed quest runs out, as a whole time rather than a
 * countdown, so the client draws the clock itself.
 */
namespace quests
{
    /// Four things per quest, because that is what the middle word holds.
    uint8 const COUNTERS_PER_QUEST = 4;

    /// Six bits apiece.
    uint8 const BITS_PER_COUNTER = 6;
    uint8 const MOST_PER_COUNTER = (1 << BITS_PER_COUNTER) - 1;

    /// The byte the state sits in, counting from the low end of the word.
    uint8 const STATE_BYTE = 3;

    /// Which field of the character's own the given word of a slot is.
    inline uint16 FieldOf(uint16 slot, uint16 word)
    {
        return uint16(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + word);
    }

    /// The counter as it stands in the packed word.
    inline uint8 CounterIn(uint32 packed, uint8 which)
    {
        return uint8((packed >> (which * BITS_PER_COUNTER)) & MOST_PER_COUNTER);
    }

    /**
     * The word with one counter replaced.
     *
     * A count over what six bits hold is written as the largest that fits rather
     * than wrapping round into the counter beside it.
     */
    inline uint32 WithCounter(uint32 packed, uint8 which, uint8 count)
    {
        uint32 const room = uint32(MOST_PER_COUNTER) << (which * BITS_PER_COUNTER);
        uint32 const kept = packed & ~room;

        if (count > MOST_PER_COUNTER)
        {
            count = MOST_PER_COUNTER;
        }

        return kept | (uint32(count) << (which * BITS_PER_COUNTER));
    }

    /// The state byte out of the packed word.
    inline uint8 StateIn(uint32 packed)
    {
        return uint8((packed >> (STATE_BYTE * 8)) & 0xFF);
    }

    /// Whether the four counters and the state can share the word without one
    /// running into the other.
    inline bool CountersFitBelowState()
    {
        return COUNTERS_PER_QUEST * BITS_PER_COUNTER <= STATE_BYTE * 8;
    }
}
