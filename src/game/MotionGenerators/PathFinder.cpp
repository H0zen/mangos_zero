/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
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

#include <cmath>
#include "Utilities/Errors.h"
#include <algorithm>
#include "../recastnavigation/Detour/Include/DetourCommon.h"

#include "MoveMap.h"
#include "GridMap.h"
#include "Creature.h"
#include "Map.h"
#include "PathFinder.h"
#include "PathPolyline.h"
#include "Log.h"
#include "Player.h"

#include <cfloat>

namespace
{
    constexpr float COLLISION_PROBE_HEIGHT = 2.0f;
    constexpr float COLLISION_STANDOFF = 0.5f;
    constexpr int TILE_BORDER_STEPS = 16;
}

////////////////// PathFinder //////////////////

/**
 * @brief Constructor for PathFinder.
 * @param owner The unit that owns this PathFinder.
 */
PathFinder::PathFinder(const Unit* owner)
    : PathFinder(owner, owner->GetMapId())
{
}

PathFinder::PathFinder(const Unit* owner, uint32 mapId)
    : m_polyLength(0), m_type(PATHFIND_BLANK),
    m_forceDestination(false), m_destBeyondMesh(false), m_pathLengthLimit(0.0f),
    m_sourceUnit(owner), m_mapId(mapId), m_navMesh(NULL), m_navMeshQuery(NULL)
{
    DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::PathFinder for %s \n", m_sourceUnit->GetGuidStr().c_str());

    memset(m_pathPolyRefs, 0, sizeof(m_pathPolyRefs));

    createFilter();
}

NavAgent PathFinder::Agent() const
{
    bool steered = m_sourceUnit->GetTypeId() == TYPEID_PLAYER;
#ifdef ENABLE_PLAYERBOTS
    steered = steered && !const_cast<Player*>(static_cast<Player const*>(m_sourceUnit))->GetPlayerbotAI();
#endif
    return AgentFor(m_sourceUnit->Where().Extent(), steered);
}

void PathFinder::DropStaleCorridor()
{
    for (uint32 i = 0; i < m_polyLength; ++i)
    {
        if (!m_navMesh->isValidPolyRef(m_pathPolyRefs[i]))
        {
            clear();
            return;
        }
    }
}

/**
 * @brief Destructor for PathFinder.
 */
PathFinder::~PathFinder()
{
    DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::~PathFinder() for %s \n", m_sourceUnit->GetGuidStr().c_str());
}

/**
 * @brief Calculates the path from the source unit to the destination.
 * @param destX The X-coordinate of the destination.
 * @param destY The Y-coordinate of the destination.
 * @param destZ The Z-coordinate of the destination.
 * @param forceDest Whether to force the destination.
 * @return True if the path was successfully calculated, false otherwise.
 */
bool PathFinder::calculate(float destX, float destY, float destZ, bool forceDest)
{
    float x, y, z;
    x = m_sourceUnit->Where().X();
    y = m_sourceUnit->Where().Y();
    z = m_sourceUnit->Where().Z();

    return calculate(x, y, z, destX, destY, destZ, forceDest);
}

/**
 * @brief Calculates the path from an explicit start position to the destination.
 * @param startX The X-coordinate of the start position.
 * @param startY The Y-coordinate of the start position.
 * @param startZ The Z-coordinate of the start position.
 * @param destX The X-coordinate of the destination.
 * @param destY The Y-coordinate of the destination.
 * @param destZ The Z-coordinate of the destination.
 * @param forceDest Whether to force the destination.
 * @return True if the path was successfully calculated, false otherwise.
 */
bool PathFinder::calculate(float startX, float startY, float startZ, float destX, float destY, float destZ, bool forceDest)
{
    if (!MaNGOS::IsValidMapCoord(startX, startY, startZ) || !MaNGOS::IsValidMapCoord(destX, destY, destZ))
    {
        return false;
    }

    MMAP::NavMeshLease lease;
    if (MMAP::MMapFactory::IsPathfindingEnabled(m_mapId, m_sourceUnit))
    {
        lease = MMAP::MMapFactory::createOrGetMMapManager()->Lease(m_mapId, Agent());
    }
    m_navMesh = lease.Mesh();
    m_navMeshQuery = lease.Query();

    const bool routed = Route(Vector3(startX, startY, startZ), Vector3(destX, destY, destZ), forceDest);

    m_navMesh = NULL;
    m_navMeshQuery = NULL;
    return routed;
}

