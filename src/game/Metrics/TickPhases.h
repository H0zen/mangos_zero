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

// What a map's tick is made of.
//
// A tick time says a map is slow. It does not say which part, and the parts do
// very different amounts of work: a continent with nobody on it should be
// spending its time on almost nothing, so when it is not, the interesting
// question is which of these is awake.

#include "Metrics/Distribution.h"

#include <array>

namespace metrics
{
    enum class TickPhase
    {
        Mailbox,        ///< packets the serial phase routed here
        Players,        ///< per-player update
        GridObjects,    ///< creatures and props around players
        ActiveObjects,  ///< non-player objects that keep themselves awake
        ObjectUpdates,  ///< building and sending the update packets
        GridStates,     ///< grid loading, unloading and ageing
        Scripts,        ///< queued scripts, instance data, weather
        Vessels,        ///< global transports, and their decks nested inside

        Count
    };

    inline const char* PhaseName(TickPhase phase)
    {
        switch (phase)
        {
            case TickPhase::Mailbox:        return "mailbox";
            case TickPhase::Players:        return "players";
            case TickPhase::GridObjects:    return "grid";
            case TickPhase::ActiveObjects:  return "active";
            case TickPhase::ObjectUpdates:  return "updates";
            case TickPhase::GridStates:     return "gridstate";
            case TickPhase::Scripts:        return "scripts";
            case TickPhase::Vessels:        return "vessels";
            default:                        return "?";
        }
    }

    /// One window per phase. Small windows: this is read to find a culprit, not
    /// to plot a graph.
    class TickBreakdown
    {
        public:

            void Add(TickPhase phase, uint32 ms)
            {
                m_phases[static_cast<size_t>(phase)].Add(ms);
            }

            uint32 Median(TickPhase phase) const
            {
                return m_phases[static_cast<size_t>(phase)].Percentile(0.5f);
            }

            /// The phase with the largest median, and what it costs. This is the
            /// answer to "where did the tick go".
            TickPhase Worst(uint32& ms) const
            {
                TickPhase worst = TickPhase::Mailbox;
                ms = 0;
                for (size_t i = 0; i < static_cast<size_t>(TickPhase::Count); ++i)
                {
                    const uint32 median = m_phases[i].Percentile(0.5f);
                    if (median > ms)
                    {
                        ms = median;
                        worst = TickPhase(i);
                    }
                }
                return worst;
            }

        private:

            std::array<Distribution<64>, static_cast<size_t>(TickPhase::Count)> m_phases;
    };

    /**
     * @brief A stopwatch that files the time between marks.
     *
     * One line at each boundary rather than a scope per phase: the phases of a
     * map tick run one after another in a single function, and bracketing each
     * one would mean rewriting the function's shape to measure it.
     */
    template <class Sink>
    class PhaseClock
    {
        public:
            PhaseClock(Sink& sink, uint32 now) : m_sink(sink), m_last(now) {}

            void Mark(TickPhase phase, uint32 now)
            {
                m_sink.RecordPhase(phase, now - m_last);
                m_last = now;
            }

        private:
            Sink& m_sink;
            uint32 m_last;
    };
}
