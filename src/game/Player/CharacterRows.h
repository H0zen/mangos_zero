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

#include "ObjectGuid.h"
#include "Platform/Define.h"
#include "Util.h"

class QueryResult;
class WorldPacket;

/**
 * The rows the `characters` table holds, reached by guid.
 *
 * Everything here is about a character who is not in the world: one being
 * offered in the character list, one whose level a command wants to know before
 * loading him, one being deleted. There is no character object to ask, so these
 * go to the row instead.
 *
 * Anything a loaded character can answer for himself belongs on him, not here.
 * The two do overlap -- a level is a level -- and where they do, the row is only
 * consulted when nothing is loaded.
 */
class CharacterRows
{
    public:
        /// Writes the account's characters into the list the client is offered
        /// at the character screen, one after another. Comes back false when the
        /// query holds nothing.
        static bool WriteCharacterList(QueryResult* result, WorldPacket* into);

        static uint32 LevelOf(ObjectGuid guid);
        static uint32 ZoneOf(ObjectGuid guid);
        static uint32 GuildOf(ObjectGuid guid);
        static uint32 GuildRankOf(ObjectGuid guid);

        /// Where a character stands, and whether he is in the air on a taxi at
        /// the time. Comes back false when there is no such row.
        static bool PlaceOf(ObjectGuid guid, uint32& mapid, float& x, float& y, float& z,
                            float& o, bool& in_flight);

        /// Moves a character who is not loaded, by writing the row directly.
        static void SetPlaceOf(ObjectGuid guid, uint32 mapid, float x, float y, float z,
                               float o, uint32 zone);

        /// Removes a character and everything hanging off him. A deletion may be
        /// kept for a while instead of carried out, which is what the realm's
        /// deletion method decides; deleteFinally forces the row to go now.
        static void Delete(ObjectGuid playerguid, uint32 accountId,
                           bool updateRealmChars = true, bool deleteFinally = false);

        /// Carries out the deletions that were kept, once they are old enough.
        /// The default is the realm's configured number of days.
        static void DeleteLongDeleted();
        static void DeleteLongDeleted(uint32 keepDays);

    private:
        /// Overwrites one value in a row stored as a run of numbers in text, of
        /// which the equipment cache is the only one left.
        static void PutInto(Tokens& data, uint16 index, uint32 value);
};
