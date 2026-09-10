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

#include "Player.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "Language.h"
#include "SpellMgr.h"
#include "World.h"
#include "CinematicFlyover.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "ItemDestination.h"

Item* Player::StoreNewItem(ItemPosCountVec const& dest, uint32 item, bool update, int32 randomPropertyId)
{
    uint32 count = 0;
    for (ItemPosCountVec::const_iterator itr = dest.begin(); itr != dest.end(); ++itr)
    {
        count += itr->count;
    }

    Item* pItem = Item::CreateItem(item, count, this, randomPropertyId);
    if (pItem)
    {
        m_journal.ItemGained(item, count);
        pItem = StoreItem(dest, pItem, update);
    }
    return pItem;
}

Item* Player::EquipNewItem(uint16 pos, uint32 item, bool update)
{
    if (Item* pItem = Item::CreateItem(item, 1, this))
    {
        m_journal.ItemGained(item, 1);
        return EquipItem(pos, pItem, update);
    }

    return nullptr;
}

Item* Player::EquipItem(uint16 pos, Item* pItem, bool update)
{
    m_inventory.StartClocks(pItem);

    uint8 bag = pos >> 8;
    uint8 slot = pos & 255;

    Item* pItem2 = GetItemByPos(bag, slot);

    if (!pItem2)
    {
        m_inventory.Wear(slot, pItem);

        if (IsAlive())
        {
            ItemPrototype const* pProto = pItem->GetProto();

            if (pProto && pProto->ItemSet)
            {
                AddItemsSetItem(this, pItem);
            }

            _ApplyItemMods(pItem, slot, true);

            if (pProto && IsInCombat() && (pProto->Class == ITEM_CLASS_WEAPON || pProto->InventoryType == INVTYPE_RELIC) && Arms().ChangeTimer() == 0)
            {
                uint32 cooldownSpell = SPELL_ID_WEAPON_SWITCH_COOLDOWN_1_5s;

                if (getClass() == CLASS_ROGUE)
                {
                    cooldownSpell = SPELL_ID_WEAPON_SWITCH_COOLDOWN_1_0s;
                }

                SpellEntry const* spellProto = sSpellStore.LookupEntry(cooldownSpell);

                if (!spellProto)
                {
                    sLog.outError("Weapon switch cooldown spell %u couldn't be found in Spell.dbc", cooldownSpell);
                }
                else
                {
                    Arms().ChangeTimer(spellProto->StartRecoveryTime);

                    WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + 4 + 4);
                    data << GetObjectGuid();
                    data << uint32(cooldownSpell);
                    data << uint32(0);
                    GetSession()->SendPacket(&data);
                }
            }
        }

        m_inventory.Arrived(pItem, update);

        ApplyEquipCooldown(pItem);
    }
    else
    {
        m_inventory.Merge(pItem2, pItem, pItem->GetCount(), update);
        pItem2->SetState(ITEM_CHANGED, this);

        ApplyEquipCooldown(pItem2);

        return pItem2;
    }

    return pItem;
}

void Player::RemoveItem(uint8 bag, uint8 slot, bool update)
{
    Item* pItem = m_inventory.At(bag, slot);
    if (!pItem)
    {
        return;
    }

    if (Inventory::IsHisOwn(bag) && slot < INVENTORY_SLOT_BAG_END)
    {
        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto && pProto->ItemSet)
        {
            RemoveItemsSetItem(this, pProto);
        }

        _ApplyItemMods(pItem, slot, false);

        if (slot < EQUIPMENT_SLOT_END)
        {
            RemoveItemDependentAurasAndCasts(pItem);

            if (slot == EQUIPMENT_SLOT_MAINHAND)
            {
                pItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_3);
            }
        }
    }

    m_inventory.Take(bag, slot, update);
}

void Player::MoveItemFromInventory(uint8 bag, uint8 slot, bool update)
{
    if (Item* it = GetItemByPos(bag, slot))
    {
        m_journal.ItemLost(it->GetEntry(), it->GetCount());
        RemoveItem(bag, slot, update);
        m_inventory.Saves().Forget(it);
        if (it->IsInWorld())
        {
            it->RemoveFromWorld();
            it->DestroyForPlayer(this);
        }
    }
}

