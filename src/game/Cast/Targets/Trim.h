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
#include "Utilities/Util.h"

#include <list>

namespace cast
{

    template<class T>
    void KeepAtMost(std::list<T*>& found, uint32 cap, T* chosen, bool keepsTheChosen)
    {
        if (cap == 0 || found.size() <= cap)
        {
            return;
        }

        uint32 removedChosen = 0;
        for (auto one = found.begin(), next = one; one != found.end(); one = next)
        {
            ++next;
            if (*one != nullptr && *one == chosen)
            {
                found.erase(one);
                removedChosen = 1;
            }
        }

        while (found.size() > cap - removedChosen)
        {
            uint32 draw = urand(0, found.size() - 1);
            for (auto one = found.begin(); one != found.end(); ++one, --draw)
            {
                if (*one == nullptr)
                {
                    continue;
                }

                if (draw == 0)
                {
                    found.erase(one);
                    break;
                }
            }
        }

        if (keepsTheChosen && removedChosen != 0 && chosen != nullptr)
        {
            found.push_back(chosen);
        }
    }
}
