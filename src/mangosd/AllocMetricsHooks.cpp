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

/**
 * @file AllocMetricsHooks.cpp
 * @brief Global operator new/delete, counting, for ALLOC_METRICS builds only.
 *
 * This whole translation unit compiles to nothing unless ALLOC_METRICS is on. It
 * lives in the executable rather than in a static library on purpose: a
 * replacement operator new sitting in an archive is only pulled in if the linker
 * happens to need that object for some other symbol, and when it is not pulled
 * in the build still succeeds -- with the counters reading zero and no
 * indication why. In the executable's own source list it is always linked.
 *
 * Why replace the global operator rather than instrument the interesting call
 * sites and be done: the named counters in AllocMetrics.h say where the traffic
 * we already know about goes, and they cannot say anything at all about the
 * traffic we do not. The number that matters -- "one cast costs about two
 * hundred allocations" -- is only trustworthy if it was not assembled from a
 * list somebody wrote by reading the code. This is the part that cannot be
 * fooled by an oversight.
 *
 * The counters are thread-local and unsynchronised, so the hook is a couple of
 * instructions and does not serialise threads that would otherwise not meet.
 * That also means each thread reports its own traffic: read them on the thread
 * that did the work.
 */

#include "Utilities/AllocMetrics.h"

#ifdef ALLOC_METRICS

#include <cstdlib>
#include <new>

namespace
{
    // malloc/free rather than the default operators, which is the point: calling
    // ::operator new here would recurse into this very function.
    inline void* CountedAlloc(std::size_t size)
    {
        // A zero-sized request must still return a distinct pointer.
        void* p = std::malloc(size ? size : 1);
        if (p)
        {
            AllocMetrics::CountAlloc();
        }
        return p;
    }

    inline void CountedFree(void* p)
    {
        if (p)
        {
            AllocMetrics::CountFree();
            std::free(p);
        }
    }
}

void* operator new(std::size_t size)
{
    void* p = CountedAlloc(size);
    if (!p)
    {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t size)
{
    void* p = CountedAlloc(size);
    if (!p)
    {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new(std::size_t size, std::nothrow_t const&) noexcept   { return CountedAlloc(size); }
void* operator new[](std::size_t size, std::nothrow_t const&) noexcept { return CountedAlloc(size); }

void operator delete(void* p) noexcept   { CountedFree(p); }
void operator delete[](void* p) noexcept { CountedFree(p); }

void operator delete(void* p, std::nothrow_t const&) noexcept   { CountedFree(p); }
void operator delete[](void* p, std::nothrow_t const&) noexcept { CountedFree(p); }

// Sized deallocation (C++14). Without these the compiler emits calls to them for
// types whose size it knows, they resolve to the DEFAULT implementation, and the
// free count silently undercounts against the allocation count -- which reads
// exactly like a leak that is not there.
void operator delete(void* p, std::size_t) noexcept   { CountedFree(p); }
void operator delete[](void* p, std::size_t) noexcept { CountedFree(p); }

#endif // ALLOC_METRICS
