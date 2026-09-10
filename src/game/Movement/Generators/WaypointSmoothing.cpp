#include <algorithm>
#include "WaypointSmoothing.h"

bool IsWaypointSmoothingSafe(WaypointSmoothingNode const& node)
{

    return !node.hasDelay &&
           !node.hasScript &&
           !node.hasBehavior;
}

bool HasReachedWaypointEndpoint(int32 currentPathIdx, size_t endpointPathIndex)
{
    if (currentPathIdx <= 0)
    {
        return false;
    }

    return static_cast<size_t>(currentPathIdx) >= endpointPathIndex;
}

WaypointSegmentUpdateState GetWaypointSegmentUpdateState(bool splineFinalized, bool creatureStopped)
{

    if (creatureStopped)
    {
        return WaypointSegmentUpdateState::Stopped;
    }

    if (splineFinalized)
    {
        return WaypointSegmentUpdateState::Finalized;
    }

    return WaypointSegmentUpdateState::Moving;
}

void AddWaypointSmoothingPoint(WaypointSmoothingBounds& bounds, float x, float y, float z)
{
    if (!bounds.initialized)
    {
        bounds.minX = bounds.maxX = x;
        bounds.minY = bounds.maxY = y;
        bounds.minZ = bounds.maxZ = z;
        bounds.initialized = true;
        return;
    }

    bounds.minX = std::min(bounds.minX, x);
    bounds.maxX = std::max(bounds.maxX, x);
    bounds.minY = std::min(bounds.minY, y);
    bounds.maxY = std::max(bounds.maxY, y);
    bounds.minZ = std::min(bounds.minZ, z);
    bounds.maxZ = std::max(bounds.maxZ, z);
}

bool IsWaypointSmoothingWithinBudget(WaypointSmoothingBounds const& bounds)
{
    if (!bounds.initialized)
    {
        return true;
    }

    return (bounds.maxX - bounds.minX) <= WAYPOINT_SMOOTHING_MAX_XY_SPAN &&
           (bounds.maxY - bounds.minY) <= WAYPOINT_SMOOTHING_MAX_XY_SPAN &&
           (bounds.maxZ - bounds.minZ) <= WAYPOINT_SMOOTHING_MAX_Z_SPAN;
}
