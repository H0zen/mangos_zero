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

#include "MineralVein.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"

MineralVeins sMineralVeins;

void MineralVeins::Holds(uint32 entry, uint32 poorer, uint32 richer)
{
    Ladder& rung = m_ladder[entry];
    rung.poorer = poorer;
    rung.richer = richer;
}

void MineralVeins::GroundHolds(uint32 zone, uint32 entry)
{
    m_ground[zone] = entry;
}

void MineralVeins::Clear()
{
    m_ladder.clear();
    m_ground.clear();
}

uint32 MineralVeins::PoorerThan(uint32 entry) const
{
    auto const found = m_ladder.find(entry);
    return found == m_ladder.end() ? 0 : found->second.poorer;
}

uint32 MineralVeins::RicherThan(uint32 entry) const
{
    auto const found = m_ladder.find(entry);
    return found == m_ladder.end() ? 0 : found->second.richer;
}

uint32 MineralVeins::InZone(uint32 zone) const
{
    auto const found = m_ground.find(zone);
    return found == m_ground.end() ? 0 : found->second;
}

uint32 MineralVeins::SpawnedAs(uint32 entry, uint32 zone, bool zoneRoll, bool poorer, bool richer) const
{
    if (uint32 const ofTheGround = InZone(zone))
    {
        // The ground settles it either way: no other roll is made there.
        return zoneRoll ? ofTheGround : entry;
    }

    if (poorer)
    {
        if (uint32 const lesser = PoorerThan(entry))
        {
            entry = lesser;
        }
    }

    if (richer)
    {
        // Of whatever it has come up as, which is the point of rolling in this order.
        if (uint32 const rare = RicherThan(entry))
        {
            entry = rare;
        }
    }

    return entry;
}

void LoadMineralVeins()
{
    sMineralVeins.Clear();

    uint32 ladders = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `poorer_entry`, `richer_entry` FROM `mineral_vein`");

    if (result)
    {
        BarGoLink bar(result->GetRowCount());

        do
        {
            bar.step();

            Field* fields = result->Fetch();
            uint32 const entry = fields[0].GetUInt32();
            uint32 const poorer = fields[1].GetUInt32();
            uint32 const richer = fields[2].GetUInt32();

            if (!ObjectMgr::GetGameObjectInfo(entry))
            {
                sLog.outErrorDb("Table `mineral_vein` has vein %u that is in no `gameobject_template`, skipped", entry);
                continue;
            }

            if (poorer && !ObjectMgr::GetGameObjectInfo(poorer))
            {
                sLog.outErrorDb("Table `mineral_vein` sends vein %u down to %u, which is in no `gameobject_template`", entry, poorer);
                continue;
            }

            if (richer && !ObjectMgr::GetGameObjectInfo(richer))
            {
                sLog.outErrorDb("Table `mineral_vein` sends vein %u up to %u, which is in no `gameobject_template`", entry, richer);
                continue;
            }

            sMineralVeins.Holds(entry, poorer, richer);
            ++ladders;
        }
        while (result->NextRow());

        delete result;
    }

    uint32 grounds = 0;
    result = WorldDatabase.Query("SELECT `zone`, `entry` FROM `mineral_vein_zone`");

    if (result)
    {
        BarGoLink bar(result->GetRowCount());

        do
        {
            bar.step();

            Field* fields = result->Fetch();
            uint32 const zone = fields[0].GetUInt32();
            uint32 const entry = fields[1].GetUInt32();

            if (!ObjectMgr::GetGameObjectInfo(entry))
            {
                sLog.outErrorDb("Table `mineral_vein_zone` gives zone %u the vein %u, which is in no `gameobject_template`", zone, entry);
                continue;
            }

            sMineralVeins.GroundHolds(zone, entry);
            ++grounds;
        }
        while (result->NextRow());

        delete result;
    }

    sLog.outString(">> Loaded %u mineral vein ladder(s) and %u zone(s) whose ground holds an ore", ladders, grounds);
    sLog.outString();
}