bool PathFinder::Route(const Vector3& start, const Vector3& dest, bool forceDest)
{
    setStartPosition(start);
    setEndPosition(dest);

    m_forceDestination = forceDest;
    m_destBeyondMesh = false;

    DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::calculate() for %s \n", m_sourceUnit->GetGuidStr().c_str());

    if (!m_navMesh || !m_navMeshQuery || m_sourceUnit->hasUnitState(UNIT_STAT_IGNORE_PATHFINDING))
    {
        BuildShortcut();
        m_type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
        return true;
    }

#ifdef ENABLE_PLAYERBOTS
    if (m_sourceUnit->GetTypeId() == TYPEID_PLAYER &&
        ((Player*)m_sourceUnit)->GetPlayerbotAI() &&
        (m_sourceUnit->GetMap()->GetTerrain()->IsInWater(start.x, start.y, start.z + 1.0) ||
            m_sourceUnit->GetMap()->GetTerrain()->IsInWater(dest.x, dest.y, dest.z)) &&
        m_sourceUnit->GetMap()->IsInLineOfSight(start.x, start.y, start.z + 2.0f, dest.x, dest.y, dest.z + 2.0f))
    {
        BuildShortcut();
        m_type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
        return true;
    }
#endif

    if (!HaveTile(start))
    {
        BuildShortcut();
        m_type = PATHFIND_NOPATH;
        return true;
    }
    m_destBeyondMesh = !HaveTile(dest);

    updateFilter();
    DropStaleCorridor();

    BuildPolyPath(start, m_destBeyondMesh ? LastLoadedPointToward(start, dest) : dest);
    if (m_destBeyondMesh && m_type == PATHFIND_NORMAL)
    {
        m_type = PATHFIND_INCOMPLETE;
    }
    return true;
}

Vector3 PathFinder::LastLoadedPointToward(const Vector3& from, const Vector3& to) const
{
    float loaded = 0.0f;
    float missing = 1.0f;
    for (int step = 0; step < TILE_BORDER_STEPS; ++step)
    {
        const float middle = (loaded + missing) * 0.5f;
        (HaveTile(from + (to - from) * middle) ? loaded : missing) = middle;
    }
    return from + (to - from) * loaded;
}

void PathFinder::AcceptAgainstWorld()
{
    Map const* map = m_sourceUnit->GetMap();
    for (size_t i = 1; i < m_pathPoints.size(); ++i)
    {
        const Vector3 from = m_pathPoints[i - 1];
        Vector3 hit = m_pathPoints[i];
        hit.z += COLLISION_PROBE_HEIGHT;
        if (!map->GetHitPosition(from.x, from.y, from.z + COLLISION_PROBE_HEIGHT, hit.x, hit.y, hit.z, COLLISION_STANDOFF))
        {
            continue;
        }

        hit.z -= COLLISION_PROBE_HEIGHT;
        m_pathPoints.resize(i);
        if ((hit - from).length() > COLLISION_STANDOFF)
        {
            m_pathPoints.push_back(hit);
        }
        m_type = PATHFIND_INCOMPLETE;
        return;
    }
}

/**
 * @brief Gets the nearest polygon reference by position.
 * @param polyPath The polygon path.
 * @param polyPathSize The size of the polygon path.
 * @param point The point to find the nearest polygon for.
 * @param distance The distance to the nearest polygon.
 * @return The nearest polygon reference.
 */
