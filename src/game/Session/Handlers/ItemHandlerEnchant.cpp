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

#include "Platform/Define.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ItemEnchantAnswers.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Item.h"
#include "UpdateData.h"
#include "Chat.h"
#include "World.h"

void WorldSession::SendEnchantmentLog(ObjectGuid targetGuid, ObjectGuid casterGuid, uint32 itemId, uint32 spellId)
{
    WorldPacket data(SMSG_ENCHANTMENTLOG, (8 + 8 + 4 + 4 + 1));
    data << static_cast<ObjectGuid>(targetGuid);
    data << static_cast<ObjectGuid>(casterGuid);
    data << uint32(itemId);
    data << uint32(spellId);
    data << uint8(0);
    SendPacket(&data);
}

void WorldSession::SendItemEnchantTimeUpdate(ObjectGuid playerGuid, ObjectGuid itemGuid, uint32 slot, uint32 duration)
{

    WorldPacket data(SMSG_ITEM_ENCHANT_TIME_UPDATE, (8 + 4 + 4 + 8));
    data << static_cast<ObjectGuid>(itemGuid);
    data << uint32(slot);
    data << uint32(duration);
    data << static_cast<ObjectGuid>(playerGuid);
    SendPacket(&data);
}

void items::WrapItem(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_WRAP_ITEM");

    uint8 gift_bag, gift_slot, item_bag, item_slot;

    recv_data >> gift_bag >> gift_slot;
    recv_data >> item_bag >> item_slot;

    DEBUG_LOG("WRAP: receive gift_bag = %u, gift_slot = %u, item_bag = %u, item_slot = %u", gift_bag, gift_slot, item_bag, item_slot);

    Item* gift = who.GetItemByPos(gift_bag, gift_slot);
    if (!gift)
    {
        who.SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, gift, nullptr);
        return;
    }

    if (!(gift->GetProto()->Flags & ITEM_FLAG_WRAPPER) || gift->GetMaxStackCount() == 1)
    {
        who.SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, gift, nullptr);
        return;
    }

    Item* item = who.GetItemByPos(item_bag, item_slot);

    if (!item)
    {
        who.SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, item, nullptr);
        return;
    }

    if (item == gift)
    {
        who.SendEquipError(EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, nullptr);
        return;
    }

    if (item->IsEquipped())
    {
        who.SendEquipError(EQUIP_ERR_EQUIPPED_CANT_BE_WRAPPED, item, nullptr);
        return;
    }

    if (item->GetGiftCreatorGuid())
    {
        who.SendEquipError(EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, nullptr);
        return;
    }

    if (item->IsBag())
    {
        who.SendEquipError(EQUIP_ERR_BAGS_CANT_BE_WRAPPED, item, nullptr);
        return;
    }

    if (item->IsSoulBound())
    {
        who.SendEquipError(EQUIP_ERR_BOUND_CANT_BE_WRAPPED, item, nullptr);
        return;
    }

    if (item->GetMaxStackCount() != 1)
    {
        who.SendEquipError(EQUIP_ERR_STACKABLE_CANT_BE_WRAPPED, item, nullptr);
        return;
    }

    if (item->GetProto()->MaxCount > 0)
    {
        who.SendEquipError(EQUIP_ERR_UNIQUE_CANT_BE_WRAPPED, item, nullptr);
        return;
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("INSERT INTO `character_gifts` VALUES ('%u', '%u', '%u', '%u')", GuidCounter(item->GetOwnerGuid()), item->GetGUIDLow(), item->GetEntry(), item->GetItemFlags());
    item->SetEntry(gift->GetEntry());

    switch (item->GetEntry())
    {
        case 5042:  item->SetEntry(5043); break;
        case 5048:  item->SetEntry(5044); break;
        case 17303: item->SetEntry(17302); break;
        case 17304: item->SetEntry(17305); break;
        case 17307: item->SetEntry(17308); break;
        case 21830: item->SetEntry(21831); break;
    }
    item->SetGiftCreatorGuid(who.GetObjectGuid());
    item->SetAllItemFlags(ITEM_DYNFLAG_WRAPPED);
    item->SetState(ITEM_CHANGED, &who);

    if (item->GetState() == ITEM_NEW)
    {

        who.ItemSaves().Forget(item);
        item->SaveToDB();
    }
    CharacterDatabase.CommitTransaction();

    uint32 count = 1;
    who.DestroyItemCount(gift, count, true);
}

void items::CancelTempEnchantment(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_CANCEL_TEMP_ENCHANTMENT");

    uint32 eslot;

    recv_data >> eslot;

    if (!Inventory::IsWorn(INVENTORY_SLOT_BAG_0, eslot))
    {
        return;
    }

    Item* item = who.GetItemByPos(INVENTORY_SLOT_BAG_0, eslot);

    if (!item)
    {
        return;
    }

    if (!item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
    {
        return;
    }

    who.ApplyEnchantment(item, TEMP_ENCHANTMENT_SLOT, false);
    item->ClearEnchantment(TEMP_ENCHANTMENT_SLOT);
}
