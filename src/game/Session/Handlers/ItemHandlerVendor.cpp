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

#include <cmath>
#include "Platform/Define.h"
#include "Common/ServerDefines.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ItemVendorAnswers.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Item.h"
#include "UpdateData.h"
#include "Chat.h"
#include "World.h"

void items::SellItem(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SELL_ITEM");

    ObjectGuid vendorGuid = 0;
    ObjectGuid itemGuid = 0;
    uint8 _count;

    recv_data >> vendorGuid;
    recv_data >> itemGuid;
    recv_data >> _count;

    uint32 count = _count;

    if (!itemGuid)
    {
        return;
    }

    Creature* pCreature = who.GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleSellItemOpcode - %s not found or you can't interact with him.", GuidString(vendorGuid).c_str());
        who.SendSellError(SELL_ERR_CANT_FIND_VENDOR, nullptr, itemGuid, 0);
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    Item* pItem = who.GetItemByGuid(itemGuid);
    if (pItem)
    {

        if (who.GetObjectGuid() != pItem->GetOwnerGuid())
        {
            who.SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            return;
        }

        if (pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
        {
            who.SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            return;
        }

        if (who.GetLootGuid() == pItem->GetObjectGuid())
        {
            who.SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            return;
        }

        if (count == 0)
        {
            count = pItem->GetCount();
        }
        else
        {

            if (count > pItem->GetCount())
            {
                who.SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
                return;
            }
        }

        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto)
        {
            if (pProto->SellPrice > 0)
            {
                if (count < pItem->GetCount())
                {
                    Item* pNewItem = pItem->CloneItem(count, &who);
                    if (!pNewItem)
                    {
                        sLog.outError("WORLD: HandleSellItemOpcode - could not create clone of item %u; count = %u", pItem->GetEntry(), count);
                        who.SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
                        return;
                    }

                    pItem->SetCount(pItem->GetCount() - count);
                    who.Journal().ItemLost(pItem->GetEntry(), count);
                    if (who.IsInWorld())
                    {
                        pItem->SendCreateUpdateToPlayer(&who);
                    }
                    pItem->SetState(ITEM_CHANGED, &who);

                    who.AddItemToBuyBackSlot(pNewItem);
                    if (who.IsInWorld())
                    {
                        pNewItem->SendCreateUpdateToPlayer(&who);
                    }
                }
                else
                {
                    who.Journal().ItemLost(pItem->GetEntry(), pItem->GetCount());
                    who.RemoveItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
                    who.ItemSaves().Forget(pItem);
                    who.AddItemToBuyBackSlot(pItem);
                }

                uint32 money = pProto->SellPrice * count;

                who.ModifyMoney(money);
            }
            else
            {
                who.SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            }
            return;
        }
    }
    who.SendSellError(SELL_ERR_CANT_FIND_ITEM, pCreature, itemGuid, 0);
    return;
}

void items::BuybackItem(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_BUYBACK_ITEM");
    ObjectGuid vendorGuid = 0;
    uint32 slot;

    recv_data >> vendorGuid >> slot;

    Creature* pCreature = who.GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleBuybackItem - %s not found or you can't interact with him.", GuidString(vendorGuid).c_str());
        who.SendSellError(SELL_ERR_CANT_FIND_VENDOR, nullptr, 0, 0);
        return;
    }

    if (who.hasUnitState(UNIT_STAT_DIED))
    {
        who.RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    Item* pItem = who.GetItemFromBuyBackSlot(slot);
    if (pItem)
    {
        uint32 price = who.GetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + slot - BUYBACK_SLOT_START);
        if (who.GetMoney() < price)
        {
            who.SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, pItem->GetEntry(), 0);
            return;
        }

        ItemPosCountVec dest;
        InventoryResult msg = who.CanStoreItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
        if (msg == EQUIP_ERR_OK)
        {
            who.ModifyMoney(-(int32)price);
            who.RemoveItemFromBuyBackSlot(slot, false);
            who.Journal().ItemGained(pItem->GetEntry(), pItem->GetCount());
            who.StoreItem(dest, pItem, true);
        }
        else
        {
            who.SendEquipError(msg, pItem, nullptr);
        }
        return;
    }
    else
    {
        who.SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, 0, 0);
    }
}

void items::BuyItem(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_BUY_ITEM");
    ObjectGuid vendorGuid = 0;
    uint32 item;
    uint8 count, unk1;

    recv_data >> vendorGuid >> item >> count >> unk1;

    who.BuyItemFromVendor(vendorGuid, item, count, NULL_BAG, NULL_SLOT);
}

