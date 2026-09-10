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
#include <string>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ItemAnswers.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Item.h"
#include "UpdateData.h"
#include "Chat.h"
#include "World.h"

void items::SplitItem(Player& who, WorldPacket& recv_data)
{

    uint8 srcbag, srcslot, dstbag, dstslot, count;

    recv_data >> srcbag >> srcslot >> dstbag >> dstslot >> count;

    uint16 src = ((srcbag << 8) | srcslot);
    uint16 dst = ((dstbag << 8) | dstslot);

    if (src == dst)
    {
        return;
    }

    if (count == 0)
    {
        return;
    }

    if (!who.IsValidPos(srcbag, srcslot, true))
    {
        who.SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    if (!who.IsValidPos(dstbag, dstslot, false))
    {
        who.SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, nullptr, nullptr);
        return;
    }

    who.SplitItem(src, dst, count);
}

void items::SwapInvItem(Player& who, WorldPacket& recv_data)
{

    uint8 srcslot, dstslot;

    recv_data >> srcslot >> dstslot;

    if (srcslot == dstslot)
    {
        return;
    }

    if (!who.IsValidPos(INVENTORY_SLOT_BAG_0, srcslot, true))
    {
        who.SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    if (!who.IsValidPos(INVENTORY_SLOT_BAG_0, dstslot, true))
    {
        who.SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, nullptr, nullptr);
        return;
    }

    uint16 src = ((INVENTORY_SLOT_BAG_0 << 8) | srcslot);
    uint16 dst = ((INVENTORY_SLOT_BAG_0 << 8) | dstslot);

    who.SwapItem(src, dst);
}

void items::AutoEquipItemSlot(Player& who, WorldPacket& recv_data)
{
    ObjectGuid itemGuid = 0;
    uint8 dstslot;
    recv_data >> itemGuid >> dstslot;

    if (!Inventory::IsWorn(INVENTORY_SLOT_BAG_0, dstslot))
    {
        return;
    }

    Item* item = who.GetItemByGuid(itemGuid);
    uint16 dstpos = dstslot | (INVENTORY_SLOT_BAG_0 << 8);

    if (!item || item->GetPos() == dstpos)
    {
        return;
    }

    who.SwapItem(item->GetPos(), dstpos);
}

void items::SwapItem(Player& who, WorldPacket& recv_data)
{

    uint8 dstbag, dstslot, srcbag, srcslot;

    recv_data >> dstbag >> dstslot >> srcbag >> srcslot ;

    uint16 src = ((srcbag << 8) | srcslot);
    uint16 dst = ((dstbag << 8) | dstslot);

    if (src == dst)
    {
        return;
    }

    if (!who.IsValidPos(srcbag, srcslot, true))
    {
        who.SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    if (!who.IsValidPos(dstbag, dstslot, true))
    {
        who.SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, nullptr, nullptr);
        return;
    }

    who.SwapItem(src, dst);
}

void items::AutoEquipItem(Player& who, WorldPacket& recv_data)
{

    uint8 srcbag, srcslot;

    recv_data >> srcbag >> srcslot;

    Item* pSrcItem  = who.GetItemByPos(srcbag, srcslot);
    if (!pSrcItem)
    {
        return;
    }

    uint16 dest;
    InventoryResult msg = who.CanEquipItem(NULL_SLOT, dest, pSrcItem, !pSrcItem->IsBag());
    if (msg != EQUIP_ERR_OK)
    {
        who.SendEquipError(msg, pSrcItem, nullptr);
        return;
    }

    uint16 src = pSrcItem->GetPos();
    if (dest == src)
    {
        return;
    }

    Item* pDstItem = who.GetItemByPos(dest);
    if (!pDstItem)
    {
        who.RemoveItem(srcbag, srcslot, true);
        who.EquipItem(dest, pSrcItem, true);
        who.AutoUnequipOffhandIfNeed();
    }
    else
    {
        uint8 dstbag = pDstItem->GetBagSlot();
        uint8 dstslot = pDstItem->GetSlot();

        msg = who.CanUnequipItem(dest, !pSrcItem->IsBag());
        if (msg != EQUIP_ERR_OK)
        {
            who.SendEquipError(msg, pDstItem, nullptr);
            return;
        }

        ItemPosCountVec sSrc;
        uint16 eSrc = 0;
        if (Inventory::IsCarried(src))
        {
            msg = who.CanStoreItem(srcbag, srcslot, sSrc, pDstItem, true);
            if (msg != EQUIP_ERR_OK)
            {
                msg = who.CanStoreItem(srcbag, NULL_SLOT, sSrc, pDstItem, true);
            }
            if (msg != EQUIP_ERR_OK)
            {
                msg = who.CanStoreItem(NULL_BAG, NULL_SLOT, sSrc, pDstItem, true);
            }
        }
        else if (Inventory::IsBanked(src))
        {
            msg = who.CanBankItem(srcbag, srcslot, sSrc, pDstItem, true);
            if (msg != EQUIP_ERR_OK)
            {
                msg = who.CanBankItem(srcbag, NULL_SLOT, sSrc, pDstItem, true);
            }
            if (msg != EQUIP_ERR_OK)
            {
                msg = who.CanBankItem(NULL_BAG, NULL_SLOT, sSrc, pDstItem, true);
            }
        }
        else if (Inventory::IsWorn(src))
        {
            msg = who.CanEquipItem(srcslot, eSrc, pDstItem, true);
            if (msg == EQUIP_ERR_OK)
            {
                msg = who.CanUnequipItem(eSrc, true);
            }
        }

        if (msg != EQUIP_ERR_OK)
        {
            who.SendEquipError(msg, pDstItem, pSrcItem);
            return;
        }

        who.RemoveItem(dstbag, dstslot, false);
        who.RemoveItem(srcbag, srcslot, false);

        who.EquipItem(dest, pSrcItem, true);

        if (Inventory::IsCarried(src))
        {
            who.StoreItem(sSrc, pDstItem, true);
        }
        else if (Inventory::IsBanked(src))
        {
            who.BankItem(sSrc, pDstItem, true);
        }
        else if (Inventory::IsWorn(src))
        {
            who.EquipItem(eSrc, pDstItem, true);
        }

        who.AutoUnequipOffhandIfNeed();
    }
}

