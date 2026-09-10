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

#include <algorithm>
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"

#include "MoveMapSharedDefines.h"
#include "Movement/Spline/MoveSplineInitArgs.h"

using Movement::Vector3;
using Movement::PointsArray;

class Unit;

#define MAX_PATH_LENGTH         74
#define MAX_POINT_PATH_LENGTH   74

#define SMOOTH_PATH_STEP_SIZE   4.0f
#define SMOOTH_PATH_SLOP        0.3f
#define SMOOTH_PATH_HEIGHT      1.0f

#define VERTEX_SIZE       3
#define INVALID_POLYREF   0

enum PathType
{
    PATHFIND_BLANK          = 0x0000,
    PATHFIND_NORMAL         = 0x0001,
    PATHFIND_SHORTCUT       = 0x0002,
    PATHFIND_INCOMPLETE     = 0x0004,
    PATHFIND_NOPATH         = 0x0008,
    PATHFIND_NOT_USING_PATH = 0x0010
};

class PathFinder
{
    public:

        PathFinder(Unit const* owner);

        PathFinder(Unit const* owner, uint32 mapId);

        ~PathFinder();

        bool calculate(float destX, float destY, float destZ, bool forceDest = false);

        bool calculate(float startX, float startY, float startZ, float destX, float destY, float destZ, bool forceDest = false);

        void setUseStrightPath(bool useStraightPath) { m_useStraightPath = useStraightPath; };

        void setPathLengthLimit(float distance) { m_pointPathLimit = std::min<uint32>(uint32(distance / SMOOTH_PATH_STEP_SIZE), MAX_POINT_PATH_LENGTH); };

        Vector3 getStartPosition() const { return m_startPosition; }

        Vector3 getEndPosition() const { return m_endPosition; }

        Vector3 getActualEndPosition() const { return m_actualEndPosition; }

        void NormalizePath(uint32& size);

        PointsArray& getPath()
        {
            return m_pathPoints;
        }

        PathType getPathType() const { return m_type; }

    private:

        dtPolyRef      m_pathPolyRefs[MAX_PATH_LENGTH];
        uint32         m_polyLength;

        PointsArray    m_pathPoints;
        PathType       m_type;

        bool           m_useStraightPath;
        bool           m_forceDestination;
        uint32         m_pointPathLimit;

        Vector3        m_startPosition;
        Vector3        m_endPosition;
        Vector3        m_actualEndPosition;

        const Unit* const       m_sourceUnit;
        const dtNavMesh*        m_navMesh;
        const dtNavMeshQuery*   m_navMeshQuery;

        dtQueryFilter m_filter;

        void setStartPosition(const Vector3 &point) { m_startPosition = point; }

        void setEndPosition(const Vector3 &point) { m_actualEndPosition = point; m_endPosition = point; }

        void setActualEndPosition(const Vector3 &point) { m_actualEndPosition = point; }

        void clear()
        {
            m_polyLength = 0;
            m_pathPoints.clear();
        }

        bool inRange(const Vector3& p1, const Vector3& p2, float r, float h) const;

        float dist3DSqr(const Vector3& p1, const Vector3& p2) const;

        bool inRangeYZX(const float* v1, const float* v2, float r, float h) const;

        dtPolyRef getPathPolyByPosition(const dtPolyRef* polyPath, uint32 polyPathSize, const float* point, float* distance = nullptr) const;

        dtPolyRef getPolyByLocation(const float* point, float* distance) const;

        bool HaveTile(const Vector3& p) const;

        void BuildPolyPath(const Vector3& startPos, const Vector3& endPos);

        void BuildPointPath(const float* startPoint, const float* endPoint);

        void BuildShortcut();

        NavTerrain getNavTerrain(float x, float y, float z);

        void createFilter();

        void updateFilter();

        uint32 fixupCorridor(dtPolyRef* path, uint32 npath, uint32 maxPath,
            const dtPolyRef* visited, uint32 nvisited);

        bool getSteerTarget(const float* startPos, const float* endPos, float minTargetDist,
            const dtPolyRef* path, uint32 pathSize, float* steerPos,
            unsigned char& steerPosFlag, dtPolyRef& steerPosRef);

        dtStatus findSmoothPath(const float* startPos, const float* endPos,
            const dtPolyRef* polyPath, uint32 polyPathSize,
            float* smoothPath, int* smoothPathSize, uint32 smoothPathMaxSize);
};
