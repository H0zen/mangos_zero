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

#include "AnimatedTraps.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"

AnimatedTraps sAnimatedTraps;

void LoadAnimatedTraps()
{
    sAnimatedTraps.Clear();

    QueryResult* result = WorldDatabase.Query("SELECT `displayId` FROM `gameobject_trap_anim`");

    if (!result)
    {
        sLog.outString(">> Loaded 0 trap models that need telling; table `gameobject_trap_anim` is empty");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        sAnimatedTraps.Add(fields[0].GetUInt32());
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u trap model(s) that need telling to play", uint32(sAnimatedTraps.Count()));
    sLog.outString();
}
