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

#include "Corpse.h"

#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"

#include <sstream>

void Corpse::SaveToDB()
{

    MANGOS_ASSERT(GetType() != CORPSE_BONES);

    CharacterDatabase.BeginTransaction();
    DeleteFromDB();

    std::ostringstream ss;
    ss  << "INSERT INTO `corpse` (`guid`,`player`,`position_x`,`position_y`,`position_z`,`orientation`,`map`,`time`,`corpse_type`,`instance`) VALUES ("
        << GetGUIDLow() << ", "
        << GuidCounter(GetOwnerGuid()) << ", "
        << Where().X() << ", "
        << Where().Y() << ", "
        << Where().Z() << ", "
        << Where().Facing() << ", "
        << GetMapId() << ", "
        << uint64(m_time) << ", "
        << uint32(GetType()) << ", "
        << int(GetInstanceId()) << ")";
    CharacterDatabase.Execute(ss.str().c_str());
    CharacterDatabase.CommitTransaction();
}

void Corpse::DeleteFromDB()
{

    MANGOS_ASSERT(GetType() != CORPSE_BONES);

    static SqlStatementID id;

    SqlStatement stmt = CharacterDatabase.CreateStatement(id, "DELETE FROM `corpse` WHERE `player` = ? AND `corpse_type` <> '0'");
    stmt.PExecute(GuidCounter(GetOwnerGuid()));
}

bool Corpse::LoadFromDB(uint32 lowguid, Field* fields)
{

    uint32 playerLowGuid = fields[1].GetUInt32();
    float positionX     = fields[2].GetFloat();
    float positionY     = fields[3].GetFloat();
    float positionZ     = fields[4].GetFloat();
    float orientation   = fields[5].GetFloat();
    uint32 mapid        = fields[6].GetUInt32();

    Object::_Create(lowguid, 0, HIGHGUID_CORPSE);

    m_time = time_t(fields[7].GetUInt64());
    m_type = CorpseType(fields[8].GetUInt32());

    if (m_type >= MAX_CORPSE_TYPE)
    {
        sLog.outError("%s Owner %s have wrong corpse type (%i), not load.", GetGuidStr().c_str(), GuidString(GetOwnerGuid()).c_str(), m_type);
        return false;
    }

    uint32 instanceid   = fields[9].GetUInt32();
    uint8 gender        = fields[10].GetUInt8();
    uint8 race          = fields[11].GetUInt8();
    uint8 _class        = fields[12].GetUInt8();
    uint32 playerBytes  = fields[13].GetUInt32();
    uint32 playerBytes2 = fields[14].GetUInt32();
    uint32 guildId      = fields[16].GetUInt32();
    uint32 playerFlags  = fields[17].GetUInt32();

    ObjectGuid guid = MakeGuid(HIGHGUID_CORPSE, lowguid);
    ObjectGuid playerGuid = MakeGuid(HIGHGUID_PLAYER, playerLowGuid);

    SetGuidValue(OBJECT_FIELD_GUID, guid);
    SetGuidValue(CORPSE_FIELD_OWNER, playerGuid);

    SetObjectScale(DEFAULT_OBJECT_SCALE);

    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(race, _class);
    if (!info)
    {
        sLog.outError("Player %u has incorrect race/class pair.", GetGUIDLow());
        return false;
    }
    SetUInt32Value(CORPSE_FIELD_DISPLAY_ID, gender == GENDER_FEMALE ? info->displayId_f : info->displayId_m);

    Tokens data = StrSplit(fields[15].GetCppString(), " ");
    for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        uint32 visualbase = slot * 2;
        uint32 item_id = GetUInt32ValueFromArray(data, visualbase);
        const ItemPrototype* proto = ObjectMgr::GetItemPrototype(item_id);
        if (!proto)
        {
            SetUInt32Value(CORPSE_FIELD_ITEM + slot, 0);
            continue;
        }

        SetUInt32Value(CORPSE_FIELD_ITEM + slot, proto->DisplayInfoID | (proto->InventoryType << 24));
    }

    uint8 skin       = (uint8)(playerBytes);
    uint8 face       = (uint8)(playerBytes >> 8);
    uint8 hairstyle  = (uint8)(playerBytes >> 16);
    uint8 haircolor  = (uint8)(playerBytes >> 24);
    uint8 facialhair = (uint8)(playerBytes2);
    SetUInt32Value(CORPSE_FIELD_BYTES_1, ((0x00) | (race << 8) | (gender << 16) | (skin << 24)));
    SetUInt32Value(CORPSE_FIELD_BYTES_2, ((face) | (hairstyle << 8) | (haircolor << 16) | (facialhair << 24)));

    SetUInt32Value(CORPSE_FIELD_GUILD, guildId);

    uint32 flags = CORPSE_FLAG_UNK2;
    if (playerFlags & PLAYER_FLAGS_HIDE_HELM)
    {
        flags |= CORPSE_FLAG_HIDE_HELM;
    }
    if (playerFlags & PLAYER_FLAGS_HIDE_CLOAK)
    {
        flags |= CORPSE_FLAG_HIDE_CLOAK;
    }
    SetUInt32Value(CORPSE_FIELD_FLAGS, flags);

    SetLocationInstanceId(instanceid);
    SetLocationMapId(mapid);
    Place().MoveTo(positionX, positionY, positionZ, orientation);

    if (!IsPlaceable(*this))
    {
        sLog.outError("%s Owner %s not created. Suggested coordinates isn't valid (X: %f Y: %f)",
            GetGuidStr().c_str(), GuidString(GetOwnerGuid()).c_str(), Where().X(), Where().Y());
        return false;
    }

    m_grid = MaNGOS::ComputeGridPair(Where().X(), Where().Y());

    return true;
}
