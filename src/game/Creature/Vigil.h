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

namespace vigil
{

    struct Decay
    {
        uint32 normal = 0;
        uint32 rare = 0;
        uint32 elite = 0;
        uint32 rareElite = 0;
        uint32 worldBoss = 0;
    };

    inline uint32 DecayFor(uint32 rank, Decay const& how)
    {
        switch (rank)
        {
            case CREATURE_ELITE_RARE:      return how.rare;
            case CREATURE_ELITE_ELITE:     return how.elite;
            case CREATURE_ELITE_RAREELITE: return how.rareElite;
            case CREATURE_ELITE_WORLDBOSS: return how.worldBoss;
            default:                       return how.normal;
        }
    }
}

class Vigil
{
    public:

        time_t CorpseGoesAt() const { return m_corpseGoesAt; }
        void CorpseGoesAt(time_t when) { m_corpseGoesAt = when; }

        time_t RespawnsAt() const { return m_respawnsAt; }
        void RespawnsAt(time_t when) { m_respawnsAt = when; }

        uint32 RespawnDelay() const { return m_respawnDelay; }
        void RespawnDelay(uint32 seconds) { m_respawnDelay = seconds; }

        uint32 CorpseDelay() const { return m_corpseDelay; }
        void CorpseDelay(uint32 seconds) { m_corpseDelay = seconds; }

        uint32 AggroDelay() const { return m_aggroDelay; }
        void AggroDelay(uint32 milliseconds) { m_aggroDelay = milliseconds; }

        time_t KilledAt() const { return m_killedAt; }
        void KilledAt(time_t when) { m_killedAt = when; }

        bool DeadByDefault() const { return m_deadByDefault; }
        void DeadByDefault(bool dead) { m_deadByDefault = dead; }

        time_t BackAt(time_t now) const
        {
            if (m_respawnsAt > now)
            {
                return m_respawnsAt;
            }

            if (m_corpseGoesAt > now)
            {
                return m_corpseGoesAt + m_respawnDelay;
            }

            return now;
        }

        bool StillDazed(uint32 elapsed)
        {
            if (m_aggroDelay <= elapsed)
            {
                m_aggroDelay = 0;
                return false;
            }

            m_aggroDelay -= elapsed;
            return true;
        }

    private:

        time_t m_corpseGoesAt = 0;
        time_t m_respawnsAt = 0;
        time_t m_killedAt = 0;

        uint32 m_respawnDelay = 25;
        uint32 m_corpseDelay = 60;
        uint32 m_aggroDelay = 0;

        bool m_deadByDefault = false;
};
