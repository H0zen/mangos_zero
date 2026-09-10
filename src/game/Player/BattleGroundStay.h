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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#pragma once

#include "BattleGround.h"
#include "Geometry/Placement.h"
#include "Platform/Define.h"
#include "SharedDefines.h"

class BattleGround;
class Group;
class Player;

class BattleGroundStay
{
    public:

        explicit BattleGroundStay(Player& who) : m_owner(who) {}

        bool InOne() const { return m_instanceId != 0; }

        uint32 Id() const { return m_instanceId; }
        BattleGroundTypeId Kind() const { return m_kind; }

        BattleGround* Ground() const;

        void In(uint32 instanceId, BattleGroundTypeId kind)
        {
            m_instanceId = instanceId;
            m_kind = kind;
            m_unsaved = true;
        }

        Team Side() const;
        void Side(Team team)
        {
            m_side = team;
            m_unsaved = true;
        }

        Team SideAsSet() const { return m_side; }

        Geometry::Placement const& CameFrom() const { return m_cameFrom; }

        void FromRow(uint32 instanceId, Team side, Geometry::Placement const& cameFrom)
        {
            m_instanceId = instanceId;
            m_side = side;
            m_cameFrom = cameFrom;
        }

        void KindIsKnown(BattleGroundTypeId kind) { m_kind = kind; }

        void RecordTheWayBack(Player* leader = nullptr);

        bool TeleportBack();

        void Leave(bool teleportBack = true);

        bool MayJoin() const;

        bool Unsaved() const { return m_unsaved; }
        void Saved() { m_unsaved = false; }

    private:

        Player& m_owner;

        uint32 m_instanceId = 0;
        BattleGroundTypeId m_kind = BATTLEGROUND_TYPE_NONE;
        Team m_side = TEAM_NONE;

        Geometry::Placement m_cameFrom;

        bool m_unsaved = false;
};
