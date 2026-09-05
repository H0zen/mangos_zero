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
#include "MapManager.h"
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

// Return stored item (if stored to stack, it can diff. from pItem). And pItem ca be deleted in this case.
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
        ItemAddedQuestCheck(item, count);
        pItem = StoreItem(dest, pItem, update);
    }
    return pItem;
}

Item* Player::EquipNewItem(uint16 pos, uint32 item, bool update)
{
    if (Item* pItem = Item::CreateItem(item, 1, this))
    {
        ItemAddedQuestCheck(item, 1);
        return EquipItem(pos, pItem, update);
    }

    return nullptr;
}

/**
 * @brief Equips an item into the specified destination position.
 *
 * @param pos The packed destination position.
 * @param pItem The item to equip.
 * @param update True to send world updates for the equip action.
 * @return The equipped or merged item instance.
 */
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

            // item set bonuses applied only at equip and removed at unequip, and still active for broken items
            if (pProto && pProto->ItemSet)
            {
                AddItemsSetItem(this, pItem);
            }

            _ApplyItemMods(pItem, slot, true);

            // Weapons and also Totem/Relic/Sigil/etc
            if (pProto && IsInCombat() && (pProto->Class == ITEM_CLASS_WEAPON || pProto->InventoryType == INVTYPE_RELIC) && m_weaponChangeTimer == 0)
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
                    m_weaponChangeTimer = spellProto->StartRecoveryTime;

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

    // A worn piece stops doing whatever it was doing for him before it leaves
    // its place. A bag slot counts as worn here, which is why the bound is the
    // end of the bag slots and not the end of the gear.
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

// Common operation need to remove item from inventory without delete in trade, auction, guild bank, mail....
void Player::MoveItemFromInventory(uint8 bag, uint8 slot, bool update)
{
    if (Item* it = GetItemByPos(bag, slot))
    {
        ItemRemovedQuestCheck(it->GetEntry(), it->GetCount());
        RemoveItem(bag, slot, update);
        m_inventory.Saves().Forget(it);
        if (it->IsInWorld())
        {
            it->RemoveFromWorld();
            it->DestroyForPlayer(this);
        }
    }
}

// Common operation need to add item from inventory without delete in trade, guild bank, mail....
void Player::MoveItemToInventory(ItemPosCountVec const& dest, Item* pItem, bool update, bool in_characterInventoryDB)
{
    // update quest counters
    ItemAddedQuestCheck(pItem->GetEntry(), pItem->GetCount());

    // store item
    Item* pLastItem = StoreItem(dest, pItem, update);

    // only set if not merged to existing stack (pItem can be deleted already but we can compare pointers any way)
    if (pLastItem == pItem)
    {
        // update owner for last item (this can be original item with wrong owner
        if (pLastItem->GetOwnerGuid() != GetObjectGuid())
        {
            pLastItem->SetOwnerGuid(GetObjectGuid());
        }

        // if this original item then it need create record in inventory
        // in case trade we already have item in other player inventory
        pLastItem->SetState(in_characterInventoryDB ? ITEM_CHANGED : ITEM_NEW, this);
    }
}

/**
 * @brief Permanently destroys an item from player storage.
 *
 * @param bag The bag containing the item.
 * @param slot The slot containing the item.
 * @param update True to send inventory updates to the client.
 */