void Player::MoveItemToInventory(ItemPosCountVec const& dest, Item* pItem, bool update, bool in_characterInventoryDB)
{

    m_journal.ItemGained(pItem->GetEntry(), pItem->GetCount());

    Item* pLastItem = StoreItem(dest, pItem, update);

    if (pLastItem == pItem)
    {

        if (pLastItem->GetOwnerGuid() != GetObjectGuid())
        {
            pLastItem->SetOwnerGuid(GetObjectGuid());
        }

        pLastItem->SetState(in_characterInventoryDB ? ITEM_CHANGED : ITEM_NEW, this);
    }
}

void Player::DestroyItem(uint8 bag, uint8 slot, bool update)
{
    Item* pItem = m_inventory.At(bag, slot);
    if (!pItem)
    {
        return;
    }

    if (pItem->IsBag() && pItem->IsEquipped())
    {
        for (uint8 inside = 0; inside < MAX_BAG_SIZE; ++inside)
        {
            DestroyItem(slot, inside, update);
        }
    }

    if (pItem->HasItemFlag(ITEM_DYNFLAG_WRAPPED))
    {
        static SqlStatementID delGifts ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delGifts, "DELETE FROM `character_gifts` WHERE `item_guid` = ?");
        stmt.PExecute(pItem->GetGUIDLow());
    }

    m_journal.ItemLost(pItem->GetEntry(), pItem->GetCount());

    if (Inventory::IsHisOwn(bag) && slot < INVENTORY_SLOT_BAG_END)
    {
        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto && pProto->ItemSet)
        {
            RemoveItemsSetItem(this, pProto);
        }

        _ApplyItemMods(pItem, slot, false);

        if (slot < EQUIPMENT_SLOT_END)
        {
            RemoveItemDependentAurasAndCasts(pItem);
        }
    }

    m_inventory.Destroy(bag, slot, update);
}

