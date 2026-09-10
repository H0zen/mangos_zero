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

enum PlayerDelayedOperations
{
    DELAYED_SAVE_PLAYER         = 0x01,
    DELAYED_RESURRECT_PLAYER    = 0x02,
    DELAYED_SPELL_CAST_DESERTER = 0x04,
    DELAYED_END
};

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

        bool InFlight() const { return m_near || m_far; }
        bool InFlightNear() const { return m_near; }
        bool InFlightFar() const { return m_far; }

        void FlyingNear(bool flying) { m_near = flying; }
        void FlyingFar(bool flying) { m_far = flying; }

        void MayWait(bool may) { m_mayWait = may; }

        bool WaitIfItMay(bool aliveNow)
        {
            m_waiting = m_mayWait;
            m_wasAliveWhenGiven = aliveNow;
            return m_waiting;
        }

        bool Waits(bool aliveNow) const
        {
            return m_waiting && (aliveNow || !m_wasAliveWhenGiven);
        }

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

        bool m_wasAliveWhenGiven = true;

        uint32 m_onArrival = 0;
};
