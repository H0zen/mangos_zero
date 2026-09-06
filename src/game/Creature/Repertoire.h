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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#pragma once

#include "Platform/Define.h"
#include "SharedDefines.h"

#include <ctime>
#include <iterator>
#include <map>

/**
 * The spells a unit was made knowing, and when it may cast each again.
 *
 * The four it knows come from its row and never change: a creature learns
 * nothing, and a slot it was given nothing for holds nothing. Anything it casts
 * beyond these four is cast by a script or an aura, which does not ask here.
 *
 * Two clocks run, and they are not kept the same way round. A spell's own
 * cooldown is stored as the hour it comes back, because that is what a caller
 * wants to know. A category's is stored as the hour it was last used, because
 * how long a category holds is a property of whichever spell is being asked
 * about, not of the category -- two spells of one category can hold it for
 * different lengths.
 *
 * A player has one of these and never uses it: what he knows is a book of
 * hundreds and his cooldowns are their own component.
 */
class Repertoire
{
    public:

        /// The spell in one of the four slots, or nothing.
        uint32 Slot(uint8 which) const { return which < CREATURE_MAX_SPELLS ? m_bar[which] : 0; }
        void Slot(uint8 which, uint32 spellId)
        {
            if (which < CREATURE_MAX_SPELLS)
            {
                m_bar[which] = spellId;
            }
        }

        /// It was made knowing this spell.
        bool Knows(uint32 spellId) const
        {
            for (uint32 known : m_bar)
            {
                if (known == spellId)
                {
                    return true;
                }
            }

            return false;
        }

        /// The hour a spell comes back.
        void ReadyAt(uint32 spellId, time_t when) { m_readyAt[spellId] = when; }

        /// The hour a category was last used.
        void CategoryUsedAt(uint32 category, time_t when) { m_categoryUsedAt[category] = when; }

        /// Seconds until the spell itself comes back, its category aside.
        uint32 Left(uint32 spellId, time_t now) const
        {
            auto itr = m_readyAt.find(spellId);
            return itr != m_readyAt.end() && itr->second > now ? uint32(itr->second - now) : 0;
        }

        /// The spell itself is still down.
        bool SpellDown(uint32 spellId, time_t now) const { return Left(spellId, now) != 0; }

        /**
         * The category is still down, given how long this particular spell holds
         * it for.
         */
        bool CategoryDown(uint32 category, uint32 holdsForSeconds, time_t now) const
        {
            auto itr = m_categoryUsedAt.find(category);
            return itr != m_categoryUsedAt.end() && time_t(itr->second + holdsForSeconds) > now;
        }

        /// The spells still down, and the hour each comes back. What has already
        /// come back may still be in here: ask ForgetExpired first.
        std::map<uint32, time_t> const& StillDown() const { return m_readyAt; }

        /// The categories still held, and the hour each was last used.
        std::map<uint32, time_t> const& CategoriesUsed() const { return m_categoryUsedAt; }

        /// Drops the ones whose hour has come, so what is left is all ahead.
        void ForgetExpired(time_t now)
        {
            for (auto itr = m_readyAt.begin(); itr != m_readyAt.end();)
            {
                itr = itr->second <= now ? m_readyAt.erase(itr) : std::next(itr);
            }
        }

        /// Nothing is down any more.
        bool NothingDown() const { return m_readyAt.empty(); }

        /// Everything comes back at once.
        void Clear()
        {
            m_readyAt.clear();
            m_categoryUsedAt.clear();
        }

    private:

        uint32 m_bar[CREATURE_MAX_SPELLS] = {};

        std::map<uint32, time_t> m_readyAt;
        std::map<uint32, time_t> m_categoryUsedAt;
};