uint32 Player::DestroyItemCount(uint32 item, uint32 count, bool update, bool unequip_check, bool delete_from_bank,bool delete_from_buyback)
{
    DEBUG_LOG("STORAGE: DestroyItemCount item = %u, count = %u", item, count);
    uint32 remcount = 0;

    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetEntry() == item && !pItem->IsInTrade())
            {
                if (pItem->GetCount() + remcount <= count)
                {

                    remcount += pItem->GetCount();
                    DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                    if (remcount >= count)
                    {
                        return remcount;
                    }
                }
                else
                {
                    m_journal.ItemLost(pItem->GetEntry(), count - remcount);
                    pItem->SetCount(pItem->GetCount() - count + remcount);
                    m_inventory.Changed(pItem, update);
                    pItem->SetState(ITEM_CHANGED, this);
                    return remcount;
                }
            }
        }
    }

    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetEntry() == item && !pItem->IsInTrade())
            {
                if (pItem->GetCount() + remcount <= count)
                {

                    remcount += pItem->GetCount();
                    DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                    if (remcount >= count)
                    {
                        return remcount;
                    }
                }
                else
                {
                    m_journal.ItemLost(pItem->GetEntry(), count - remcount);
                    pItem->SetCount(pItem->GetCount() - count + remcount);
                    m_inventory.Changed(pItem, update);
                    pItem->SetState(ITEM_CHANGED, this);
                    return remcount;
                }
            }
        }
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                if (Item* pItem = pBag->GetItemByPos(j))
                {
                    if (pItem->GetEntry() == item && !pItem->IsInTrade())
                    {

                        if (pItem->GetCount() + remcount <= count)
                        {
                            remcount += pItem->GetCount();
                            DestroyItem(i, j, update);

                            if (remcount >= count)
                            {
                                return remcount;
                            }
                        }
                        else
                        {
                            m_journal.ItemLost(pItem->GetEntry(), count - remcount);
                            pItem->SetCount(pItem->GetCount() - count + remcount);
                            m_inventory.Changed(pItem, update);
                            pItem->SetState(ITEM_CHANGED, this);
                            return remcount;
                        }
                    }
                }
            }
        }
    }

    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem && pItem->GetEntry() == item && !pItem->IsInTrade())
            {
                if (pItem->GetCount() + remcount <= count)
                {
                    if (!unequip_check || CanUnequipItem(INVENTORY_SLOT_BAG_0 << 8 | i, false) == EQUIP_ERR_OK)
                    {
                        remcount += pItem->GetCount();
                        DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
                        if (remcount >= count)
                        {
                            return remcount;
                        }
                    }
                }
                else
                {
                    m_journal.ItemLost(pItem->GetEntry(), count - remcount);
                    pItem->SetCount(pItem->GetCount() - count + remcount);
                    m_inventory.Changed(pItem, update);
                    pItem->SetState(ITEM_CHANGED, this);
                    return remcount;
                }
            }
        }
    }

    if (delete_from_bank)
    {

        for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        {
            if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (pItem->GetEntry() == item && !pItem->IsInTrade())
                {
                    if (pItem->GetCount() + remcount <= count)
                    {

                        remcount += pItem->GetCount();
                        DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                        if (remcount >= count)
                        {
                            return remcount;
                        }
                    }
                    else
                    {
                        m_journal.ItemLost(pItem->GetEntry(), count - remcount);
                        pItem->SetCount(pItem->GetCount() - count + remcount);
                        m_inventory.Changed(pItem, update);
                        pItem->SetState(ITEM_CHANGED, this);
                        return remcount;
                    }
                }
            }
        }

        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    if (Item* pItem = pBag->GetItemByPos(j))
                    {
                        if (pItem->GetEntry() == item && !pItem->IsInTrade())
                        {

                            if (pItem->GetCount() + remcount <= count)
                            {
                                remcount += pItem->GetCount();
                                DestroyItem(i, j, update);

                                if (remcount >= count)
                                {
                                    return remcount;
                                }
                            }
                            else
                            {
                                m_journal.ItemLost(pItem->GetEntry(), count - remcount);
                                pItem->SetCount(pItem->GetCount() - count + remcount);
                                m_inventory.Changed(pItem, update);
                                pItem->SetState(ITEM_CHANGED, this);
                                return remcount;
                            }
                        }
                    }
                }
            }
        }
    }

    if (delete_from_buyback)
    {
        for (int i = BUYBACK_SLOT_START; i < BUYBACK_SLOT_END; ++i)
        {
            if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (pItem->GetEntry() == item && !pItem->IsInTrade())
                {
                    if (pItem->GetCount() + remcount <= count)
                    {

                        remcount += pItem->GetCount();
                        DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                        if (remcount >= count)
                        {
                            return remcount;
                        }
                    }
                    else
                    {
                        m_journal.ItemLost(pItem->GetEntry(), count - remcount);
                        pItem->SetCount(pItem->GetCount() - count + remcount);
                        m_inventory.Changed(pItem, update);
                        pItem->SetState(ITEM_CHANGED, this);
                        return remcount;
                    }
                }
            }
        }
    }

    return remcount;
}

void Player::DestroyZoneLimitedItem(bool update, uint32 new_zone)
{
    DEBUG_LOG("STORAGE: DestroyZoneLimitedItem in map %u and area %u", GetMapId(), new_zone);

    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
            {
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
            }
        }
    }

    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
            {
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
            }
        }
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                if (Item* pItem = pBag->GetItemByPos(j))
                {
                    if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
                    {
                        DestroyItem(i, j, update);
                    }
                }
            }
        }
    }

    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
            {
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
            }
        }
    }
}

void Player::DestroyConjuredItems(bool update)
{

    DEBUG_LOG("STORAGE: DestroyConjuredItems");

    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->IsConjuredConsumable())
            {
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
            }
        }
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                if (Item* pItem = pBag->GetItemByPos(j))
                {
                    if (pItem->IsConjuredConsumable())
                    {
                        DestroyItem(i, j, update);
                    }
                }
            }
        }
    }

    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->IsConjuredConsumable())
            {
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
            }
        }
    }
}

