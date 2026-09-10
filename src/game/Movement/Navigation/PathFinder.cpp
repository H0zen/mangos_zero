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

#include <cmath>
#include "Utilities/Errors.h"
#include <algorithm>
#include "../recastnavigation/Detour/Include/DetourCommon.h"

#include "MoveMap.h"
#include "GridMap.h"
#include "Creature.h"
#include "Map.h"
#include "PathFinder.h"
#include "Log.h"
#include "Player.h"

#include <cfloat>

PathFinder::PathFinder(const Unit* owner)
    : PathFinder(owner, owner->GetMapId())
{
}

PathFinder::PathFinder(const Unit* owner, uint32 mapId)
    : m_polyLength(0), m_type(PATHFIND_BLANK),
    m_useStraightPath(false), m_forceDestination(false), m_pointPathLimit(MAX_POINT_PATH_LENGTH),
    m_sourceUnit(owner), m_navMesh(nullptr), m_navMeshQuery(nullptr)
{
    DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::PathFinder for %s \n", m_sourceUnit->GetGuidStr().c_str());

    memset(m_pathPolyRefs, 0, sizeof(m_pathPolyRefs));

    if (MMAP::MMapFactory::IsPathfindingEnabled(mapId, owner))
    {
        MMAP::MMapManager* mmap = MMAP::MMapFactory::createOrGetMMapManager();
        m_navMesh = mmap->GetNavMesh(mapId);
        m_navMeshQuery = mmap->GetNavMeshQuery(mapId, m_sourceUnit->GetInstanceId());
    }

    createFilter();
}

PathFinder::~PathFinder()
{
    DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::~PathFinder() for %s \n", m_sourceUnit->GetGuidStr().c_str());
}

bool PathFinder::calculate(float destX, float destY, float destZ, bool forceDest)
{
    float x, y, z;
    x = m_sourceUnit->Where().X();
    y = m_sourceUnit->Where().Y();
    z = m_sourceUnit->Where().Z();

    return calculate(x, y, z, destX, destY, destZ, forceDest);
}

bool PathFinder::calculate(float startX, float startY, float startZ, float destX, float destY, float destZ, bool forceDest)
{
    if (!MaNGOS::IsValidMapCoord(startX, startY, startZ) || !MaNGOS::IsValidMapCoord(destX, destY, destZ))
    {
        return false;
    }

    Vector3 start(startX, startY, startZ);
    setStartPosition(start);

    Vector3 dest(destX, destY, destZ);
    setEndPosition(dest);

    m_forceDestination = forceDest;

    DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::calculate() for %s \n", m_sourceUnit->GetGuidStr().c_str());

    if (!m_navMesh || !m_navMeshQuery || m_sourceUnit->hasUnitState(UNIT_STAT_IGNORE_PATHFINDING) ||
        !HaveTile(start) || !HaveTile(dest))
    {
        BuildShortcut();
        m_type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
        return true;
    }

    updateFilter();

    BuildPolyPath(start, dest);
    return true;
}

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

        if (dtStatusFailed(m_navMeshQuery->closestPointOnPoly(polyPath[i], point, closestPoint, nullptr)))
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

        if (minDist2d < 1.0f)
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

dtPolyRef PathFinder::getPolyByLocation(const float* point, float* distance) const
{

    dtPolyRef polyRef = getPathPolyByPosition(m_pathPolyRefs, m_polyLength, point, distance);
    if (polyRef != INVALID_POLYREF)
    {
        return polyRef;
    }

    float extents[VERTEX_SIZE] = {3.0f, 5.0f, 3.0f};
    float closestPoint[VERTEX_SIZE] = {0.0f, 0.0f, 0.0f};
    if (dtStatusSucceed(m_navMeshQuery->findNearestPoly(point, extents, &m_filter, &polyRef, closestPoint)) && polyRef != INVALID_POLYREF)
    {
        *distance = dtVdist(closestPoint, point);
        return polyRef;
    }

    extents[1] = 200.0f;
    if (dtStatusSucceed(m_navMeshQuery->findNearestPoly(point, extents, &m_filter, &polyRef, closestPoint)) && polyRef != INVALID_POLYREF)
    {
        *distance = dtVdist(closestPoint, point);
        return polyRef;
    }

    return INVALID_POLYREF;
}

