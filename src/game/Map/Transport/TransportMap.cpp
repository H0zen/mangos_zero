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
#include "TransportMap.h"
#include "InitialWorldEntry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <list>
#include <string>
#include <vector>

#include "Transports.h"
#include "Creature.h"
#include "Pet.h"
#include "Player.h"
#include "Fleet.h"
#include "DBCStores.h"
#include "Movement/Generators/MotionMaster.h"
#include "WorldPacket.h"
#include "Log.h"
#include "terrain/GoModelStore.hpp"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"

std::string DescribeSpatially(Unit* u)
{
    if (!u)
    {
        return "(null)";
    }

    Map* on = u->FindMap();
    Geometry::Placement const& at = u->Where();

    char buf[320];
    snprintf(buf, sizeof(buf),
             "%s map=%u%s measured=%s/%u pos=(%.2f %.2f %.2f) inworld=%d alive=%d gen=%u",
             u->GetGuidStr().c_str(),
             on ? on->GetId() : 0u,
             on && on->AsTransport() ? "[deck]" : "",
             at.IsPlaced() ? std::to_string(at.MapId()).c_str() : "nowhere",
             at.InstanceId(),
             at.X(), at.Y(), at.Z(),
             u->IsInWorld() ? 1 : 0, u->IsAlive() ? 1 : 0,
             unsigned(u->GetMotionMaster()->GetCurrentMovementGeneratorType()));

    return buf;
}

namespace
{

    bool InSameGrid(Geometry::Placement const& a, Geometry::Placement const& b)
    {
        uint32 ax = 0, ay = 0, bx = 0, by = 0;
        Cell::GridOf(a.X(), a.Y(), ax, ay);
        Cell::GridOf(b.X(), b.Y(), bx, by);
        return ax == bx && ay == by;
    }

    constexpr float HULL_SEARCH_UP = 3.0f;
    constexpr float HULL_SEARCH_DOWN = 6.0f;

    constexpr float HULL_PROBE_HEIGHT = 1.0f;

    constexpr int HULL_SPOT_BEARINGS = 8;

    bool IsPlanted(Unit const* minion)
    {
        return IsCreature(minion) &&
               static_cast<Creature const*>(minion)->IsTotem();
    }

    bool CanRide(Creature const* crew)
    {
        CreatureInfo const* info = crew->GetCreatureInfo();

        return !info || (info->CreatureTypeFlags & CREATURE_TYPEFLAGS_TRANSPORT_FORBIDDEN)
                        != CREATURE_TYPEFLAGS_TRANSPORT_FORBIDDEN;
    }

    void ForgetMinion(Creature* minion, Unit* watcher)
    {
        Player* client = watcher &&IsPlayer(watcher)
                             ? static_cast<Player*>(watcher) : nullptr;
        if (!client)
        {
            return;
        }

        minion->DestroyForPlayer(client);
        client->m_clientGUIDs.erase(minion->GetObjectGuid());
    }

    void DrawMinionTo(Unit* minion, Unit* master, Map* dest)
    {
        if (!minion || !master || !dest)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                             "DrawMinionTo: SKIP null (minion=%p master=%p dest=%p)",
                             (void*)minion, (void*)master, (void*)dest);
            return;
        }

        if (!minion->IsInWorld() || !minion->IsAlive())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                             "DrawMinionTo: SKIP not-drawable %s", DescribeSpatially(minion).c_str());
            return;
        }

        if (minion->FindMap() == dest)
        {
            return;
        }

        Creature* c = static_cast<Creature*>(minion);

        DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                         "DrawMinionTo: FROM %s", DescribeSpatially(c).c_str());
        DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                         "DrawMinionTo:   TO map=%u%s beside %s", dest->GetId(),
                         dest->AsTransport() ? "[deck]" : "", DescribeSpatially(master).c_str());

        float x, y, z;
        ClosePointNear(*master, x, y, z, minion->Where().Extent(), PET_FOLLOW_DIST,
                       PET_FOLLOW_ANGLE);

        c->StopMoving();

        ForgetMinion(c, master);

        for (Map::PlayerList::const_iterator itr = c->GetMap()->GetPlayers().begin();
             itr != c->GetMap()->GetPlayers().end(); ++itr)
        {
            ForgetMinion(c, itr->getSource());
        }

        c->GetMap()->Remove(c, false);
        c->Place().MoveTo(x, y, z, master->Where().Facing());
        dest->Add(c);

        if (c->GetCharmInfo() && c->GetCharmInfo()->HasCommandState(COMMAND_STAY))
        {
            c->GetMotionMaster()->MoveIdle();
        }
        else
        {
            c->GetMotionMaster()->MoveFollow(master, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
        }

        c->SendHeartBeat();

        DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                         "DrawMinionTo: DONE %s", DescribeSpatially(c).c_str());
    }

    void DrawMinionsTo(Player* master, Map* dest)
    {
        if (!master || !dest)
        {
            return;
        }

        int seen = 0;
        master->CallForAllControlledUnits(
            [master, dest, &seen](Unit* minion) { ++seen; DrawMinionTo(minion, master, dest); },
            CONTROLLED_PET | CONTROLLED_MINIPET | CONTROLLED_GUARDIANS | CONTROLLED_TOTEMS);

        DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                         "DrawMinionsTo: %s -> map=%u%s, %d controlled unit(s), petguid=%s",
                         master->GetGuidStr().c_str(), dest->GetId(),
                         dest->AsTransport() ? "[deck]" : "", seen,
                         GuidString(master->GetPetGuid()).c_str());
    }
}

