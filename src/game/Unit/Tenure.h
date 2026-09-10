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

#include "Platform/Define.h"
#include "ObjectGuid.h"

class Creature;

enum TempSpawnType
{
    TEMPSPAWN_MANUAL_DESPAWN = 0,
    TEMPSPAWN_DEAD_DESPAWN = 1,
    TEMPSPAWN_CORPSE_DESPAWN = 2,
    TEMPSPAWN_CORPSE_TIMED_DESPAWN = 3,
    TEMPSPAWN_TIMED_DESPAWN = 4,
    TEMPSPAWN_TIMED_OOC_DESPAWN = 5,
    TEMPSPAWN_TIMED_OR_DEAD_DESPAWN = 6,
    TEMPSPAWN_TIMED_OR_CORPSE_DESPAWN = 7,
    TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN = 8,
    TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN = 9
};

namespace tenure
{

    struct Body
    {
        bool inCombat = false;
        bool alive = true;
        bool dead = false;
        bool corpse = false;
        bool despawned = false;
    };

    struct Verdict
    {
        bool vanish = false;
        uint32 left = 0;
    };

    Verdict Tick(TempSpawnType rule, uint32 left, uint32 granted, uint32 elapsed, Body const& body);

    Body BodyOf(Creature const& who);
}

class Tenure
{
    public:

        void SummonedBy(ObjectGuid who) { m_summoner = who; }

        void Grant(TempSpawnType rule, uint32 howLong)
        {
            m_rule = rule;
            m_granted = howLong;
            m_left = howLong;
        }

        ObjectGuid Summoner() const { return m_summoner; }
        TempSpawnType Rule() const { return m_rule; }
        uint32 Left() const { return m_left; }
        uint32 Granted() const { return m_granted; }

        bool Bounded() const { return m_granted > 0; }

        bool RunsOut(uint32 elapsed, tenure::Body const& body)
        {
            tenure::Verdict const said = tenure::Tick(m_rule, m_left, m_granted, elapsed, body);
            m_left = said.left;
            return said.vanish;
        }

    private:
        ObjectGuid m_summoner = 0;
        TempSpawnType m_rule = TEMPSPAWN_MANUAL_DESPAWN;
        uint32 m_granted = 0;
        uint32 m_left = 0;
};
