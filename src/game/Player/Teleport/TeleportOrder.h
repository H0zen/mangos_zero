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

#include "Geometry/Placement.h"
#include "Platform/Define.h"

/// What is left to do once he lands.
enum PlayerDelayedOperations
{
    DELAYED_SAVE_PLAYER         = 0x01,
    DELAYED_RESURRECT_PLAYER    = 0x02,
    DELAYED_SPELL_CAST_DESERTER = 0x04,
    DELAYED_END
};

/**
 * An order to send a character somewhere, from the moment it is given until he
 * has arrived and everything owed on arrival has been done.
 *
 * An order is either near or far. Near keeps him on the map he is on and is
 * finished the moment the client acknowledges the move; far takes him to another
 * map, and he is out of the world between the two. Both are called a flight
 * here, and only one of them is ever in the air.
 *
 * ## Putting one off
 *
 * A spell cast while the character is being ticked can order a teleport in the
 * middle of that tick, and moving him there and then would pull the ground out
 * from under the rest of it. So for the length of a tick an order may wait: it
 * is recorded instead of carried out, and made at the end.
 *
 * An order that waits remembers whether he was alive when it was given. A
 * character who was alive then and is dead now is not sent: he has become a
 * ghost at a graveyard during the same tick, and the order would drag him off
 * it. One given to a character already dead is honoured either way.
 */
class TeleportOrder
{
    public:
        Geometry::Placement const& To() const { return m_to; }
        Geometry::Placement& To() { return m_to; }
        uint32 Options() const { return m_options; }

        void Aim(Geometry::Placement const& to, uint32 options)
        {
            m_to = to;
            m_options = options;
        }

        /// He has been sent and has not arrived.
        bool InFlight() const { return m_near || m_far; }
        bool InFlightNear() const { return m_near; }
        bool InFlightFar() const { return m_far; }

        void FlyingNear(bool flying) { m_near = flying; }
        void FlyingFar(bool flying) { m_far = flying; }

        /// Whether an order given now may wait for the tick to finish.
        void MayWait(bool may) { m_mayWait = may; }

        /**
         * Records an order to be made at the end of the tick, if one may wait at
         * all, and remembers whether he was alive as it was given.
         *
         * Comes back true when the order was taken, which is the caller's signal
         * to stop and let the end of the tick do the rest.
         */
        bool WaitIfItMay(bool aliveNow)
        {
            m_waiting = m_mayWait;
            m_wasAliveWhenGiven = aliveNow;
            return m_waiting;
        }

        /// Whether an order is waiting and is still worth making.
        bool Waits(bool aliveNow) const
        {
            return m_waiting && (aliveNow || !m_wasAliveWhenGiven);
        }

        /// What is owed on arrival, added to as the journey is prepared.
        void OnArrival(uint32 what)
        {
            if (what < DELAYED_END)
            {
                m_onArrival |= what;
            }
        }

        uint32 Owed() const { return m_onArrival; }
        bool Owes(uint32 what) const { return (m_onArrival & what) != 0; }
        void Settled() { m_onArrival = 0; }

    private:
        Geometry::Placement m_to;
        uint32 m_options = 0;

        bool m_near = false;
        bool m_far = false;

        bool m_mayWait = false;
        bool m_waiting = false;

        /// True unless he was dead when the waiting order was given. It starts
        /// true because an order that never waited is never asked about.
        bool m_wasAliveWhenGiven = true;

        uint32 m_onArrival = 0;
};
