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
#define HONOR_RANK_COUNT 19

struct HonorRankInfo
{
    uint8 rank;
    int8 visualRank;
    float maxRP;
    float minRP;
    bool positive;
};

enum HonorKind
{
    HONORABLE    = 1,
    DISHONORABLE = 2,
};

enum HonorEntryState
{
    HK_NEW = 0,
    HK_OLD = 1,
    HK_DELETED = 2,
    HK_UNCHANGED = 3
};

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

struct HonorWindows
{
    uint32 today = 0;
    uint32 thisWeekBegin = 0;

    uint32 Yesterday() const { return today - 1; }
    uint32 ThisWeekEnd() const { return thisWeekBegin + 7; }
    uint32 LastWeekBegin() const { return thisWeekBegin - 7; }
    uint32 LastWeekEnd() const { return LastWeekBegin() + 7; }
};

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

    uint32 lifetimeHonorable = 0;
    uint32 lifetimeDishonorable = 0;
};

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

inline bool IsAheadOfToday(HonorEntry const& entry, uint32 today)
{
    return entry.isKill && entry.type == DISHONORABLE && entry.date > today;
}

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