void items::ListInventory(Player& who, WorldPacket& recv_data)
{
    ObjectGuid guid = 0;

    recv_data >> guid;

    if (!who.IsAlive())
    {
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_LIST_INVENTORY");

    who.GetSession()->SendListInventory(guid);
}

void WorldSession::SendListInventory(ObjectGuid vendorguid)
{
    DEBUG_LOG("WORLD: Sent SMSG_LIST_INVENTORY");

    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorguid, UNIT_NPC_FLAG_VENDOR);

    if (!pCreature)
    {
        DEBUG_LOG("WORLD: SendListInventory - %s not found or you can't interact with him.", GuidString(vendorguid).c_str());
        _player->SendSellError(SELL_ERR_CANT_FIND_VENDOR, nullptr, 0, 0);
        return;
    }

    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveAurasOfType(SPELL_AURA_FEIGN_DEATH);
    }

    pCreature->StopMoving();

    VendorItemData const* vItems = pCreature->GetVendorItems();
    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();

    if (!vItems && !tItems)
    {
        WorldPacket data(SMSG_LIST_INVENTORY, (8 + 1 + 1));
        data << static_cast<ObjectGuid>(vendorguid);
        data << uint8(0);
        data << uint8(0);
        SendPacket(&data);
        return;
    }

    uint8 customitems = vItems ? vItems->GetItemCount() : 0;
    uint8 numitems = customitems + (tItems ? tItems->GetItemCount() : 0);

    uint8 count = 0;

    WorldPacket data(SMSG_LIST_INVENTORY, (8 + 1 + numitems * 7 * 4));
    data << static_cast<ObjectGuid>(vendorguid);

    size_t count_pos = data.wpos();
    data << uint8(count);

    float discountMod = _player->GetReputationPriceDiscount(pCreature);

    for (int i = 0; i < numitems; ++i)
    {
        VendorItem const* crItem = i < customitems ? vItems->GetItem(i) : tItems->GetItem(i - customitems);

        if (crItem)
        {
            uint32 itemId = crItem->item;
            ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemId);
            if (pProto)
            {
                if (!_player->isGameMaster())
                {

                    if ((pProto->AllowableClass & _player->getClassMask()) == 0 && pProto->Bonding == BIND_WHEN_PICKED_UP)
                    {
                        continue;
                    }

                    if ((pProto->AllowableRace & _player->getRaceMask()) == 0)
                    {
                        continue;
                    }

                    if (!pProto->RequiredReputationFaction && pProto->RequiredReputationRank > 0 &&
                        ReputationRank(pProto->RequiredReputationRank) > _player->GetReputationRank(pCreature->getFactionTemplateEntry()->Faction))
                    {
                        continue;
                    }

                    if (crItem->conditionId && !sObjectMgr.IsPlayerMeetToCondition(crItem->conditionId, _player, pCreature->GetMap(), pCreature, CONDITION_FROM_VENDOR))
                    {
                        continue;
                    }
                }

                ++count;

                uint32 price = 0;

                switch (itemId)
                {
                    case 1132:
                    case 2411:
                    case 2414:
                    case 5655:
                    case 5656:
                    case 5665:
                    case 5668:
                    case 5864:
                    case 5872:
                    case 5873:
                    case 8563:
                    case 8588:
                    case 8591:
                    case 8592:
                    case 8595:
                    case 8629:
                    case 8631:
                    case 8632:
                    case 12325:
                    case 12326:
                    case 12327:
                    case 13321:
                    case 13322:
                    case 13331:
                    case 13332:
                    case 13333:
                    case 15277:
                    case 15290:
                    case 18241:
                    case 18242:
                    case 18243:
                    case 18244:
                    case 18245:
                    case 18246:
                    case 18247:
                    case 18248:

                        price = uint32(floor(AccountTypes(sWorld.getConfig(CONFIG_UINT32_MOUNT_COST)) * discountMod));
                        break;
                    case 12302:
                    case 12303:
                    case 12330:
                    case 12351:
                    case 12353:
                    case 12354:
                    case 13086:
                    case 13326:
                    case 13327:
                    case 13328:
                    case 13329:
                    case 13334:
                    case 13335:
                    case 18766:
                    case 18767:
                    case 18768:
                    case 18772:
                    case 18773:
                    case 18774:
                    case 18776:
                    case 18777:
                    case 18778:
                    case 18785:
                    case 18786:
                    case 18787:
                    case 18788:
                    case 18789:
                    case 18790:
                    case 18791:
                    case 18793:
                    case 18794:
                    case 18795:
                    case 18796:
                    case 18797:
                    case 18798:
                    case 18902:

                        price = uint32(floor(AccountTypes(sWorld.getConfig(CONFIG_UINT32_EPIC_MOUNT_COST)) * discountMod));
                        break;
                    default:

                        price = uint32(floor(pProto->BuyPrice * discountMod));
                        break;
                }

                data << uint32(count);
                data << uint32(itemId);
                data << uint32(pProto->DisplayInfoID);
                data << uint32(crItem->maxcount <= 0 ? 0xFFFFFFFF : pCreature->GetVendorItemCurrentCount(crItem));
                data << uint32(price);
                data << uint32(pProto->MaxDurability);
                data << uint32(pProto->BuyCount);
            }
        }
    }

    if (count == 0)
    {
        data << uint8(0);
        SendPacket(&data);
        return;
    }

    data.put<uint8>(count_pos, count);
    SendPacket(&data);
}

