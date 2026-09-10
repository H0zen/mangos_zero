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

    std::vector<Object*> hulls;
    std::vector<Object*> rest;
    rest.reserve(m_waiting.size());

    for (Object* obj : m_waiting)
    {
        ((GuidHigh(obj->GetObjectGuid()) == HIGHGUID_MO_TRANSPORT) ? hulls : rest).push_back(obj);
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

    WorldPacket packet;
    for (UpdateDataMapType::iterator iter = update_players.begin(); iter != update_players.end(); ++iter)
    {
        iter->second.BuildPacket(&packet);
        iter->first->GetSession()->SendPacket(&packet);
        packet.clear();
    }
}
