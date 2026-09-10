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

#include "terrain/Geometry.hpp"
#include "terrain/ILiveGeometry.hpp"

#include <cfloat>
#include <cstdint>
#include <unordered_map>
#include <vector>

class GameObjectModel;

class DynamicCollision : public world::terrain::ILiveGeometry
{
    public:
        DynamicCollision() = default;

        void Insert(GameObjectModel& model);
        void Remove(GameObjectModel& model);
        bool Contains(const GameObjectModel& model) const;

        void Refresh(GameObjectModel& model);

        int Size() const { return static_cast<int>(m_all.size()); }

        bool IsInLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2,
                             uint32_t phasemask) const;

        float NearestHitFraction(float x1, float y1, float z1, float x2, float y2,
                                 float z2, uint32_t phasemask) const;

        void AddSurfaces(float x, float y, float zTop, float zBottom, uint32_t filter,
                         world::terrain::Column& out) const override;

    private:
        void FileBody(GameObjectModel& model);
        void UnfileBody(GameObjectModel& model);

        template <typename F>
        void ForEachCandidate(float minx, float miny, float maxx, float maxy, F&& f) const;

        static uint32_t CellKey(int tx, int ty)
        {
            return uint32_t(tx) * 64u + uint32_t(ty);
        }

        std::unordered_map<uint32_t, std::vector<GameObjectModel*>> m_buckets;
        std::vector<GameObjectModel*> m_all;

        mutable uint32_t m_epoch = 0;
};