dtPolyRef PathFinder::getPathPolyByPosition(const dtPolyRef* polyPath, uint32 polyPathSize, const float* point, float* distance) const
{
    if (!polyPath || !polyPathSize)
    {
        return INVALID_POLYREF;
    }

    dtPolyRef nearestPoly = INVALID_POLYREF;
    float minDist2d = FLT_MAX;
    float minDist3d = 0.0f;

    for (uint32 i = 0; i < polyPathSize; ++i)
    {
        float closestPoint[VERTEX_SIZE];

        if (dtStatusFailed(m_navMeshQuery->closestPointOnPoly(polyPath[i], point, closestPoint, NULL)))
        {
            continue;
        }

        float d = dtVdist2DSqr(point, closestPoint);
        if (d < minDist2d)
        {
            minDist2d = d;
            nearestPoly = polyPath[i];
            minDist3d = dtVdistSqr(point, closestPoint);
        }

        if (minDist2d < 1.0f) // shortcut out - close enough for us
        {
            break;
        }
    }

    if (distance)
    {
        *distance = dtMathSqrtf(minDist3d);
    }

    return (minDist2d < 3.0f) ? nearestPoly : INVALID_POLYREF;
}

/**
 * @brief Gets the polygon reference by location.
 * @param point The point to find the polygon for.
 * @param distance The distance to the polygon.
 * @return The polygon reference.
 */
dtPolyRef PathFinder::getPolyByLocation(const float* point, float* distance) const
{
    // first we check the current path
    // if the current path doesn't contain the current poly,
    // we need to use the expensive navMesh.findNearestPoly
    dtPolyRef polyRef = getPathPolyByPosition(m_pathPolyRefs, m_polyLength, point, distance);
    if (polyRef != INVALID_POLYREF)
    {
        return polyRef;
    }

    // we don't have it in our old path
    // try to get it by findNearestPoly()
    // first try with low search box
    float extents[VERTEX_SIZE] = {3.0f, 5.0f, 3.0f};    // bounds of poly search area
    float closestPoint[VERTEX_SIZE] = {0.0f, 0.0f, 0.0f};
    if (dtStatusSucceed(m_navMeshQuery->findNearestPoly(point, extents, &m_filter, &polyRef, closestPoint)) && polyRef != INVALID_POLYREF)
    {
        *distance = dtVdist(closestPoint, point);
        return polyRef;
    }

    // still nothing ..
    // try with bigger search box
    extents[0] = extents[2] = 10.0f;
    extents[1] = 200.0f;
    if (dtStatusSucceed(m_navMeshQuery->findNearestPoly(point, extents, &m_filter, &polyRef, closestPoint)) && polyRef != INVALID_POLYREF)
    {
        *distance = dtVdist(closestPoint, point);
        return polyRef;
    }

    return INVALID_POLYREF;
}

/**
 * @brief Builds the polygon path from the start position to the end position.
 * @param startPos The start position.
 * @param endPos The end position.
 */
