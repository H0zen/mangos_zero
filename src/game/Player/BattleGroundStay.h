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

/**
 * A character's stay in a battleground: which one, which side, and the way back.
 *
 * The side he fights on is kept apart from the side he was born on, because a
 * battleground may put him on either and the two must not be confused; where
 * nothing has been set, his own side answers.
 *
 * The way back is written down when he is called in, not when he leaves, since
 * by the time he leaves he is already standing inside and the place he came from
 * would be lost. A character who joined from a dungeon is sent to that dungeon's
 * graveyard instead, and one whose entry point cannot be worked out at all goes
 * to his inn.
 *
 * The way back survives a logout, so it is written to its own row and the stay
 * remembers whether that row is behind what it holds.
 */
class BattleGroundStay
{
    public:

        explicit BattleGroundStay(Player& who) : m_owner(who) {}

        /// He is standing in one.
        bool InOne() const { return m_instanceId != 0; }

        uint32 Id() const { return m_instanceId; }
        BattleGroundTypeId Kind() const { return m_kind; }

        /// The battleground itself, or nothing when he is in none.
        BattleGround* Ground() const;

        /// Says which battleground he is in, or none.
        void In(uint32 instanceId, BattleGroundTypeId kind)
        {
            m_instanceId = instanceId;
            m_kind = kind;
            m_unsaved = true;
        }

        /// The side he fights on there, falling back to the side he was born on.
        Team Side() const;
        void Side(Team team)
        {
            m_side = team;
            m_unsaved = true;
        }

        /// The side exactly as it stands, without the fallback. For the row.
        Team SideAsSet() const { return m_side; }

        Geometry::Placement const& CameFrom() const { return m_cameFrom; }

        /// Reads the stay back out of its row, which by definition is not behind.
        void FromRow(uint32 instanceId, Team side, Geometry::Placement const& cameFrom)
        {
            m_instanceId = instanceId;
            m_side = side;
            m_cameFrom = cameFrom;
        }

        /// Which battleground it turned out to be, learnt from the live one at
        /// login. The row already holds the instance, so it is not behind.
        void KindIsKnown(BattleGroundTypeId kind) { m_kind = kind; }

        /// Works out the way back and writes it down.
        void RecordTheWayBack(Player* leader = nullptr);

        /// Sends him back the way he came.
        bool TeleportBack();

        /// Takes him out, with the deserter's penalty where it is due.
        void Leave(bool teleportBack = true);

        /// The deserter's debuff keeps him out.
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
