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
 */

#include "Summoning.h"
#include "CinematicFlyover.h"
#include "Player.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "Map.h"
#include "Camera.h"
#include "Log.h"
#include "Config/Config.h"
#include "SharedDefines.h"

static const uint32 CONFIG_UPDATE_INTERVAL_MS = 200;
static const uint32 CONFIG_TIMEOUT_SEC = 120;
static const uint32 CONFIG_VISIBILITY_DISTANCE = 250;

CinematicFlyover::CinematicFlyover(Player* player, uint8 raceId)
    : m_player(player), m_route(nullptr), m_viewerMap(nullptr),
      m_viewerRadius(0.0f), m_bodyEntry(0), m_elapsedMs(0),
      m_updateTimer(0), m_timeoutMs(0), m_armed(false), m_begun(false),
      m_active(false)
{

    if (!sConfig.GetBoolDefault("Cinematic.Flyover.Enable", false))
    {
        sLog.outDebug("CinematicFlyover: Feature disabled by config");
        return;
    }

    m_route = GetCinematicFlyoverRouteForRace(raceId);
    if (!m_route)
    {
        sLog.outDebug("CinematicFlyover: No route found for race %u", raceId);
        return;
    }

    if (m_route->mapId != m_player->GetMapId())
    {
        sLog.outDebug("CinematicFlyover: Route map %u does not match player map %u",
                      m_route->mapId, m_player->GetMapId());
        return;
    }

    if (m_route->keyframeCount == 0)
    {
        sLog.outError("CinematicFlyover: Route has no keyframes");
        return;
    }

    uint32 bodyEntry = sConfig.GetIntDefault("Cinematic.Flyover.BodyEntry",
                                            12999);
    if (bodyEntry == 0)
    {
        sLog.outError("CinematicFlyover: BodyEntry is 0, cannot create body");
        return;
    }

    if (!ObjectMgr::GetCreatureTemplate(bodyEntry))
    {
        sLog.outError("CinematicFlyover: Creature entry %u not found in "
                      "creature_template", bodyEntry);
        return;
    }

    m_bodyEntry = bodyEntry;

    m_armed = true;

    sLog.outDebug("CinematicFlyover: Armed for player %s (race %u), "
                  "awaiting cinematic start", m_player->GetName(), raceId);
}

void CinematicFlyover::Begin()
{

    if (!m_armed || m_begun)
    {
        return;
    }

    if (!m_player || !m_player->IsInWorld() || !m_route ||
        m_route->mapId != m_player->GetMapId())
    {
        sLog.outDebug("CinematicFlyover: Begin aborted, player not on route map");
        m_armed = false;
        return;
    }

    const CinematicFlyoverKeyframe& startKeyframe = m_route->keyframes[0];

    uint32 despawnMs = m_route->durationMs + (CONFIG_TIMEOUT_SEC * 1000) +
                       60000;
    Creature* body = SummonCreature(*m_player, m_bodyEntry, startKeyframe.x,
                                              startKeyframe.y, startKeyframe.z,
                                              startKeyframe.orientation,
                                              TEMPSPAWN_TIMED_DESPAWN,
                                              despawnMs, true, false);

    if (!body)
    {
        sLog.outError("CinematicFlyover: Failed to summon body creature entry %u", m_bodyEntry);
        m_armed = false;
        return;
    }

    m_bodyGuid = body->GetObjectGuid();

    float visDist = float(sConfig.GetIntDefault(
                          "Cinematic.Flyover.VisibilityDistance",
                          CONFIG_VISIBILITY_DISTANCE));
    body->SetVisibilityDistanceOverride(visDist);

    m_viewerMap = body->GetMap();
    m_viewerRadius = visDist;
    m_viewerMap->AddCinematicViewer(m_viewerRadius);

    m_player->GetCamera().SetView(body, true);

    m_elapsedMs = 0;
    m_updateTimer = 0;
    m_timeoutMs = m_route->durationMs + (CONFIG_TIMEOUT_SEC * 1000);
    m_begun = true;
    m_active = true;

    sLog.outDebug("CinematicFlyover: Began for player %s, route duration %u ms",
                  m_player->GetName(), m_route->durationMs);
}

CinematicFlyover::~CinematicFlyover()
{
    Stop();
}

