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

namespace npcs
{

    bool Listed(Creature const& who);

    void Save(Creature& who);

    void SaveOn(Creature& who, uint32 mapId);

    void Forget(Creature& who);

    void Forget(uint32 lowGuid, CreatureData const* data);

    void SaveRespawnTime(Creature& who);

    void SpawnInMaps(uint32 lowGuid, CreatureData const* data);

    void RemoveFromMaps(uint32 lowGuid, CreatureData const* data);
}
