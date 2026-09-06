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

class Creature;
struct CreatureData;

/**
 * @brief What the world database knows about where a creature stands.
 *
 * The row in `creature` and everything hung off it. A creature that was called
 * up rather than placed has no row here: a pet, a totem, a temporary summon.
 */
namespace npcs
{
    /// Whether the `creature` table lists this one.
    bool Listed(Creature const& who);

    /// Writes its row, on the map it stands on.
    void Save(Creature& who);

    /// Writes its row against a named map.
    void SaveOn(Creature& who, uint32 mapId);

    /// Drops its row and everything hung off it.
    void Forget(Creature& who);

    /// Drops a row nothing is standing on any more.
    void Forget(uint32 lowGuid, CreatureData const* data);

    /// Writes down the hour it comes back, for one the table knows.
    void SaveRespawnTime(Creature& who);

    /// Puts it into every loaded map that wants it.
    void SpawnInMaps(uint32 lowGuid, CreatureData const* data);

    /// Takes it out of every loaded map that has it.
    void RemoveFromMaps(uint32 lowGuid, CreatureData const* data);
}
