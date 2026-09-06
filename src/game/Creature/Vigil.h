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

#include <ctime>

/**
 * The watch kept over a unit from the blow that kills it to the hour it stands
 * again.
 *
 * Three things follow one another. It dies, and its body lies where it fell for
 * the corpse delay -- long enough for whoever killed it to open it. The body
 * goes. Then the respawn delay runs, and it is back.
 *
 * The two hours are held rather than the two delays alone, because the corpse
 * can be hurried: a body that has been emptied decays at a rate the world sets
 * instead of waiting out its full time, and the hour it comes back moves with
 * it. Whichever of the two hours is still ahead is the answer to when it will be
 * seen again.
 *
 * A newly respawned thing is dazed for a moment before it will notice anyone.
 * That grace is counted in milliseconds against the tick, not in seconds against
 * the clock, because it is short enough that the difference shows.
 *
 * Some things spawn dead and are meant to be found that way -- a body to be
 * looted, a corpse a quest is about. For those the whole sequence runs the other
 * way round: alive is the state that ends, and the corpse is what it returns to.
 *
 * A player has one of these and never uses it: his body is an object of its own
 * and his return is his own business.
 */
class Vigil
{
    public:

        /// The hour its body goes.
        time_t CorpseGoesAt() const { return m_corpseGoesAt; }
        void CorpseGoesAt(time_t when) { m_corpseGoesAt = when; }

        /// The hour it stands again.
        time_t RespawnsAt() const { return m_respawnsAt; }
        void RespawnsAt(time_t when) { m_respawnsAt = when; }

        /// Seconds from its body going to its standing again.
        uint32 RespawnDelay() const { return m_respawnDelay; }
        void RespawnDelay(uint32 seconds) { m_respawnDelay = seconds; }

        /// Seconds its body lies there, before anything hurries it.
        uint32 CorpseDelay() const { return m_corpseDelay; }
        void CorpseDelay(uint32 seconds) { m_corpseDelay = seconds; }

        /// Milliseconds left of the grace in which it notices nobody.
        uint32 AggroDelay() const { return m_aggroDelay; }
        void AggroDelay(uint32 milliseconds) { m_aggroDelay = milliseconds; }

        /// The hour it was killed, to the second.
        time_t KilledAt() const { return m_killedAt; }
        void KilledAt(time_t when) { m_killedAt = when; }

        /// It is meant to be found dead.
        bool DeadByDefault() const { return m_deadByDefault; }
        void DeadByDefault(bool dead) { m_deadByDefault = dead; }

        /**
         * The hour it will be seen again, given the hour it is now.
         *
         * If it is waiting to respawn, that hour. If its body is still lying
         * there, the hour the body goes plus the wait after. If neither, it is
         * here already and the answer is now.
         */
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

        /// Counts the grace down by a tick and says whether any is left.
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