void PathFinder::BuildPolyPath(const Vector3& startPos, const Vector3& endPos)
{

    float distToStartPoly, distToEndPoly;
    float startPoint[VERTEX_SIZE] = {startPos.y, startPos.z, startPos.x};
    float endPoint[VERTEX_SIZE] = {endPos.y, endPos.z, endPos.x};

    dtPolyRef startPoly = getPolyByLocation(startPoint, &distToStartPoly);
    dtPolyRef endPoly = getPolyByLocation(endPoint, &distToEndPoly);

    dtStatus dtResult;

    if (startPoly == INVALID_POLYREF || endPoly == INVALID_POLYREF)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: (startPoly == 0 || endPoly == 0) for %s\n", m_sourceUnit->GetGuidStr().c_str());
        BuildShortcut();

        if (IsCreature(m_sourceUnit))
        {

            if ((startPoly == INVALID_POLYREF && m_sourceUnit->GetMap()->GetTerrain()->IsUnderWater(startPos.x, startPos.y, startPos.z)) ||
                (endPoly == INVALID_POLYREF && m_sourceUnit->GetMap()->GetTerrain()->IsUnderWater(endPos.x, endPos.y, endPos.z)))
            {
                m_type = static_cast<Creature const*>(m_sourceUnit)->CanSwim() ? PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH) : PATHFIND_NOPATH;
            }
            else
            {
                m_type = static_cast<Creature const*>(m_sourceUnit)->CanFly() ? PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH) : PATHFIND_NOPATH;
            }
        }
        else
        {
            m_type = PATHFIND_NOPATH;
        }

        return;
    }

    bool farFromPoly = (distToStartPoly > 7.0f || distToEndPoly > 7.0f);
    if (farFromPoly)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: farFromPoly distToStartPoly=%.3f distToEndPoly=%.3f for %s\n",
            distToStartPoly, distToEndPoly, m_sourceUnit->GetGuidStr().c_str());

        bool buildShortcut = false;
        if (IsCreature(m_sourceUnit))
        {
            const Creature* owner = static_cast<Creature const*>(m_sourceUnit);

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

            dtResult = m_navMeshQuery->closestPointOnPoly(endPoly, endPoint, closestPoint, nullptr);
            if (dtStatusSucceed(dtResult))
            {
                dtVcopy(endPoint, closestPoint);
                setActualEndPosition(Vector3(endPoint[2], endPoint[0], endPoint[1]));
            }

            m_type = PATHFIND_INCOMPLETE;
        }
    }

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

    bool startPolyFound = false;
    bool endPolyFound = false;
    uint32 pathStartIndex, pathEndIndex;

    if (m_polyLength)
    {
        for (pathStartIndex = 0; pathStartIndex < m_polyLength; ++pathStartIndex)
        {

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

        m_polyLength = pathEndIndex - pathStartIndex + 1;
        memmove(m_pathPolyRefs, m_pathPolyRefs + pathStartIndex, m_polyLength * sizeof(dtPolyRef));
    }
    else if (startPolyFound && !endPolyFound)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: (startPolyFound && !endPolyFound) for %s\n", m_sourceUnit->GetGuidStr().c_str());

        m_polyLength -= pathStartIndex;

        uint32 prefixPolyLength = uint32(m_polyLength * 0.8f + 0.5f);
        memmove(m_pathPolyRefs, m_pathPolyRefs + pathStartIndex, prefixPolyLength * sizeof(dtPolyRef));

        dtPolyRef suffixStartPoly = m_pathPolyRefs[prefixPolyLength - 1];

        float suffixEndPoint[VERTEX_SIZE];
        dtResult = m_navMeshQuery->closestPointOnPoly(suffixStartPoly, endPoint, suffixEndPoint, nullptr);
        if (dtStatusFailed(dtResult))
        {

            --prefixPolyLength;
            suffixStartPoly = m_pathPolyRefs[prefixPolyLength - 1];
            dtResult = m_navMeshQuery->closestPointOnPoly(suffixStartPoly, endPoint, suffixEndPoint, nullptr);
            if (dtStatusFailed(dtResult))
            {

                BuildShortcut();
                m_type = PATHFIND_NOPATH;
                return;
            }
        }

        uint32 suffixPolyLength = 0;
        dtResult = m_navMeshQuery->findPath(
            suffixStartPoly,
            endPoly,
            suffixEndPoint,
            endPoint,
            &m_filter,
            m_pathPolyRefs + prefixPolyLength - 1,
            (int*)&suffixPolyLength,
            MAX_PATH_LENGTH - prefixPolyLength);

        if (!suffixPolyLength || dtStatusFailed(dtResult))
        {

            sLog.outError("%u's Path Build failed: 0 length path", m_sourceUnit->GetGUIDLow());
        }

        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ m_polyLength=%u prefixPolyLength=%u suffixPolyLength=%u for %s\n",
            m_polyLength, prefixPolyLength, suffixPolyLength, m_sourceUnit->GetGuidStr().c_str());

        m_polyLength = prefixPolyLength + suffixPolyLength - 1;
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ BuildPolyPath :: (!startPolyFound && !endPolyFound) for %s\n", m_sourceUnit->GetGuidStr().c_str());

        clear();

        dtResult = m_navMeshQuery->findPath(
            startPoly,
            endPoly,
            startPoint,
            endPoint,
            &m_filter,
            m_pathPolyRefs,
            (int*)&m_polyLength,
            MAX_PATH_LENGTH);

        if (!m_polyLength || dtStatusFailed(dtResult))
        {

            sLog.outError("Path Build failed: 0 length path for %s", m_sourceUnit->GetGuidStr().c_str());
            BuildShortcut();
            m_type = PATHFIND_NOPATH;
            return;
        }
    }

    if (m_pathPolyRefs[m_polyLength - 1] == endPoly && !(m_type & PATHFIND_INCOMPLETE))
    {
        m_type = PATHFIND_NORMAL;
    }
    else
    {
        m_type = PATHFIND_INCOMPLETE;
    }

    BuildPointPath(startPoint, endPoint);
}

