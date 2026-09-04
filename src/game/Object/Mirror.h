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

/**
 * The copy of an object that a client keeps, and what has moved in it.
 *
 * The client calls this block a mirror, and that is what it is: a run of dwords
 * whose meaning comes from the field table, plus one bit per dword saying
 * whether the copy out there is still current.
 *
 * It is a store and nothing more. It does not know what a health point is, who
 * may be told about one, or when a packet goes out -- those belong to the field
 * table, the audience and the map's tick respectively.
 */
class Mirror
{
    public:
        Mirror() = default;
        ~Mirror();

        Mirror(Mirror const&) = delete;
        Mirror& operator=(Mirror const&) = delete;

        /// Size the block for a class of object. The width comes from the field
        /// table, so a class does not get to declare its own.
        void Open(uint8 typeId);

        bool IsOpen() const { return m_values != nullptr; }
        uint16 Count() const { return m_count; }

        uint32 Read(uint16 index) const { return m_values[index]; }
        float ReadFloat(uint16 index) const;

        /// The address of a dword, for the two that make a guid.
        uint32 const* At(uint16 index) const { return m_values + index; }

        /// Store a value. Answers whether it was different from what was there,
        /// which is what decides whether anyone needs telling.
        bool Write(uint16 index, uint32 value);
        bool WriteFloat(uint16 index, float value);

        /// Mark a dword as needing to go out again although it did not change.
        void Touch(uint16 index) { m_dirty[index] = true; }

        bool Changed(uint16 index) const { return m_dirty[index]; }

        /// Every copy out there is current again.
        void Settle();

    private:
        uint32* m_values = nullptr;
        std::vector<bool> m_dirty;
        uint16 m_count = 0;
};