void PathFinder::BuildPolyPath(const Vector3& startPos, const Vector3& endPos)
{
    // *** getting start/end poly logic ***

    float distToStartPoly, distToEndPoly;
    float startPoint[VERTEX_SIZE] = {startPos.y, startPos.z, startPos.x};
    float endPoint[VERTEX_SIZE] = {endPos.y, endPos.z, endPos.x};

    dtPolyRef startPoly = getPolyByLocation(startPoint, &distToStartPoly);
    dtPolyRef endPoly = getPolyByLocation(endPoint, &distToEndPoly);

    dtStatus dtResult;

    // we have a hole in our mesh
    // make shortcut path and mark it as NOPATH ( with flying exception )
    // its up to caller how he will use this info
    if (startPoly == INVALID_POLYREF || endPoly == INVALID_POLYREF)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: (startPoly == 0 || endPoly == 0) for %s\n", m_sourceUnit->GetGuidStr().c_str());
        BuildShortcut();

        if (m_sourceUnit->GetTypeId() == TYPEID_UNIT)
        {
            // Check for swimming or flying shortcut
            if ((startPoly == INVALID_POLYREF && m_sourceUnit->GetMap()->GetTerrain()->IsUnderWater(startPos.x, startPos.y, startPos.z)) ||
                (endPoly == INVALID_POLYREF && m_sourceUnit->GetMap()->GetTerrain()->IsUnderWater(endPos.x, endPos.y, endPos.z)))
            {
                m_type = m_sourceUnit->ToCreature()->CanSwim() ? PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH) : PATHFIND_NOPATH;
            }
            else
            {
                m_type = m_sourceUnit->ToCreature()->CanFly() ? PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH) : PATHFIND_NOPATH;
            }
        }
        else
        {
            m_type = PATHFIND_NOPATH;
        }

        return;
    }

    // we may need a better number here
    bool farFromPoly = (distToStartPoly > 7.0f || distToEndPoly > 7.0f);
    if (farFromPoly)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: farFromPoly distToStartPoly=%.3f distToEndPoly=%.3f for %s\n",
            distToStartPoly, distToEndPoly, m_sourceUnit->GetGuidStr().c_str());

        bool buildShortcut = false;
        if (m_sourceUnit->GetTypeId() == TYPEID_UNIT)
        {
            const Creature* owner = m_sourceUnit->ToCreature();

            Vector3 p = (distToStartPoly > 7.0f) ? startPos : endPos;
            if (m_sourceUnit->GetMap()->GetTerrain()->IsInWater(p.x, p.y, p.z + 1.0))
            {
                DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: underWater case for %s\n", m_sourceUnit->GetGuidStr().c_str());
                if (owner->CanSwim())
                {
                    buildShortcut = true;
                }
            }
            else
            {
                DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: flying case for %s\n", m_sourceUnit->GetGuidStr().c_str());
                if (owner->CanFly())
                {
                    buildShortcut = true;
                }
            }
        }

        if (buildShortcut)
        {
            BuildShortcut();
            m_type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
            return;
        }
        else
        {
            float closestPoint[VERTEX_SIZE];
            // we may want to use closestPointOnPolyBoundary instead
            dtResult = m_navMeshQuery->closestPointOnPoly(endPoly, endPoint, closestPoint, NULL);
            if (dtStatusSucceed(dtResult))
            {
                dtVcopy(endPoint, closestPoint);
                setActualEndPosition(Vector3(endPoint[2], endPoint[0], endPoint[1]));
            }

            m_type = PATHFIND_INCOMPLETE;
        }
    }

    // *** poly path generating logic ***

    // start and end are on same polygon
    // just need to move in straight line
    if (startPoly == endPoly)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: (startPoly == endPoly) for %s\n", m_sourceUnit->GetGuidStr().c_str());

        BuildShortcut();

        m_pathPolyRefs[0] = startPoly;
        m_polyLength = 1;

        m_type = farFromPoly ? PATHFIND_INCOMPLETE : PATHFIND_NORMAL;
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: path type %d for %s\n", m_type, m_sourceUnit->GetGuidStr().c_str());
        return;
    }

    // look for startPoly/endPoly in current path
    // TODO: we can merge it with getPathPolyByPosition() loop
    bool startPolyFound = false;
    bool endPolyFound = false;
    uint32 pathStartIndex, pathEndIndex;

    if (m_polyLength)
    {
        for (pathStartIndex = 0; pathStartIndex < m_polyLength; ++pathStartIndex)
        {
            // here to catch few bugs
            MANGOS_ASSERT(m_pathPolyRefs[pathStartIndex] != INVALID_POLYREF || m_sourceUnit->PrintEntryError("PathFinder::BuildPolyPath"));

            if (m_pathPolyRefs[pathStartIndex] == startPoly)
            {
                startPolyFound = true;
                break;
            }
        }

        for (pathEndIndex = m_polyLength - 1; pathEndIndex > pathStartIndex; --pathEndIndex)
        {
            if (m_pathPolyRefs[pathEndIndex] == endPoly)
            {
                endPolyFound = true;
                break;
            }
        }
    }

    if (startPolyFound && endPolyFound)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: (startPolyFound && endPolyFound) for %s\n", m_sourceUnit->GetGuidStr().c_str());

        // we moved along the path and the target did not move out of our old poly-path
        // our path is a simple subpath case, we have all the data we need
        // just "cut" it out

        m_polyLength = pathEndIndex - pathStartIndex + 1;
        memmove(m_pathPolyRefs, m_pathPolyRefs + pathStartIndex, m_polyLength * sizeof(dtPolyRef));
    }
    else if (startPolyFound && !endPolyFound)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: (startPolyFound && !endPolyFound) for %s\n", m_sourceUnit->GetGuidStr().c_str());

        // we are moving on the old path but target moved out
        // so we have atleast part of poly-path ready

        m_polyLength -= pathStartIndex;

        // try to adjust the suffix of the path instead of recalculating entire length
        // at given interval the target can not get too far from its last location
        // thus we have less poly to cover
        // sub-path of optimal path is optimal

        // take ~80% of the original length
        // TODO : play with the values here
        uint32 prefixPolyLength = uint32(m_polyLength * 0.8f + 0.5f);
        memmove(m_pathPolyRefs, m_pathPolyRefs + pathStartIndex, prefixPolyLength * sizeof(dtPolyRef));

        dtPolyRef suffixStartPoly = m_pathPolyRefs[prefixPolyLength - 1];

        // we need any point on our suffix start poly to generate poly-path, so we need last poly in prefix data
        float suffixEndPoint[VERTEX_SIZE];
        dtResult = m_navMeshQuery->closestPointOnPoly(suffixStartPoly, endPoint, suffixEndPoint, NULL);
        if (dtStatusFailed(dtResult))
        {
            // we can hit offmesh connection as last poly - closestPointOnPoly() don't like that
            // try to recover by using prev polyref
            if (prefixPolyLength < 2)
            {
                BuildShortcut();
                m_type = PATHFIND_NOPATH;
                return;
            }
            --prefixPolyLength;
            suffixStartPoly = m_pathPolyRefs[prefixPolyLength - 1];
            dtResult = m_navMeshQuery->closestPointOnPoly(suffixStartPoly, endPoint, suffixEndPoint, NULL);
            if (dtStatusFailed(dtResult))
            {
                // suffixStartPoly is still invalid, error state
                BuildShortcut();
                m_type = PATHFIND_NOPATH;
                return;
            }
        }

        // generate suffix
        int suffixFound = 0;
        dtResult = m_navMeshQuery->findPath(
            suffixStartPoly,    // start polygon
            endPoly,            // end polygon
            suffixEndPoint,     // start position
            endPoint,           // end position
            &m_filter,          // polygon search filter
            m_pathPolyRefs + prefixPolyLength - 1,    // [out] path
            &suffixFound,
            MAX_PATH_LENGTH - prefixPolyLength); // max number of polygons in output path
        const uint32 suffixPolyLength = uint32(suffixFound);

        if (!suffixPolyLength || dtStatusFailed(dtResult))
        {
            // this is probably an error state, but we'll leave it
            // and hopefully recover on the next Update
            // we still need to copy our preffix
            sLog.outError("%u's Path Build failed: 0 length path", m_sourceUnit->GetGUIDLow());
        }

        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ m_polyLength=%u prefixPolyLength=%u suffixPolyLength=%u for %s\n",
            m_polyLength, prefixPolyLength, suffixPolyLength, m_sourceUnit->GetGuidStr().c_str());

        // new path = prefix + suffix - overlap
        m_polyLength = (suffixPolyLength && !dtStatusFailed(dtResult))
                           ? prefixPolyLength + suffixPolyLength - 1
                           : prefixPolyLength;
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: (!startPolyFound && !endPolyFound) for %s\n", m_sourceUnit->GetGuidStr().c_str());

        // either we have no path at all -> first run
        // or something went really wrong -> we aren't moving along the path to the target

        // just generate new path

        // free and invalidate old path data
        clear();

        int polyFound = 0;
        dtResult = m_navMeshQuery->findPath(
            startPoly,          // start polygon
            endPoly,            // end polygon
            startPoint,         // start position
            endPoint,           // end position
            &m_filter,          // polygon search filter
            m_pathPolyRefs,     // [out] path
            &polyFound,
            MAX_PATH_LENGTH);   // max number of polygons in output path
        m_polyLength = uint32(polyFound);

        if (!m_polyLength || dtStatusFailed(dtResult))
        {
            // only happens if we passed bad data to findPath(), or navmesh is messed up
            sLog.outError("Path Build failed: 0 length path for %s", m_sourceUnit->GetGuidStr().c_str());
            BuildShortcut();
            m_type = PATHFIND_NOPATH;
            return;
        }
    }

    // by now we know what type of path we can get
    if (m_pathPolyRefs[m_polyLength - 1] == endPoly && !(m_type & PATHFIND_INCOMPLETE))
    {
        m_type = PATHFIND_NORMAL;
    }
    else
    {
        m_type = PATHFIND_INCOMPLETE;
    }

    // generate the point-path out of our up-to-date poly-path
    BuildPointPath(startPoint, endPoint);
}

