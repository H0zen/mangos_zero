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

class DungeonBinds
{
    public:

        explicit DungeonBinds(Player& who) : m_owner(who) {}
        ~DungeonBinds();

        void Load(QueryResult* result);

        DungeonHold* To(uint32 mapId);

        DungeonHolds& All() { return m_held; }
        DungeonHolds const& All() const { return m_held; }

        void Release(uint32 mapId, bool unload = false);
        void Release(DungeonHolds::iterator& itr, bool unload = false);

        DungeonHold* BindTo(DungeonPersistentState* state, bool permanent, bool load = false);

        DungeonPersistentState* CopyForHimOrHisGroup(uint32 mapId);

        void TellRaidInfo();
        void TellSaved();

        void Reset(InstanceResetMethod method);

        bool StillWelcome() const { return m_stillWelcome; }
        void StillWelcome(bool welcome) { m_stillWelcome = welcome; }

    private:

        Player& m_owner;

        DungeonHolds m_held;
        bool m_stillWelcome = true;
};
