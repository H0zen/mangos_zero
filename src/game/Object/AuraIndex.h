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

#include "SpellAuraDefines.h"

#include <cstddef>

#include <algorithm>
#include <memory>
#include <vector>

class Aura;

namespace auras
{
    /**
     * The auras of one unit, grouped by aura type.
     *
     * A unit is asked "what are your auras of type T" constantly while carrying
     * auras of only a few types at once, so the type is the key and only the
     * types actually present cost anything.
     *
     * The container is built around one demand the callers make of it: an aura
     * may be applied or removed in the middle of a walk over these very auras,
     * because that is what procs and effect handlers do. Three properties make
     * that safe.
     *
     *  - Each type owns a separately allocated block, so touching one type never
     *    disturbs a walk over another.
     *  - A removal leaves a hole instead of shifting its neighbours, so the
     *    positions a walk has yet to reach do not change under it.
     *  - A walk holds an index rather than a pointer, so it survives the block
     *    growing beneath it, and sees an aura appended while it runs.
     */
    class Index
    {
        private:
            struct Bucket
            {
                AuraType type = static_cast<AuraType>(0);
                size_t holes = 0;
                std::vector<Aura*> auras;
            };

        public:
            /// The auras of one type, holes skipped.
            class Range
            {
                public:
                    class Iterator
                    {
                        public:
                            Iterator() : m_bucket(nullptr), m_at(0) {}
                            Iterator(const Bucket* bucket, size_t at) : m_bucket(bucket), m_at(at) { Settle(); }

                            Aura* operator*() const { return m_bucket->auras[m_at]; }

                            Iterator& operator++()
                            {
                                ++m_at;
                                Settle();
                                return *this;
                            }

                            bool operator==(const Iterator& other) const
                            {
                                const bool mine = AtEnd();
                                const bool theirs = other.AtEnd();
                                return (mine || theirs) ? (mine && theirs) : (m_at == other.m_at);
                            }

                            bool operator!=(const Iterator& other) const { return !(*this == other); }

                        private:
                            bool AtEnd() const { return !m_bucket || m_at >= m_bucket->auras.size(); }

                            /// Holes are not elements; step over them.
                            void Settle()
                            {
                                while (m_bucket && m_at < m_bucket->auras.size() && !m_bucket->auras[m_at])
                                {
                                    ++m_at;
                                }
                            }

                            const Bucket* m_bucket;
                            size_t m_at;
                    };

                    Range() : m_bucket(nullptr) {}
                    explicit Range(const Bucket* bucket) : m_bucket(bucket) {}

                    Iterator begin() const { return Iterator(m_bucket, 0); }
                    Iterator end() const { return Iterator(); }

                    bool empty() const { return !m_bucket || m_bucket->holes == m_bucket->auras.size(); }

                    size_t size() const { return m_bucket ? m_bucket->auras.size() - m_bucket->holes : 0; }

                    Aura* front() const { return *begin(); }

                private:
                    const Bucket* m_bucket;
            };

            void Add(AuraType type, Aura* aura)
            {
                Bucket& bucket = Claim(type);

                // Fill a hole rather than growing: it keeps the block compact
                // and puts the aura where a walk in progress has not been yet.
                if (bucket.holes)
                {
                    for (Aura*& slot : bucket.auras)
                    {
                        if (!slot)
                        {
                            slot = aura;
                            --bucket.holes;
                            return;
                        }
                    }
                }

                bucket.auras.push_back(aura);
            }

            bool Remove(AuraType type, Aura* aura)
            {
                Bucket* bucket = Find(type);
                if (!bucket)
                {
                    return false;
                }

                for (Aura*& slot : bucket->auras)
                {
                    if (slot == aura)
                    {
                        slot = nullptr;
                        ++bucket->holes;
                        return true;
                    }
                }

                return false;
            }

            Range Of(AuraType type) const { return Range(Find(type)); }

            bool Empty(AuraType type) const { return Of(type).empty(); }

            void Clear() { m_buckets.clear(); }

            /// Live auras across every type; for diagnostics, not for hot paths.
            size_t Total() const
            {
                size_t n = 0;
                for (const auto& bucket : m_buckets)
                {
                    n += bucket->auras.size() - bucket->holes;
                }
                return n;
            }

        private:
            static bool Before(const std::unique_ptr<Bucket>& bucket, AuraType wanted) { return bucket->type < wanted; }

            const Bucket* Find(AuraType type) const
            {
                const auto at = std::lower_bound(m_buckets.begin(), m_buckets.end(), type, Before);
                return (at != m_buckets.end() && (*at)->type == type) ? at->get() : nullptr;
            }

            Bucket* Find(AuraType type)
            {
                return const_cast<Bucket*>(static_cast<const Index*>(this)->Find(type));
            }

            Bucket& Claim(AuraType type)
            {
                const auto at = std::lower_bound(m_buckets.begin(), m_buckets.end(), type, Before);
                if (at != m_buckets.end() && (*at)->type == type)
                {
                    return **at;
                }

                // Each block is allocated on its own so its address outlives any
                // reshuffling of the list of blocks.
                auto fresh = std::make_unique<Bucket>();
                fresh->type = type;
                return **m_buckets.insert(at, std::move(fresh));
            }

            std::vector<std::unique_ptr<Bucket>> m_buckets;
    };
}
