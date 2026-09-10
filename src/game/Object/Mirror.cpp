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

#include "Mirror.h"

#include "FieldTable.h"

#include <algorithm>
#include <cstring>

Mirror::~Mirror()
{
    delete[] m_values;
}

void Mirror::Open(uint8 typeId)
{
    MANGOS_ASSERT(!m_values);

    Fields::Table const& table = Fields::For(typeId);

    m_count = table.count;
    m_values = new uint32[m_count];
    std::memset(m_values, 0, m_count * sizeof(uint32));
    m_dirty.assign(table.blocks, 0);
}

float Mirror::ReadFloat(uint16 index) const
{

    float value;
    std::memcpy(&value, m_values + index, sizeof(value));
    return value;
}

uint64 Mirror::ReadPair(uint16 index) const
{
    uint64 value;
    std::memcpy(&value, m_values + index, sizeof(value));
    return value;
}

bool Mirror::Write(uint16 index, uint32 value)
{
    if (m_values[index] == value)
    {
        return false;
    }

    m_values[index] = value;
    Touch(index);
    return true;
}

bool Mirror::WriteFloat(uint16 index, float value)
{
    uint32 bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return Write(index, bits);
}

bool Mirror::WritePair(uint16 index, uint64 value)
{
    bool const low = Write(index, uint32(value));
    bool const high = Write(index + 1, uint32(value >> 32));
    return low || high;
}

void Mirror::Settle()
{
    if (m_values)
    {
        std::fill(m_dirty.begin(), m_dirty.end(), 0u);
    }
}
