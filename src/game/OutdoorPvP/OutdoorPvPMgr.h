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

#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include <map>
#include "Timer.h"

enum
{
    TIMER_OPVP_MGR_UPDATE = MINUTE * IN_MILLISECONDS
};

enum OutdoorPvPTypes
{
    OPVP_ID_SI = 0,
    OPVP_ID_EP,

    MAX_OPVP_ID
};

enum OutdoorPvPZones
{
    ZONE_ID_SILITHUS            = 1377,
    ZONE_ID_TEMPLE_OF_AQ        = 3428,
    ZONE_ID_RUINS_OF_AQ         = 3429,
    ZONE_ID_GATES_OF_AQ         = 3478,

    ZONE_ID_EASTERN_PLAGUELANDS = 139,
    ZONE_ID_STRATHOLME          = 2017,
    ZONE_ID_SCHOLOMANCE         = 2057
};

struct CapturePointSlider
{

    CapturePointSlider() : Value(0.0f), IsLocked(false) {}

    CapturePointSlider(float value, bool isLocked) : Value(value), IsLocked(isLocked) {}

    float Value;
    bool IsLocked;
};

class Player;
class GameObject;
class Creature;
class OutdoorPvP;

typedef std::map<uint32 , CapturePointSlider > CapturePointSliderMap;

class OutdoorPvPMgr
{
    public:

        OutdoorPvPMgr();

        ~OutdoorPvPMgr();

        void InitOutdoorPvP();

        void HandlePlayerEnterZone(Player* player, uint32 zoneId);

        void HandlePlayerLeaveZone(Player* player, uint32 zoneId);

        OutdoorPvP* GetScript(uint32 zoneId);

        void Update(uint32 diff);

        CapturePointSliderMap const* GetCapturePointSliderMap() const { return &m_capturePointSlider; }

        void SetCapturePointSlider(uint32 entry, CapturePointSlider value) { m_capturePointSlider[entry] = value; }

    private:

        OutdoorPvP* GetScriptOfAffectedZone(uint32 zoneId);

        OutdoorPvP* m_scripts[MAX_OPVP_ID];

        CapturePointSliderMap m_capturePointSlider;

        IntervalTimer m_updateTimer;
};

#define sOutdoorPvPMgr MaNGOS::Singleton<OutdoorPvPMgr>::Instance()
