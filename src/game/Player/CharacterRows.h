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

class CharacterRows
{
    public:

        static bool WriteCharacterList(QueryResult* result, WorldPacket* into);

        static uint32 LevelOf(ObjectGuid guid);
        static uint32 ZoneOf(ObjectGuid guid);
        static uint32 GuildOf(ObjectGuid guid);
        static uint32 GuildRankOf(ObjectGuid guid);

        static bool PlaceOf(ObjectGuid guid, uint32& mapid, float& x, float& y, float& z,
                            float& o, bool& in_flight);

        static void SetPlaceOf(ObjectGuid guid, uint32 mapid, float x, float y, float z,
                               float o, uint32 zone);

        static void Delete(ObjectGuid playerguid, uint32 accountId,
                           bool updateRealmChars = true, bool deleteFinally = false);

        static void DeleteLongDeleted();
        static void DeleteLongDeleted(uint32 keepDays);

    private:

        static void PutInto(Tokens& data, uint16 index, uint32 value);
};
