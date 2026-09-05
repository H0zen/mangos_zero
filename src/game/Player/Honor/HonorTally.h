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

#include "Platform/Define.h"

#include <list>

#define NEGATIVE_HONOR_RANK_COUNT 4
#define POSITIVE_HONOR_RANK_COUNT 15
#define HONOR_RANK_COUNT 19 // negative + positive ranks

/**
 * Where a character stands on the ladder, as the client draws it.
 *
 * The internal number runs from nothing through four ranks of disgrace and
 * fourteen of standing; the number shown runs from -4 to 14, which is the one a
 * player would recognise. The floor and ceiling are the points that hold him at
 * this rank, and the bar is drawn from how far between them he stands.
 */
struct HonorRankInfo
{
    uint8 rank;      ///< Internal range [0..18]
    int8 visualRank; ///< Number visualized in rank bar [-4..14] 14 being High Warlord, -4 being Pariah)
    float maxRP;
    float minRP;
    bool positive;
};

/// Whether a contribution was won or lost. A dishonourable one is taken off his
/// standing at once instead of waiting for the week to be counted.
enum HonorKind
{
    HONORABLE    = 1,
    DISHONORABLE = 2,
};

/// Where an entry stands with the row that holds it between saves.
enum HonorEntryState
{
    HK_NEW = 0,
    HK_OLD = 1,
    HK_DELETED = 2,
    HK_UNCHANGED = 3
};

/**
 * One line in the honour ledger: what he did, to whom, on which day, and what it
 * was worth.
 *
 * The day is a day number rather than a time, because everything the client is
 * shown is cut on day boundaries -- today, yesterday, this week, last week --
 * and never on anything finer.
 */
struct HonorEntry
{
    uint8 victimType;
    uint32 victimID;
    float honorPoints;
    uint32 date;
    uint8 type;
    uint8 state;
    bool isKill;
};

typedef std::list<HonorEntry> HonorEntries;

/**
 * The four windows the client is shown, and the two running totals.
 *
 * A window is a span of day numbers. Today and yesterday are single days; a week
 * is seven days from the last maintenance. The windows overlap -- today is
 * inside this week -- so an entry is counted by every window it falls in, not by
 * the first.
 */
struct HonorWindows
{
    uint32 today = 0;
    uint32 thisWeekBegin = 0;

    uint32 Yesterday() const { return today - 1; }
    uint32 ThisWeekEnd() const { return thisWeekBegin + 7; }
    uint32 LastWeekBegin() const { return thisWeekBegin - 7; }
    uint32 LastWeekEnd() const { return LastWeekBegin() + 7; }
};

/// What the ledger adds up to over those windows.
struct HonorTally
{
    uint32 todayHonorable = 0;
    uint32 todayDishonorable = 0;

    uint32 yesterdayKills = 0;
    float yesterdayHonor = 0.0f;

    uint32 thisWeekKills = 0;
    float thisWeekHonor = 0.0f;

    uint32 lastWeekKills = 0;
    float lastWeekHonor = 0.0f;

    /// Everything he has ever done. The ledger holds only what is worth keeping
    /// day by day, so the two counts it starts from carry all the rest.
    uint32 lifetimeHonorable = 0;
    uint32 lifetimeDishonorable = 0;
};

/**
 * Adds the ledger up.
 *
 * Entries struck out are skipped. An honourable entry is counted for its kill
 * and for its points separately, because points can be won without a kill -- a
 * battleground objective -- while a kill is only ever counted once.
 *
 * This week's window is closed at both ends and last week's is half-open, which
 * is how it is sent; the two therefore overlap on the day the older one ends.
 */
inline HonorTally TallyHonor(HonorEntries const& entries, HonorWindows const& when,
                             uint32 storedHonorable, uint32 storedDishonorable)
{
    HonorTally tally;
    tally.lifetimeHonorable = storedHonorable;
    tally.lifetimeDishonorable = storedDishonorable;

    for (auto const& entry : entries)
    {
        if (entry.state == HK_DELETED)
        {
            continue;
        }

        if (entry.type == HONORABLE)
        {
            if (entry.isKill)
            {
                ++tally.lifetimeHonorable;

                if (entry.date == when.today)
                {
                    ++tally.todayHonorable;
                }
            }

            if (entry.date == when.Yesterday())
            {
                if (entry.isKill)
                {
                    ++tally.yesterdayKills;
                }
                tally.yesterdayHonor += entry.honorPoints;
            }

            if (entry.date >= when.thisWeekBegin && entry.date <= when.ThisWeekEnd())
            {
                if (entry.isKill)
                {
                    ++tally.thisWeekKills;
                }
                tally.thisWeekHonor += entry.honorPoints;
            }

            if (entry.date >= when.LastWeekBegin() && entry.date < when.LastWeekEnd())
            {
                if (entry.isKill)
                {
                    ++tally.lastWeekKills;
                }
                tally.lastWeekHonor += entry.honorPoints;
            }
        }
        else if (entry.isKill && entry.type == DISHONORABLE)
        {
            ++tally.lifetimeDishonorable;

            if (entry.date == when.today)
            {
                ++tally.todayDishonorable;
            }
        }
    }

    return tally;
}

/**
 * A dishonourable kill dated ahead of today is struck out of the running rather
 * than counted again on the day it claims.
 *
 * It still counts this time round, which is what the tally above does; this only
 * says which entries the ledger then retires.
 */
inline bool IsAheadOfToday(HonorEntry const& entry, uint32 today)
{
    return entry.isKill && entry.type == DISHONORABLE && entry.date > today;
}

/**
 * The share of a rank the client draws in the bar: full is 255 climbing and -255
 * falling.
 *
 * A rank spans from its floor to its ceiling in points, and the bar shows how far
 * through that span he stands. A rank with no span at all reads as empty rather
 * than dividing by nothing.
 */
inline int32 HonorBarFill(float points, float floorPoints, float ceilingPoints, bool climbing)
{
    float const span = ceilingPoints - floorPoints;
    if (span <= 0.0f)
    {
        return 0;
    }

    float const reached = (points < 0.0f ? -points : points) - floorPoints;
    return int32((reached / span) * (climbing ? 255.0f : -255.0f));
}
