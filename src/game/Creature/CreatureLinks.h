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

/**
 * What one creature's fortunes do to another's.
 *
 * Two creatures can be tied together in the data: one is the master and the
 * other follows it. When the master is pulled, dies, gives up the chase, is
 * cleared away or comes back, the ones tied to it are told, and they aggro,
 * despawn, respawn or follow it home in turn. A creature can also be held back
 * from spawning at all until the one it waits on is dead.
 *
 * Which creature is tied to which is the map's business -- the holder there
 * keeps the lists. What lives here is the one creature's side of it: whether
 * anything at all listens to it, whether its own spawn waits on someone, and
 * the naming of what has just happened to it.
 */
class CreatureLinks
{
    public:

        explicit CreatureLinks(Creature& whose) : m_owner(whose) {}

        /// Puts the creature on the map's lists, as a follower, as a master, or
        /// as both, according to what the data says of it.
        void Enrol(Map& map);

        /// Whether anything is tied to this creature at all.
        bool AnyoneListens() const { return m_listenedTo; }

        /// Whether its own spawn waits on another creature, and whether that
        /// other has cleared the way.
        void WaitsOnAnother() { m_spawnWaits = true; }
        bool MayRespawn() const;

        void Aggroed(Unit* enemy);
        void Evaded();
        void Died();
        void Respawned();
        void Despawned();

        /// Sends it back to walk with its master, if it has one to walk with.
        bool RefollowMaster();

    private:

        void Tell(CreatureLinkingEvent what, Unit* enemy = nullptr);

        Creature& m_owner;

        bool m_listenedTo = false;
        bool m_spawnWaits = false;
};
