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

#include "Position.h"
#include "Map.h"

#include <optional>
#include <string>
#include <vector>

class Creature;
class Player;
class Transport;
class Unit;
class UpdateData;

std::string DescribeSpatially(Unit* u);

struct RelaySource
{
    Map* map;
    float x;
    float y;

    float radius;
};

class TransportMap : public Map
{
    public:
        TransportMap(uint32 id, time_t expiry, Transport* vessel)
            : Map(id, expiry, 0), m_vessel(vessel) {}
        ~TransportMap() {}

        void Update(const uint32&) override;

        bool Add(Player* passenger, InitialWorldEntryHook* initialEntry = nullptr) override;

        TransportMap* AsTransport() override { return this; }
        TransportMap const* AsTransport() const override { return this; }

        Transport* Vessel() const { return m_vessel; }

        bool Commission();

        bool IsCommissioned() const { return m_commissioned; }

        float HullRadius() const { return m_hullRadius; }

        std::optional<float> SurfaceAt(float x, float y, float z,
                                       float searchUp, float searchDown) const;

        bool IsBlocked(Geometry::Vector3 const& from, Geometry::Vector3 const& to) const;

        std::optional<Position> FreeSpotNear(Occupant const& master, float distance2d,
                                             float angle) const;

        std::optional<Geometry::Placement> PositionOf(Occupant const& obj) const;

        void Embark(Player* passenger);

        bool Board(Player* passenger, float x, float y, float z, float o, uint32 options = 0);

        void Disembark(Player* passenger, float x, float y, float z, float o);

        void VesselLeavingWorld(Map* oldWorld, uint32 newMapId,
                                float x, float y, float z, float o);

        void VesselEnteredWorld(Map* newWorld);

        uint32 Across(Audience const& who, Listener const& tell) override;

        void EnlistCrew(Creature* crew);
        void DelistCrew(Creature* crew);
        bool HasCrew() const { return !m_crew.empty(); }

        void ReleaseCrew() { m_crew.clear(); }

        static void AppendVesselCreateBlocks(Transport* vessel, Player* observer, UpdateData& data);
        static void AnnounceVessel(Transport* vessel, Player* observer);
        static void RetractVessel(Transport* vessel, Player* observer);

        static void CollectRelaySources(Occupant const* viewer, float visibility,
                                        std::vector<RelaySource>& out);

        void SendCrewMemberCreate(Creature* crew);

        void AppendCrewCreateBlocks(UpdateData& data, Player* observer);
        void AppendCrewDestroyBlocks(UpdateData& data);

    private:

        void UpdateMinions();

        void GatherObservers();

        Transport* m_vessel;

        bool m_commissioned = false;

        float m_hullRadius = 0.0f;

        std::vector<Creature*> m_crew;
};
