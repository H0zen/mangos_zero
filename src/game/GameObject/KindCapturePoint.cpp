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

#include <algorithm>
#include <cstdlib>
#include <list>
#include "Kinds.h"
#include "GameObject.h"
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "PoolManager.h"
#include "SpellMgr.h"
#include "Spell.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "LootMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "InstanceData.h"
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Util.h"
#include "ScriptMgr.h"
#include "GameObjectModel.h"
#include "CreatureAISelector.h"
#include "SQLStorages.h"
#include "GameObjectAI.h"
#include "Geometry/Quat.h"

void CapturePointBehaviour::Restore(float value, bool isLocked)
{
    m_bar.SliderAt(value, It().GetGOInfo()->capturePoint.neutralPercent);

    if (!isLocked)
    {
        It().SetLootState(GO_ACTIVATED);
    }
}

void CapturePointBehaviour::Tick()
{

    GameObjectInfo const* info = It().GetGOInfo();
    float const radius = info->capturePoint.radius;

    std::list<Player*> capturingPlayers;
    MaNGOS::AnyPlayerInCapturePointRange u_check(&It(), radius);
    MaNGOS::PlayerListSearcher<MaNGOS::AnyPlayerInCapturePointRange> checker(capturingPlayers, u_check);
    Cell::VisitWorldObjects(&It(), checker, radius);

    uint32 const neutralPercent = info->capturePoint.neutralPercent;
    int const oldValue = static_cast<int>(m_bar.Slider());

    GuidSet gone(m_bar.Standing());
    int superiority = 0;

    for (auto* player : capturingPlayers)
    {
        superiority += player->GetTeam() == ALLIANCE ? 1 : -1;

        ObjectGuid const guid = player->GetObjectGuid();
        gone.erase(guid);

        if (m_bar.Arrived(guid))
        {
            player->SendUpdateWorldState(info->capturePoint.worldState3, neutralPercent);
            player->SendUpdateWorldState(info->capturePoint.worldState2, oldValue);
            player->SendUpdateWorldState(info->capturePoint.worldState1, WORLD_STATE_ADD);

            player->SendUpdateWorldState(info->capturePoint.worldState2, oldValue);
        }
    }

    for (auto const& guid : gone)
    {
        if (Player* owner = It().GetMap()->GetPlayer(guid))
        {
            owner->SendUpdateWorldState(info->capturePoint.worldState1, WORLD_STATE_REMOVE);
        }

        m_bar.Left(guid);
    }

    if (superiority == 0)
    {
        if (m_bar.IsDeserted())
        {
            It().SetActiveObjectState(false);
        }
        return;
    }

    It().SetActiveObjectState(true);

    int const maxSuperiority = info->capturePoint.maxSuperiority;
    superiority = std::max(-maxSuperiority, std::min(superiority, maxSuperiority));

    float seconds = info->capturePoint.minTime;
    if (int deltaSuperiority = maxSuperiority - info->capturePoint.minSuperiority)
    {
        seconds += float(maxSuperiority - std::abs(superiority)) / deltaSuperiority * (info->capturePoint.maxTime - info->capturePoint.minTime);
    }

    Team const pushing = superiority > 0 ? ALLIANCE : HORDE;

    m_bar.SliderTowards(pushing, 100.0f * (CAPTURE_TICK / 1000.0f) / seconds);

    if (static_cast<int>(m_bar.Slider()) == oldValue)
    {
        return;
    }

    for (auto* player : capturingPlayers)
    {
        player->SendUpdateWorldState(info->capturePoint.worldState2, static_cast<uint32>(m_bar.Slider()));
    }

    CaptureShift const shift = m_bar.Shift(pushing, *info);

    if (shift.objectiveTaken)
    {
        if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(capturingPlayers.front()->GetCachedZoneId()))
        {
            outdoorPvP->HandleObjectiveComplete(shift.eventId, capturingPlayers, pushing);
        }
    }

    if (shift.eventId)
    {
        StartEvents_Event(It().GetMap(), shift.eventId, &It(), &It(), true, capturingPlayers.front());
    }
}

void CapturePointBehaviour::InUse(uint32 elapsed)
{
    if (m_bar.IsTickDue(elapsed))
    {
        Tick();
    }
}

GameObjectBehaviour::Tick CapturePointBehaviour::Spent()
{

    for (auto const& guid : m_bar.Standing())
    {
        if (Player* owner = It().GetMap()->GetPlayer(guid))
        {
            owner->SendUpdateWorldState(Data().capturePoint.worldState1, WORLD_STATE_REMOVE);
        }
    }

    m_bar.Desert();
    It().SetLootState(GO_READY);

    return Tick::Stop;
}
