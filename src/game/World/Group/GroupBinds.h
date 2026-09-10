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

class GroupBinds
{
    public:

        explicit GroupBinds(Group& whose) : m_owner(whose) {}
        ~GroupBinds();

        DungeonHold* To(uint32 mapId);

        DungeonHolds& All() { return m_held; }
        DungeonHolds const& All() const { return m_held; }

        DungeonHold* BindTo(DungeonPersistentState* state, bool permanent, bool load = false);

        void Release(uint32 mapId, bool unload = false);

        void ReleasePermanent();

        void Reset(InstanceResetMethod method, Player* tellHim);

    private:

        Group& m_owner;

        DungeonHolds m_held;
};
