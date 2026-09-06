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
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "PerKind.h"

#include "Occupant.h"
#include "Creature.h"
#include "SpawnRecord.h"
#include "GameObject.h"
#include "Item.h"
#include "ItemPrototype.h"

bool StartsQuest(Object const& object, uint32 questId)
{
    if (Creature const* creature = ToCreature(&object))
    {
        return creature->OffersQuest(questId);
    }

    if (GameObject const* go = ToGameObject(&object))
    {
        return go->OffersQuest(questId);
    }

    if (object.GetTypeId() == TYPEID_ITEM || object.GetTypeId() == TYPEID_CONTAINER)
    {
        // An item starts exactly one quest, named in its prototype, and takes
        // none back.
        return static_cast<Item const&>(object).GetProto()->StartQuest == questId;
    }

    return false;
}

bool EndsQuest(Object const& object, uint32 questId)
{
    if (Creature const* creature = ToCreature(&object))
    {
        return creature->TakesQuest(questId);
    }

    if (GameObject const* go = ToGameObject(&object))
    {
        return go->TakesQuest(questId);
    }

    return false;
}

LootClaim* ClaimOn(Occupant& holder)
{
    if (Creature* creature = ToCreature(&holder))
    {
        return &creature->Claim();
    }

    if (GameObject* go = ToGameObject(&holder))
    {
        return &go->Claim();
    }

    return nullptr;
}

void SaveRespawnTime(Occupant& what)
{
    if (Creature* creature = ToCreature(&what))
    {
        npcs::SaveRespawnTime(*creature);
    }
    else if (GameObject* go = ToGameObject(&what))
    {
        go->SaveRespawnTime();
    }
}
