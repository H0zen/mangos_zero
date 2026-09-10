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

namespace combat
{

    enum class School : uint8
    {
        Physical = 0,
        Holy     = 1,
        Fire     = 2,
        Nature   = 3,
        Frost    = 4,
        Shadow   = 5,
        Arcane   = 6,
    };

    constexpr uint8 SCHOOL_COUNT = 7;

    constexpr bool IsPhysical(School school)
    {
        return school == School::Physical;
    }

    constexpr uint32 WireIndex(School school)
    {
        return static_cast<uint32>(school);
    }

    constexpr School SchoolFromIndex(uint32 index)
    {
        return index < SCHOOL_COUNT ? static_cast<School>(index) : School::Physical;
    }

    class SchoolSet
    {
        public:

            constexpr SchoolSet() = default;

            constexpr explicit SchoolSet(School school)
                : m_bits(BitOf(school))
            {
            }

            static constexpr SchoolSet FromMask(uint32 mask)
            {
                return SchoolSet(static_cast<uint8>(mask & ALL_BITS));
            }

            static constexpr SchoolSet All()
            {
                return SchoolSet(ALL_BITS);
            }

            static constexpr SchoolSet None()
            {
                return SchoolSet();
            }

            constexpr uint32 ToMask() const { return m_bits; }

            constexpr bool Contains(School school) const
            {
                return (m_bits & BitOf(school)) != 0;
            }

            constexpr bool Empty() const { return m_bits == 0; }

            SchoolSet& Add(School school)
            {
                m_bits = static_cast<uint8>(m_bits | BitOf(school));
                return *this;
            }

            SchoolSet& Remove(School school)
            {
                m_bits = static_cast<uint8>(m_bits & ~BitOf(school));
                return *this;
            }

            constexpr bool operator==(const SchoolSet& other) const
            {
                return m_bits == other.m_bits;
            }

            constexpr bool operator!=(const SchoolSet& other) const
            {
                return m_bits != other.m_bits;
            }

        private:

            static constexpr uint8 ALL_BITS = 0x7F;

            constexpr explicit SchoolSet(uint8 bits) : m_bits(bits) {}

            static constexpr uint8 BitOf(School school)
            {
                return static_cast<uint8>(1u << static_cast<uint8>(school));
            }

            uint8 m_bits = 0;
    };

    constexpr School FirstSchoolIn(uint32 mask)
    {
        return (mask & (1u << 0)) ? School::Physical :
               (mask & (1u << 1)) ? School::Holy     :
               (mask & (1u << 2)) ? School::Fire     :
               (mask & (1u << 3)) ? School::Nature   :
               (mask & (1u << 4)) ? School::Frost    :
               (mask & (1u << 5)) ? School::Shadow   :
               (mask & (1u << 6)) ? School::Arcane   :
                                    School::Physical;
    }
}
