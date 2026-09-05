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

#include "HonorLedger.h"

#include "Database/DatabaseEnv.h"
#include "Formulas.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"

namespace
{
    /// A contribution is a kill when it names something that can be killed. A
    /// battleground objective names nothing, so it is worth points and no kill.
    bool NamesAKill(uint8 victimType)
    {
        return victimType == TYPEID_UNIT || victimType == TYPEID_PLAYER;
    }
}

HonorLedger::HonorLedger(Player& owner) : m_owner(owner)
{
    MaNGOS::Honor::InitRankInfo(m_rank);
    MaNGOS::Honor::InitRankInfo(m_highest);
}

void HonorLedger::Kills(uint32 count, bool honorable)
{
    if (honorable)
    {
        m_honorableKills = count;
    }
    else
    {
        m_dishonorableKills = count;
    }
}

bool HonorLedger::Add(float honor, uint8 kind, uint32 victimId, uint8 victimType)
{
    if (honor == 0.0f)
    {
        return false;
    }

    HonorEntry entry;
    entry.date = sWorld.GetDateToday();
    entry.honorPoints = honor;
    entry.victimID = victimId;
    entry.victimType = victimType;
    entry.type = kind;
    entry.state = HK_NEW;
    entry.isKill = NamesAKill(victimType);

    if (kind == DISHONORABLE)
    {
        // The cost comes off at once rather than at the week's reckoning, and it
        // stops at nothing rather than digging into a negative standing.
        m_stored = m_points > entry.honorPoints ? m_points - entry.honorPoints : 0.0f;
    }

    m_entries.push_back(entry);

    Reckon();
    return true;
}

void HonorLedger::Reckon()
{
    DETAIL_LOG("PLAYER: UpdateHonor");

    HonorWindows when;
    when.today = sWorld.GetDateToday();
    when.thisWeekBegin = sWorld.GetDateLastMaintenanceDay();

    HonorTally const tally = TallyHonor(m_entries, when, m_honorableKills, m_dishonorableKills);

    for (auto& entry : m_entries)
    {
        if (IsAheadOfToday(entry, when.today))
        {
            entry.state = HK_OLD;
        }
    }

    m_lastWeekPlace = sObjectMgr.GetHonorStandingPositionByGUID(m_owner.GetGUIDLow(), m_owner.GetTeam());

    // What is stored is every week already reckoned; this week's earnings sit in
    // the standing table until the next reckoning folds them in.
    float points = m_stored;
    if (HonorStanding* standing = sObjectMgr.GetHonorStandingByGUID(m_owner.GetGUIDLow(), m_owner.GetTeam()))
    {
        points += standing->rpEarning;
    }
    m_points = points;

    m_rank = MaNGOS::Honor::CalculateHonorRank(m_points);
    if (m_rank.visualRank > 0 && m_rank.visualRank > m_highest.visualRank)
    {
        m_highest = m_rank;
    }

    m_owner.SetHonorBar(uint8(HonorBarFill(m_points, m_rank.minRP, m_rank.maxRP, m_rank.positive)));
    m_owner.SetShownHonorRank(m_rank.rank);
    m_owner.SetShownHighestHonorRank(m_highest.rank);

    m_owner.SetUInt16Value(PLAYER_FIELD_SESSION_KILLS, 0, tally.todayHonorable);
    m_owner.SetUInt16Value(PLAYER_FIELD_SESSION_KILLS, 1, tally.todayDishonorable);

    m_owner.SetUInt16Value(PLAYER_FIELD_YESTERDAY_KILLS, 0, tally.yesterdayKills);
    m_owner.SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION,
                           uint32(tally.yesterdayHonor > 0.0f ? tally.yesterdayHonor : 0.0f));

    m_owner.SetUInt16Value(PLAYER_FIELD_THIS_WEEK_KILLS, 0, tally.thisWeekKills);
    m_owner.SetUInt32Value(PLAYER_FIELD_THIS_WEEK_CONTRIBUTION,
                           uint32(tally.thisWeekHonor > 0.0f ? tally.thisWeekHonor : 0.0f));

    m_owner.SetUInt16Value(PLAYER_FIELD_LAST_WEEK_KILLS, 0, tally.lastWeekKills);
    m_owner.SetUInt32Value(PLAYER_FIELD_LAST_WEEK_CONTRIBUTION,
                           uint32(tally.lastWeekHonor > 0.0f ? tally.lastWeekHonor : 0.0f));
    m_owner.SetUInt32Value(PLAYER_FIELD_LAST_WEEK_RANK, m_lastWeekPlace);

    m_owner.SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, tally.lifetimeHonorable);
    m_owner.SetUInt32Value(PLAYER_FIELD_LIFETIME_DISHONORABLE_KILLS, tally.lifetimeDishonorable);
}

