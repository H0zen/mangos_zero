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
#include "Geometry/Vector2.h"
#include "Geometry/Vector3.h"
#include "Geometry/Vector4.h"

namespace Movement
{
    using Geometry::Vector2;
    using Geometry::Vector3;
    using Geometry::Vector4;

    inline uint32 SecToMS(float sec)
    {
        return static_cast<uint32>(sec * 1000.f);
    }

    inline float MSToSec(uint32 ms)
    {
        return ms / 1000.f;
    }

    template<class T, T limit>

    class counter
    {
        public:

            counter()
            {
                init();
            }

            void Increase()
            {
                if (m_counter == limit)
                {
                    init();
                }
                else
                {
                    ++m_counter;
                }
            }

            T NewId()
            {
                Increase(); return m_counter;
            }

            T getCurrent() const { return m_counter;}

        private:

            void init()
            {
                m_counter = 0;
            }

            T m_counter;
    };

    typedef counter<uint32, 0xFFFFFFFF> UInt32Counter;
}