void Player::DestroyItemCount(Item* pItem, uint32& count, bool update)
{
    if (!pItem)
    {
        return;
    }

    DEBUG_LOG("STORAGE: DestroyItemCount item (GUID: %u, Entry: %u) count = %u", pItem->GetGUIDLow(), pItem->GetEntry(), count);

    if (pItem->GetCount() <= count)
    {
        count -= pItem->GetCount();

        DestroyItem(pItem->GetBagSlot(), pItem->GetSlot(), update);
    }
    else
    {
        m_journal.ItemLost(pItem->GetEntry(), count);
        pItem->SetCount(pItem->GetCount() - count);
        count = 0;
        m_inventory.Changed(pItem, update);
        pItem->SetState(ITEM_CHANGED, this);
    }
}

void Player::SplitItem(uint16 src, uint16 dst, uint32 count)
{
    uint8 srcbag = src >> 8;
    uint8 srcslot = src & 255;

    uint8 dstbag = dst >> 8;
    uint8 dstslot = dst & 255;

    Item* pSrcItem = GetItemByPos(srcbag, srcslot);
    if (!pSrcItem)
    {
        SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pSrcItem, nullptr);
        return;
    }

    if (pSrcItem->HasGeneratedLoot())
    {

        SendEquipError(EQUIP_ERR_COULDNT_SPLIT_ITEMS, pSrcItem, nullptr);
        return;
    }

    if (pSrcItem->GetCount() == count)
    {
        SendEquipError(EQUIP_ERR_COULDNT_SPLIT_ITEMS, pSrcItem, nullptr);
        return;
    }

    if (pSrcItem->GetCount() < count)
    {
        SendEquipError(EQUIP_ERR_TRIED_TO_SPLIT_MORE_THAN_COUNT, pSrcItem, nullptr);
        return;
    }

    DEBUG_LOG("STORAGE: SplitItem bag = %u, slot = %u, item = %u, count = %u", dstbag, dstslot, pSrcItem->GetEntry(), count);
    Item* pNewItem = pSrcItem->CloneItem(count, this);
    if (!pNewItem)
    {
        SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pSrcItem, nullptr);
        return;
    }

    if (Inventory::IsCarried(dst))
    {

        pSrcItem->SetCount(pSrcItem->GetCount() - count);

        ItemPosCountVec dest;
        InventoryResult msg = CanStoreItem(dstbag, dstslot, dest, pNewItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            delete pNewItem;
            pSrcItem->SetCount(pSrcItem->GetCount() + count);
            SendEquipError(msg, pSrcItem, nullptr);
            return;
        }

        m_inventory.Changed(pSrcItem, true);
        pSrcItem->SetState(ITEM_CHANGED, this);
        StoreItem(dest, pNewItem, true);
    }
    else if (Inventory::IsBanked(dst))
    {

        pSrcItem->SetCount(pSrcItem->GetCount() - count);

        ItemPosCountVec dest;
        InventoryResult msg = CanBankItem(dstbag, dstslot, dest, pNewItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            delete pNewItem;
            pSrcItem->SetCount(pSrcItem->GetCount() + count);
            SendEquipError(msg, pSrcItem, nullptr);
            return;
        }

        m_inventory.Changed(pSrcItem, true);
        pSrcItem->SetState(ITEM_CHANGED, this);
        BankItem(dest, pNewItem, true);
    }
    else if (Inventory::IsWorn(dst))
    {

        pSrcItem->SetCount(pSrcItem->GetCount() - count);

        uint16 dest;
        InventoryResult msg = CanEquipItem(dstslot, dest, pNewItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            delete pNewItem;
            pSrcItem->SetCount(pSrcItem->GetCount() + count);
            SendEquipError(msg, pSrcItem, nullptr);
            return;
        }

        m_inventory.Changed(pSrcItem, true);
        pSrcItem->SetState(ITEM_CHANGED, this);
        EquipItem(dest, pNewItem, true);
        AutoUnequipOffhandIfNeed();
    }
}