void PathFinder::BuildPointPath(const float* startPoint, const float* endPoint)
{
    float pathPoints[MAX_POINT_PATH_LENGTH * VERTEX_SIZE];
    uint32 pointCount = 0;
    dtStatus dtResult;
    if (m_useStraightPath)
    {
        dtResult = m_navMeshQuery->findStraightPath(
            startPoint,
            endPoint,
            m_pathPolyRefs,
            m_polyLength,
            pathPoints,
            nullptr,
            nullptr,
            (int*)&pointCount,
            m_pointPathLimit);
    }
    else
    {
        dtResult = findSmoothPath(
            startPoint,
            endPoint,
            m_pathPolyRefs,
            m_polyLength,
            pathPoints,
            (int*)&pointCount,
            m_pointPathLimit);
    }

    if (pointCount < 2 || dtStatusFailed(dtResult))
    {

        DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::BuildPointPath FAILED! path sized %d returned for %s\n", pointCount, m_sourceUnit->GetGuidStr().c_str());
        BuildShortcut();
        m_type = PATHFIND_NOPATH;
        return;
    }

    m_pathPoints.resize(pointCount);
    for (uint32 i = 0; i < pointCount; ++i)
    {
        m_pathPoints[i] = Vector3(pathPoints[i * VERTEX_SIZE + 2], pathPoints[i * VERTEX_SIZE], pathPoints[i * VERTEX_SIZE + 1]);
    }

    NormalizePath(pointCount);

    setActualEndPosition(m_pathPoints[pointCount - 1]);

    if (m_forceDestination &&
        (!(m_type & PATHFIND_NORMAL) || !inRange(getEndPosition(), getActualEndPosition(), 1.0f, 1.0f)))
    {

        if (dist3DSqr(getActualEndPosition(), getEndPosition()) <
            0.3f * dist3DSqr(getStartPosition(), getEndPosition()))
        {
            setActualEndPosition(getEndPosition());
            m_pathPoints[m_pathPoints.size() - 1] = getEndPosition();
        }
        else
        {
            setActualEndPosition(getEndPosition());
            BuildShortcut();
        }

        m_type = PathType(PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH);
    }

    DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::BuildPointPath path type %d size %d poly-size %d for %s\n",
        m_type, pointCount, m_polyLength, m_sourceUnit->GetGuidStr().c_str());
}

