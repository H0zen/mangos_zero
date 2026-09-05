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

// How a quest sits in the log the client draws: three words apiece, with four
// counters and a state sharing the middle one.

#include "doctest.h"

#include "Journal/QuestSlots.h"
#include "QuestDef.h"

TEST_CASE("log: the twenty slots fill the block the client keeps for them")
{
    uint32 const block = PLAYER_VISIBLE_ITEM_1_CREATOR - PLAYER_QUEST_LOG_1_1;

    CHECK(block == uint32(MAX_QUEST_LOG_SIZE) * MAX_QUEST_OFFSET);
}

TEST_CASE("log: each slot's three words follow the one before with no gap")
{
    CHECK(quests::FieldOf(0, QUEST_ID_OFFSET) == PLAYER_QUEST_LOG_1_1);
    CHECK(quests::FieldOf(0, QUEST_TIME_OFFSET) == PLAYER_QUEST_LOG_1_1 + 2);
    CHECK(quests::FieldOf(1, QUEST_ID_OFFSET) == PLAYER_QUEST_LOG_1_1 + 3);

    // The last slot's last word is the last of the block.
    CHECK(quests::FieldOf(MAX_QUEST_LOG_SIZE - 1, QUEST_TIME_OFFSET)
          == PLAYER_VISIBLE_ITEM_1_CREATOR - 1);
}

TEST_CASE("counters: an empty word reads as four counters of nothing")
{
    for (uint8 which = 0; which < quests::COUNTERS_PER_QUEST; ++which)
    {
        CHECK(quests::CounterIn(0, which) == 0);
    }
}

TEST_CASE("counters: each of the four keeps its own six bits")
{
    uint32 packed = 0;
    packed = quests::WithCounter(packed, 0, 1);
    packed = quests::WithCounter(packed, 1, 2);
    packed = quests::WithCounter(packed, 2, 3);
    packed = quests::WithCounter(packed, 3, 4);

    CHECK(quests::CounterIn(packed, 0) == 1);
    CHECK(quests::CounterIn(packed, 1) == 2);
    CHECK(quests::CounterIn(packed, 2) == 3);
    CHECK(quests::CounterIn(packed, 3) == 4);
}

TEST_CASE("counters: writing one leaves the other three where they were")
{
    uint32 packed = 0;
    packed = quests::WithCounter(packed, 0, 63);
    packed = quests::WithCounter(packed, 1, 63);
    packed = quests::WithCounter(packed, 2, 63);
    packed = quests::WithCounter(packed, 3, 63);

    packed = quests::WithCounter(packed, 2, 0);

    CHECK(quests::CounterIn(packed, 0) == 63);
    CHECK(quests::CounterIn(packed, 1) == 63);
    CHECK(quests::CounterIn(packed, 2) == 0);
    CHECK(quests::CounterIn(packed, 3) == 63);
}

TEST_CASE("counters: no objective can be counted past what six bits hold")
{
    CHECK(quests::MOST_PER_COUNTER == 63);

    // A count over the ceiling is written as the ceiling rather than wrapping
    // round into the counter beside it.
    uint32 const packed = quests::WithCounter(0, 0, 200);

    CHECK(quests::CounterIn(packed, 0) == 63);
    CHECK(quests::CounterIn(packed, 1) == 0);
}

TEST_CASE("state: the four counters stop short of the byte the state sits in")
{
    CHECK(quests::CountersFitBelowState());

    // Filling every counter leaves the state byte untouched.
    uint32 packed = 0;
    for (uint8 which = 0; which < quests::COUNTERS_PER_QUEST; ++which)
    {
        packed = quests::WithCounter(packed, which, quests::MOST_PER_COUNTER);
    }

    CHECK(quests::StateIn(packed) == 0);
}

TEST_CASE("state: it lives in the top byte and the counters do not disturb it")
{
    uint32 packed = uint32(QUEST_STATE_COMPLETE) << (quests::STATE_BYTE * 8);

    CHECK(quests::StateIn(packed) == QUEST_STATE_COMPLETE);

    packed = quests::WithCounter(packed, 3, 63);

    CHECK(quests::StateIn(packed) == QUEST_STATE_COMPLETE);
    CHECK(quests::CounterIn(packed, 3) == 63);
}