void Player::SwapItem(uint16 src, uint16 dst)
{
    uint8 const srcbag = Inventory::Container(src);
    uint8 const srcslot = Inventory::Slot(src);
    uint8 const dstbag = Inventory::Container(dst);
    uint8 const dstslot = Inventory::Slot(dst);

    Item* pSrcItem = m_inventory.At(srcbag, srcslot);
    Item* pDstItem = m_inventory.At(dstbag, dstslot);

    if (!pSrcItem)
    {
        return;
    }

    DEBUG_LOG("STORAGE: SwapItem bag = %u, slot = %u, item = %u", dstbag, dstslot, pSrcItem->GetEntry());

    if (!IsAlive())
    {
        SendEquipError(EQUIP_ERR_YOU_ARE_DEAD, pSrcItem, pDstItem);
        return;
    }

    if (Inventory::IsWorn(src) || Inventory::HoldsBag(src))
    {
        bool const asBag = Inventory::HoldsBag(src)
            && !Inventory::HoldsBag(dst)
            && !(pDstItem && pDstItem->IsBag() && static_cast<Bag*>(pDstItem)->IsEmpty());

        InventoryResult msg = CanUnequipItem(src, !asBag);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, pSrcItem, pDstItem);
            return;
        }
    }

    if (Inventory::HoldsBag(src) && srcslot == dstbag)
    {
        SendEquipError(EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, pSrcItem, pDstItem);
        return;
    }

    if (Inventory::HoldsBag(dst) && dstslot == srcbag)
    {
        SendEquipError(EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, pDstItem, pSrcItem);
        return;
    }

    if (pDstItem && (Inventory::IsWorn(dst) || Inventory::HoldsBag(dst)))
    {
        bool const asBag = Inventory::HoldsBag(dst)
            && !Inventory::HoldsBag(src)
            && !(pSrcItem->IsBag() && static_cast<Bag*>(pSrcItem)->IsEmpty());

        InventoryResult msg = CanUnequipItem(dst, !asBag);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, pSrcItem, pDstItem);
            return;
        }
    }

    ItemDestination to(*this, dst);

    if (!pDstItem)
    {
        if (!to.Reachable())
        {
            return;
        }

        InventoryResult msg = to.Weigh(pSrcItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, pSrcItem, nullptr);
            return;
        }

        RemoveItem(srcbag, srcslot, true);
        to.Carry(pSrcItem);

        if (Inventory::IsWorn(dst))
        {
            AutoUnequipOffhandIfNeed();
        }

        return;
    }

    if (!pSrcItem->IsBag() && !pDstItem->IsBag())
    {
        if (!to.Reachable())
        {
            return;
        }

        if (to.Weigh(pSrcItem, false) == EQUIP_ERR_OK)
        {
            ItemPrototype const* itemProto = pSrcItem->GetProto();
            if (pSrcItem->GetCount() + pDstItem->GetCount() <= itemProto->GetMaxStackSize())
            {
                RemoveItem(srcbag, srcslot, true);
                to.Carry(pSrcItem);

                if (Inventory::IsWorn(dst))
                {
                    AutoUnequipOffhandIfNeed();
                }
            }
            else
            {
                pSrcItem->SetCount(pSrcItem->GetCount() + pDstItem->GetCount() - itemProto->GetMaxStackSize());
                pDstItem->SetCount(itemProto->GetMaxStackSize());
                pSrcItem->SetState(ITEM_CHANGED, this);
                pDstItem->SetState(ITEM_CHANGED, this);
                m_inventory.Changed(pSrcItem, true);
                m_inventory.Changed(pDstItem, true);
            }

            return;
        }
    }

    ItemDestination back(*this, src);

    InventoryResult msg = to.Weigh(pSrcItem, true);
    if (msg != EQUIP_ERR_OK)
    {
        SendEquipError(msg, pSrcItem, pDstItem);
        return;
    }

    msg = back.Weigh(pDstItem, true);
    if (msg != EQUIP_ERR_OK)
    {
        SendEquipError(msg, pDstItem, pSrcItem);
        return;
    }

    if (pSrcItem->IsBag() && pDstItem->IsBag() && !PourBagInto(pSrcItem, src, pDstItem, dst))
    {
        return;
    }

    RemoveItem(dstbag, dstslot, false);
    RemoveItem(srcbag, srcslot, false);

    to.Carry(pSrcItem);
    back.Carry(pDstItem);

    AutoUnequipOffhandIfNeed();
}

