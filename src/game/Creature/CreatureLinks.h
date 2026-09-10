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

#include "CreatureLinkingMgr.h"

class Creature;
class Map;
class Unit;

class CreatureLinks
{
    public:

        explicit CreatureLinks(Creature& whose) : m_owner(whose) {}

        void Enrol(Map& map);

        bool AnyoneListens() const { return m_listenedTo; }

        void WaitsOnAnother() { m_spawnWaits = true; }
        bool MayRespawn() const;

        void Aggroed(Unit* enemy);
        void Evaded();
        void Died();
        void Respawned();
        void Despawned();

        bool RefollowMaster();

    private:

        void Tell(CreatureLinkingEvent what, Unit* enemy = nullptr);

        Creature& m_owner;

        bool m_listenedTo = false;
        bool m_spawnWaits = false;
};
