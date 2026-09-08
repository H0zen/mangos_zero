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

// Every side and every centre named here is one that a cast actually asks for.
// The old set carried three more -- a circle drawn around the victim, and the
// two sides that read "everyone but his own" and "everyone but his enemies" --
// which no call site ever passed. They are gone rather than kept against a use
// nobody has.

#pragma once

#include "Creature/Creature.h"
#include "GridDefines.h"
#include "Object/Occupant.h"
#include "Unit/Reaction.h"
#include "Utilities/MathDefines.h"

#include <list>

namespace cast
{
    /// Which side of the caster an area is allowed to catch.
    enum class Side : uint8
    {
        Hostile,        ///< only those he is at war with
        Friendly,       ///< only his own side
        HostileForArea, ///< as Hostile, except a player's area spares his own side and passes over totems
        Anyone
    };

    /// What an area is drawn around.
    ///
    /// A circle drawn around the caster is not the same as a circle drawn around
    /// a spot: the first counts both bounding radii, so a large creature is
    /// caught by an edge that would miss a point standing where it stands.
    enum class Around : uint8
    {
        CasterInFront,      ///< the wide cone a cleave opens, 120 degrees
        CasterInFront90,
        CasterInFront15,
        CasterBehind,       ///< the same width, opening the other way
        Caster,
        Spot
    };

    /// Where a spell's area sits and how far it goes.
    struct Reach
    {
        Around where = Around::Spot;
        float radius = 0.0f;
        Occupant* from = nullptr;   ///< the caster the cones open from and the circle is drawn around
        Geometry::Vector3 at;       ///< the spot a Spot circle is drawn around
    };

    /**
     * @brief Everyone an area catches.
     *
     * Walks the grid cells the area covers and keeps whoever stands inside it on
     * the right side of the caster. It is handed the centre already worked out,
     * so it knows nothing about the cast that opened it.
     */
    class Catchment
    {
        public:

            Catchment(std::list<Unit*>& into, const Reach& reach, Side side, Occupant* origin,
                      bool catchesEveryone, bool takesTheDead)
                : m_into(into), m_reach(reach), m_side(side), m_origin(origin),
                  m_catchesEveryone(catchesEveryone), m_takesTheDead(takesTheDead),
                  m_originIsPlayerLed(origin != nullptr && origin->IsControlledByPlayer())
            {
            }

            template<class T> void Visit(GridRefManager<T>& cell)
            {
                if (m_origin == nullptr)
                {
                    return;
                }

                for (auto reference = cell.begin(); reference != cell.end(); ++reference)
                {
                    Unit* who = reference->getSource();

                    if (!m_catchesEveryone && !OnTheRightSide(*who))
                    {
                        continue;
                    }

                    if (Inside(*who))
                    {
                        m_into.push_back(who);
                    }
                }
            }

        private:

            bool OnTheRightSide(Unit& who) const
            {
                if (m_side != Side::Anyone && !who.IsTargetableForAttack(m_takesTheDead))
                {
                    return false;
                }

                // mostly a phase check
                if (!who.Where().ShareFrame(m_origin->Where()))
                {
                    return false;
                }

                switch (m_side)
                {
                    case Side::Hostile:
                        return IsHostile(*m_origin, who);
                    case Side::Friendly:
                        return IsFriendly(*m_origin, who);
                    case Side::HostileForArea:
                        if (who.IsCreature() && static_cast<Creature&>(who).IsTotem())
                        {
                            return false;
                        }
                        return m_originIsPlayerLed ? !IsFriendly(*m_origin, who) : IsHostile(*m_origin, who);
                    case Side::Anyone:
                        return true;
                }

                return false;
            }

            bool Inside(Unit& who) const
            {
                if (m_reach.where == Around::Spot)
                {
                    return who.Where().WithinDist(m_reach.at, m_reach.radius);
                }

                // every other centre is a thing, and a cast can be left without one
                if (m_reach.from == nullptr)
                {
                    return false;
                }

                switch (m_reach.where)
                {
                    case Around::CasterInFront:
                        return InFrontPhased(*m_reach.from, who, m_reach.radius, 2 * M_PI_F / 3);
                    case Around::CasterInFront90:
                        return InFrontPhased(*m_reach.from, who, m_reach.radius, M_PI_F / 2);
                    case Around::CasterInFront15:
                        return InFrontPhased(*m_reach.from, who, m_reach.radius, M_PI_F / 12);
                    case Around::CasterBehind:
                        return InBackPhased(*m_reach.from, who, m_reach.radius, 2 * M_PI_F / 3);
                    case Around::Caster:
                        return m_reach.from->Where().WithinDist(who.Where(), m_reach.radius);
                    case Around::Spot:
                        break;
                }

                return false;
            }

            std::list<Unit*>& m_into;
            Reach m_reach;
            Side m_side;
            Occupant* m_origin;
            bool m_catchesEveryone;
            bool m_takesTheDead;
            bool m_originIsPlayerLed;
    };

    // Only creatures and players stand on a side and can be caught.
    template<> inline void Catchment::Visit(CorpseMapType&) {}
    template<> inline void Catchment::Visit(GameObjectMapType&) {}
    template<> inline void Catchment::Visit(DynamicObjectMapType&) {}
    template<> inline void Catchment::Visit(CameraMapType&) {}
}
