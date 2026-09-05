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

#include <unordered_set>

/**
 * Trap models the client will not play unless it is told to.
 *
 * Most traps need nothing said: the client plays whatever the model does when
 * the spell lands. A few will stand there doing nothing -- the lava that opens
 * under Onyxia's lair, the fissures Heigan cracks -- unless the server sends
 * the animation itself.
 *
 * Which models those are is data, from `gameobject_trap_anim`, and it is keyed
 * by the model because that is what the shortcoming belongs to.
 */
class AnimatedTraps
{
    public:
        void Add(uint32 displayId) { m_told.insert(displayId); }
        void Clear() { m_told.clear(); }

        /// Does a trap wearing this model have to be told to play it?
        bool NeedTelling(uint32 displayId) const { return m_told.count(displayId) != 0; }

        std::size_t Count() const { return m_told.size(); }

    private:
        std::unordered_set<uint32> m_told;
};

/// The one the world loads at start-up.
extern AnimatedTraps sAnimatedTraps;

/// Reads `gameobject_trap_anim` into it.
void LoadAnimatedTraps();
