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

#include <string>
#include "InstanceData.h"
#include "Database/DatabaseEnv.h"
#include "Map.h"

void InstanceData::SaveToDB() const
{

    if (instance->IsBattleGround())
    {
        return;
    }

    if (!Save())
    {
        return;
    }

    std::string data = Save();
    CharacterDatabase.escape_string(data);

    if (instance->Instanceable())
    {
        CharacterDatabase.PExecute("UPDATE `instance` SET `data` = '%s' WHERE `id` = '%u'", data.c_str(), instance->GetInstanceId());
    }
    else
    {
        CharacterDatabase.PExecute("UPDATE `world` SET `data` = '%s' WHERE `map` = '%u'", data.c_str(), instance->GetId());
    }
}

bool InstanceData::CheckConditionCriteriaMeet(Player const* , uint32 instance_condition_id, Occupant const* , uint32 conditionSourceType) const
{
    sLog.outError("Condition system call InstanceData::CheckConditionCriteriaMeet but instance script for map %u not have implementation for player condition criteria with internal id %u (called from %u)",
        instance->GetId(), instance_condition_id, uint32(conditionSourceType));
    return false;
}
