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

#include <list>
#include <map>
#include <utility>

class Aura;
class SpellAuraHolder;

typedef std::multimap<uint32 /*spellId*/, SpellAuraHolder*> SpellAuraHolderMap;
typedef std::pair<SpellAuraHolderMap::iterator, SpellAuraHolderMap::iterator> SpellAuraHolderBounds;
typedef std::pair<SpellAuraHolderMap::const_iterator, SpellAuraHolderMap::const_iterator> SpellAuraHolderConstBounds;
typedef std::list<SpellAuraHolder*> SpellAuraHolderList;
typedef std::list<Aura*> AuraList;

/**
 * @brief What a unit is carrying, kept by the spell that put it there.
 *
 * One entry per holder, filed under its spell id, and a spell may have several -- one per
 * caster -- which is why this is a multimap rather than a map.
 *
 * The whole difficulty of this book is that it is written in while it is being read. An
 * aura's own tick removes it, an effect handler applies another, a holder taken off takes
 * others with it. Three things answer that, and they are the reason this is a type of its
 * own rather than a container on the unit:
 *
 *  - the update walk keeps a cursor here, so a holder struck off while the walk is under
 *    way moves the cursor on instead of leaving it dangling;
 *  - a removal that has to happen while somebody still holds the object is deferred, and
 *    the deferred are destroyed at the end of the tick;
 *  - a sweep that removes by a test restarts after every removal, because removing one
 *    holder can take others with it, and what is left behind is not what the walk saw.
 *
 * What a removal MEANS -- the modifiers undone, the statue unsummoned, the client told --
 * is the unit's, and none of it is here.
 */
class AuraBook
{
    public:

        AuraBook() : m_cursor(m_holders.end()) {}

        AuraBook(AuraBook const&) = delete;
        AuraBook& operator=(AuraBook const&) = delete;

        /// File a holder under its spell.
        void Enter(SpellAuraHolder* holder);

        /// Strike one holder off. The cursor of a walk in progress steps past it first.
        /// @return true when the holder was found and struck.
        bool Strike(SpellAuraHolder* holder);

        /// Every holder of one spell, from every caster.
        SpellAuraHolderBounds Of(uint32 spellId) { return m_holders.equal_range(spellId); }
        SpellAuraHolderConstBounds Of(uint32 spellId) const { return m_holders.equal_range(spellId); }

        bool Holds(uint32 spellId) const { return m_holders.find(spellId) != m_holders.end(); }

        bool Empty() const { return m_holders.empty(); }
        SpellAuraHolder* First() const { return m_holders.begin()->second; }

        SpellAuraHolderMap& All() { return m_holders; }
        SpellAuraHolderMap const& All() const { return m_holders; }

        /**
         * @brief Hand every holder to `tick` once.
         *
         * The cursor moves on before the holder is handed over, so a holder that removes
         * itself -- or the one after it -- while ticking leaves the walk on solid ground.
         */
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

        /**
         * @brief Take off every holder the test asks for.
         *
         * Starts again from the beginning after each removal: taking one holder off can
         * take others with it, so nothing after it can be trusted to still be there.
         *
         * @param test   asked of each holder; true means take it off.
         * @param strike does the taking off -- the unit's own removal, with all it means.
         */
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

        /// Somebody is still holding it, so it cannot be destroyed until the tick is over.
        void Defer(Aura* aura) { m_deferredAuras.push_back(aura); }
        void Defer(SpellAuraHolder* holder) { m_deferredHolders.push_back(holder); }

        bool NothingDeferred() const { return m_deferredAuras.empty() && m_deferredHolders.empty(); }

        /// Destroy what was deferred. Nobody is holding any of it by now.
        void SweepDeferred();

    private:

        SpellAuraHolderMap m_holders;

        /// Where the update walk has got to, or end() when no walk is under way.
        SpellAuraHolderMap::iterator m_cursor;

        AuraList m_deferredAuras;
        SpellAuraHolderList m_deferredHolders;
};
