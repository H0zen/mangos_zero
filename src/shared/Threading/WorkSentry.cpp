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

#include "WorkSentry.h"

#include "Log.h"

#include <mutex>
#include <set>
#include <string>

thread_local char const* WorkSentry::s_watched = nullptr;

namespace
{
    std::mutex g_told;
    std::set<std::string> g_offences;
}

WorkSentry::WorkSentry(char const* work) : m_outer(s_watched)
{
    s_watched = work;
}

WorkSentry::~WorkSentry()
{
    s_watched = m_outer;
}

void WorkSentry::Report(char const* work, char const* what)
{
    std::string offence(work ? work : "unnamed work");
    offence += " reached ";
    offence += what;

    {
        std::lock_guard<std::mutex> lock(g_told);
        if (!g_offences.insert(offence).second)
        {
            return;
        }
    }

    sLog.outError("SENTRY: %s, which it promised to keep clear of.", offence.c_str());
}

uint32 WorkSentry::Offences()
{
    std::lock_guard<std::mutex> lock(g_told);
    return uint32(g_offences.size());
}

void WorkSentry::Forget()
{
    std::lock_guard<std::mutex> lock(g_told);
    g_offences.clear();
}