void items::DestroyItem(Player& who, WorldPacket& recv_data)
{

    uint8 bag, slot, count, data1, data2, data3;

    recv_data >> bag >> slot >> count >> data1 >> data2 >> data3;

    uint16 pos = (bag << 8) | slot;

    if (Inventory::IsWorn(pos) || Inventory::HoldsBag(pos))
    {
        InventoryResult msg = who.CanUnequipItem(pos, false);
        if (msg != EQUIP_ERR_OK)
        {
            who.SendEquipError(msg, who.GetItemByPos(pos), nullptr);
            return;
        }
    }

    Item* pItem  = who.GetItemByPos(bag, slot);
    if (!pItem)
    {
        who.SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
        return;
    }

    if (pItem->GetProto()->Flags & ITEM_FLAG_INDESTRUCTIBLE)
    {
        who.SendEquipError(EQUIP_ERR_CANT_DROP_SOULBOUND, nullptr, nullptr);
        return;
    }

    if (count)
    {
        uint32 i_count = count;
        who.DestroyItemCount(pItem, i_count, true);
    }
    else
    {
        who.DestroyItem(bag, slot, true);
    }
}

void items::ItemQuerySingle(WorldSession& session, WorldPacket& recv_data)
{

    uint32 item;
    recv_data >> item;
    recv_data.read_skip<uint64>();

    DETAIL_LOG("STORAGE: Item Query = %u", item);

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (pProto)
    {
        int loc_idx = session.GetSessionDbLocaleIndex();

        std::string name = pProto->Name1;
        std::string description = pProto->Description;
        sObjectMgr.GetItemLocaleStrings(pProto->ItemId, loc_idx, &name, &description);

        uint32 requiredLevel = pProto->RequiredLevel;
        switch (pProto->ItemId)
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
                requiredLevel = sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_MOUNT_LEVEL);
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
                requiredLevel = sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_EPIC_MOUNT_LEVEL);
                break;
        }

        WorldPacket data(SMSG_ITEM_QUERY_SINGLE_RESPONSE, 600);
        data << pProto->ItemId;
        data << pProto->Class;

        data << (pProto->Class == ITEM_CLASS_CONSUMABLE ? uint32(0) : pProto->SubClass);
        data << name;
        data << uint8(0x00);
        data << uint8(0x00);
        data << uint8(0x00);
        data << pProto->DisplayInfoID;
        data << pProto->Quality;
        data << pProto->Flags;
        data << pProto->BuyPrice;
        data << pProto->SellPrice;
        data << pProto->InventoryType;
        data << pProto->AllowableClass;
        data << pProto->AllowableRace;
        data << pProto->ItemLevel;
        data << requiredLevel;
        data << pProto->RequiredSkill;
        data << pProto->RequiredSkillRank;
        data << pProto->RequiredSpell;
        data << pProto->RequiredHonorRank;
        data << pProto->RequiredCityRank;
        data << pProto->RequiredReputationFaction;
        data << (pProto->RequiredReputationFaction > 0  ? pProto->RequiredReputationRank : 0);
        data << pProto->MaxCount;
        data << pProto->Stackable;
        data << pProto->ContainerSlots;
        for (int i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            data << pProto->ItemStat[i].ItemStatType;
            data << pProto->ItemStat[i].ItemStatValue;
        }
        for (int i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            data << pProto->Damage[i].DamageMin;
            data << pProto->Damage[i].DamageMax;
            data << pProto->Damage[i].DamageType;
        }

        data << pProto->Armor;
        data << pProto->HolyRes;
        data << pProto->FireRes;
        data << pProto->NatureRes;
        data << pProto->FrostRes;
        data << pProto->ShadowRes;
        data << pProto->ArcaneRes;

        data << pProto->Delay;
        data << pProto->AmmoType;
        data << (float)pProto->RangedModRange;

        for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
        {

            SpellEntry const* spell = sSpellStore.LookupEntry(pProto->Spells[s].SpellId);
            if (spell)
            {
                bool db_data = pProto->Spells[s].SpellCooldown >= 0 || pProto->Spells[s].SpellCategoryCooldown >= 0;

                data << pProto->Spells[s].SpellId;
                data << pProto->Spells[s].SpellTrigger;
                data << uint32(-abs(pProto->Spells[s].SpellCharges));

                if (db_data)
                {
                    data << uint32(pProto->Spells[s].SpellCooldown);
                    data << uint32(pProto->Spells[s].SpellCategory);
                    data << uint32(pProto->Spells[s].SpellCategoryCooldown);
                }
                else
                {
                    data << uint32(spell->RecoveryTime);
                    data << uint32(spell->Category);
                    data << uint32(spell->CategoryRecoveryTime);
                }
            }
            else
            {
                data << uint32(0);
                data << uint32(0);
                data << uint32(0);
                data << uint32(-1);
                data << uint32(0);
                data << uint32(-1);
            }
        }
        data << pProto->Bonding;
        data << description;
        data << pProto->PageText;
        data << pProto->LanguageID;
        data << pProto->PageMaterial;
        data << pProto->StartQuest;
        data << pProto->LockID;
        data << pProto->Material;
        data << pProto->Sheath;
        data << pProto->RandomProperty;
        data << pProto->Block;
        data << pProto->ItemSet;
        data << pProto->MaxDurability;
        data << pProto->Area;
        data << pProto->Map;
        data << pProto->BagFamily;
        session.SendPacket(&data);
    }
    else
    {
        DEBUG_LOG("WORLD: CMSG_ITEM_QUERY_SINGLE - NO item INFO! (ENTRY: %u)", item);
        WorldPacket data(SMSG_ITEM_QUERY_SINGLE_RESPONSE, 4);
        data << uint32(item | 0x80000000);
        session.SendPacket(&data);
    }
}

