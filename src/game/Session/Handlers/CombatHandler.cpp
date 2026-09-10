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

#include "Reaction.h"
#include "Platform/Define.h"
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "CombatAnswers.h"
#include "ObjectGuid.h"
#include "Player.h"

void combat::AttackSwing(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;
    recv_data >> guid;

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Received opcode CMSG_ATTACKSWING %s", GuidString(guid).c_str());

    if (!(GuidHigh(guid) == HIGHGUID_UNIT || GuidHigh(guid) == HIGHGUID_PET || GuidHigh(guid) == HIGHGUID_PLAYER))
    {
        sLog.outError("WORLD: %s isn't unit", GuidString(guid).c_str());
        return;
    }

    Unit* pEnemy = who.GetMap()->GetUnit(guid);

    if (!pEnemy)
    {
        sLog.outError("WORLD: Enemy %s not found", GuidString(guid).c_str());

        who.GetSession()->SendAttackStop(nullptr);
        return;
    }

    if (IsFriendly(who, *pEnemy) || pEnemy->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
    {
        sLog.outError("WORLD: Enemy %s is friendly", GuidString(guid).c_str());

        who.GetSession()->SendAttackStop(pEnemy);
        return;
    }

    if (!pEnemy->IsAlive())
    {

        who.GetSession()->SendAttackStop(pEnemy);
        return;
    }

    who.Attack(pEnemy, true);
}

void combat::AttackStop(Player& who, WorldPacket& )
{
    who.AttackStop();
}

void combat::SetSheathed(Player& who, WorldPacket& recv_data)
{
    uint32 sheathed;
    recv_data >> sheathed;

    DEBUG_LOG("WORLD: Received opcode CMSG_SETSHEATHED for %s - value: %u", who.GetGuidStr().c_str(), sheathed);

    if (sheathed >= MAX_SHEATH_STATE)
    {
        sLog.outError("Unknown sheath state %u ??", sheathed);
        return;
    }

    who.SetSheath(SheathState(sheathed));
}

void WorldSession::SendAttackStop(Unit const* enemy)
{
    WorldPacket data(SMSG_ATTACKSTOP, (4 + 20));
    data << GetPlayer()->GetPackGUID();
    data << (enemy ? enemy->GetPackGUID() : PackedGuid());
    data << uint32(0);
    SendPacket(&data);
}
