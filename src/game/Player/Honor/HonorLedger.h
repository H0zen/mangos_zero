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

#include "Honor/HonorTally.h"
#include "ObjectGuid.h"

class Player;
class QueryResult;
class Unit;
struct HonorRankInfo;

class HonorLedger
{
    public:
        explicit HonorLedger(Player& owner);

        bool Add(float honor, uint8 kind, uint32 victimId, uint8 victimType);

        void Reckon();

        void Wipe();

        void Forget();

        uint32 KillsOf(Unit const* victim, uint32 fromDate, uint32 toDate) const;

        HonorRankInfo const& Rank() const { return m_rank; }
        void Rank(HonorRankInfo const& rank) { m_rank = rank; }

        HonorRankInfo const& HighestRank() const { return m_highest; }
        void HighestRank(HonorRankInfo const& rank) { m_highest = rank; }

        float Stored() const { return m_stored; }
        void Stored(float points) { m_stored = points; }

        float Points() const { return m_points; }
        void Points(float points) { m_points = points; }

        uint32 Kills(bool honorable) const { return honorable ? m_honorableKills : m_dishonorableKills; }
        void Kills(uint32 count, bool honorable);

        int32 LastWeekPlace() const { return m_lastWeekPlace; }
        void LastWeekPlace(int32 place) { m_lastWeekPlace = place; }

        HonorEntries& Entries() { return m_entries; }
        HonorEntries const& Entries() const { return m_entries; }

        void LoadFromDB(QueryResult* result);
        void SaveToDB();

    private:
        Player& m_owner;

        HonorEntries m_entries;
        HonorRankInfo m_rank;
        HonorRankInfo m_highest;

        float m_points = 0.0f;
        float m_stored = 0.0f;
        uint32 m_honorableKills = 0;
        uint32 m_dishonorableKills = 0;
        int32 m_lastWeekPlace = 0;
};
