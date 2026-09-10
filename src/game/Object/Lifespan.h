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

class Lifespan
{
    public:

        void Grant(uint32 ms)
        {
            m_granted = ms;
            m_left = ms;
        }

        void Restore() { m_left = m_granted; }

        void Clear()
        {
            m_granted = 0;
            m_left = 0;
        }

        bool Bounded() const { return m_granted > 0; }

        uint32 Left() const { return m_left; }
        uint32 Granted() const { return m_granted; }

        bool Spent() const { return Bounded() && m_left == 0; }

        bool Spend(uint32 elapsed)
        {
            if (!Bounded())
            {
                return false;
            }

            m_left = elapsed >= m_left ? 0 : m_left - elapsed;

            return m_left == 0;
        }

        void Shorten(int32 ms)
        {
            if (!Bounded())
            {
                return;
            }

            if (ms >= 0)
            {
                m_left = uint32(ms) >= m_left ? 0 : m_left - uint32(ms);
                return;
            }

            const uint32 added = uint32(-ms);
            m_left = m_left + added > m_granted ? m_granted : m_left + added;
        }

    private:

        uint32 m_granted = 0;
        uint32 m_left = 0;
};
