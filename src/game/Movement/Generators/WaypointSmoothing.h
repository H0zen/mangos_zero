#pragma once

#include "Platform/Define.h"

constexpr size_t WAYPOINT_SMOOTHING_MAX_LOOKAHEAD = 32;

constexpr float WAYPOINT_SMOOTHING_MAX_XY_SPAN = 200.0f;

constexpr float WAYPOINT_SMOOTHING_MAX_Z_SPAN = 100.0f;

constexpr float WAYPOINT_SMOOTHING_MIN_SEGMENT_LENGTH = 0.1f;

struct WaypointSmoothingNode
{
    bool hasDelay = false;
    bool hasScript = false;
    bool hasBehavior = false;
};

enum class WaypointSegmentUpdateState
{
    Moving,
    Stopped,
    Finalized
};

struct WaypointSmoothingBounds
{
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    bool initialized = false;
};

bool IsWaypointSmoothingSafe(WaypointSmoothingNode const& node);

bool HasReachedWaypointEndpoint(int32 currentPathIdx, size_t endpointPathIndex);

WaypointSegmentUpdateState GetWaypointSegmentUpdateState(bool splineFinalized, bool creatureStopped);

void AddWaypointSmoothingPoint(WaypointSmoothingBounds& bounds, float x, float y, float z);

bool IsWaypointSmoothingWithinBudget(WaypointSmoothingBounds const& bounds);