void Player::DestroyItem(uint8 bag, uint8 slot, bool update)
{
    Item* pItem = m_inventory.At(bag, slot);
    if (!pItem)
    {
        return;
    }

    // A bag goes with what is in it. Only a worn bag can hold anything, which is
    // also what keeps this from turning on itself when the bag is its own slot.
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

    ItemRemovedQuestCheck(pItem->GetEntry(), pItem->GetCount());

    // A worn piece stops doing whatever it was doing for him before it goes.
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

/**
 * @brief Destroys up to a requested count of an item across player storage.
 *
 * @param item The item entry to destroy.
 * @param count The requested quantity to destroy.
 * @param update True to send inventory updates to the client.
 * @param unequip_check True to validate equipped items before destroying them.
 * @param delete_from_bank True to include bank storage in the search.
 * @param delete_from_buyback True to include vendor buyback slots in the search.
 * @return The number of items removed.
 */
uint32 Player::DestroyItemCount(uint32 item, uint32 count, bool update, bool unequip_check, bool delete_from_bank,bool delete_from_buyback)
{
    DEBUG_LOG("STORAGE: DestroyItemCount item = %u, count = %u", item, count);
    uint32 remcount = 0;

    // Search in default bagpack
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetEntry() == item && !pItem->IsInTrade())
            {
                if (pItem->GetCount() + remcount <= count)
                {
                    // all items in inventory can unequipped
                    remcount += pItem->GetCount();
                    DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                    if (remcount >= count)
                    {
                        return remcount;
                    }
                }
                else
                {
                    ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
                    pItem->SetCount(pItem->GetCount() - count + remcount);
                    m_inventory.Changed(pItem, update);
                    pItem->SetState(ITEM_CHANGED, this);
                    return remcount;
                }
            }
        }
    }

    // Search in keyring slots
    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetEntry() == item && !pItem->IsInTrade())
            {
                if (pItem->GetCount() + remcount <= count)
                {
                    // all keys can be unequipped
                    remcount += pItem->GetCount();
                    DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                    if (remcount >= count)
                    {
                        return remcount;
                    }
                }
                else
                {
                    ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
                    pItem->SetCount(pItem->GetCount() - count + remcount);
                    m_inventory.Changed(pItem, update);
                    pItem->SetState(ITEM_CHANGED, this);
                    return remcount;
                }
            }
        }
    }

    // Search in inventory bags
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
                        // all items in bags can be unequipped
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
                            ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
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

    // Search in Equiped items
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
                    ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
                    pItem->SetCount(pItem->GetCount() - count + remcount);
                    m_inventory.Changed(pItem, update);
                    pItem->SetState(ITEM_CHANGED, this);
                    return remcount;
                }
            }
        }
    }

    // Search in bank items
    if (delete_from_bank)
    {
        // Normal bank slots
        for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        {
            if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (pItem->GetEntry() == item && !pItem->IsInTrade())
                {
                    if (pItem->GetCount() + remcount <= count)
                    {
                        // all items in inventory can unequipped
                        remcount += pItem->GetCount();
                        DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                        if (remcount >= count)
                        {
                            return remcount;
                        }
                    }
                    else
                    {
                        ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
                        pItem->SetCount(pItem->GetCount() - count + remcount);
                        m_inventory.Changed(pItem, update);
                        pItem->SetState(ITEM_CHANGED, this);
                        return remcount;
                    }
                }
            }
        }

        // Bank bagslots
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
                            // all items in bags can be unequipped
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
                                ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
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

    // Search in buyback npcs vendor tab
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
                        // all keys can be unequipped
                        remcount += pItem->GetCount();
                        DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                        if (remcount >= count)
                        {
                            return remcount;
                        }
                    }
                    else
                    {
                        ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
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

/**
 * @brief Destroys items that are no longer valid in the player's current zone or map.
 *
 * @param update True to send inventory updates to the client.
 * @param new_zone The zone identifier to validate against.
 */
void Player::DestroyZoneLimitedItem(bool update, uint32 new_zone)
{
    DEBUG_LOG("STORAGE: DestroyZoneLimitedItem in map %u and area %u", GetMapId(), new_zone);

    // in inventory
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

    // in inventory bags
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

    // in equipment and bag list
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

/**
 * @brief Destroys conjured consumables from player storage.
 *
 * @param update True to send inventory updates to the client.
 */
void Player::DestroyConjuredItems(bool update)
{
    // destroys all conjured items
    DEBUG_LOG("STORAGE: DestroyConjuredItems");

    // in inventory
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

    // in inventory bags
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

    // in equipment and bag list
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

/**
 * @brief Destroys or decrements a specific item instance by a requested count.
 *
 * @param pItem The item instance to modify.
 * @param count The remaining quantity to destroy; updated by the call.
 * @param update True to send inventory updates to the client.
 */
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
        ItemRemovedQuestCheck(pItem->GetEntry(), count);
        pItem->SetCount(pItem->GetCount() - count);
        count = 0;
        m_inventory.Changed(pItem, update);
        pItem->SetState(ITEM_CHANGED, this);
    }
}

/**
 * @brief Splits a stack into a new destination position.
 *
 * @param src The packed source position.
 * @param dst The packed destination position.
 * @param count The quantity to split from the source stack.
 */
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

    if (pSrcItem->HasGeneratedLoot())                       // prevent split looting item (stackable items can has only temporary loot and this meaning that loot window open)
    {
        // best error message found for attempting to split while looting
        SendEquipError(EQUIP_ERR_COULDNT_SPLIT_ITEMS, pSrcItem, nullptr);
        return;
    }

    // not let split all items (can be only at cheating)
    if (pSrcItem->GetCount() == count)
    {
        SendEquipError(EQUIP_ERR_COULDNT_SPLIT_ITEMS, pSrcItem, nullptr);
        return;
    }

    // not let split more existing items (can be only at cheating)
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
        // change item amount before check (for unique max count check)
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
        // change item amount before check (for unique max count check)
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
        // change item amount before check (for unique max count check), provide space for splitted items
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

/**
 * @brief Moves, merges, or swaps items between two storage positions.
 *
 * @param src The packed source position.
 * @param dst The packed destination position.
 */
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

    // A bag may be swapped into an empty bag slot, or against another bag that
    // is empty; the contents are weighed further down. Anything else has to be
    // able to come off where it stands.
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

    // A bag cannot be put inside itself.
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

    // Nothing is there: the item simply goes across.
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

    // The same thing is there: the two stacks join, or the one that is there is
    // topped up to full and the rest stays behind.
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

    // Neither joining nor topping up will do, so the two change places. Both
    // ways are weighed before either is carried out.
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

    // Two bags change places by pouring one into the other, which is only
    // possible when the one being poured into is both empty and not hanging in a
    // bag slot of its own.
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

/**
 * @brief Empties one bag into the other so that two bags can change places.
 *
 * @param pSrcItem The bag being moved from src.
 * @param src The packed position it is coming from.
 * @param pDstItem The bag being moved from dst.
 * @param dst The packed position it is coming from.
 * @return False when the contents cannot be moved, having told him why.
 */
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

    // Two bags that both hold something, or that both hang in bag slots, simply
    // change places with what is in them.
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

void Player::SendEquipError(InventoryResult msg, Item* pItem, Item* pItem2, uint32 itemid /*= 0*/) const
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
        data << (pItem ? pItem->GetObjectGuid() : ObjectGuid());
        data << (pItem2 ? pItem2->GetObjectGuid() : ObjectGuid());
        data << uint8(0);                                   // bag type subclass, used with EQUIP_ERR_EVENT_AUTOEQUIP_BIND_CONFIRM and EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2
    }
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends a buy failure result to the client.
 *
 * @param msg The buy failure code.
 * @param pCreature The vendor involved in the transaction.
 * @param item The item entry that failed to purchase.
 * @param param Unused extra parameter.
 */
void Player::SendBuyError(BuyResult msg, Creature* pCreature, uint32 item, uint32 /*param*/)
{
    DEBUG_LOG("WORLD: Sent SMSG_BUY_FAILED");
    WorldPacket data(SMSG_BUY_FAILED, (8 + 4 + 1));
    data << (pCreature ? pCreature->GetObjectGuid() : ObjectGuid());
    data << uint32(item);
    data << uint8(msg);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends a sell failure result to the client.
 *
 * @param msg The sell failure code.
 * @param pCreature The vendor involved in the transaction.
 * @param itemGuid The item GUID that failed to sell.
 * @param param Unused extra parameter.
 */
void Player::SendSellError(SellResult msg, Creature* pCreature, ObjectGuid itemGuid, uint32 /*param*/)
{
    DEBUG_LOG("WORLD: Sent SMSG_SELL_ITEM");
    WorldPacket data(SMSG_SELL_ITEM, (8 + 8 + /*(param ? 4 : 0) +*/ 1)); // last check [ZERO]
    data << (pCreature ? pCreature->GetObjectGuid() : ObjectGuid());
    data << ObjectGuid(itemGuid);
    data << uint8(msg);
    GetSession()->SendPacket(&data);
}
