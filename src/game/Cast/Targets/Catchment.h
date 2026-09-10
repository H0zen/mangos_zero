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

#include "Creature/Creature.h"
#include "GridDefines.h"
#include "Object/Occupant.h"
#include "Unit/Reaction.h"
#include "Utilities/MathDefines.h"

#include <list>

namespace cast
{

    enum class Side : uint8
    {
        Hostile,
        Friendly,
        HostileForArea,
        Anyone
    };

    enum class Around : uint8
    {
        CasterInFront,
        CasterInFront90,
        CasterInFront15,
        CasterBehind,
        Caster,
        Spot
    };

    struct Reach
    {
        Around where = Around::Spot;
        float radius = 0.0f;
        Occupant* from = nullptr;
        Geometry::Vector3 at;
    };

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
                        if (IsCreature(&who) && static_cast<Creature&>(who).IsTotem())
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

    template<> inline void Catchment::Visit(CorpseMapType&) {}
    template<> inline void Catchment::Visit(GameObjectMapType&) {}
    template<> inline void Catchment::Visit(DynamicObjectMapType&) {}
    template<> inline void Catchment::Visit(CameraMapType&) {}
}