void PathFinder::BuildShortcut()
{
    DEBUG_FILTER_LOG(LOG_FILTER_PATHFINDING, "++ PathFinder::BuildShortcut :: making shortcut for %s\n", m_sourceUnit->GetGuidStr().c_str());

    clear();

    Vector3 start = getStartPosition();
    Vector3 end = getActualEndPosition();

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

void PathFinder::createFilter()
{
    uint16 includeFlags = 0;
    uint16 excludeFlags = 0;

    if (IsCreature(m_sourceUnit))
    {
        Creature* creature = (Creature*)m_sourceUnit;
        if (creature->CanWalk())
        {
            includeFlags |= NAV_GROUND;
        }

        if (creature->CanSwim())
        {
            includeFlags |= (NAV_WATER | NAV_MAGMA | NAV_SLIME);
        }
    }
    else if (IsPlayer(m_sourceUnit))
    {

        includeFlags |= (NAV_GROUND | NAV_WATER);
    }

    m_filter.setIncludeFlags(includeFlags);
    m_filter.setExcludeFlags(excludeFlags);

    updateFilter();
}

void PathFinder::updateFilter()
{

    if (m_sourceUnit->IsInWater() || m_sourceUnit->IsUnderWater())
    {
        uint16 includedFlags = m_filter.getIncludeFlags();
        includedFlags |= getNavTerrain(m_sourceUnit->Where().X(),
            m_sourceUnit->Where().Y(),
            m_sourceUnit->Where().Z());

        m_filter.setIncludeFlags(includedFlags);
    }
}

NavTerrain PathFinder::getNavTerrain(float x, float y, float z)
{
    GridMapLiquidData data;
    m_sourceUnit->GetMap()->GetTerrain()->getLiquidStatus(x, y, z, MAP_ALL_LIQUIDS, &data);

    switch (data.type_flags)
    {
        case MAP_LIQUID_TYPE_WATER:
        case MAP_LIQUID_TYPE_OCEAN:
            return NAV_WATER;
        case MAP_LIQUID_TYPE_MAGMA:
            return NAV_MAGMA;
        case MAP_LIQUID_TYPE_SLIME:
            return NAV_SLIME;
        default:
            return NAV_GROUND;
    }
}

bool PathFinder::HaveTile(const Vector3& p) const
{
    int tx, ty;
    float point[VERTEX_SIZE] = {p.y, p.z, p.x};

    m_navMesh->calcTileLoc(point, &tx, &ty);
    return (m_navMesh->getTileAt(tx, ty, 0) != nullptr);
}

uint32 PathFinder::fixupCorridor(dtPolyRef* path, uint32 npath, uint32 maxPath,
    const dtPolyRef* visited, uint32 nvisited)
{
    int32 furthestPath = -1;
    int32 furthestVisited = -1;

    for (int32 i = npath - 1; i >= 0; --i)
    {
        bool found = false;
        for (int32 j = nvisited - 1; j >= 0; --j)
        {
            if (path[i] == visited[j])
            {
                furthestPath = i;
                furthestVisited = j;
                found = true;
            }
        }
        if (found)
        {
            break;
        }
    }

    if (furthestPath == -1 || furthestVisited == -1)
    {
        return npath;
    }

    uint32 req = nvisited - furthestVisited;
    uint32 orig = uint32(furthestPath + 1) < npath ? furthestPath + 1 : npath;
    uint32 size = npath > orig ? npath - orig : 0;
    if (req + size > maxPath)
    {
        size = maxPath - req;
    }

    if (size)
    {
        memmove(path + req, path + orig, size * sizeof(dtPolyRef));
    }

    for (uint32 i = 0; i < req; ++i)
    {
        path[i] = visited[(nvisited - 1) - i];
    }

    return req + size;
}

bool PathFinder::getSteerTarget(const float* startPos, const float* endPos,
    float minTargetDist, const dtPolyRef* path, uint32 pathSize,
    float* steerPos, unsigned char& steerPosFlag, dtPolyRef& steerPosRef)
{

    static const uint32 MAX_STEER_POINTS = 3;
    float steerPath[MAX_STEER_POINTS * VERTEX_SIZE];
    unsigned char steerPathFlags[MAX_STEER_POINTS];
    dtPolyRef steerPathPolys[MAX_STEER_POINTS];
    uint32 nsteerPath = 0;
    dtStatus dtResult = m_navMeshQuery->findStraightPath(startPos, endPos, path, pathSize,
        steerPath, steerPathFlags, steerPathPolys, (int*)&nsteerPath, MAX_STEER_POINTS);
    if (!nsteerPath || dtStatusFailed(dtResult))
    {
        return false;
    }

    uint32 ns = 0;
    while (ns < nsteerPath)
    {

        if ((steerPathFlags[ns] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ||
            !inRangeYZX(&steerPath[ns * VERTEX_SIZE], startPos, minTargetDist, 1000.0f))
        {
            break;
        }
        ++ns;
    }

    if (ns >= nsteerPath)
    {
        return false;
    }

    dtVcopy(steerPos, &steerPath[ns * VERTEX_SIZE]);
    steerPos[1] = startPos[1];
    steerPosFlag = steerPathFlags[ns];
    steerPosRef = steerPathPolys[ns];

    return true;
}

dtStatus PathFinder::findSmoothPath(const float* startPos, const float* endPos,
    const dtPolyRef* polyPath, uint32 polyPathSize,
    float* smoothPath, int* smoothPathSize, uint32 maxSmoothPathSize)
{
    *smoothPathSize = 0;
    uint32 nsmoothPath = 0;

    dtPolyRef polys[MAX_PATH_LENGTH];
    memcpy(polys, polyPath, sizeof(dtPolyRef)*polyPathSize);
    uint32 npolys = polyPathSize;

    float iterPos[VERTEX_SIZE], targetPos[VERTEX_SIZE];
    dtStatus dtResult = m_navMeshQuery->closestPointOnPolyBoundary(polys[0], startPos, iterPos);
    if (dtStatusFailed(dtResult))
    {
        return DT_FAILURE;
    }

    dtResult = m_navMeshQuery->closestPointOnPolyBoundary(polys[npolys - 1], endPos, targetPos);
    if (dtStatusFailed(dtResult))
    {
        return DT_FAILURE;
    }

    dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
    ++nsmoothPath;

    while (npolys && nsmoothPath < maxSmoothPathSize)
    {

        float steerPos[VERTEX_SIZE];
        unsigned char steerPosFlag;
        dtPolyRef steerPosRef = INVALID_POLYREF;

        if (!getSteerTarget(iterPos, targetPos, SMOOTH_PATH_SLOP, polys, npolys, steerPos, steerPosFlag, steerPosRef))
        {
            break;
        }

        bool endOfPath = (steerPosFlag & DT_STRAIGHTPATH_END);
        bool offMeshConnection = (steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION);

        float delta[VERTEX_SIZE];
        dtVsub(delta, steerPos, iterPos);
        float len = dtMathSqrtf(dtVdot(delta, delta));

        if ((endOfPath || offMeshConnection) && len < SMOOTH_PATH_STEP_SIZE)
        {
            len = 1.0f;
        }
        else
        {
            len = SMOOTH_PATH_STEP_SIZE / len;
        }

        float moveTgt[VERTEX_SIZE];
        dtVmad(moveTgt, iterPos, delta, len);

        float result[VERTEX_SIZE];
        const static uint32 MAX_VISIT_POLY = 16;
        dtPolyRef visited[MAX_VISIT_POLY];

        uint32 nvisited = 0;
        m_navMeshQuery->moveAlongSurface(polys[0], iterPos, moveTgt, &m_filter, result, visited, (int*)&nvisited, MAX_VISIT_POLY);
        npolys = fixupCorridor(polys, npolys, MAX_PATH_LENGTH, visited, nvisited);

        m_navMeshQuery->getPolyHeight(polys[0], result, &result[1]);
        result[1] += 0.5f;
        dtVcopy(iterPos, result);

        if (endOfPath && inRangeYZX(iterPos, steerPos, SMOOTH_PATH_SLOP, 1.0f))
        {

            dtVcopy(iterPos, targetPos);
            if (nsmoothPath < maxSmoothPathSize)
            {
                dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
                ++nsmoothPath;
            }
            break;
        }
        else if (offMeshConnection && inRangeYZX(iterPos, steerPos, SMOOTH_PATH_SLOP, 1.0f))
        {

            dtPolyRef prevRef = INVALID_POLYREF;
            dtPolyRef polyRef = polys[0];
            uint32 npos = 0;
            while (npos < npolys && polyRef != steerPosRef)
            {
                prevRef = polyRef;
                polyRef = polys[npos];
                ++npos;
            }

            for (uint32 i = npos; i < npolys; ++i)
            {
                polys[i - npos] = polys[i];
            }

            npolys -= npos;

            float newStartPos[VERTEX_SIZE], newEndPos[VERTEX_SIZE];

            dtResult = m_navMesh->getOffMeshConnectionPolyEndPoints(prevRef, polyRef, newStartPos, newEndPos);
            if (dtStatusSucceed(dtResult))
            {

                if (nsmoothPath < maxSmoothPathSize)
                {
                    dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], newStartPos);
                    ++nsmoothPath;
                }

                dtVcopy(iterPos, newEndPos);

                m_navMeshQuery->getPolyHeight(polys[0], iterPos, &iterPos[1]);
                iterPos[1] += 0.5f;
            }
        }

        if (nsmoothPath < maxSmoothPathSize)
        {
            dtVcopy(&smoothPath[nsmoothPath * VERTEX_SIZE], iterPos);
            ++nsmoothPath;
        }
    }

    *smoothPathSize = nsmoothPath;

    return nsmoothPath < MAX_POINT_PATH_LENGTH ? DT_SUCCESS : DT_FAILURE;
}

bool PathFinder::inRangeYZX(const float* v1, const float* v2, float r, float h) const
{
    const float dx = v2[0] - v1[0];
    const float dy = v2[1] - v1[1];
    const float dz = v2[2] - v1[2];
    return (dx * dx + dz * dz) < r * r && fabsf(dy) < h;
}

bool PathFinder::inRange(const Vector3& p1, const Vector3& p2, float r, float h) const
{
    Vector3 d = p1 - p2;
    return (d.x * d.x + d.y * d.y) < r * r && fabsf(d.z) < h;
}

float PathFinder::dist3DSqr(const Vector3& p1, const Vector3& p2) const
{
    return (p1 - p2).squaredLength();
}

void PathFinder::NormalizePath(uint32& size)
{
    for (uint32 i = 0; i < m_pathPoints.size(); ++i)
    {
        ClampToAllowedZ(*m_sourceUnit, m_pathPoints[i].x, m_pathPoints[i].y, m_pathPoints[i].z);
    }

    size = static_cast<uint32>(m_pathPoints.size());
}
