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

#include "Utilities/MathDefines.h"
#include "MotionFrame.h"
#include "Map.h"
#include "PathFinder.h"
#include "Player.h"
#include "Transports.h"
#include "TransportMap.h"
#include "Unit.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace Motion
{
    namespace
    {

        constexpr float DEFAULT_PATH_LENGTH =
            float(MAX_POINT_PATH_LENGTH) * SMOOTH_PATH_STEP_SIZE;

        class WorldPathQuery final : public IPathQuery
        {
            public:
                explicit WorldPathQuery(Unit const& mover) : m_path(&mover) {}

                bool Calculate(Vector3 const& start, Vector3 const& goal,
                               bool forceDestination, float lengthLimit) override
                {
                    m_path.setPathLengthLimit(lengthLimit > 0.0f ? lengthLimit
                                                                 : DEFAULT_PATH_LENGTH);

                    if (!m_path.calculate(start.x, start.y, start.z,
                                          goal.x, goal.y, goal.z, forceDestination))
                    {
                        return false;
                    }

                    return m_path.getPath().size() >= 2;
                }

                PointsArray const& Points() const override { return m_path.getPath(); }

                bool Failed() const override
                {
                    return (m_path.getPathType() & PATHFIND_NOPATH) != 0;
                }

                bool Routed() const override
                {
                    return (m_path.getPathType() &
                            (PATHFIND_NOPATH | PATHFIND_NOT_USING_PATH)) == 0;
                }

                bool Reachable() const override
                {
                    return (m_path.getPathType() & PATHFIND_NORMAL) != 0;
                }

            private:

                mutable PathFinder m_path;
        };

        class WorldFrame : public IMotionFrame
        {
            public:
                FrameKind Kind() const override { return FrameKind::World; }

                std::unique_ptr<IPathQuery> CreatePathQuery(Unit const& mover) const override
                {
                    return std::make_unique<WorldPathQuery>(mover);
                }

                Vector3 MoverPosition(Unit const& mover) const override
                {
                    Vector3 p;
                    p.x = mover.Where().X(), p.y = mover.Where().Y(), p.z = mover.Where().Z();
                    return p;
                }

                Vector3 FromWorld(Unit const& , Vector3 const& world) const override
                {
                    return world;
                }

                Vector3 ObjectPosition(Unit const& , Occupant const& obj) const override
                {
                    Vector3 p;
                    p.x = obj.Where().X(), p.y = obj.Where().Y(), p.z = obj.Where().Z();
                    return p;
                }

                float ObjectOrientation(Unit const& , Occupant const& obj) const override
                {
                    return obj.Where().Facing();
                }

                Vector3 NearPoint(Unit const& mover, Occupant const& target,
                                  float searcherBounding, float distance2d,
                                  float absAngle) const override
                {
                    Vector3 p;
                    FindFreeSpotNear(target, &mover, p.x, p.y, p.z, searcherBounding, distance2d, absAngle);
                    return p;
                }

                std::optional<Vector3> RandomPoint(Unit& mover, Vector3 const& centre,
                                                   float radius) const override
                {
                    Vector3 p = centre;
                    if (!mover.GetMap()->GetReachableRandomPosition(&mover, p.x, p.y, p.z, radius))
                    {
                        return std::nullopt;
                    }

                    return p;
                }

                std::optional<Vector3> GroundPoint(Unit& mover, Vector3 const& from,
                                                   Vector3 const& guess) const override
                {
                    Map* map = mover.GetMap();

                    Vector3 p = guess;
                    if (auto floor = map->FloorNear(p.x, p.y, p.z))
                    {
                        p.z = *floor;
                    }
                    else
                    {
                        return std::nullopt;
                    }

                    if (IsPlayer(&mover))
                    {
                        float testZ = p.z + 0.5f;
                        if (map->GetHitPosition(from.x, from.y, from.z + 0.5f,
                                                p.x, p.y, testZ, -0.1f))
                        {
                            p.z = testZ;
                            if (auto floor = map->FloorNear(p.x, p.y, p.z))
                            {
                                p.z = *floor;
                            }
                            else
                            {
                                return std::nullopt;
                            }
                        }
                    }

                    return p;
                }
        };

        constexpr float DECK_SEARCH_UP = 2.0f;
        constexpr float DECK_SEARCH_DOWN = 6.0f;

        constexpr float DECK_PROBE_HEIGHT = 1.0f;

        constexpr int DECK_RANDOM_TRIES = 8;

        std::optional<Vector3> DeckDrop(TransportMap const& hull, Vector3 const& local)
        {
            const auto z = hull.SurfaceAt(local.x, local.y, local.z,
                                          DECK_SEARCH_UP, DECK_SEARCH_DOWN);
            if (!z)
            {
                return std::nullopt;
            }

            return Vector3(local.x, local.y, *z);
        }

        bool DeckBlocked(TransportMap const& hull, Vector3 const& from, Vector3 const& to)
        {
            return hull.IsBlocked(Vector3(from.x, from.y, from.z + DECK_PROBE_HEIGHT),
                                  Vector3(to.x, to.y, to.z + DECK_PROBE_HEIGHT));
        }

        class DeckPathQuery final : public IPathQuery
        {
            public:
                DeckPathQuery(Unit const& mover, uint32 deckMapId)
                    : m_path(&mover, deckMapId)
                {
                }

                bool Calculate(Vector3 const& start, Vector3 const& goal,
                               bool forceDestination, float lengthLimit) override
                {
                    m_path.setPathLengthLimit(lengthLimit > 0.0f ? lengthLimit
                                                                 : DEFAULT_PATH_LENGTH);

                    if (!m_path.calculate(start.x, start.y, start.z,
                                          goal.x, goal.y, goal.z, forceDestination))
                    {
                        return false;
                    }

                    return m_path.getPath().size() >= 2;
                }

                PointsArray const& Points() const override { return m_path.getPath(); }

                bool Failed() const override
                {
                    return (m_path.getPathType() & PATHFIND_NOPATH) != 0;
                }

                bool Routed() const override
                {
                    return (m_path.getPathType() &
                            (PATHFIND_NOPATH | PATHFIND_NOT_USING_PATH)) == 0;
                }

                bool Reachable() const override
                {
                    return (m_path.getPathType() & PATHFIND_NORMAL) != 0;
                }

            private:
                mutable PathFinder m_path;
        };

        class TransportFrame final : public WorldFrame
        {
            public:
                FrameKind Kind() const override { return FrameKind::Transport; }

                Vector3 NearPoint(Unit const& mover, Occupant const& target,
                                  float , float distance2d,
                                  float absAngle) const override
                {

                    const Vector3 t = ObjectPosition(mover, target);
                    const Vector3 guess(t.x + distance2d * std::cos(absAngle),
                                        t.y + distance2d * std::sin(absAngle),
                                        t.z);

                    TransportMap* hull = mover.GetMap()->AsTransport();
                    if (!hull)
                    {
                        return guess;
                    }

                    if (const auto onDeck = DeckDrop(*hull, guess))
                    {
                        return *onDeck;
                    }

                    return guess;
                }

                std::optional<Vector3> RandomPoint(Unit& mover, Vector3 const& centre,
                                                   float radius) const override
                {
                    TransportMap* hull = mover.GetMap()->AsTransport();
                    if (!hull)
                    {
                        return std::nullopt;
                    }

                    for (int attempt = 0; attempt < DECK_RANDOM_TRIES; ++attempt)
                    {
                        const float angle = frand(0.0f, 2 * M_PI_F);
                        const float dist = radius * std::sqrt(frand(0.0f, 1.0f));

                        const Vector3 guess(centre.x + dist * std::cos(angle),
                                            centre.y + dist * std::sin(angle),
                                            centre.z);

                        const auto onDeck = DeckDrop(*hull, guess);
                        if (onDeck && !DeckBlocked(*hull, centre, *onDeck))
                        {
                            return onDeck;
                        }
                    }

                    return std::nullopt;
                }

                std::optional<Vector3> GroundPoint(Unit& mover, Vector3 const& from,
                                                   Vector3 const& guess) const override
                {
                    TransportMap* hull = mover.GetMap()->AsTransport();
                    if (!hull)
                    {
                        return std::nullopt;
                    }

                    const auto onDeck = DeckDrop(*hull, guess);
                    if (!onDeck)
                    {
                        return std::nullopt;

                    }

                    if (DeckBlocked(*hull, from, *onDeck))
                    {
                        return std::nullopt;
                    }

                    return onDeck;
                }
        };

        const WorldFrame s_worldFrame;
        const TransportFrame s_transportFrame;
    }

    IMotionFrame const& FrameFor(Unit const& mover)
    {

        if (Map const* on = mover.FindMap())
        {
            if (on->AsTransport())
            {
                return s_transportFrame;
            }
        }

        return s_worldFrame;
    }
}