bool TransportMap::Commission()
{
    GameObjectInfo const* goinfo = m_vessel ? m_vessel->GetGOInfo() : nullptr;
    if (!goinfo)
    {
        return false;
    }

    auto model = world::terrain::GoModelStore::Instance().Get(goinfo->displayId);
    if (model)
    {
        Geometry::Aabb const& b = model->Bounds();
        const float hx = std::max(std::fabs(b.lo.x), std::fabs(b.hi.x));
        const float hy = std::max(std::fabs(b.lo.y), std::fabs(b.hi.y));
        m_hullRadius = std::sqrt(hx * hx + hy * hy);

        uint32 pinned = 0;
        for (float gx = b.lo.x; gx < b.hi.x + SIZE_OF_GRIDS; gx += SIZE_OF_GRIDS)
        {
            for (float gy = b.lo.y; gy < b.hi.y + SIZE_OF_GRIDS; gy += SIZE_OF_GRIDS)
            {

                ForceLoadGrid(std::min(gx, b.hi.x), std::min(gy, b.hi.y));
                ++pinned;
            }
        }

        DETAIL_LOG("Transport %u map %u: %u grid(s) pinned, hull x[%.1f %.1f] y[%.1f %.1f]",
                   goinfo->id, GetId(), pinned, b.lo.x, b.hi.x, b.lo.y, b.hi.y);
    }

    if (!GetTerrain()->ColumnAt(0.0f, 0.0f, m_hullRadius * 3.0f, -m_hullRadius * 3.0f)
                     .HighestSolid())
    {
        sLog.outErrorDb("Transport %u (%s, display %u): map %u has no baked terrain "
                        "(w_%u.tile missing). Re-bake the vessel maps; it will carry no crew "
                        "and board no passengers until then.",
                        goinfo->id, goinfo->name, goinfo->displayId, GetId(), GetId());
        return false;
    }

    m_commissioned = true;
    return true;
}

std::optional<float> TransportMap::SurfaceAt(float x, float y, float z,
                                             float searchUp, float searchDown) const
{

    return GetTerrain()->ColumnAt(x, y, z + searchUp, z - searchDown)
           .HighestSolidAtOrBelow(z + searchUp);
}

bool TransportMap::IsBlocked(Geometry::Vector3 const& from, Geometry::Vector3 const& to) const
{
    const Geometry::Vector3 seg = to - from;
    const float len = std::sqrt(seg.x * seg.x + seg.y * seg.y + seg.z * seg.z);
    if (len < 1e-4f)
    {
        return false;
    }

    return !GetTerrain()->IsInLineOfSight(from.x, from.y, from.z, to.x, to.y, to.z);
}

std::optional<Geometry::Placement> TransportMap::PositionOf(Occupant const& obj) const
{

    if (obj.GetMap() == this)
    {
        return obj.Where();
    }

    return std::nullopt;
}

std::optional<Position> TransportMap::FreeSpotNear(Occupant const& master, float distance2d,
                                                   float angle) const
{
    const auto anchor = PositionOf(master);
    if (!anchor)
    {
        return std::nullopt;
    }

    for (int step = 0; step < HULL_SPOT_BEARINGS; ++step)
    {
        const float bearing = anchor->Facing() + angle +
                              (step ? (2 * M_PI_F * float(step) / float(HULL_SPOT_BEARINGS)) : 0.0f);

        const float x = anchor->X() + distance2d * std::cos(bearing);
        const float y = anchor->Y() + distance2d * std::sin(bearing);

        const auto z = SurfaceAt(x, y, anchor->Z(), HULL_SEARCH_UP, HULL_SEARCH_DOWN);
        if (!z)
        {
            continue;
        }

        if (IsBlocked(Geometry::Vector3(anchor->X(), anchor->Y(), anchor->Z() + HULL_PROBE_HEIGHT),
                      Geometry::Vector3(x, y, *z + HULL_PROBE_HEIGHT)))
        {
            continue;
        }

        return Position(x, y, *z, anchor->Facing());
    }

    return std::nullopt;
}

