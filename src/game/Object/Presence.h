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

#define CONTACT_DISTANCE            0.5f
#define INTERACTION_DISTANCE        5.0f
#define ATTACK_DISTANCE             5.0f
#define TRADE_DISTANCE              11.11f                  // max distance for trading
#define MAX_VISIBILITY_DISTANCE     333.0f                  // max distance for visible object show, limited in 333 yards
#define DEFAULT_VISIBILITY_DISTANCE 90.0f                   // default visible distance, 90 yards on continents
#define DEFAULT_VISIBILITY_INSTANCE 120.0f                  // default visible distance in instances, 120 yards
#define DEFAULT_VISIBILITY_BGARENAS 180.0f                  // default visible distance in BG/Arenas, 180 yards

#define DEFAULT_WORLD_OBJECT_SIZE   0.388999998569489f      // currently used (correctly?) for any non Unit world objects. This is actually the bounding_radius, like player/creature from creature_model_data
#define MAX_STEALTH_DETECT_RANGE    45.0f

// How far a deck map extends from its origin. A deck map is the hull, so its bounds are
// the hull's. Used only when the vessel cannot be resolved and its real extent read; the
// job is to reject the absolute continent coordinates a leaving zeppelin sometimes reports.
#define MAX_DECK_EXTENT             250.0f
#define DECK_EDGE_MARGIN            10.0f

/**
 * @brief Temporary spawn type enumeration
 *
 * Defines when and how temporary spawns should despawn.
 */
enum TempSpawnType
{
    TEMPSPAWN_MANUAL_DESPAWN = 0,             ///< Despawns when UnSummon() is called
    TEMPSPAWN_DEAD_DESPAWN = 1,               ///< Despawns when the creature disappears
    TEMPSPAWN_CORPSE_DESPAWN = 2,             ///< Despawns instantly after death
    TEMPSPAWN_CORPSE_TIMED_DESPAWN = 3,       ///< Despawns after a specified time after death (or when the creature disappears)
    TEMPSPAWN_TIMED_DESPAWN = 4,              ///< Despawns after a specified time
    TEMPSPAWN_TIMED_OOC_DESPAWN = 5,          ///< Despawns after a specified time after the creature is out of combat
    TEMPSPAWN_TIMED_OR_DEAD_DESPAWN = 6,      ///< Despawns after a specified time OR when the creature disappears
    TEMPSPAWN_TIMED_OR_CORPSE_DESPAWN = 7,    ///< Despawns after a specified time OR when the creature dies
    TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN = 8,  ///< Despawns after a specified time (OOC) OR when the creature disappears
    TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN = 9 ///< Despawns after a specified time (OOC) OR when the creature dies
};

/**
 * @brief World update counter
 *
 * Measures time between world update ticks.
 * Essential for units updating their spells after cells become active.
 */
class WorldUpdateCounter
{
    public:
        /**
         * @brief Constructor
         */
        WorldUpdateCounter() : m_tmStart(0) {}

        /**
         * @brief Get elapsed time since start
         * @return Elapsed time in milliseconds
         */
        time_t timeElapsed()
        {
            if (!m_tmStart)
            {
                m_tmStart = GameTime::GetGameTimeMS();
            }

            return getMSTimeDiff(m_tmStart, GameTime::GetGameTimeMS());
        }

        /**
         * @brief Reset the counter
         */
        void Reset()
        {
            m_tmStart = GameTime::GetGameTimeMS();
        }

    private:
        uint32 m_tmStart; ///< Start time in milliseconds
};

struct PresenceChangeAccumulator;

class Presence : public Object
{
    friend struct PresenceChangeAccumulator;

    public:

        // class is used to manipulate with WorldUpdateCounter
        // it is needed in order to get time diff between two object's Update() calls
        class UpdateHelper
        {
            public:
                explicit UpdateHelper(Presence* obj) : m_obj(obj) {}
                ~UpdateHelper() {}

