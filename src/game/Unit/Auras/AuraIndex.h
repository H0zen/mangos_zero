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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class Aura;

namespace auras
{

    class Index
    {
        private:
            struct Entry
            {
                Aura* aura = nullptr;
                uint64_t rank = 0;
            };

            struct Bucket
            {
                AuraType type = static_cast<AuraType>(0);
                size_t holes = 0;
                std::vector<Entry> entries;
            };

        public:

            class Range
            {
                public:
                    class Iterator
                    {
                        public:
                            Iterator() : m_bucket(nullptr), m_at(0) {}
                            Iterator(const Bucket* bucket, size_t at) : m_bucket(bucket), m_at(at) { Settle(); }

                            Aura* operator*() const { return m_bucket->entries[m_at].aura; }

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
                            bool AtEnd() const { return !m_bucket || m_at >= m_bucket->entries.size(); }

                            void Settle()
                            {
                                while (m_bucket && m_at < m_bucket->entries.size() && !m_bucket->entries[m_at].aura)
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

                    bool empty() const { return !m_bucket || m_bucket->holes == m_bucket->entries.size(); }

                    size_t size() const { return m_bucket ? m_bucket->entries.size() - m_bucket->holes : 0; }

                    Aura* front() const { return *begin(); }

                private:
                    const Bucket* m_bucket;
            };

            void Add(AuraType type, Aura* aura)
            {
                Bucket& bucket = Claim(type);
                const Entry fresh{aura, ++m_clock};

                if (bucket.holes)
                {
                    for (auto& slot : bucket.entries)
                    {
                        if (!slot.aura)
                        {
                            slot = fresh;
                            --bucket.holes;
                            return;
                        }
                    }
                }

                bucket.entries.push_back(fresh);
            }

            bool Remove(AuraType type, Aura* aura)
            {
                Bucket* bucket = Find(type);
                if (!bucket)
                {
                    return false;
                }

                for (auto& slot : bucket->entries)
                {
                    if (slot.aura == aura)
                    {
                        slot = Entry();
                        ++bucket->holes;
                        return true;
                    }
                }

                return false;
            }

            Range Of(AuraType type) const { return Range(Find(type)); }

            bool Empty(AuraType type) const { return Of(type).empty(); }

            Aura* Newest(AuraType type) const
            {
                const Bucket* bucket = Find(type);
                if (!bucket)
                {
                    return nullptr;
                }

                const Entry* best = nullptr;
                for (const auto& slot : bucket->entries)
                {
                    if (slot.aura && (!best || slot.rank > best->rank))
                    {
                        best = &slot;
                    }
                }
                return best ? best->aura : nullptr;
            }

            std::vector<Aura*> ByRecency(AuraType type) const
            {
                std::vector<Aura*> ordered;
                const Bucket* bucket = Find(type);
                if (!bucket)
                {
                    return ordered;
                }

                std::vector<const Entry*> live;
                live.reserve(bucket->entries.size() - bucket->holes);
                for (const auto& slot : bucket->entries)
                {
                    if (slot.aura)
                    {
                        live.push_back(&slot);
                    }
                }

                std::sort(live.begin(), live.end(),
                          [](const Entry* a, const Entry* b) { return a->rank > b->rank; });

                ordered.reserve(live.size());
                for (const auto* slot : live)
                {
                    ordered.push_back(slot->aura);
                }
                return ordered;
            }

            void Clear() { m_buckets.clear(); }

            size_t Total() const
            {
                size_t n = 0;
                for (const auto& bucket : m_buckets)
                {
                    n += bucket->entries.size() - bucket->holes;
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

                auto fresh = std::make_unique<Bucket>();
                fresh->type = type;
                return **m_buckets.insert(at, std::move(fresh));
            }

            std::vector<std::unique_ptr<Bucket>> m_buckets;
            uint64_t m_clock = 0;
    };
}
