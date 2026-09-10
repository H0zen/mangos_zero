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
#include "LootMgr.h"
#include "ObjectGuid.h"

class Map;
class Unit;
class Player;
class GameObject;
class Creature;
class Occupant;

enum InstanceConditionIDs
{

    INSTANCE_CONDITION_ID_NORMAL_MODE       = 0,
    INSTANCE_CONDITION_ID_HARD_MODE         = 1,
    INSTANCE_CONDITION_ID_HARD_MODE_2       = 2,
    INSTANCE_CONDITION_ID_HARD_MODE_3       = 3,
    INSTANCE_CONDITION_ID_HARD_MODE_4       = 4,

    INSTANCE_CONDITION_ID_TEAM_HORDE        = 67,
    INSTANCE_CONDITION_ID_TEAM_ALLIANCE     = 469,
};

class InstanceData
{
    public:

        explicit InstanceData(Map* map) : instance(map) {}
        virtual ~InstanceData() {}

        Map* instance;

        virtual void Initialize() {}

        virtual void Load(const char* ) {}

        virtual const char* Save() const { return ""; }

        void SaveToDB() const;

        virtual void Update(uint32 ) {}

        virtual bool IsEncounterInProgress() const { return false; };

        virtual void OnPlayerEnter(Player*) {}

        virtual void OnPlayerDeath(Player*) {}

        virtual void OnPlayerLeave(Player*) {}

        virtual void OnObjectCreate(GameObject*) {}

        virtual void OnCreatureCreate(Creature* ) {}

        virtual void OnCreatureEnterCombat(Creature* ) {}

        virtual void OnCreatureEvade(Creature* ) {}

        virtual void OnCreatureDeath(Creature* ) {}

        virtual void OnCreatureDespawn(Creature* ) {}

        virtual void OnEventHappened(uint16 , bool , bool ) {}

        virtual void OnCreatureLooted(Creature* , LootType) {}

        virtual uint64 GetData64(uint32 ) const { return 0; }
        virtual void SetData64(uint32 , uint64 ) {}

        ObjectGuid GetGuid(uint32 dataIdx) const { return static_cast<ObjectGuid>(GetData64(dataIdx)); }
        void SetGuid(uint32 dataIdx, ObjectGuid value) { SetData64(dataIdx, value); }

        virtual uint32 GetData(uint32 ) const { return 0; }
        virtual void SetData(uint32 , uint32 ) {}

        virtual bool CheckConditionCriteriaMeet(Player const* source, uint32 instance_condition_id, Occupant const* conditionSource, uint32 conditionSourceType) const;
};