                void Update(uint32 time_diff)
                {
                    m_obj->Update(m_obj->m_updateTracker.timeElapsed(), time_diff);
                    m_obj->m_updateTracker.Reset();
                }

            private:
                UpdateHelper(const UpdateHelper&);
                UpdateHelper& operator=(const UpdateHelper&);

                Presence* const m_obj;
        };

        virtual ~Presence();

        virtual void Update(uint32 update_diff, uint32 /*time_diff*/);

        void _Create(uint32 guidlow, HighGuid guidhigh);

        /// WHERE THIS OBJECT IS -- the whole spatial API. An object HAS a placement; it
        /// is not a bag of coordinates with geometry methods bolted on, so there are no
        /// GetPositionX/GetDistance/HasInArc here and there never will be. Ask the
        /// component: obj->Where().DistanceTo(other->Where()).
        Geometry::Placement const& Where() const { return m_placement; }

        /// Mutation of the pose. Movement drives this; nobody else should need it.
        Geometry::Placement& Place() { return m_placement; }

        /// The extent lives in the component; this only pushes a new value in when the
        /// per-class formula's inputs change (a model, a scale -- rarely).
        void RefreshBoundingRadius() { m_placement.Resize(ComputeBoundingRadius()); }

        void OnScaleChanged() override { RefreshBoundingRadius(); }

        uint32 GetMapId() const { return m_mapId; }
        uint32 GetInstanceId() const { return m_InstanceId; }


        InstanceData* GetInstanceData() const;

        const char* GetName() const { return m_name.c_str(); }
        void SetName(const std::string& newname) { m_name = newname; }

        virtual const char* GetNameForLocaleIdx(int32 /*locale_idx*/) const { return GetName(); }

        virtual void CleanupsBeforeDelete();                // used in destructor or explicitly before mass creature delete to remove cross-references to already deleted units





        virtual bool IsControlledByPlayer() const { return false; }

        void AddObjectToRemoveList();

        void UpdateObjectVisibility();
        virtual void UpdateVisibilityAndView();             // update visibility for object and object for all around

        // main visibility check function in normal case (ignore grey zone distance check)
        bool IsVisibleFor(Player const* u, Presence const* viewPoint) const { return IsVisibleForInState(u, viewPoint, false); }

        // low level function for visibility change code, must be define in all main world object subclasses
        virtual bool IsVisibleForInState(Player const* u, Presence const* viewPoint, bool inVisibleList) const = 0;

        void SetMap(Map* map);
        Map* GetMap() const { MANGOS_ASSERT(m_currMap); return m_currMap; }

        /// The map, or NULL, for the paths that legitimately run on an object which never
        /// reached one -- a destructor after LoadFromDB failed, above all. GetMap() asserts
        /// there, so `if (GetMap())` is not a guard, it is the crash.
        Map* FindMap() const { return m_currMap; }

        TerrainInfo const* GetTerrain() const;

        void AddToClientUpdateList() override;
        void RemoveFromClientUpdateList() override;
        void BuildUpdateData(UpdateDataMapType&) override;


        bool IsActiveObject() const { return m_isActiveObject || m_viewPoint.hasViewers(); }

        void SetActiveObjectState(bool active);

        // Per-object visibility distance. 0 means use the map default; a positive
        // value overrides it when this object is the viewpoint (e.g. the cinematic
        // flyover body widens the populate radius without touching the map).
        float GetVisibilityDistanceOverride() const { return m_visibilityDistanceOverride; }
        void SetVisibilityDistanceOverride(float dist) { m_visibilityDistanceOverride = dist; }

        ViewPoint& GetViewPoint()
        {
            return m_viewPoint;
        }

        // ASSERT print helper
        bool PrintCoordinatesError(float x, float y, float z, char const* descr) const;




    protected:
        explicit Presence();

        /// The per-class spatial extent. Overridden where the object is not a default
        /// blob: a unit reads its model, a gameobject its geometry box.
        virtual float ComputeBoundingRadius() const { return DEFAULT_WORLD_OBJECT_SIZE; }

