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
#include "terrain/Column.hpp"
#include "terrain/ICollisionModel.hpp"

#include <cfloat>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class GameObject;
class DynamicCollision;

class GameObjectModel
{
    public:
        ~GameObjectModel() = default;

        static GameObjectModel* Create(const GameObject* pGo);

        static GameObjectModel* CreateStandalone(
            std::shared_ptr<const world::terrain::ICollisionModel> model,
            const Geometry::Transform& xf, uint32 phaseMask);

        const Geometry::Aabb& GetBounds() const { return m_bounds; }
        const Geometry::Vector3& GetPosition() const { return m_xf.pos; }
        const GameObject* GetOwner() const { return m_owner; }
        uint32 GetPhaseMask() const { return m_phaseMask; }
        bool IsCollidable() const { return m_collidable; }

        void SetCollidable(bool enabled) { m_collidable = enabled; }
        void SetPhaseMask(uint32 phaseMask = 0) { m_phaseMask = phaseMask; }

        void UpdatePose();

        void SetPose(const Geometry::Transform& xf);

        float SegmentHitFraction(const Geometry::Vector3& a, const Geometry::Vector3& b) const;

        void AddSurfaces(float x, float y, float zTop, float zBottom,
                         world::terrain::Column& out) const;

    private:
        friend class DynamicCollision;

        GameObjectModel() = default;
        bool Initialize(const GameObject* pGo, uint32 displayId);
        void DeriveBounds();

        bool m_collidable = false;
        uint32 m_phaseMask = 0;
        std::shared_ptr<const world::terrain::ICollisionModel> m_model;
        const GameObject* m_owner = nullptr;

        Geometry::Transform m_xf;
        Geometry::Aabb m_bounds;

        std::vector<uint32_t> m_cells;
        mutable uint32_t m_epoch = 0;
};
