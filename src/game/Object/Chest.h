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

#include <ctime>

class Player;

/// How long a chest stands about after the last thing is taken out of it.
uint32 const CHEST_LINGER = 5 * MINUTE;

/**
 * What becomes of a chest once players have started taking from it.
 *
 * Two things outlive the opening. It waits a while before vanishing, so that a
 * group finishing a fight is not robbed of the box in front of them. And it
 * remembers who has already learned something by opening it: a lock picked, a
 * vein mined, a herb gathered teaches a player once and never again, however
 * many times they come back to the same one.
 */
class Chest
{
    public:
        /// It is empty and should go at this moment, not before.
        void EmptyAt(time_t when) { m_emptyAt = when; }

        /// @return true when that moment has come and it was ever set.
        bool IsEmptyingDue(time_t now) const { return m_emptyAt != 0 && m_emptyAt <= now; }

        bool HasTaught(ObjectGuid const& learner) const
        {
            return m_taught.find(learner) != m_taught.end();
        }

        void Taught(ObjectGuid const& learner) { m_taught.insert(learner); }

        /// Everyone may learn from it again, which is what a fresh one is.
        void ForgetLearners() { m_taught.clear(); }

    private:
        GuidSet m_taught;
        time_t  m_emptyAt = 0;
};
