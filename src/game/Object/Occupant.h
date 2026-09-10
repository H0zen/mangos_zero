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

#include "Object.h"
#include "Position.h"
#include "Tenure.h"
#include "Camera.h"
#include "GameTime.h"
#include "Geometry/Placement.h"

class TerrainInfo;
class Map;
class InstanceData;
class Player;

#define CONTACT_DISTANCE            0.5f
#define INTERACTION_DISTANCE        5.0f
#define ATTACK_DISTANCE             5.0f
#define TRADE_DISTANCE              11.11f
#define MAX_VISIBILITY_DISTANCE     333.0f
#define DEFAULT_VISIBILITY_DISTANCE 90.0f
#define DEFAULT_VISIBILITY_INSTANCE 120.0f
#define DEFAULT_VISIBILITY_BGARENAS 180.0f

#define DEFAULT_WORLD_OBJECT_SIZE   0.388999998569489f
#define MAX_STEALTH_DETECT_RANGE    45.0f

#define MAX_DECK_EXTENT             250.0f
#define DECK_EDGE_MARGIN            10.0f

class WorldUpdateCounter
{
    public:

        WorldUpdateCounter() : m_tmStart(0) {}

        time_t timeElapsed()
        {
            if (!m_tmStart)
            {
                m_tmStart = GameTime::GetGameTimeMS();
            }

            return getMSTimeDiff(m_tmStart, GameTime::GetGameTimeMS());
        }

        void Reset()
        {
            m_tmStart = GameTime::GetGameTimeMS();
        }

    private:
        uint32 m_tmStart;
};

struct OccupantChangeAccumulator;

class Occupant : public Object
{
    friend struct OccupantChangeAccumulator;

    public:

        class UpdateHelper
        {
            public:
                explicit UpdateHelper(Occupant* obj) : m_obj(obj) {}
                ~UpdateHelper() {}

                void Update(uint32 time_diff)
                {
                    m_obj->Update(m_obj->m_updateTracker.timeElapsed(), time_diff);
                    m_obj->m_updateTracker.Reset();
                }

            private:
                UpdateHelper(const UpdateHelper&);
                UpdateHelper& operator=(const UpdateHelper&);

                Occupant* const m_obj;
        };

        virtual ~Occupant();

        virtual void Update(uint32 update_diff, uint32 );

        void _Create(uint32 guidlow, HighGuid guidhigh);

        Geometry::Placement const& Where() const { return m_placement; }

        Geometry::Placement& Place() { return m_placement; }

        void RefreshBoundingRadius() { m_placement.Resize(ComputeBoundingRadius()); }

        void OnScaleChanged() override { RefreshBoundingRadius(); }

        uint32 GetMapId() const { return m_mapId; }
        uint32 GetInstanceId() const { return m_InstanceId; }

        InstanceData* GetInstanceData() const;

        const char* GetName() const { return m_name.c_str(); }
        void SetName(const std::string& newname) { m_name = newname; }

        virtual const char* GetNameForLocaleIdx(int32 ) const { return GetName(); }

        virtual void CleanupsBeforeDelete();

        virtual bool IsControlledByPlayer() const { return false; }

        virtual bool OutlivesItsGrid() const { return false; }

        void AddObjectToRemoveList();

        void UpdateObjectVisibility();
        virtual void UpdateVisibilityAndView();

        bool IsVisibleFor(Player const* u, Occupant const* viewPoint) const { return IsVisibleForInState(u, viewPoint, false); }

        virtual bool IsVisibleForInState(Player const* u, Occupant const* viewPoint, bool inVisibleList) const = 0;

        void SetMap(Map* map);
        Map* GetMap() const { MANGOS_ASSERT(m_currMap); return m_currMap; }

        Map* FindMap() const { return m_currMap; }

        TerrainInfo const* GetTerrain() const;

        void AddToClientUpdateList() override;
        void RemoveFromClientUpdateList() override;
        void BuildUpdateData(UpdateDataMapType&) override;

        bool IsActiveObject() const { return m_isActiveObject || m_viewPoint.hasViewers(); }

        void SetActiveObjectState(bool active);

        float GetVisibilityDistanceOverride() const { return m_visibilityDistanceOverride; }
        void SetVisibilityDistanceOverride(float dist) { m_visibilityDistanceOverride = dist; }

        ViewPoint& GetViewPoint()
        {
            return m_viewPoint;
        }

        bool PrintCoordinatesError(float x, float y, float z, char const* descr) const;

    protected:
        explicit Occupant();

        virtual float ComputeBoundingRadius() const { return DEFAULT_WORLD_OBJECT_SIZE; }

        void SetLocationMapId(uint32 _mapId) { m_mapId = _mapId; RefreshFrame(); }
        void SetLocationInstanceId(uint32 _instanceId) { m_InstanceId = _instanceId; RefreshFrame(); }

        void RefreshFrame()
        {
            m_placement.Rebase(m_mapId, m_InstanceId);
        }

        std::string m_name;

    private:
        Map* m_currMap;

        uint32 m_mapId;
        uint32 m_InstanceId;

        Geometry::Placement m_placement;
        ViewPoint m_viewPoint;
        WorldUpdateCounter m_updateTracker;
        bool m_isActiveObject;
        float m_visibilityDistanceOverride;
};

bool CanInteract(Occupant const& a, Occupant const& b);

bool CanBeSeen(Occupant const& seen, Occupant const& viewer);

bool SeenWithin(Occupant const& seen, Occupant const& viewer, float dist, bool is3D = true);

bool InReach(Occupant const& a, Occupant const& b, float dist, bool is3D = true);
bool InFrontPhased(Occupant const& a, Occupant const& b, float dist, float arc);
bool InBackPhased(Occupant const& a, Occupant const& b, float dist, float arc);
bool HasLineOfSight(Occupant const& a, Occupant const& b);
bool HasLineOfSight(Occupant const& a, Geometry::Vector3 const& point);
bool IsPlaceable(Occupant const& obj);

Geometry::Vector3 PointNear(Occupant const& anchor, float distance2d, float absAngle);
void DropToGround(Occupant const& obj, float x, float y, float& z);
void ClampToAllowedZ(Occupant const& obj, float x, float y, float& z, Map* atMap = nullptr);
Geometry::Vector3 RandomGroundPointNear(Occupant const& obj, Geometry::Vector3 const& centre,
                                        float distance, float minDist = 0.0f, float const* ori = nullptr);
void FindFreeSpotNear(Occupant const& anchor, Occupant const* searcher, float& x, float& y, float& z,
                      float searcher_bounding_radius, float distance2d, float absAngle);
void ClosePointNear(Occupant const& anchor, float& x, float& y, float& z, float bounding_radius,
                    float distance2d = 0.0f, float angle = 0.0f, Occupant const* searcher = nullptr);
void ContactPointNear(Occupant const& anchor, Occupant const* obj, float& x, float& y, float& z,
                      float distance2d = CONTACT_DISTANCE);
