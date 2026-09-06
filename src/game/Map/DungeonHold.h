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

#include <unordered_map>

class DungeonPersistentState;

/**
 * A hold on one copy of a dungeon: which copy, and whether it lasts.
 *
 * A temporary hold is taken on stepping inside and goes when the copy resets.
 * A permanent one is taken when a boss falls in a raid, or on entering a copy
 * the leader is already permanently held to, and goes only when it is given up.
 *
 * A character and a group hold a dungeon the same way. Who holds it decides
 * which table the hold is written to and which roll the copy keeps him on.
 */
struct DungeonHold
{
    DungeonPersistentState* state = nullptr;
    bool permanent = false;
};

/// The holds someone has, one to a map.
typedef std::unordered_map<uint32 /*mapId*/, DungeonHold> DungeonHolds;