bool Player::PourBagInto(Item* pSrcItem, uint16 src, Item* pDstItem, uint16 dst)
{
    Bag* emptyBag = nullptr;
    Bag* fullBag = nullptr;

    if (static_cast<Bag*>(pSrcItem)->IsEmpty() && !Inventory::HoldsBag(src))
    {
        emptyBag = static_cast<Bag*>(pSrcItem);
        fullBag = static_cast<Bag*>(pDstItem);
    }
    else if (static_cast<Bag*>(pDstItem)->IsEmpty() && !Inventory::HoldsBag(dst))
    {
        emptyBag = static_cast<Bag*>(pDstItem);
        fullBag = static_cast<Bag*>(pSrcItem);
    }

    if (!emptyBag || !fullBag)
    {
        return true;
    }

    ItemPrototype const* emptyProto = emptyBag->GetProto();
    uint32 count = 0;

    for (uint32 i = 0; i < fullBag->GetBagSize(); ++i)
    {
        Item* bagItem = fullBag->GetItemByPos(uint8(i));
        if (!bagItem)
        {
            continue;
        }

        ItemPrototype const* bagItemProto = bagItem->GetProto();
        if (!bagItemProto || !ItemCanGoIntoBag(bagItemProto, emptyProto))
        {
            SendEquipError(EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, pSrcItem, pDstItem);
            return false;
        }

        ++count;
    }

    if (count > emptyBag->GetBagSize())
    {
        SendEquipError(EQUIP_ERR_ITEMS_CANT_BE_SWAPPED, pSrcItem, pDstItem);
        return false;
    }

    count = 0;
    for (uint32 i = 0; i < fullBag->GetBagSize(); ++i)
    {
        Item* bagItem = fullBag->GetItemByPos(uint8(i));
        if (!bagItem)
        {
            continue;
        }

        fullBag->RemoveItem(uint8(i));
        emptyBag->StoreItem(uint8(count), bagItem);
        bagItem->SetState(ITEM_CHANGED, this);

        ++count;
    }

    return true;
}

void Player::SendEquipError(InventoryResult msg, Item* pItem, Item* pItem2, uint32 itemid ) const
{
    DEBUG_LOG("WORLD: Sent SMSG_INVENTORY_CHANGE_FAILURE (%u)", msg);
    WorldPacket data(SMSG_INVENTORY_CHANGE_FAILURE, (msg == EQUIP_ERR_CANT_EQUIP_LEVEL_I ? 22 : (msg == EQUIP_ERR_OK ? 1 : 18)));
    data << uint8(msg);

    if (msg != EQUIP_ERR_OK)
    {
        if (msg == EQUIP_ERR_CANT_EQUIP_LEVEL_I)
        {
            ItemPrototype const* proto = pItem ? pItem->GetProto() : ObjectMgr::GetItemPrototype(itemid);
            data << uint32(proto ? proto->RequiredLevel : 0);
        }
        data << (pItem ? pItem->GetObjectGuid() : 0);
        data << (pItem2 ? pItem2->GetObjectGuid() : 0);
        data << uint8(0);
    }
    GetSession()->SendPacket(&data);
}

void Player::SendBuyError(BuyResult msg, Creature* pCreature, uint32 item, uint32 )
{
    DEBUG_LOG("WORLD: Sent SMSG_BUY_FAILED");
    WorldPacket data(SMSG_BUY_FAILED, (8 + 4 + 1));
    data << (pCreature ? pCreature->GetObjectGuid() : 0);
    data << uint32(item);
    data << uint8(msg);
    GetSession()->SendPacket(&data);
}

void Player::SendSellError(SellResult msg, Creature* pCreature, ObjectGuid itemGuid, uint32 )
{
    DEBUG_LOG("WORLD: Sent SMSG_SELL_ITEM");
    WorldPacket data(SMSG_SELL_ITEM, (8 + 8 +  1));
    data << (pCreature ? pCreature->GetObjectGuid() : 0);
    data << static_cast<ObjectGuid>(itemGuid);
    data << uint8(msg);
    GetSession()->SendPacket(&data);
}
