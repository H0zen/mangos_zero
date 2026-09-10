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
#include <map>
#include <list>
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "OutdoorPvPMgr.h"

class WorldPacket;
class Occupant;
class Player;
class GameObject;
class Unit;
class Creature;

enum CapturePointArtKits
{
    CAPTURE_ARTKIT_ALLIANCE = 2,
    CAPTURE_ARTKIT_HORDE    = 1,
    CAPTURE_ARTKIT_NEUTRAL  = 21
};

enum CapturePointAnimations
{
    CAPTURE_ANIM_ALLIANCE   = 1,
    CAPTURE_ANIM_HORDE      = 0,
    CAPTURE_ANIM_NEUTRAL    = 2
};

typedef std::map < ObjectGuid , bool  > GuidZoneMap;

class OutdoorPvP
{
    friend class OutdoorPvPMgr;

    public:

        OutdoorPvP() {}

        virtual ~OutdoorPvP() {}

        virtual void FillInitialWorldStates(WorldPacket& , uint32& ) {}

        virtual bool HandleEvent(uint32 , GameObject* ) { return false; }

        virtual void HandleObjectiveComplete(uint32 , const std::list<Player*> &, Team ) {}

        virtual void HandleCreatureCreate(Creature* ) {}

        virtual void HandleGameObjectCreate(GameObject* );

        virtual void HandleGameObjectRemove(GameObject* );

        virtual void HandleCreatureDeath(Creature* ) {}

        virtual bool HandleGameObjectUse(Player* , GameObject* ) { return false; }

        virtual bool HandleAreaTrigger(Player* , uint32 ) { return false; }

        virtual bool HandleDropFlag(Player* , uint32 ) { return false; }

        virtual void Update(uint32 ) {}

        void HandlePlayerKill(Player* killer, Player* victim);

    protected:

        virtual void HandlePlayerEnterZone(Player* , bool );

        virtual void HandlePlayerLeaveZone(Player* , bool );

        virtual void SendRemoveWorldStates(Player* ) {}

        virtual void HandlePlayerKillInsideArea(Player* ) {}

        void SendUpdateWorldState(uint32 field, uint32 value);

        void BuffTeam(Team team, uint32 spellId, bool remove = false);

        uint32 GetBannerArtKit(Team team, uint32 artKitAlliance = CAPTURE_ARTKIT_ALLIANCE, uint32 artKitHorde = CAPTURE_ARTKIT_HORDE, uint32 artKitNeutral = CAPTURE_ARTKIT_NEUTRAL);

        void SetBannerVisual(const Occupant* objRef, ObjectGuid goGuid, uint32 artKit, uint32 animId);

        void SetBannerVisual(GameObject* go, uint32 artKit, uint32 animId);

        void RespawnGO(const Occupant* objRef, ObjectGuid goGuid, bool respawn);

        GuidZoneMap m_zonePlayers;
};