void items::AutoStoreBagItem(Player& who, WorldPacket& recv_data)
{

    uint8 srcbag, srcslot, dstbag;

    recv_data >> srcbag >> srcslot >> dstbag;

    Item* pItem = who.GetItemByPos(srcbag, srcslot);
    if (!pItem)
    {
        return;
    }

    if (!who.IsValidPos(dstbag, NULL_SLOT, false))
    {
        who.SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, nullptr, nullptr);
        return;
    }

    uint16 src = pItem->GetPos();

    if (Inventory::IsWorn(src) || Inventory::HoldsBag(src))
    {
        InventoryResult msg = who.CanUnequipItem(src, !Inventory::HoldsBag(src));
        if (msg != EQUIP_ERR_OK)
        {
            who.SendEquipError(msg, pItem, nullptr);
            return;
        }
    }

    ItemPosCountVec dest;
    InventoryResult msg = who.CanStoreItem(dstbag, NULL_SLOT, dest, pItem, false);
    if (msg != EQUIP_ERR_OK)
    {
        who.SendEquipError(msg, pItem, nullptr);
        return;
    }

    if (dest.size() == 1 && dest[0].pos == src)
    {

        who.SendEquipError(EQUIP_ERR_NONE, pItem, nullptr);
        return;
    }

    who.RemoveItem(srcbag, srcslot, true);
    who.StoreItem(dest, pItem, true);
}

bool WorldSession::CheckBanker(ObjectGuid guid)
{

    if (guid == GetPlayer()->GetObjectGuid())
    {

        if (!ChatHandler(GetPlayer()).FindCommand("bank"))
        {
            DEBUG_LOG("%s attempt open bank in cheating way.", GuidString(guid).c_str());
            return false;
        }
    }

    else
    {
        if (!GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_BANKER))
        {
            DEBUG_LOG("Banker %s not found or you can't interact with him.", GuidString(guid).c_str());
            return false;
        }
    }

    return true;
}

void items::BuyBankSlot(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_BUY_BANK_SLOT");

    ObjectGuid guid = 0;
    recvPacket >> guid;

    WorldPacket data(SMSG_BUY_BANK_SLOT_RESULT, 4);

    if (!session.CheckBanker(guid))
    {
        data << uint32(ERR_BANKSLOT_NOTBANKER);
        session.SendPacket(&data);
        return;
    }

    uint32 slot = session.GetPlayer()->GetBankBagSlotCount();

    ++slot;

    DETAIL_LOG("PLAYER: Buy bank bag slot, slot number = %u", slot);

    BankBagSlotPricesEntry const* slotEntry = sBankBagSlotPricesStore.LookupEntry(slot);

    if (!slotEntry)
    {
        data << uint32(ERR_BANKSLOT_FAILED_TOO_MANY);
        session.SendPacket(&data);
        return;
    }

    uint32 price = slotEntry->Price;

    if (session.GetPlayer()->GetMoney() < price)
    {
        data << uint32(ERR_BANKSLOT_INSUFFICIENT_FUNDS);
        session.SendPacket(&data);
        return;
    }

    session.GetPlayer()->SetBankBagSlotCount(slot);
    session.GetPlayer()->ModifyMoney(-int32(price));
}

void items::AutoBankItem(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_AUTOBANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item* pItem = who.GetItemByPos(srcbag, srcslot);
    if (!pItem)
    {
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = who.CanBankItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
    if (msg != EQUIP_ERR_OK)
    {
        who.SendEquipError(msg, pItem, nullptr);
        return;
    }

    if (dest.size() == 1 && dest[0].pos == pItem->GetPos())
    {

        who.SendEquipError(EQUIP_ERR_NONE, pItem, nullptr);
        return;
    }

    who.RemoveItem(srcbag, srcslot, true);
    who.BankItem(dest, pItem, true);
}

void items::AutoStoreBankItem(Player& who, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_AUTOSTORE_BANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item* pItem = who.GetItemByPos(srcbag, srcslot);
    if (!pItem)
    {
        return;
    }

    if (Inventory::IsBanked(srcbag, srcslot))
    {
        ItemPosCountVec dest;
        InventoryResult msg = who.CanStoreItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            who.SendEquipError(msg, pItem, nullptr);
            return;
        }

        who.RemoveItem(srcbag, srcslot, true);
        who.StoreItem(dest, pItem, true);
    }
    else
    {
        ItemPosCountVec dest;
        InventoryResult msg = who.CanBankItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            who.SendEquipError(msg, pItem, nullptr);
            return;
        }

        who.RemoveItem(srcbag, srcslot, true);
        who.BankItem(dest, pItem, true);
    }
}

void items::SetAmmo(Player& who, WorldPacket& recv_data)
{
    if (!who.IsAlive())
    {
        who.SendEquipError(EQUIP_ERR_YOU_ARE_DEAD, nullptr, nullptr);
        return;
    }

    DEBUG_LOG("WORLD: CMSG_SET_AMMO");
    uint32 item;

    recv_data >> item;

    if (!item)
    {
        who.RemoveAmmo();
    }
    else
    {
        who.SetAmmo(item);
    }
}
