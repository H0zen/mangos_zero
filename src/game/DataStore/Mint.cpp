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

#include "Mint.h"

#include "Log.h"
#include "World.h"

#include <limits>

Mint::Mint()
    : m_players("player guid", ObjectGuid::GetMaxCounter(HIGHGUID_PLAYER)),
      m_items("item guid", ObjectGuid::GetMaxCounter(HIGHGUID_ITEM)),
      m_corpses("corpse guid", ObjectGuid::GetMaxCounter(HIGHGUID_CORPSE)),
      m_auctions("auction id", std::numeric_limits<uint32>::max()),
      m_guilds("guild id", std::numeric_limits<uint32>::max()),
      m_groups("group id", std::numeric_limits<uint32>::max()),
      m_mails("mail id", std::numeric_limits<uint32>::max()),
      m_pets("pet number", std::numeric_limits<uint32>::max()),
      m_staticCreatures("creature guid", ObjectGuid::GetMaxCounter(HIGHGUID_UNIT)),
      m_staticGameObjects("gameobject guid", ObjectGuid::GetMaxCounter(HIGHGUID_GAMEOBJECT))
{
}

uint32 Mint::Counter::Next()
{
    uint32 const given = m_next.fetch_add(1, std::memory_order_relaxed);

    if (given >= m_ceiling - 1)
    {
        // A spent range must never wrap. For players the value one below the
        // ceiling is the reserved auction-house system owner, and handing it out
        // would brand a real character with the forged owner's guid before the
        // shutdown takes effect -- so nothing is handed out at all.
        sLog.outError("%s overflow!! Can't continue, shutting down server. ", m_name);
        World::StopNow(ERROR_EXIT_CODE);
        return 0;
    }

    return given;
}

uint32 Mint::StaticCreatureGuid()
{
    if (m_staticCreatures.NextAfterMaxUsed() >= m_firstTemporaryCreature)
    {
        return 0;
    }

    return m_staticCreatures.Next();
}

uint32 Mint::StaticGameObjectGuid()
{
    if (m_staticGameObjects.NextAfterMaxUsed() >= m_firstTemporaryGameObject)
    {
        return 0;
    }

    return m_staticGameObjects.Next();
}
