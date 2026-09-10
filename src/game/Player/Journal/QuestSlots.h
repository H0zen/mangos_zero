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

enum QuestSlotOffsets
{
    QUEST_ID_OFFSET             = 0,
    QUEST_COUNT_STATE_OFFSET    = 1,
    QUEST_TIME_OFFSET           = 2
};

#define MAX_QUEST_OFFSET 3

enum QuestSlotStateMask
{
    QUEST_STATE_NONE            = 0x0000,
    QUEST_STATE_COMPLETE        = 0x0001,
    QUEST_STATE_FAIL            = 0x0002
};

namespace quests
{

    uint8 const COUNTERS_PER_QUEST = 4;

    uint8 const BITS_PER_COUNTER = 6;
    uint8 const MOST_PER_COUNTER = (1 << BITS_PER_COUNTER) - 1;

    uint8 const STATE_BYTE = 3;

    inline uint16 FieldOf(uint16 slot, uint16 word)
    {
        return uint16(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + word);
    }

    inline uint8 CounterIn(uint32 packed, uint8 which)
    {
        return uint8((packed >> (which * BITS_PER_COUNTER)) & MOST_PER_COUNTER);
    }

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

    inline uint8 StateIn(uint32 packed)
    {
        return uint8((packed >> (STATE_BYTE * 8)) & 0xFF);
    }

    inline bool CountersFitBelowState()
    {
        return COUNTERS_PER_QUEST * BITS_PER_COUNTER <= STATE_BYTE * 8;
    }

    inline uint32 CountsTowards(uint32 held, uint32 needed, uint32 gained)
    {
        if (held >= needed)
        {
            return 0;
        }

        uint32 const wanted = needed - held;
        return gained < wanted ? gained : wanted;
    }

    inline uint32 CountsAgainst(uint32 held, uint32 needed, uint32 lost)
    {
        uint32 const spare = held > needed ? held - needed : 0;
        uint32 const off = lost > spare ? lost - spare : 0;
        return off < held ? off : held;
    }
}
