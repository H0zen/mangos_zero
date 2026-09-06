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

class Group;
class Player;

/**
 * The dungeons a group is held to, one to a map.
 *
 * A group takes its holds so that everyone who walks in walks into the same
 * copy, and keeps them under the leader's name -- which is why a change of
 * leader moves the rows rather than rewriting them. A permanent group hold
 * stands only while the leader holds the same copy permanently himself.
 *
 * A battleground party never holds anything: its copy is gone when the fight
 * is, so nothing is written down.
 */
class GroupBinds
{
    public:

        explicit GroupBinds(Group& whose) : m_owner(whose) {}
        ~GroupBinds();

        /// The hold on that map, or nothing.
        DungeonHold* To(uint32 mapId);

        DungeonHolds& All() { return m_held; }
        DungeonHolds const& All() const { return m_held; }

        /// Takes a hold on the given copy.
        DungeonHold* BindTo(DungeonPersistentState* state, bool permanent, bool load = false);

        /// Gives up the hold on that map. `unload` leaves the row alone, for
        /// paths that are taking the group or the copy apart anyway.
        void Release(uint32 mapId, bool unload = false);

        /// Drops every permanent hold, for when the leadership passes on.
        void ReleasePermanent();

        void Reset(InstanceResetMethod method, Player* tellHim);

    private:

        Group& m_owner;

        DungeonHolds m_held;
};
