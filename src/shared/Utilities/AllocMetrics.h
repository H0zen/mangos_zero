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

#ifndef MANGOS_H_ALLOCMETRICS
#define MANGOS_H_ALLOCMETRICS

/**
 * @file AllocMetrics.h
 * @brief Heap traffic counters, compiled out unless ALLOC_METRICS is on.
 *
 * The point of these is one number: how many times does one spell cast reach the
 * allocator. Everything the optimization plan claims is a before/after on that
 * number, and a claim nobody can reproduce is not a measurement.
 *
 * Two kinds of counter, and they answer different questions:
 *
 *   `Total()` comes from the global operator new/delete override in
 *   AllocMetricsHooks.cpp and counts EVERY allocation on the calling thread,
 *   including the ones nobody thought to instrument. It is the honest number.
 *
 *   `Site()` counts named places -- the event queue, the packet encoder, the
 *   spell object. It is what says WHERE the total went, and it is what proves a
 *   given stage of the plan actually removed what it claimed to.
 *
 * Everything is thread-local and unsynchronised on purpose. A world update runs
 * on one thread; an atomic per allocation would make the instrument change what
 * it measures. Read the counters from the same thread that ran the work.
 *
 * With ALLOC_METRICS off, every symbol here is an empty inline and the whole
 * header costs nothing -- no atomics, no thread-local storage, no branch.
 */

#include "Platform/Define.h"

namespace AllocMetrics
{
    /// Named allocation sites. Keep in step with SiteName().
    enum Site
    {
        SITE_EVENT_QUEUE    = 0,    ///< a node inserted into EventProcessor's scheduled map
        SITE_EVENT_TICK     = 1,    ///< an event handed to the per-tick lane (no node)
        SITE_PACKET_ENCODE  = 2,    ///< a wire frame built in PacketCodec::Encode
        SITE_SPELL          = 3,    ///< a Spell object constructed
        SITE_TARGET_LIST    = 4,    ///< a spell target container had to grow
        SITE_MAX            = 5
    };

    inline char const* SiteName(Site s)
    {
        switch (s)
        {
            case SITE_EVENT_QUEUE:   return "event-queue";
            case SITE_EVENT_TICK:    return "event-tick";
            case SITE_PACKET_ENCODE: return "packet-encode";
            case SITE_SPELL:         return "spell";
            case SITE_TARGET_LIST:   return "target-list";
            default:                 return "?";
        }
    }

#ifdef ALLOC_METRICS

    // C++17 inline variables: no separate definition, one object per thread.
    inline thread_local uint64 t_allocations = 0;
    inline thread_local uint64 t_frees       = 0;
    inline thread_local uint64 t_site[SITE_MAX] = {};

    /// Called from the global operator new/delete override.
    inline void CountAlloc() { ++t_allocations; }
    inline void CountFree()  { ++t_frees; }

    inline void Count(Site s) { ++t_site[s]; }

    inline uint64 Total()      { return t_allocations; }
    inline uint64 TotalFrees() { return t_frees; }
    inline uint64 SiteTotal(Site s) { return t_site[s]; }

#else

    inline void CountAlloc() {}
    inline void CountFree()  {}
    inline void Count(Site) {}

    inline uint64 Total()      { return 0; }
    inline uint64 TotalFrees() { return 0; }
    inline uint64 SiteTotal(Site) { return 0; }

#endif

    /**
     * @brief A snapshot of the counters, and the delta since one was taken.
     *
     * Held by Spell across its whole life, which is why it must stay this small:
     * with metrics off it is an empty struct and every method is a no-op, so the
     * member costs nothing in a normal build.
     */
    struct Snapshot
    {
#ifdef ALLOC_METRICS
        uint64 allocations;
        uint64 frees;
        uint64 site[SITE_MAX];
        bool   taken;

        Snapshot() : allocations(0), frees(0), site(), taken(false) {}

        void Take()
        {
            allocations = t_allocations;
            frees       = t_frees;
            for (int i = 0; i < SITE_MAX; ++i)
            {
                site[i] = t_site[i];
            }
            taken = true;
        }

        uint64 AllocationsSince() const { return t_allocations - allocations; }
        uint64 FreesSince()       const { return t_frees - frees; }
        uint64 SiteSince(Site s)  const { return t_site[s] - site[s]; }
        bool   Taken()            const { return taken; }
#else
        void Take() {}
        uint64 AllocationsSince() const { return 0; }
        uint64 FreesSince()       const { return 0; }
        uint64 SiteSince(Site)    const { return 0; }
        bool   Taken()            const { return false; }
#endif
    };
}

#endif