/**
 * @brief Builds the point path from the start point to the end point.
 * @param startPoint The start point.
 * @param endPoint The end point.
 */
void PathFinder::BuildPointPath(const float* startPoint, const float* endPoint)
{
    float pathPoints[MAX_POINT_PATH_LENGTH * VERTEX_SIZE];
    int pointFound = 0;
    const dtStatus dtResult = m_navMeshQuery->findStraightPath(startPoint, endPoint, m_pathPolyRefs,
                                                               int(m_polyLength), pathPoints, NULL, NULL,
                                                               &pointFound, MAX_POINT_PATH_LENGTH);
    const uint32 pointCount = uint32(pointFound);

    if (pointCount < 2 || dtStatusFailed(dtResult))
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::BuildPointPath FAILED! path sized %u returned for %s\n", pointCount, m_sourceUnit->GetGuidStr().c_str());
        BuildShortcut();
        m_type = PATHFIND_NOPATH;
        return;
    }

    if (dtStatusDetail(dtResult, DT_BUFFER_TOO_SMALL))
    {
        m_type = PATHFIND_INCOMPLETE;
    }

    m_pathPoints.resize(pointCount);
    for (uint32 i = 0; i < pointCount; ++i)
    {
        m_pathPoints[i] = Vector3(pathPoints[i * VERTEX_SIZE + 2], pathPoints[i * VERTEX_SIZE], pathPoints[i * VERTEX_SIZE + 1]);
    }

    PathPolyline::CutToLength(m_pathPoints, m_pathLengthLimit);
    PathPolyline::Subdivide(m_pathPoints, PathPolyline::MAX_EDGE);
    NormalizePath();
    AcceptAgainstWorld();

    setActualEndPosition(m_pathPoints.back());

    if (m_forceDestination && !m_destBeyondMesh &&
        !inRange(getEndPosition(), getActualEndPosition(), 1.0f, 1.0f))
    {
        const Vector3 last = m_pathPoints.back();
        const Vector3 end = getEndPosition();
        if (m_sourceUnit->GetMap()->IsInLineOfSight(last.x, last.y, last.z + COLLISION_PROBE_HEIGHT,
                                                    end.x, end.y, end.z + COLLISION_PROBE_HEIGHT))
        {
            m_pathPoints.push_back(end);
            PathPolyline::Subdivide(m_pathPoints, PathPolyline::MAX_EDGE);
            setActualEndPosition(end);
            m_type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
        }
    }

    DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::BuildPointPath path type %d size %d poly-size %u for %s\n",
        m_type, int(m_pathPoints.size()), m_polyLength, m_sourceUnit->GetGuidStr().c_str());
}

