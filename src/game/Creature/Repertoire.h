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

class Repertoire
{
    public:

        uint32 Slot(uint8 which) const { return which < CREATURE_MAX_SPELLS ? m_bar[which] : 0; }
        void Slot(uint8 which, uint32 spellId)
        {
            if (which < CREATURE_MAX_SPELLS)
            {
                m_bar[which] = spellId;
            }
        }

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

        void ReadyAt(uint32 spellId, time_t when) { m_readyAt[spellId] = when; }

        void CategoryUsedAt(uint32 category, time_t when) { m_categoryUsedAt[category] = when; }

        uint32 Left(uint32 spellId, time_t now) const
        {
            auto itr = m_readyAt.find(spellId);
            return itr != m_readyAt.end() && itr->second > now ? uint32(itr->second - now) : 0;
        }

        bool SpellDown(uint32 spellId, time_t now) const { return Left(spellId, now) != 0; }

        bool CategoryDown(uint32 category, uint32 holdsForSeconds, time_t now) const
        {
            auto itr = m_categoryUsedAt.find(category);
            return itr != m_categoryUsedAt.end() && time_t(itr->second + holdsForSeconds) > now;
        }

        std::map<uint32, time_t> const& StillDown() const { return m_readyAt; }

        std::map<uint32, time_t> const& CategoriesUsed() const { return m_categoryUsedAt; }

        void ForgetExpired(time_t now)
        {
            for (auto itr = m_readyAt.begin(); itr != m_readyAt.end();)
            {
                itr = itr->second <= now ? m_readyAt.erase(itr) : std::next(itr);
            }
        }

        bool NothingDown() const { return m_readyAt.empty(); }

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