bool TransportMap::Add(Player* passenger, InitialWorldEntryHook* initialEntry)
{

    Position const* aboard = passenger->m_movementInfo.GetTransportPos();
    passenger->Place().MoveTo(aboard->x, aboard->y, aboard->z, aboard->o);

    passenger->GetMapRef().link(this, passenger);
    passenger->SetMap(this);

    CellPair p = MaNGOS::ComputeCellPair(passenger->Where().X(), passenger->Where().Y());
    Cell cell(p);
    EnsureGridLoadedAtEnter(cell, passenger);
    passenger->AddToWorld();

    if (initialEntry)
    {
        initialEntry->AfterAddToWorld(*passenger);
    }

    std::optional<InitialWorldUpdateBatch> initialUpdates;
    if (passenger->GetSession()->PlayerLoading() && passenger->GetCamera().GetBody() == passenger)
    {
        initialUpdates.emplace();
    }
    auto* batch = initialUpdates ? &*initialUpdates : nullptr;

    UpdateData localData;
    UpdateData& data = batch ? batch->Data() : localData;
    m_vessel->BuildCreateUpdateBlockForPlayer(&data, passenger);
    passenger->BuildCreateUpdateBlockForPlayer(&data, passenger);

    if (batch)
    {
        batch->MarkTransport();
    }
    else
    {
        WorldPacket packet;

        data.BuildPacket(&packet);
        passenger->GetSession()->SendPacket(&packet);
    }

    if (Map* sailed = m_vessel->IsCrossing() ? nullptr : m_vessel->GetMap())
    {
        for (Transport* other : sFleet.On(sailed->GetId()))
        {
            if (other != m_vessel && other->GetMap() == sailed)
            {
                if (batch)
                {
                    AppendVesselCreateBlocks(other, passenger, batch->Data());
                    batch->MarkTransport();
                }
                else
                {
                    AnnounceVessel(other, passenger);
                }
            }
        }
    }

    NGridType* grid = getNGrid(cell.GridX(), cell.GridY());
    passenger->GetViewPoint().Event_AddedToWorld(
        &(*grid)(cell.CellX(), cell.CellY()), passenger, batch);

    if (batch && !batch->WasSent())
    {
        if (!batch->FlushAttempted())
        {
            sLog.outError("Initial object update batch for passenger %u was not flushed by the owner camera", passenger->GetGUIDLow());
        }
        passenger->GetSession()->KickPlayer();
    }
    else
    {
        UpdateObjectVisibility(passenger, cell, p);
    }

    return true;
}

void TransportMap::Embark(Player* passenger)
{
    if (!m_commissioned || !passenger->IsInWorld() || passenger->GetMap() == this)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS, "Embark: %s",
                     DescribeSpatially(passenger).c_str());

    Rebind(passenger, passenger->Where().X(), passenger->Where().Y(),
           passenger->Where().Z(), passenger->Where().Facing());

    DrawMinionsTo(passenger, this);
}

bool TransportMap::Board(Player* passenger, float x, float y, float z, float o, uint32 options)
{
    Map* sailed = m_vessel ? m_vessel->GetMap() : nullptr;

    if (!m_commissioned || !sailed || !MaNGOS::IsValidMapCoord(x, y, z, o))
    {
        return false;
    }

    if (m_vessel->IsCrossing())
    {
        return false;
    }

    Transport* const wasOn = passenger->GetTransport();
    ObjectGuid const wasGuid = passenger->m_movementInfo.GetTransportGuid();
    Position const wasAt = *passenger->m_movementInfo.GetTransportPos();

    passenger->SetTransport(m_vessel);
    passenger->m_movementInfo.SetTransportData(m_vessel->GetObjectGuid(), x, y, z, o, 0);

    if (passenger->TeleportTo(sailed->GetId(),
                              m_vessel->Where().X(), m_vessel->Where().Y(),
                              m_vessel->Where().Z(), m_vessel->Where().Facing(),
                              options | TELE_TO_NOT_LEAVE_TRANSPORT))
    {
        return true;
    }

    passenger->SetTransport(wasOn);
    if (wasOn)
    {
        passenger->m_movementInfo.SetTransportData(wasGuid, wasAt.x, wasAt.y, wasAt.z, wasAt.o, 0);
    }
    else
    {
        passenger->m_movementInfo.ClearTransportData();
    }

    return false;
}

