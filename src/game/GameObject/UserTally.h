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

#include "ObjectGuid.h"

/**
 * A count of how often something has been used, and by whom.
 *
 * Most objects only need the count: a vein gives up so many ores, a fishing
 * hole so many casts, a trap so many charges, and when the count reaches what
 * the template allows the object is done.
 *
 * A summoning ritual needs the names as well. It completes when enough
 * different players have joined it, so the same one clicking twice must not
 * count twice, and the spell is cast by whoever laid the circle down rather
 * than by whoever happened to click last. A goober needs them for the same
 * reason at the other end: everyone who touched it is paid at once when it
 * shuts.
 */
class UserTally
{
    public:
        /// One more use, by nobody worth remembering.
        void Used() { ++m_uses; }

        /// One more use, by someone the object has to keep track of.
        void UsedBy(ObjectGuid const& who)
        {
            Used();

            if (!m_first)
            {
                m_first = who;
            }

            m_users.insert(who);
        }

        uint32 Uses() const { return m_uses; }
        uint32 Distinct() const { return static_cast<uint32>(m_users.size()); }

        /// Whoever used it first, which for a ritual is whoever laid it down.
        ObjectGuid const& First() const { return m_first; }
        GuidSet const& Everyone() const { return m_users; }

        /// The object is as good as untouched again.
        void Forget()
        {
            m_uses = 0;
            m_first.Clear();
            m_users.clear();
        }

    private:
        uint32     m_uses = 0;
        ObjectGuid m_first;
        GuidSet    m_users;
};
