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

#pragma once

#include "Opcodes.h"
#include "WorldSession.h"

extern void InitializeOpcodes();

enum SessionStatus
{
    STATUS_AUTHED = 0,
    STATUS_LOGGEDIN,
    STATUS_TRANSFER,
    STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT,
    STATUS_NEVER,
    STATUS_UNHANDLED
};

enum PacketProcessing
{
    PROCESS_INPLACE = 0,
    PROCESS_THREADUNSAFE,
    PROCESS_THREADSAFE
};

class Player;

/**
 * Who answers one message from the client.
 *
 * An answer is written either as the session's, when it is about the account or
 * about a hero who is not in the world yet, or as the hero's own. A hero's
 * answer never sees the session: the dispatcher has already made sure he is
 * there and in the world before it calls, so there is nothing left to check.
 */
struct OpcodeHandler
{
    char const* name;
    SessionStatus status;
    PacketProcessing packetProcessing;
    void (WorldSession::*handler)(WorldPacket& recvPacket);
    void (*answer)(WorldSession& session, WorldPacket& packet);
};

/// Hands a hero's answer the hero, so the table can hold one shape of answer.
template <void (*Answer)(Player& who, WorldPacket& packet)>
void PlayerAnswers(WorldSession& session, WorldPacket& packet)
{
    Answer(*session.GetPlayer(), packet);
}

extern OpcodeHandler opcodeTable[NUM_MSG_TYPES];

inline const char* LookupOpcodeName(uint16 id)
{
    if (id >= NUM_MSG_TYPES)
    {
        return "Received unknown opcode, it's more than max!";
    }

    return opcodeTable[id].name ? opcodeTable[id].name : "UNKNOWN";
}
