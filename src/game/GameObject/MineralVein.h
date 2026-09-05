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

#include <unordered_map>

/**
 * What a mineral vein comes up as.
 *
 * A vein is placed in the world as one entry and may spawn as another. Two
 * ladders run through the ores: a vein can come up as the ore one rung poorer
 * than the one that was placed, and a vein can come up as the rare ore that
 * shares its rock -- tin holding silver, iron holding gold, mithril holding
 * truesilver. The two are rolled in that order, so a placed iron vein that
 * comes up as tin is then rolled for the rare ore of tin rather than of iron.
 *
 * Some ground holds an ore of its own, which every vein standing on it can come
 * up as instead; where it does, that roll is the only one made.
 *
 * Which ore holds which is data, from `mineral_vein` and `mineral_vein_zone`.
 */
class MineralVeins
{
    public:
        void Holds(uint32 entry, uint32 poorer, uint32 richer);
        void GroundHolds(uint32 zone, uint32 entry);
        void Clear();

        /// The ore one rung poorer, or 0 at the bottom of a ladder.
        uint32 PoorerThan(uint32 entry) const;
        /// The rare ore sharing this one's rock, or 0 when none does.
        uint32 RicherThan(uint32 entry) const;
        /// What the ground of that zone holds, or 0 when it holds nothing of its own.
        uint32 InZone(uint32 zone) const;

        /**
         * The entry a placed vein actually comes up as.
         *
         * @param zoneRoll the roll for the ore the ground holds came up
         * @param poorer   the roll for the poorer ore came up
         * @param richer   the roll for the rare ore came up
         */
        uint32 SpawnedAs(uint32 entry, uint32 zone, bool zoneRoll, bool poorer, bool richer) const;

        std::size_t Ladders() const { return m_ladder.size(); }
        std::size_t Grounds() const { return m_ground.size(); }

    private:
        struct Ladder
        {
            uint32 poorer = 0;
            uint32 richer = 0;
        };

        std::unordered_map<uint32, Ladder> m_ladder;
        std::unordered_map<uint32, uint32> m_ground;
};

/// The one the world loads at start-up.
extern MineralVeins sMineralVeins;

/// Reads `mineral_vein` and `mineral_vein_zone` into it.
void LoadMineralVeins();