        // these functions are used mostly for Relocate() and Corpse/Player specific stuff...
        // use them ONLY in LoadFromDB()/Create() funcs and nowhere else!
        // mapId/instanceId should be set in SetMap() function!
        void SetLocationMapId(uint32 _mapId) { m_mapId = _mapId; RefreshFrame(); }
        void SetLocationInstanceId(uint32 _instanceId) { m_InstanceId = _instanceId; RefreshFrame(); }

        /// Re-anchor the component to the frame the object's map identity names. The
        /// pose is untouched: this says where the numbers are measured, not what they are.
        void RefreshFrame()
        {
            m_placement.Rebase(Geometry::Frame::World(m_mapId, m_InstanceId));
        }


        std::string m_name;

    private:
        Map* m_currMap;                                     // current object's Map location

        uint32 m_mapId;                                     // object at map with map_id
        uint32 m_InstanceId;                                // in map copy with instance id

        Geometry::Placement m_placement;
        ViewPoint m_viewPoint;
        WorldUpdateCounter m_updateTracker;
        bool m_isActiveObject;
        float m_visibilityDistanceOverride;
};

// Tests that are NOT geometry, so they are not the component's and never the object's:
// world membership is game state, line of sight is a terrain question, and a map's
// coordinate bounds belong to the map. Each asks the placement for the geometry and adds
// only what the placement must not know.
// Delivering a packet to the people who can see something. The map owns the
// cells and the cameras, so it answers who; these add what the map must not
// know -- the relay across a vessel's map boundary, and the subject's own
// client when the subject has one.
void Broadcast(Presence const& from, WorldPacket* data, bool toSubject);
void BroadcastWithin(Presence const& from, WorldPacket* data, float dist,
                     bool toSubject, bool ownTeamOnly = false);
void BroadcastExcept(Presence const& from, WorldPacket* data, Player const* skip);

/// Can A reach B -- a common frame is required. Melee, spells, threat, aggro.
bool CanInteract(Presence const& a, Presence const& b);

/// Can B be shown A -- the wider question, and never the same one as reaching it.
bool CanBeSeen(Presence const& seen, Presence const& viewer);

/// CanBeSeen plus "near enough to bother".
bool SeenWithin(Presence const& seen, Presence const& viewer, float dist, bool is3D = true);

bool InReach(Presence const& a, Presence const& b, float dist, bool is3D = true);
bool InFrontPhased(Presence const& a, Presence const& b, float dist, float arc);
bool InBackPhased(Presence const& a, Presence const& b, float dist, float arc);
bool HasLineOfSight(Presence const& a, Presence const& b);
bool HasLineOfSight(Presence const& a, Geometry::Vector3 const& point);
bool IsPlaceable(Presence const& obj);

// Terrain and grid answers about a position. The component supplies the geometry; the
// height, the collision sweep and the map's bounds come from the engines that own them.
Geometry::Vector3 PointNear(Presence const& anchor, float distance2d, float absAngle);
void DropToGround(Presence const& obj, float x, float y, float& z);
void ClampToAllowedZ(Presence const& obj, float x, float y, float& z, Map* atMap = NULL);
Geometry::Vector3 RandomGroundPointNear(Presence const& obj, Geometry::Vector3 const& centre,
                                        float distance, float minDist = 0.0f, float const* ori = NULL);
void FindFreeSpotNear(Presence const& anchor, Presence const* searcher, float& x, float& y, float& z,
                      float searcher_bounding_radius, float distance2d, float absAngle);
void ClosePointNear(Presence const& anchor, float& x, float& y, float& z, float bounding_radius,
                    float distance2d = 0.0f, float angle = 0.0f, Presence const* searcher = NULL);
void ContactPointNear(Presence const& anchor, Presence const* obj, float& x, float& y, float& z,
                      float distance2d = CONTACT_DISTANCE);

