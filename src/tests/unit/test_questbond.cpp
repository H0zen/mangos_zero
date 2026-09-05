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

// What ties a spawn to a quest.
//
// Two things are pinned down here. The relation lookup, which a creature and a
// gameobject both make of the same shaped maps, and the one field per gameobject
// type that names the quest lighting it up -- the field the loader and the
// sparkle have to agree on.

#include "doctest.h"

#include "QuestBond.h"
#include "GameObject.h"

#include <map>

namespace
{
    typedef std::multimap<uint32, uint32> Relations;

    /// A template of the given type with nothing else filled in.
    GameObjectInfo Templated(uint32 type)
    {
        GameObjectInfo info = {};
        info.id = 1000 + type;
        info.type = type;
        return info;
    }
}

TEST_CASE("quest bond: an entry with no relations names no quest")
{
    Relations relations;

    CHECK_FALSE(NamesQuest(relations.equal_range(70), 42u));
}

TEST_CASE("quest bond: an entry names every quest hanging off it")
{
    Relations relations;
    relations.insert(std::make_pair(70u, 42u));
    relations.insert(std::make_pair(70u, 43u));
    relations.insert(std::make_pair(70u, 44u));

    CHECK(NamesQuest(relations.equal_range(70), 42u));
    CHECK(NamesQuest(relations.equal_range(70), 43u));
    CHECK(NamesQuest(relations.equal_range(70), 44u));

    // One it does not hand out at all.
    CHECK_FALSE(NamesQuest(relations.equal_range(70), 45u));
}

TEST_CASE("quest bond: the range does not leak into a neighbouring entry")
{
    Relations relations;
    relations.insert(std::make_pair(69u, 41u));
    relations.insert(std::make_pair(70u, 42u));
    relations.insert(std::make_pair(71u, 43u));

    // The quest is in the map, but on somebody else's entry.
    CHECK_FALSE(NamesQuest(relations.equal_range(70), 41u));
    CHECK_FALSE(NamesQuest(relations.equal_range(70), 43u));
    CHECK(NamesQuest(relations.equal_range(70), 42u));
}

TEST_CASE("quest bond: a hand-in is pending until the reward is paid")
{
    // Still being worked on, and ready to turn in: both leave business here.
    CHECK(IsHandInPending(QUEST_STATUS_INCOMPLETE, false));
    CHECK(IsHandInPending(QUEST_STATUS_COMPLETE, false));

    // Paid for. Nothing to come back for, even while the log still reads
    // complete for a repeatable.
    CHECK_FALSE(IsHandInPending(QUEST_STATUS_COMPLETE, true));
    CHECK_FALSE(IsHandInPending(QUEST_STATUS_INCOMPLETE, true));

    // Never taken, or dropped.
    CHECK_FALSE(IsHandInPending(QUEST_STATUS_NONE, false));
    CHECK_FALSE(IsHandInPending(QUEST_STATUS_FAILED, false));
    CHECK_FALSE(IsHandInPending(QUEST_STATUS_AVAILABLE, false));
}

TEST_CASE("gameobject template: the quest id is read from the type's own field")
{
    GameObjectInfo chest = Templated(GAMEOBJECT_TYPE_CHEST);
    chest.chest.questId = 8;
    CHECK(chest.GetQuestId() == 8);

    GameObjectInfo generic = Templated(GAMEOBJECT_TYPE_GENERIC);
    generic._generic.questID = 9;
    CHECK(generic.GetQuestId() == 9);

    GameObjectInfo focus = Templated(GAMEOBJECT_TYPE_SPELL_FOCUS);
    focus.spellFocus.questID = 10;
    CHECK(focus.GetQuestId() == 10);

    GameObjectInfo goober = Templated(GAMEOBJECT_TYPE_GOOBER);
    goober.goober.questId = 11;
    CHECK(goober.GetQuestId() == 11);
}

TEST_CASE("gameobject template: the questgiver carries its quests in the relation maps")
{
    // Its data block has no quest field at all -- what it hands out is the
    // relation map, which is why the loader asks a different question for it.
    GameObjectInfo questgiver = Templated(GAMEOBJECT_TYPE_QUESTGIVER);

    CHECK(questgiver.GetQuestId() == 0);
}

TEST_CASE("gameobject template: no other type names a quest")
{
    for (uint32 type = 0; type < MAX_GAMEOBJECT_TYPE; ++type)
    {
        // Every word of the data block set, so a type reading the wrong field
        // would come back with something.
        GameObjectInfo info = Templated(type);
        for (uint32& word : info.raw.data)
        {
            word = 0xFFFFFFFF;
        }

        bool const carriesOne = type == GAMEOBJECT_TYPE_CHEST ||
                                type == GAMEOBJECT_TYPE_GENERIC ||
                                type == GAMEOBJECT_TYPE_SPELL_FOCUS ||
                                type == GAMEOBJECT_TYPE_GOOBER;

        CHECK((info.GetQuestId() != 0) == carriesOne);
    }
}