/**
 * @brief Builds a shortcut path directly from the start position to the end position.
 */
void PathFinder::BuildShortcut()
{
    DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::BuildShortcut :: making shortcut for %s\n", m_sourceUnit->GetGuidStr().c_str());

    clear();

    Vector3 start = getStartPosition();
    Vector3 end = getActualEndPosition();

    // Subdivide the shortcut into segments so that each point can be snapped
    // to terrain height. This prevents the "bunny hop" effect on steep
    // slopes when no navmesh is available.
    const float segmentLength = 5.0f;
    float dist = sqrt(dist3DSqr(start, end));
    uint32 segments = std::max(1u, uint32(dist / segmentLength));
    uint32 size = segments + 1;

    m_pathPoints.resize(size);
    m_pathPoints[0] = start;
    m_pathPoints[size - 1] = end;

    for (uint32 i = 1; i < size - 1; ++i)
    {
        float t = float(i) / float(segments);
        Vector3 point = start + (end - start) * t;
        if (!m_sourceUnit->GetMap()->GetTerrain()->IsInWater(point.x, point.y, point.z))
            ClampToAllowedZ(*m_sourceUnit, point.x, point.y, point.z);
        m_pathPoints[i] = point;
    }

    m_type = PATHFIND_SHORTCUT;
}

/**
 * @brief Creates a filter for the pathfinding algorithm.
 */
