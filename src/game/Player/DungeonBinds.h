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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#pragma once

#include "DungeonHold.h"
#include "SharedDefines.h"

class Player;
class QueryResult;

/**
 * The dungeons a character is held to, one to a map.
 *
 * Being held to a dungeon means going back to the same copy of it: the same
 * corpses on the floor, the same doors open, the same bosses dead. He can be
 * held to one copy of each map and no more, so the holds are kept by map rather
 * than by copy.
 *
 * A hold is shared with the copy it names -- the copy counts who is held to it,
 * and giving up the hold tells it so. That is why giving one up is not simply
 * erasing a row.
 *
 * Alongside them is whether the copy he is standing in still has him. A group
 * change or a reset can take him off its roll while he is still inside; he is
 * then sent home at the next chance, and until then this is false.
 */
class DungeonBinds
{
    public:

        explicit DungeonBinds(Player& who) : m_owner(who) {}
        ~DungeonBinds();

        /// Reads back the holds saved for him.
        void Load(QueryResult* result);

        /// The hold on that map, or nothing.
        DungeonHold* To(uint32 mapId);

        DungeonHolds& All() { return m_held; }
        DungeonHolds const& All() const { return m_held; }

        /// Gives up the hold on that map. `unload` leaves the row alone, for
        /// paths that are taking the character or the copy apart anyway.
        void Release(uint32 mapId, bool unload = false);
        void Release(DungeonHolds::iterator& itr, bool unload = false);

        /// Takes a hold on the given copy.
        DungeonHold* BindTo(DungeonPersistentState* state, bool permanent, bool load = false);

        /// The copy he would go back to for that map, his own or his group's.
        DungeonPersistentState* CopyForHimOrHisGroup(uint32 mapId);

        void TellRaidInfo();
        void TellSaved();

        void Reset(InstanceResetMethod method);

        /// Whether the copy he is standing in still has him on its roll.
        bool StillWelcome() const { return m_stillWelcome; }
        void StillWelcome(bool welcome) { m_stillWelcome = welcome; }

    private:

        Player& m_owner;

        DungeonHolds m_held;
        bool m_stillWelcome = true;
};
