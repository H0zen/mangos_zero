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

/**
 * What a character has done in the war, and where it leaves him standing.
 *
 * The ledger holds every contribution of the last few days, one line apiece, and
 * two running totals for everything older than that. The client is never sent
 * the lines: it is sent what they add up to over four windows -- today,
 * yesterday, this week, last week -- plus a rank, a bar and a place in the
 * week's table.
 *
 * Rank is not stored. It is worked out from the points every time the ledger is
 * added up, so a rank can never drift from what earned it. The highest rank ever
 * reached is stored on its own, because the points that earned it may be gone
 * and nothing else records that he once held it.
 *
 * A dishonourable kill is the one thing that takes effect at once rather than at
 * the week's reckoning: its cost comes off his standing the moment it is written
 * down.
 */
class HonorLedger
{
    public:
        explicit HonorLedger(Player& owner);

        /// Writes one contribution down and adds the ledger up again. Nothing is
        /// written for a contribution worth nothing.
        bool Add(float honor, uint8 kind, uint32 victimId, uint8 victimType);

        /// Adds the ledger up and sends the client everything it draws: the four
        /// windows, the rank, the bar, the lifetime counts and the standing.
        void Reckon();

        /// Empties the ledger for good, in the row as well as in memory.
        void Wipe();

        /// Sets everything back to a character who has done nothing, without
        /// touching the row.
        void Forget();

        /// How many times he has killed this one, between two days inclusive.
        /// Creatures are counted by entry and players by guid, so the two can
        /// never be mistaken for one another.
        uint32 KillsOf(Unit const* victim, uint32 fromDate, uint32 toDate) const;

        HonorRankInfo const& Rank() const { return m_rank; }
        void Rank(HonorRankInfo const& rank) { m_rank = rank; }

        HonorRankInfo const& HighestRank() const { return m_highest; }
        void HighestRank(HonorRankInfo const& rank) { m_highest = rank; }

        /// The points carried forward from every week already reckoned. This
        /// week's earnings are added to it when the standing is read, not here.
        float Stored() const { return m_stored; }
        void Stored(float points) { m_stored = points; }

        /// The points he stands on now: what is stored plus this week so far.
        float Points() const { return m_points; }
        void Points(float points) { m_points = points; }

        uint32 Kills(bool honorable) const { return honorable ? m_honorableKills : m_dishonorableKills; }
        void Kills(uint32 count, bool honorable);

        /// Where he finished in last week's table for his side.
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