void items::ReadItem(Player& who, WorldPacket& recv_data)
{

    uint8 bag, slot;
    recv_data >> bag >> slot;

    Item* pItem = who.GetItemByPos(bag, slot);

    if (pItem && pItem->GetProto()->PageText)
    {
        WorldPacket data;

        InventoryResult msg = who.CanUseItem(pItem);

        if (msg == EQUIP_ERR_OK)
        {
            data.Initialize(SMSG_READ_ITEM_OK, 8);
            data << static_cast<ObjectGuid>(pItem->GetObjectGuid());
            DETAIL_LOG("STORAGE: Item page sent");
        }
        else
        {
            data.Initialize(SMSG_READ_ITEM_FAILED, 8 + 1);
            data << static_cast<ObjectGuid>(pItem->GetObjectGuid());
            data << uint8(0);

            DETAIL_LOG("STORAGE: Unable to read item");
            who.SendEquipError(msg, pItem, nullptr);
        }
        who.GetSession()->SendPacket(&data);
    }
    else
    {
        who.SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, nullptr, nullptr);
    }
}

void WorldSession::HandlePageQuerySkippedOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_PAGE_TEXT_QUERY");

    uint32 itemid;
    ObjectGuid guid = 0;

    recv_data >> itemid >> guid;

    DETAIL_LOG("Packet Info: itemid: %u guid: %s", itemid, GuidString(guid).c_str());
}

void items::BuyItemInSlot(Player& who, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_BUY_ITEM_IN_SLOT");
    ObjectGuid vendorGuid = 0;
    ObjectGuid bagGuid = 0;
    uint32 item;
    uint8 bagslot, count;

    recv_data >> vendorGuid >> item >> bagGuid >> bagslot >> count;

    uint8 bag = NULL_BAG;

    if (bagGuid == who.GetObjectGuid())
    {
        bag = INVENTORY_SLOT_BAG_0;
    }
    else
    {
        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = (Bag*)who.GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (bagGuid == pBag->GetObjectGuid())
                {
                    bag = i;
                    break;
                }
            }
        }
    }

    if (bag == NULL_BAG)
    {
        return;
    }

    who.BuyItemFromVendor(vendorGuid, item, count, bag, bagslot);
}

void items::ItemNameQuery(WorldSession& session, WorldPacket& recv_data)
{
    uint32 itemid;
    recv_data >> itemid;
    recv_data.read_skip<uint64>();

    DEBUG_LOG("WORLD: CMSG_ITEM_NAME_QUERY %u", itemid);
    if (ItemPrototype const *pProto = ObjectMgr::GetItemPrototype(itemid))
    {
        int loc_idx = session.GetSessionDbLocaleIndex();

        std::string name = pProto->Name1;
        sObjectMgr.GetItemLocaleStrings(pProto->ItemId, loc_idx, &name);

        WorldPacket data(SMSG_ITEM_NAME_QUERY_RESPONSE, (4 + 10));
        data << uint32(pProto->ItemId);
        data << name;

        session.SendPacket(&data);
        return;
    }
    else
    {
        sLog.outError("WORLD: CMSG_ITEM_NAME_QUERY for item %u failed (unknown item)", itemid);
    }
}
