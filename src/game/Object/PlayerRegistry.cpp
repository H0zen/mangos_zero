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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "PlayerRegistry.h"

#include "Player.h"
#include "World.h"
#include "WorldSession.h"

#include <cstring>

Player* PlayerRegistry::Find(ObjectGuid guid, bool inWorld ) const
{
    WorkSentry::Reached("the roster of everyone online");

    if (!guid)
    {
        return nullptr;
    }

    Player* player = m_players.Find(guid);
    if (!player)
    {
        return nullptr;
    }

    return (player->IsInWorld() || !inWorld) ? player : nullptr;
}

Player* PlayerRegistry::FindByName(const char* name) const
{
    WorkSentry::Reached("the roster of everyone online");

    if (!name)
    {
        return nullptr;
    }

    return m_players.FindWith([name](const ObjectGuid&, Player* player) -> bool
    {
        return player->IsInWorld() && std::strcmp(name, player->GetName()) == 0;
    });
}

void PlayerRegistry::Kick(ObjectGuid guid) const
{

    if (Player* player = Find(guid, false))
    {
        WorldSession* session = player->GetSession();
        session->KickPlayer();
        session->LogoutPlayer(false);
    }
}

void PlayerRegistry::SaveAll() const
{

    for (const auto& iter : sWorld.GetAllSessions())
    {
        if (Player* player = iter.second->GetPlayer())
        {
            if (player->IsInWorld())
            {
                player->SaveToDB();
            }
        }
    }
}

void PlayerRegistry::Add(Player* player)
{
    m_players.Insert(player->GetObjectGuid(), player);
}

void PlayerRegistry::Remove(Player* player)
{
    m_players.Remove(player->GetObjectGuid());
}
