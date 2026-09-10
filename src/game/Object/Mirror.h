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

#include <vector>

class Mirror
{
    public:
        Mirror() = default;
        ~Mirror();

        Mirror(Mirror const&) = delete;
        Mirror& operator=(Mirror const&) = delete;

        void Open(uint8 typeId);

        bool IsOpen() const { return m_values != nullptr; }
        uint16 Count() const { return m_count; }

        uint16 Blocks() const { return uint16(m_dirty.size()); }

        uint32 Read(uint16 index) const { return m_values[index]; }
        float ReadFloat(uint16 index) const;
        uint64 ReadPair(uint16 index) const;

        bool Write(uint16 index, uint32 value);
        bool WriteFloat(uint16 index, float value);
        bool WritePair(uint16 index, uint64 value);

        void Touch(uint16 index) { m_dirty[index >> 5] |= 1u << (index & 31); }

        uint32 const* Dirty() const { return m_dirty.data(); }

        void Settle();

    private:
        uint32* m_values = nullptr;
        std::vector<uint32> m_dirty;
        uint16 m_count = 0;
};
