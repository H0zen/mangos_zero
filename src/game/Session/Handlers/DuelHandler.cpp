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

#include <ctime>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "DuelAnswers.h"
#include "Log.h"
#include "Player.h"

void duels::DuelAccepted(Player& who, WorldPacket& recvPacket)
{
    ObjectGuid guid = 0;
    recvPacket >> guid;

    if (!who.Duelling().Stands())
    {
        return;
    }

    Player* pl       = &who;
    Player* plTarget = pl->Duelling().Against();

    if (pl == pl->Duelling().Initiator() || !plTarget || pl == plTarget || pl->Duelling().StartedAt() != 0 || plTarget->Duelling().StartedAt() != 0)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: received CMSG_DUEL_ACCEPTED");
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "Player 1 is: %u (%s)", pl->GetGUIDLow(), pl->GetName());
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "Player 2 is: %u (%s)", plTarget->GetGUIDLow(), plTarget->GetName());

    time_t now = time(nullptr);
    pl->Duelling().Accepted(now);
    plTarget->Duelling().Accepted(now);

    pl->Duelling().TellCountdown(3000);
    plTarget->Duelling().TellCountdown(3000);
}

void duels::DuelCancelled(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_DUEL_CANCELLED");

    if (!who.Duelling().Stands())
    {
        return;
    }

    if (who.Duelling().StartedAt() != 0)
    {
        who.CombatStopWithPets(true);
        if (who.Duelling().Against())
        {
            who.Duelling().Against()->CombatStopWithPets(true);
        }

        who.CastSpell(&who, 7267, true);
        who.Duelling().Complete(DUEL_WON);
        return;
    }

    ObjectGuid guid = 0;
    recvPacket >> guid;

    who.Duelling().Complete(DUEL_INTERRUPTED);
}
