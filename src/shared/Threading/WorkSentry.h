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

/**
 * A watch kept over one named piece of work that has promised to touch nothing
 * but what it owns.
 *
 * The sentry stands for as long as the work runs, and the few places that reach
 * past a single owner tell it so. The first time a given piece of work reaches a
 * given thing, it is written to the log; after that the pair is remembered and
 * kept quiet, because a broken promise repeats every tick and one line per
 * offence is what is wanted.
 *
 * Nothing is refused and nothing throws. The sentry only reports: stopping the
 * server over a breach is worse than the breach.
 *
 * One watch per thread, and it nests: an inner watch stands until it goes out of
 * scope, then the outer one is back.
 */
class WorkSentry
{
    public:

        /// Stands watch over `work` for as long as this object lives.
        explicit WorkSentry(char const* work);
        ~WorkSentry();

        WorkSentry(WorkSentry const&) = delete;
        WorkSentry& operator=(WorkSentry const&) = delete;

        /// The work under watch on this thread, or nothing when none is.
        static char const* Watched() { return s_watched; }

        /// Says that whatever is under watch has reached `what`. Costs a single
        /// read of a thread-local when no watch is standing, which is nearly
        /// always.
        static void Reached(char const* what)
        {
            if (s_watched)
            {
                Report(s_watched, what);
            }
        }

        /// How many distinct offences have been told since the server came up.
        static uint32 Offences();

        /// Forgets what has been told, so the next offence is told again.
        static void Forget();

    private:

        static void Report(char const* work, char const* what);

        static thread_local char const* s_watched;

        char const* m_outer;
};
