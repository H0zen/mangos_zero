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

#include "UpdateBacklog.h"

#include "Object.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "UpdateData.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <vector>

void UpdateBacklog::Send()
{
    UpdateDataMapType update_players;

    // A passenger's block names its hull by guid and carries (0,0,0) for a world
    // position, so the client can only place it once that hull exists. Hulls go
    // into every observer's batch first; the set this came from is ordered by
    // pointer, which says nothing about either.
    std::vector<Object*> hulls;
    std::vector<Object*> rest;
    rest.reserve(m_waiting.size());

    for (Object* obj : m_waiting)
    {
        (obj->GetObjectGuid().IsMOTransport() ? hulls : rest).push_back(obj);
    }
    m_waiting.clear();

    for (Object* obj : hulls)
    {
        obj->BuildUpdateData(update_players);
    }
    for (Object* obj : rest)
    {
        obj->BuildUpdateData(update_players);
    }

    WorldPacket packet;                                     // here we allocate a std::vector with a size of 0x10000
    for (UpdateDataMapType::iterator iter = update_players.begin(); iter != update_players.end(); ++iter)
    {
        iter->second.BuildPacket(&packet);
        iter->first->GetSession()->SendPacket(&packet);
        packet.clear();                                     // clean the string
    }
}
