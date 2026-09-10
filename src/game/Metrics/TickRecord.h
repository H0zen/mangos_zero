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
#include "Metrics/Distribution.h"
#include "Metrics/TickPhases.h"

#include <cstddef>

namespace metrics
{

    class TickRecord
    {
        public:

            void Record(uint32 elapsedMs, uint32 budgetMs)
            {
                m_ms.Add(elapsedMs);
                if (elapsedMs > budgetMs)
                {
                    ++m_overruns;
                }
            }

            void RecordPhase(TickPhase phase, uint32 ms) { m_phases.Add(phase, ms); }

            uint32 Ms(float percentile) const { return m_ms.Percentile(percentile); }
            uint32 MsMax() const { return m_ms.Max(); }
            uint32 Overruns() const { return m_overruns; }
            std::size_t Samples() const { return m_ms.Count(); }

            TickBreakdown const& Phases() const { return m_phases; }

        private:
            Distribution<256> m_ms;
            uint32 m_overruns = 0;
            TickBreakdown m_phases;
    };
}
