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
#include "MapManager.h"
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

/**
 * @brief Opens the capture point at a given slider value.
 *
 * @param value The slider value the point is left at.
 * @param isLocked true when the point is not to be contested yet.
 */
void GameObject::SetCapturePointSlider(float value, bool isLocked)
{
    m_capture.SliderAt(value, GetGOInfo()->capturePoint.neutralPercent);

    // only activate non-locked capture point
    if (!isLocked)
    {
        SetLootState(GO_ACTIVATED);
    }
}

/**
 * @brief Pushes the bar toward whichever side has more players by the point.
 */
void GameObject::TickCapturePoint()
{
    // TODO: On retail: Ticks every 5.2 seconds. slider value increase when new player enters on tick

    GameObjectInfo const* info = GetGOInfo();
    float const radius = info->capturePoint.radius;

    std::list<Player*> capturingPlayers;
    MaNGOS::AnyPlayerInCapturePointRange u_check(this, radius);
    MaNGOS::PlayerListSearcher<MaNGOS::AnyPlayerInCapturePointRange> checker(capturingPlayers, u_check);
    Cell::VisitWorldObjects(this, checker, radius);

    uint32 const neutralPercent = info->capturePoint.neutralPercent;
    int const oldValue = static_cast<int>(m_capture.Slider());

    // Alliance counts up and horde counts down, so what is left is by how much
    // one side outnumbers the other, and its sign says which side that is.
    GuidSet gone(m_capture.Standing());
    int superiority = 0;

    for (auto* player : capturingPlayers)
    {
        superiority += player->GetTeam() == ALLIANCE ? 1 : -1;

        ObjectGuid const guid = player->GetObjectGuid();
        gone.erase(guid);

        if (m_capture.Arrived(guid))
        {
            player->SendUpdateWorldState(info->capturePoint.worldState3, neutralPercent);
            player->SendUpdateWorldState(info->capturePoint.worldState2, oldValue);
            player->SendUpdateWorldState(info->capturePoint.worldState1, WORLD_STATE_ADD);
            // also redundantly sent on retail to prevent displaying the initial capture direction on client capture slider incorrectly
            player->SendUpdateWorldState(info->capturePoint.worldState2, oldValue);
        }
    }

    for (auto const& guid : gone)
    {
        if (Player* owner = GetMap()->GetPlayer(guid))
        {
            owner->SendUpdateWorldState(info->capturePoint.worldState1, WORLD_STATE_REMOVE);
        }

        m_capture.Left(guid);
    }

    // nobody outnumbers anybody, so the bar stays where it is (works because minSuperiority is always 1)
    if (superiority == 0)
    {
        if (m_capture.IsDeserted())
        {
            SetActiveObjectState(false);
        }
        return;
    }

    // keeps the object loaded while anyone stands by it, so that an idle grid
    // cannot freeze the list of who is there
    SetActiveObjectState(true);

    int const maxSuperiority = info->capturePoint.maxSuperiority;
    superiority = std::max(-maxSuperiority, std::min(superiority, maxSuperiority));

    // time to capture from 0% to 100% is maxTime for minSuperiority amount of players and minTime for maxSuperiority amount of players (linear function: y = dy/dx*x+d)
    float seconds = info->capturePoint.minTime;
    if (int deltaSuperiority = maxSuperiority - info->capturePoint.minSuperiority)
    {
        seconds += float(maxSuperiority - std::abs(superiority)) / deltaSuperiority * (info->capturePoint.maxTime - info->capturePoint.minTime);
    }

    Team const pushing = superiority > 0 ? ALLIANCE : HORDE;

    // the share of the whole bar that one tick is worth
    m_capture.SliderTowards(pushing, 100.0f * (CAPTURE_TICK / 1000.0f) / seconds);

    // the bar is read in whole percents, so a smaller move says nothing yet
    if (static_cast<int>(m_capture.Slider()) == oldValue)
    {
        return;
    }

    // on retail this is also sent to newly added players even though they already received a slider value
    for (auto* player : capturingPlayers)
    {
        player->SendUpdateWorldState(info->capturePoint.worldState2, static_cast<uint32>(m_capture.Slider()));
    }

    CaptureShift const shift = m_capture.Shift(pushing, *info);

    if (shift.objectiveTaken)
    {
        if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(capturingPlayers.front()->GetCachedZoneId()))
        {
            outdoorPvP->HandleObjectiveComplete(shift.eventId, capturingPlayers, pushing);
        }
    }

    if (shift.eventId)
    {
        StartEvents_Event(GetMap(), shift.eventId, this, this, true, capturingPlayers.front());
    }
}
