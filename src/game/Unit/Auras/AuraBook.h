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

#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <list>
#include <map>
#include <set>
#include <utility>

class Aura;
class PetAura;
class SpellAuraHolder;
struct SpellEntry;

typedef std::multimap<uint32 , SpellAuraHolder*> SpellAuraHolderMap;
typedef std::pair<SpellAuraHolderMap::iterator, SpellAuraHolderMap::iterator> SpellAuraHolderBounds;
typedef std::pair<SpellAuraHolderMap::const_iterator, SpellAuraHolderMap::const_iterator> SpellAuraHolderConstBounds;
typedef std::list<SpellAuraHolder*> SpellAuraHolderList;
typedef std::list<Aura*> AuraList;

typedef std::map<SpellEntry const*, ObjectGuid > TrackedAuraTargetMap;

typedef std::set<PetAura const*> PetAuraSet;

class AuraBook
{
    public:

        AuraBook() : m_cursor(m_holders.end()) {}

        AuraBook(AuraBook const&) = delete;
        AuraBook& operator=(AuraBook const&) = delete;

        void Enter(SpellAuraHolder* holder);

        bool Strike(SpellAuraHolder* holder);

        SpellAuraHolderBounds Of(uint32 spellId) { return m_holders.equal_range(spellId); }
        SpellAuraHolderConstBounds Of(uint32 spellId) const { return m_holders.equal_range(spellId); }

        bool Holds(uint32 spellId) const { return m_holders.find(spellId) != m_holders.end(); }

        bool Empty() const { return m_holders.empty(); }
        SpellAuraHolder* First() const { return m_holders.begin()->second; }

        SpellAuraHolderMap& All() { return m_holders; }
        SpellAuraHolderMap const& All() const { return m_holders; }

        template<typename Tick>
        void EachHolder(Tick&& tick)
        {
            for (m_cursor = m_holders.begin(); m_cursor != m_holders.end();)
            {
                SpellAuraHolder* holder = m_cursor->second;
                ++m_cursor;
                tick(holder);
            }

            m_cursor = m_holders.end();
        }

        template<typename Test, typename Strike>
        void RemoveWhere(Test&& test, Strike&& strike)
        {
            for (auto itr = m_holders.begin(); itr != m_holders.end();)
            {
                SpellAuraHolder* holder = itr->second;

                if (test(holder))
                {
                    strike(holder);
                    itr = m_holders.begin();
                }
                else
                {
                    ++itr;
                }
            }
        }

        TrackedAuraTargetMap&       Tracked(TrackedAuraType type)       { return m_tracked[type]; }
        TrackedAuraTargetMap const& Tracked(TrackedAuraType type) const { return m_tracked[type]; }

        PetAuraSet&       ForItsPet()       { return m_petAuras; }
        PetAuraSet const& ForItsPet() const { return m_petAuras; }

        void Defer(Aura* aura) { m_deferredAuras.push_back(aura); }
        void Defer(SpellAuraHolder* holder) { m_deferredHolders.push_back(holder); }

        bool NothingDeferred() const { return m_deferredAuras.empty() && m_deferredHolders.empty(); }

        void SweepDeferred();

    private:

        SpellAuraHolderMap m_holders;

        SpellAuraHolderMap::iterator m_cursor;

        AuraList m_deferredAuras;
        SpellAuraHolderList m_deferredHolders;

        TrackedAuraTargetMap m_tracked[MAX_TRACKED_AURA_TYPES];
        PetAuraSet m_petAuras;
};