void PathFinder::createFilter()
{
    uint16 includeFlags = 0;
    uint16 excludeFlags = 0;

    if (m_sourceUnit->GetTypeId() == TYPEID_UNIT)
    {
        Creature* creature = (Creature*)m_sourceUnit;
        if (creature->CanWalk())
        {
            includeFlags |= NAV_GROUND;
        }

        if (creature->CanSwim())
        {
            includeFlags |= NAV_WATER | NAV_DEEP_WATER;
        }

        const uint32 immune = creature->GetCreatureInfo()->SchoolImmuneMask;
        if (immune & SPELL_SCHOOL_MASK_FIRE)
        {
            includeFlags |= NAV_MAGMA;
        }
        if (immune & SPELL_SCHOOL_MASK_NATURE)
        {
            includeFlags |= NAV_SLIME;
        }
    }
    else if (m_sourceUnit->GetTypeId() == TYPEID_PLAYER)
    {
        includeFlags |= NAV_GROUND | NAV_WATER;
    }

    m_filter.setIncludeFlags(includeFlags);
    m_filter.setExcludeFlags(excludeFlags);

    updateFilter();
}

/**
 * @brief Updates the filter for the pathfinding algorithm.
 */
void PathFinder::updateFilter()
{
    // allow creatures to cheat and use different movement types if they are moved
    // forcefully into terrain they can't normally move in
    if (m_sourceUnit->IsInWater() || m_sourceUnit->IsUnderWater())
    {
        uint16 includedFlags = m_filter.getIncludeFlags();
        includedFlags |= getNavTerrain(m_sourceUnit->Where().X(),
            m_sourceUnit->Where().Y(),
            m_sourceUnit->Where().Z());

        m_filter.setIncludeFlags(includedFlags);
    }
}

/**
 * @brief Gets the navigation terrain type at the specified coordinates.
 * @param x The X-coordinate.
 * @param y The Y-coordinate.
 * @param z The Z-coordinate.
 * @return The navigation terrain type.
 */
NavTerrain PathFinder::getNavTerrain(float x, float y, float z)
{
    GridMapLiquidData data;
    m_sourceUnit->GetMap()->GetTerrain()->getLiquidStatus(x, y, z, MAP_ALL_LIQUIDS, &data);

    switch (data.type_flags)
    {
        case MAP_LIQUID_TYPE_WATER:
        case MAP_LIQUID_TYPE_OCEAN:
            return NavTerrain(NAV_WATER | NAV_DEEP_WATER);
        case MAP_LIQUID_TYPE_MAGMA:
            return NAV_MAGMA;
        case MAP_LIQUID_TYPE_SLIME:
            return NAV_SLIME;
        default:
            return NAV_GROUND;
    }
}

/**
 * @brief Checks if the specified point has a tile in the navigation mesh.
 * @param p The point to check.
 * @return True if the point has a tile, false otherwise.
 */
bool PathFinder::HaveTile(const Vector3& p) const
{
    int tx, ty;
    float point[VERTEX_SIZE] = {p.y, p.z, p.x};

    m_navMesh->calcTileLoc(point, &tx, &ty);
    return (m_navMesh->getTileAt(tx, ty, 0) != NULL);
}

/**
 * @brief Checks whether two path points are within horizontal range and vertical tolerance.
 *
 * @param p1 The first point.
 * @param p2 The second point.
 * @param r The maximum horizontal range.
 * @param h The maximum vertical difference.
 * @return true if the points are within range; otherwise false.
 */
bool PathFinder::inRange(const Vector3& p1, const Vector3& p2, float r, float h) const
{
    Vector3 d = p1 - p2;
    return (d.x * d.x + d.y * d.y) < r * r && fabsf(d.z) < h;
}

/**
 * @brief Returns the squared three-dimensional distance between two points.
 *
 * @param p1 The first point.
 * @param p2 The second point.
 * @return float The squared distance.
 */
float PathFinder::dist3DSqr(const Vector3& p1, const Vector3& p2) const
{
    return (p1 - p2).squaredLength();
}

/**
 * @brief Normalizes computed path points against allowed terrain heights.
 */
void PathFinder::NormalizePath()
{
    for (Vector3& point : m_pathPoints)
    {
        ClampToAllowedZ(*m_sourceUnit, point.x, point.y, point.z);
    }
}
