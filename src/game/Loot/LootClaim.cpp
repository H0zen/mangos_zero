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

#include "LootClaim.h"

#include "Group.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "Unit.h"

bool LootClaim::StakedBy(Unit* taker)
{
    if (!taker)
    {
        m_takerGuid.Clear();
        m_groupId = 0;
        return false;
    }

    // A creature nobody drives stakes nothing, which is how one beast killing
    // another leaves the body to whichever player comes upon it.
    Player* player = taker->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!player)
    {
        return false;
    }

    m_takerGuid = player->GetObjectGuid();

    if (Group* group = player->GetGroup())
    {
        m_groupId = group->GetId();
    }

    return true;
}

Player* LootClaim::Taker() const
{
    return m_takerGuid ? sPlayerRegistry.Find(m_takerGuid) : nullptr;
}

Group* LootClaim::HoldingGroup() const
{
    return m_groupId ? sObjectMgr.GetGroupById(m_groupId) : nullptr;
}

Player* LootClaim::Entitled() const
{
    Group* group = HoldingGroup();
    Player* player = Taker();

    // No group holds it, so it is the taker's alone, present or not.
    if (!group)
    {
        return player;
    }

    if (player && player->GetGroup() == group)
    {
        return player;
    }

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* member = itr->getSource())
        {
            return member;
        }
    }

    return nullptr;
}

void LootClaim::StartRoll(Group* group, uint32 timer)
{
    m_rollGroupId = group->GetId();
    m_rollTimer = timer;
}

void LootClaim::StopRoll()
{
    if (!m_rollGroupId)
    {
        return;
    }

    if (Group* group = sObjectMgr.GetGroupById(m_rollGroupId))
    {
        group->EndRoll();
    }

    m_rollTimer = 0;
    m_rollGroupId = 0;
}

bool LootClaim::TickRoll(uint32 diff)
{
    if (!m_rollGroupId)
    {
        return false;
    }

    if (m_rollTimer <= diff)
    {
        StopRoll();
        return true;
    }

    m_rollTimer -= diff;
    return false;
}