void TransportMap::Disembark(Player* passenger, float x, float y, float z, float o)
{
    if (passenger->GetMap() != this)
    {
        return;
    }

    Map* sailed = m_vessel ? m_vessel->GetMap() : nullptr;
    if (!sailed)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS, "Disembark: %s",
                     DescribeSpatially(passenger).c_str());

    passenger->SetTransport(nullptr);

    sailed->Rebind(passenger, x, y, z, o);

    DrawMinionsTo(passenger, sailed);
}

void TransportMap::VesselLeavingWorld(Map* oldWorld, uint32 newMapId,
                                      float x, float y, float z, float o)
{
    if (!oldWorld)
    {
        return;
    }

    PlayerList const& ashore = oldWorld->GetPlayers();
    for (PlayerList::const_iterator itr = ashore.begin(); itr != ashore.end(); ++itr)
    {
        if (Player* leaving = itr->getSource())
        {
            RetractVessel(m_vessel, leaving);
        }
    }

    std::vector<Player*> aboard;
    for (PlayerList::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
    {
        if (Player* passenger = itr->getSource())
        {
            aboard.push_back(passenger);
        }
    }

    for (Player* passenger : aboard)
    {
        if (passenger->IsDead() && !passenger->HasPlayerFlag(PLAYER_FLAGS_GHOST))
        {
            passenger->ResurrectPlayer(1.0);
        }

        passenger->TeleportTo(newMapId, x, y, z, o, TELE_TO_NOT_LEAVE_TRANSPORT);
    }
}

void TransportMap::VesselEnteredWorld(Map* newWorld)
{
    if (!newWorld)
    {
        return;
    }

    PlayerList const& arriving = newWorld->GetPlayers();
    for (PlayerList::const_iterator itr = arriving.begin(); itr != arriving.end(); ++itr)
    {
        if (Player* found = itr->getSource())
        {
            AnnounceVessel(m_vessel, found);
        }
    }
}

void TransportMap::EnlistCrew(Creature* crew)
{
    if (!crew || !m_commissioned)
    {
        return;
    }

    if (!CanRide(crew))
    {
        sLog.outErrorDb("Transport map %u: creature %u (guid %u) carries CreatureTypeFlags "
                        "0x%X, which includes TRANSPORT_FORBIDDEN; both of those bits together "
                        "kill every client watching a moving transport. Removed. Clear either "
                        "one of them if it really must sail.",
                        GetId(), crew->GetEntry(), crew->GetGUIDLow(),
                        crew->GetCreatureInfo() ? crew->GetCreatureInfo()->CreatureTypeFlags : 0);

        crew->AddObjectToRemoveList();
        return;
    }

    if (std::find(m_crew.begin(), m_crew.end(), crew) == m_crew.end())
    {
        m_crew.push_back(crew);
    }

    crew->SetActiveObjectState(true);

}

void TransportMap::DelistCrew(Creature* crew)
{
    m_crew.erase(std::remove(m_crew.begin(), m_crew.end(), crew), m_crew.end());
}

void TransportMap::UpdateMinions()
{

    for (PlayerList::const_iterator itr = GetPlayers().begin(); itr != GetPlayers().end(); ++itr)
    {
        Player* master = itr->getSource();
        if (!master)
        {
            continue;
        }

        master->CallForAllControlledUnits(
            [this, master](Unit* minion) { DrawMinionTo(minion, master, this); },
            CONTROLLED_PET | CONTROLLED_MINIPET | CONTROLLED_GUARDIANS | CONTROLLED_TOTEMS);
    }

    std::vector<Creature*> stranded;

    for (auto const& entry : GetObjectsStore().GetElements<Pet>())
    {
        Creature* aboard = entry.second;
        if (!aboard || !aboard->IsInWorld() || IsPlanted(aboard))
        {
            continue;
        }

        Unit* master = aboard->GetOwner();
        if (master && master->IsInWorld() && master->FindMap() != this)
        {
            stranded.push_back(aboard);
        }
    }

    for (Creature* minion : stranded)
    {
        Unit* master = minion->GetOwner();
        DEBUG_FILTER_LOG(LOG_FILTER_DECK_MINIONS,
                         "UpdateMinions: STRANDED aboard %s", DescribeSpatially(minion).c_str());
        DrawMinionTo(minion, master, master->FindMap());
    }
}

void TransportMap::AppendCrewCreateBlocks(UpdateData& data, Player* observer)
{
    for (Creature* crew : m_crew)
    {
        if (crew->IsInWorld())
        {
            crew->BuildCreateUpdateBlockForPlayer(&data, observer);
        }
    }
}

void TransportMap::AppendCrewDestroyBlocks(UpdateData& data)
{
    for (Creature* crew : m_crew)
    {
        crew->BuildOutOfRangeUpdateBlock(&data);
    }
}

void TransportMap::SendCrewMemberCreate(Creature* crew)
{
    if (!crew || !crew->IsInWorld() || !m_vessel || !m_vessel->GetMap())
    {
        return;
    }

    PlayerList const& everyone = m_vessel->GetMap()->GetPlayers();
    for (PlayerList::const_iterator itr = everyone.begin(); itr != everyone.end(); ++itr)
    {
        Player* observer = itr->getSource();
        if (!observer)
        {
            continue;
        }

        UpdateData data;
        crew->BuildCreateUpdateBlockForPlayer(&data, observer);

        WorldPacket packet;
        data.BuildPacket(&packet);
        observer->SendDirectMessage(&packet);
    }
}

void TransportMap::AppendVesselCreateBlocks(Transport* vessel, Player* observer, UpdateData& data)
{
    if (!vessel || !observer)
    {
        return;
    }

    vessel->BuildCreateUpdateBlockForPlayer(&data, observer);

    if (TransportMap* hull = vessel->AsMap())
    {
        hull->AppendCrewCreateBlocks(data, observer);
    }
}

void TransportMap::AnnounceVessel(Transport* vessel, Player* observer)
{
    if (!vessel || !observer)
    {
        return;
    }

    UpdateData data;
    AppendVesselCreateBlocks(vessel, observer, data);

    WorldPacket packet;
    data.BuildPacket(&packet);
    observer->SendDirectMessage(&packet);
}

void TransportMap::RetractVessel(Transport* vessel, Player* observer)
{
    if (!vessel || !observer)
    {
        return;
    }

    UpdateData data;

    if (TransportMap* hull = vessel->AsMap())
    {
        hull->AppendCrewDestroyBlocks(data);
    }

    vessel->BuildOutOfRangeUpdateBlock(&data);
    observer->ForgetAtClient(vessel->GetObjectGuid());

    WorldPacket packet;
    data.BuildPacket(&packet);
    observer->SendDirectMessage(&packet);
}

void TransportMap::CollectRelaySources(Occupant const* viewer, float visibility,
                                       std::vector<RelaySource>& out)
{
    if (!viewer || !viewer->GetMap())
    {
        return;
    }

    if (TransportMap const* hull = viewer->GetMap()->AsTransport())
    {
        Transport* vessel = hull->Vessel();
        if (Map* sailed = vessel ? vessel->GetMap() : nullptr)
        {
            out.push_back({sailed, vessel->Where().X(), vessel->Where().Y(), 0.0f});
        }
        return;
    }

    for (Transport* vessel : sFleet.On(viewer->GetMapId()))
    {
        TransportMap* hull = vessel->AsMap();
        if (!hull || vessel->GetMap() != viewer->GetMap())
        {
            continue;
        }

        if (!InSameGrid(vessel->Where(), viewer->Where()))
        {
            continue;
        }

        out.push_back({hull, 0.0f, 0.0f, hull->HullRadius() * 2.0f + visibility});
    }
}

void TransportMap::GatherObservers()
{
    Map* world = m_vessel ? m_vessel->GetMap() : nullptr;
    if (!world)
    {
        return;
    }

    struct AnyoneInTheGrid
    {
        Occupant const* focus;
        Occupant const& GetFocusObject() const { return *focus; }
        bool operator()(Player* watcher) const { return watcher->IsInWorld(); }
    };

    std::list<Player*> found;
    AnyoneInTheGrid check{m_vessel};
    MaNGOS::PlayerListSearcher<AnyoneInTheGrid> searcher(found, check);
    Cell::VisitWorldObjectsInGrid(m_vessel->Where().X(), m_vessel->Where().Y(), world, searcher);

    SetExternalObservers(std::vector<Player*>(found.begin(), found.end()));
}

uint32 TransportMap::Across(Audience const& who, MapBroadcaster::Listener const& tell)
{

    uint32 told = 0;

    for (Player* observer : ExternalObservers())
    {
        if (!who.Admits(observer) || !observer->GetSession())
        {
            continue;
        }

        tell(observer);
        ++told;
    }

    return told;
}

void TransportMap::Update(const uint32& t_diff)
{

    GatherObservers();
    UpdateMinions();

    Map::Update(t_diff);
}