void HonorLedger::Wipe()
{
    CharacterDatabase.PExecute("DELETE FROM `character_honor_cp` WHERE `guid` = '%u'", m_owner.GetGUIDLow());
    Forget();
    Reckon();
}

void HonorLedger::Forget()
{
    m_entries.clear();
    m_honorableKills = 0;
    m_dishonorableKills = 0;
    m_stored = 0.0f;
    m_lastWeekPlace = 0;

    MaNGOS::Honor::InitRankInfo(m_rank);
    MaNGOS::Honor::InitRankInfo(m_highest);
}

uint32 HonorLedger::KillsOf(Unit const* victim, uint32 fromDate, uint32 toDate) const
{
    if (!victim)
    {
        return 0;
    }

    uint8 const kind = victim->GetTypeId();
    uint32 named = 0;

    switch (kind)
    {
        case TYPEID_PLAYER:
            named = static_cast<Player const*>(victim)->GetGUIDLow();
            break;
        case TYPEID_UNIT:
            named = victim->GetEntry();
            break;
        default:
            return 0;
    }

    uint32 kills = 0;
    for (auto const& entry : m_entries)
    {
        if (entry.victimType == TYPEID_OBJECT || entry.victimType != kind || entry.victimID != named)
        {
            continue;
        }

        if (entry.date >= fromDate && entry.date <= toDate)
        {
            ++kills;
        }
    }

    return kills;
}

void HonorLedger::LoadFromDB(QueryResult* result)
{
    if (!result)
    {
        return;
    }

    m_entries.clear();

    do
    {
        Field* fields = result->Fetch();

        HonorEntry entry;
        entry.victimType = fields[0].GetUInt8();
        entry.victimID = fields[1].GetUInt32();
        entry.honorPoints = fields[2].GetFloat();
        entry.date = fields[3].GetUInt32();
        entry.type = fields[4].GetUInt8();
        entry.state = HK_UNCHANGED;
        entry.isKill = NamesAKill(entry.victimType);

        m_entries.push_back(entry);
    }
    while (result->NextRow());

    delete result;
}

void HonorLedger::SaveToDB()
{
    // An entry struck out is dropped rather than written; the kills it stood for
    // are already carried by the two counts in the character's own row.
    for (auto itr = m_entries.begin(); itr != m_entries.end();)
    {
        switch (itr->state)
        {
            case HK_NEW:
                CharacterDatabase.PExecute(
                    "INSERT INTO `character_honor_cp` (`guid`,`victim_type`,`victim`,`honor`,`date`,`type`) "
                    "VALUES (%u,%u,%u,%f,%u,%u)",
                    m_owner.GetGUIDLow(), itr->victimType, itr->victimID,
                    itr->honorPoints, itr->date, itr->type);
                itr->state = HK_UNCHANGED;
                ++itr;
                break;

            case HK_UNCHANGED:
                ++itr;
                break;

            default:
                itr = m_entries.erase(itr);
                break;
        }
    }
}
