/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
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

#ifndef MANGOS_H_SMALLVECTOR
#define MANGOS_H_SMALLVECTOR

/**
 * @file SmallVector.h
 * @brief A contiguous sequence with room for N elements inside itself.
 *
 * For the many places that hold a handful of small records and were reaching for
 * std::list because it was there. A spell's target list is the motivating case:
 * one element in the overwhelming majority of casts, and std::list charged a
 * separate allocation for it, plus a pointer chase on each of the four to six
 * walks a single cast performs over that list.
 *
 * Deliberately not a general-purpose container:
 *
 *   TRIVIALLY COPYABLE ELEMENTS ONLY. Growth is a memcpy and clear() just resets
 *   the size. That is what keeps this small enough to be obviously correct
 *   rather than merely tested; anything with a destructor or a non-trivial move
 *   belongs in std::vector.
 *
 *   NO erase(), NO insert(). Nothing needed them, and both are where a
 *   hand-rolled container earns its reputation.
 *
 * It also carries a guard the std::list it replaces made unnecessary. Code that
 * walks these containers hands the ADDRESS of an element to game code -- and
 * game code can do anything, including adding another element. With list nodes
 * that was harmless. With a contiguous buffer a growth would move everything and
 * leave the callee holding a dangling pointer, on a path that only shows up
 * under load. Walk() asserts instead: while a walk is in progress, growing is a
 * hard error at the moment it happens rather than corruption later.
 */

#include "Platform/Define.h"
#include "Utilities/Errors.h"
#include "Utilities/AllocMetrics.h"

#include <cstring>
#include <new>
#include <type_traits>

template<class T, size_t N>
class SmallVector
{
        static_assert(std::is_trivially_copyable<T>::value,
            "SmallVector grows by memcpy; give it a trivially copyable element");
        static_assert(std::is_trivially_destructible<T>::value,
            "SmallVector::clear() does not run destructors");

    public:

        typedef T                value_type;
        typedef T*               iterator;
        typedef T const*         const_iterator;

        SmallVector() : m_data(m_inline), m_size(0), m_capacity(N), m_walkers(0) {}

        ~SmallVector()
        {
            Release();
        }

        // Copying one would have to decide what to do about the inline buffer
        // and about an in-progress walk. Nothing needs it.
        SmallVector(SmallVector const&) = delete;
        SmallVector& operator=(SmallVector const&) = delete;

        void push_back(T const& value)
        {
            // See the Walk note above: this is the growth that would have
            // invalidated a pointer somebody is holding right now.
            MANGOS_ASSERT(m_walkers == 0);

            if (m_size == m_capacity)
            {
                Grow(m_capacity * 2);
            }

            m_data[m_size++] = value;
        }

        void reserve(size_t wanted)
        {
            MANGOS_ASSERT(m_walkers == 0);

            if (wanted > m_capacity)
            {
                Grow(wanted);
            }
        }

        /// Keeps the buffer. These containers are refilled, not thrown away.
        void clear() { m_size = 0; }

        bool   empty() const { return m_size == 0; }
        size_t size()  const { return m_size; }

        T&       operator[](size_t i)       { return m_data[i]; }
        T const& operator[](size_t i) const { return m_data[i]; }

        iterator       begin()       { return m_data; }
        iterator       end()         { return m_data + m_size; }
        const_iterator begin() const { return m_data; }
        const_iterator end()   const { return m_data + m_size; }

        /**
         * @brief Declares that element addresses are being handed out.
         *
         * Hold one of these across any loop that passes `&element` to code it
         * does not control. Growing the container while one is alive is an
         * assertion failure, which is the whole point: the alternative is a
         * dangling pointer inside a callee, discovered much later and somewhere
         * else entirely.
         */
        class Walk
        {
            public:
                explicit Walk(SmallVector& v) : m_v(v) { ++m_v.m_walkers; }
                ~Walk() { --m_v.m_walkers; }
                Walk(Walk const&) = delete;
                Walk& operator=(Walk const&) = delete;
            private:
                SmallVector& m_v;
        };

    private:

        // Walk is a member class and already has access to these; it needs no
        // friend declaration.

        void Grow(size_t wanted)
        {
            size_t const capacity = wanted > N ? wanted : N + 1;

            AllocMetrics::Count(AllocMetrics::SITE_TARGET_LIST);
            T* fresh = static_cast<T*>(::operator new(capacity * sizeof(T)));

            if (m_size)
            {
                std::memcpy(fresh, m_data, m_size * sizeof(T));
            }

            Release();

            m_data = fresh;
            m_capacity = capacity;
        }

        void Release()
        {
            if (m_data != m_inline)
            {
                ::operator delete(m_data);
            }
        }

        T*     m_data;                  ///< m_inline, or the heap buffer
        T      m_inline[N];
        size_t m_size;
        size_t m_capacity;

        /// Number of live Walk objects. Never read in a release decision -- it
        /// exists only to make an unsafe growth loud.
        int    m_walkers;
};

#endif
