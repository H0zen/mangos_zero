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

#include "Loot/Spoilable.h"
#include "Platform/Define.h"
#include <ctime>
#include "Occupant.h"
#include "Database/DatabaseEnv.h"
#include "GridDefines.h"
#include "LootMgr.h"

enum CorpseType
{
    CORPSE_BONES             = 0,
    CORPSE_RESURRECTABLE_PVE = 1,
    CORPSE_RESURRECTABLE_PVP = 2
};
#define MAX_CORPSE_TYPE        3

#define CORPSE_RECLAIM_RADIUS 39

enum CorpseFlags
{
    CORPSE_FLAG_NONE        = 0x00,
    CORPSE_FLAG_BONES       = 0x01,
    CORPSE_FLAG_UNK1        = 0x02,
    CORPSE_FLAG_UNK2        = 0x04,
    CORPSE_FLAG_HIDE_HELM   = 0x08,
    CORPSE_FLAG_HIDE_CLOAK  = 0x10,
    CORPSE_FLAG_LOOTABLE    = 0x20
};

class Corpse : public Occupant, public Spoilable
{
    public:
        explicit Corpse(CorpseType type = CORPSE_BONES);
        ~Corpse();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        bool Create(uint32 guidlow);
        bool Create(uint32 guidlow, Player* owner);

        void SaveToDB();
        bool LoadFromDB(uint32 guid, Field* fields);

        void DeleteBonesFromWorld();
        void DeleteFromDB();

        ObjectGuid const& GetOwnerGuid() const { return GetGuidValue(CORPSE_FIELD_OWNER); }

        bool HasCorpseDynFlag(uint32 flag) const { return HasFlag(CORPSE_FIELD_DYNAMIC_FLAGS, flag); }
        void SetCorpseDynFlag(uint32 flag) { SetFlag(CORPSE_FIELD_DYNAMIC_FLAGS, flag); }
        void RemoveCorpseDynFlag(uint32 flag) { RemoveFlag(CORPSE_FIELD_DYNAMIC_FLAGS, flag); }

        time_t const& GetGhostTime() const { return m_time; }
        void ResetGhostTime()
        {
            m_time = time(nullptr);
        }
        CorpseType GetType() const { return m_type; }

        bool IsControlledByPlayer() const override { return true; }

        bool OutlivesItsGrid() const override { return m_type != CORPSE_BONES; }

        GridPair const& GetGrid() const { return m_grid; }
        void SetGrid(GridPair const& grid) { m_grid = grid; }

        bool IsVisibleForInState(Player const* u, Occupant const* viewPoint, bool inVisibleList) const override;

        Loot loot;

        Loot* Spoils() override { return &loot; }

        bool OpenableBy(Player const& who) const override;
        bool FillSpoilsFor(Player& who, LootType& how, PermissionTypes& permission) override;
        Player* lootRecipient;
        bool lootForBody;

        GridReference<Corpse>& GetGridRef()
        {
            return m_gridRef;
        }

        bool IsExpired(time_t now) const;
    private:
        GridReference<Corpse> m_gridRef;

        CorpseType m_type;
        time_t m_time;
        GridPair m_grid;
};