void CinematicFlyover::Update(uint32 updateDiff)
{
    if (!m_active)
    {
        return;
    }

    Creature* body = ResolveBody();
    if (!body)
    {
        sLog.outDebug("CinematicFlyover: Body no longer valid, stopping");
        Stop();
        return;
    }

    m_elapsedMs += updateDiff;

    m_updateTimer += updateDiff;
    uint32 updateInterval = sConfig.GetIntDefault(
                              "Cinematic.Flyover.UpdateIntervalMs",
                              CONFIG_UPDATE_INTERVAL_MS);
    if (m_updateTimer < updateInterval)
    {
        return;
    }

    m_updateTimer -= updateInterval;
    if (m_updateTimer > updateInterval)
    {
        m_updateTimer = updateInterval;
    }

    if (m_elapsedMs > m_timeoutMs)
    {
        sLog.outDebug("CinematicFlyover: Timeout after %u ms", m_elapsedMs);
        Stop();
        return;
    }

    float x, y, z, o;
    if (!InterpolatePosition(m_elapsedMs, x, y, z, o))
    {
        sLog.outDebug("CinematicFlyover: Interpolation failed, stopping");
        Stop();
        return;
    }

    if (body->GetMap())
    {
        body->GetMap()->CreatureRelocation(body, x, y, z, o);

        if (sConfig.GetBoolDefault("Cinematic.Flyover.Debug", false))
        {
            sLog.outDebug("CinematicFlyover: Relocated body to (%.2f, %.2f, %.2f, %.2f)",
                          x, y, z, o);
        }
    }
}

void CinematicFlyover::Stop()
{

    m_armed = false;

    if (!m_active)
    {
        return;
    }

    sLog.outDebug("CinematicFlyover: Stopping for player %s",
                  m_player->GetName());

    m_player->GetCamera().ResetView(true);

    Creature* body = ResolveBody();
    if (body)
    {
        body->AddObjectToRemoveList();
    }

    if (m_viewerMap)
    {
        m_viewerMap->RemoveCinematicViewer(m_viewerRadius);
        m_viewerMap = nullptr;
        m_viewerRadius = 0.0f;
    }

    m_bodyGuid = 0;
    m_active = false;
}

bool CinematicFlyover::InterpolatePosition(uint32 atMs, float& x, float& y, float& z, float& o)
{
    if (!m_route || m_route->keyframeCount == 0)
    {
        return false;
    }

    if (atMs <= m_route->keyframes[0].timestampMs)
    {
        const CinematicFlyoverKeyframe& first = m_route->keyframes[0];
        x = first.x;
        y = first.y;
        z = first.z;
        o = first.orientation;
        return true;
    }

    const CinematicFlyoverKeyframe* prev = nullptr;
    const CinematicFlyoverKeyframe* next = nullptr;

    for (uint32 i = 0; i < m_route->keyframeCount - 1; ++i)
    {
        if (m_route->keyframes[i].timestampMs <= atMs &&
            m_route->keyframes[i + 1].timestampMs > atMs)
        {
            prev = &m_route->keyframes[i];
            next = &m_route->keyframes[i + 1];
            break;
        }
    }

    if (!prev && !next)
    {
        if (atMs >= m_route->keyframes[m_route->keyframeCount - 1].timestampMs)
        {
            const CinematicFlyoverKeyframe& last =
                m_route->keyframes[m_route->keyframeCount - 1];
            x = last.x;
            y = last.y;
            z = last.z;
            o = last.orientation;
            return true;
        }
        return false;
    }

    if (prev && next)
    {
        uint32 timeDiff = next->timestampMs - prev->timestampMs;
        if (timeDiff == 0)
        {
            return false;
        }

        float t = float(atMs - prev->timestampMs) / float(timeDiff);
        x = prev->x + t * (next->x - prev->x);
        y = prev->y + t * (next->y - prev->y);
        z = prev->z + t * (next->z - prev->z);

        o = prev->orientation + t * (next->orientation - prev->orientation);

        return true;
    }

    return false;
}

Creature* CinematicFlyover::ResolveBody() const
{
    if ((m_bodyGuid == 0))
    {
        return nullptr;
    }

    if (!m_player || !m_player->GetMap())
    {
        return nullptr;
    }

    return m_player->GetMap()->GetCreature(m_bodyGuid);
}
