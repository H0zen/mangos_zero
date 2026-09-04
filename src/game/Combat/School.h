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

// The kind of a blow, and a set of kinds.
//
// A blow has exactly ONE school. Captures of live 1.8 through 1.12 servers carry
// a single kind in every attacker-state update, and a spell's damage report has
// no room for a second one. The five slots the client can parse are capacity
// nobody fills, so nothing here is a collection.
//
// Coverage is the opposite case and the reason SchoolSet exists: an immunity, an
// absorbing shield or a resistance-piercing effect speaks about several schools
// at once, and there the set is the nature of the thing rather than an accident.
//
// So: School answers "what is this blow", SchoolSet answers "what does this
// cover". A mask standing in for a single school is the mistake this pair
// exists to prevent -- it forces a lossy "first bit set" step at every use.

#include "Platform/Define.h"

namespace combat
{
    /// Physical is a school like any other; what differs is that armour, rather
    /// than resistance, is what stands in its way.
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

    /// The index the wire carries for a school.
    constexpr uint32 WireIndex(School school)
    {
        return static_cast<uint32>(school);
    }

    /// The school a wire index names. An index out of range is physical, which
    /// is what an absent or malformed school has always meant.
    constexpr School SchoolFromIndex(uint32 index)
    {
        return index < SCHOOL_COUNT ? static_cast<School>(index) : School::Physical;
    }

    /**
     * @brief The schools something covers.
     *
     * Bit per school, in the same order the client's resistance table uses, so
     * ToMask() and FromMask() are the wire's own representation and need no
     * translation table.
     */
    class SchoolSet
    {
        public:

            constexpr SchoolSet() = default;

            constexpr explicit SchoolSet(School school)
                : m_bits(BitOf(school))
            {
            }

            /// Reads a mask that arrives from spell data.
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

    /**
     * @brief The single school a mask names.
     *
     * Spell data stores a mask even where one school is meant. This is the one
     * place that narrowing happens, so the loss is visible rather than repeated
     * at every call site. The lowest bit wins, which is the order the client's
     * own table is indexed in.
     */
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
