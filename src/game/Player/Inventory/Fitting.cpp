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



#include "Inventory.h"

#include "Bag.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Unit.h"

/**
 * @brief Checks whether the player can carry more copies of a limited item.
 *
 * @param entry The item entry to evaluate.
 * @param count The additional quantity to add.
 * @param pItem An item instance to exclude from current ownership checks.
 * @param no_space_count Optional output for the quantity that exceeds the limit.
 * @return The inventory result describing the carry-limit check.
 */
InventoryResult Inventory::RoomForMore(uint32 entry, uint32 count, Item* pItem, uint32* no_space_count) const
{
    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(entry);
    if (!pProto)
    {
        if (no_space_count)
        {
            *no_space_count = count;
        }
        return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
    }

    // no maximum
    if (pProto->MaxCount > 0)
    {
        uint32 curcount = Count(pProto->ItemId, SCOPE_EVERYWHERE, pItem);

        if (curcount + count > uint32(pProto->MaxCount))
        {
            if (no_space_count)
            {
                *no_space_count = count + curcount - pProto->MaxCount;
            }
            return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
        }
    }

    return EQUIP_ERR_OK;
}

/**
 * @brief Checks whether the player has an item from a required totem category.
 *
 * @param TotemCategory The required totem category identifier.
 * @return True if a matching item is present; otherwise, false.
 */
/**
 * @brief Checks whether the player has an item from a required totem category.
 *
 * @param TotemCategory The required totem category identifier.
 * @return True if a matching item is present; otherwise, false.
 */
bool Inventory::HasTotem(uint32 /*TotemCategory*/) const
{

    return false;
}

/**
 * @brief Checks whether item count can be stored in a specific slot.
 *
 * @param bag The destination bag identifier.
 * @param slot The destination slot identifier.
 * @param dest The accumulated destination positions.
 * @param pProto The item prototype being stored.
 * @param count The remaining quantity to place.
 * @param swap True to allow occupying an already used slot.
 * @param pSrcItem The source item being moved.
 * @return The inventory result for the slot check.
 */
/**
 * @brief Checks whether item count can be stored in a specific slot.
 *
 * @param bag The destination bag identifier.
 * @param slot The destination slot identifier.
 * @param dest The accumulated destination positions.
 * @param pProto The item prototype being stored.
 * @param count The remaining quantity to place.
 * @param swap True to allow occupying an already used slot.
 * @param pSrcItem The source item being moved.
 * @return The inventory result for the slot check.
 */
InventoryResult Inventory::FitsHere(uint8 bag, uint8 slot, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool swap, Item* pSrcItem) const
{
    Item* pItem2 = At(bag, slot);

    // ignore move item (this slot will be empty at move)
    if (pItem2 == pSrcItem)
    {
        pItem2 = nullptr;
    }

    uint32 need_space;

    // empty specific slot - check item fit to slot
    if (!pItem2 || swap)
    {
        if (bag == INVENTORY_SLOT_BAG_0)
        {
            // keyring case
            if (slot >= KEYRING_SLOT_START && slot < KEYRING_SLOT_START + MaxKeyring() && !(pProto->BagFamily == BAG_FAMILY_KEYS))
            {
                return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
            }

            // prevent cheating
            if ((slot >= BUYBACK_SLOT_START && slot < BUYBACK_SLOT_END) || slot >= PLAYER_SLOT_END)
            {
                return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
            }
        }
        else
        {
            Bag* pBag = (Bag*)At(INVENTORY_SLOT_BAG_0, bag);
            if (!pBag)
            {
                return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
            }

            ItemPrototype const* pBagProto = pBag->GetProto();
            if (!pBagProto)
            {
                return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
            }

            if (slot >= pBagProto->ContainerSlots)
            {
                return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
            }

            if (!ItemCanGoIntoBag(pProto, pBagProto))
            {
                return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
            }
        }

        // non empty stack with space
        need_space = pProto->Stackable;
    }
    // non empty slot, check item type
    else
    {
        // can be merged at least partly
        InventoryResult res  = pItem2->CanBeMergedPartlyWith(pProto);
        if (res != EQUIP_ERR_OK)
        {
            return res;
        }

        need_space = pProto->Stackable - pItem2->GetCount();
    }

    if (need_space > count)
    {
        need_space = count;
    }

    ItemPosCount newPosition = ItemPosCount((bag << 8) | slot, need_space);
    if (!newPosition.isContainedIn(dest))
    {
        dest.push_back(newPosition);
        count -= need_space;
    }
    return EQUIP_ERR_OK;
}

/**
 * @brief Searches a bag for valid storage positions for an item.
 *
 * @param bag The bag identifier to search.
 * @param dest The accumulated destination positions.
 * @param pProto The item prototype being stored.
 * @param count The remaining quantity to place.
 * @param merge True to search existing stacks; false to search empty slots.
 * @param non_specialized True to restrict search to plain containers.
 * @param pSrcItem The source item being moved.
 * @param skip_bag A bag identifier to skip.
 * @param skip_slot A slot identifier to skip.
 * @return The inventory result for the bag search.
 */
/**
 * @brief Searches a bag for valid storage positions for an item.
 *
 * @param bag The bag identifier to search.
 * @param dest The accumulated destination positions.
 * @param pProto The item prototype being stored.
 * @param count The remaining quantity to place.
 * @param merge True to search existing stacks; false to search empty slots.
 * @param non_specialized True to restrict search to plain containers.
 * @param pSrcItem The source item being moved.
 * @param skip_bag A bag identifier to skip.
 * @param skip_slot A slot identifier to skip.
 * @return The inventory result for the bag search.
 */
InventoryResult Inventory::FitsInBag(uint8 bag, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool merge, bool non_specialized, Item* pSrcItem, uint8 skip_bag, uint8 skip_slot) const
{
    // skip specific bag already processed in first called _CanStoreItem_InBag
    if (bag == skip_bag)
    {
        return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
    }

    // skip nonexistent bag or self targeted bag
    Bag* pBag = (Bag*)At(INVENTORY_SLOT_BAG_0, bag);
    if (!pBag || pBag == pSrcItem)
    {
        return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
    }

    ItemPrototype const* pBagProto = pBag->GetProto();
    if (!pBagProto)
    {
        return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
    }

    // specialized bag mode or non-specilized
    if (non_specialized != (pBagProto->Class == ITEM_CLASS_CONTAINER && pBagProto->SubClass == ITEM_SUBCLASS_CONTAINER))
    {
        return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
    }

    if (!ItemCanGoIntoBag(pProto, pBagProto))
    {
        return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
    }

    for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
    {
        // skip specific slot already processed in first called _CanStoreItem_InSpecificSlot
        if (j == skip_slot)
        {
            continue;
        }

        Item* pItem2 = At(bag, j);

        // ignore move item (this slot will be empty at move)
        if (pItem2 == pSrcItem)
        {
            pItem2 = nullptr;
        }

        // if merge skip empty, if !merge skip non-empty
        if ((pItem2 != nullptr) != merge)
        {
            continue;
        }

        uint32 need_space = pProto->GetMaxStackSize();

        if (pItem2)
        {
            // can be merged at least partly
            uint8 res  = pItem2->CanBeMergedPartlyWith(pProto);
            if (res != EQUIP_ERR_OK)
            {
                continue;
            }

            // decrease at current stacksize
            need_space -= pItem2->GetCount();
        }

        if (need_space > count)
        {
            need_space = count;
        }

        ItemPosCount newPosition = ItemPosCount((bag << 8) | j, need_space);
        if (!newPosition.isContainedIn(dest))
        {
            dest.push_back(newPosition);
            count -= need_space;

            if (count == 0)
            {
                return EQUIP_ERR_OK;
            }
        }
    }
    return EQUIP_ERR_OK;
}

/**
 * @brief Searches a range of inventory slots for valid storage positions.
 *
 * @param slot_begin The first slot in the search range.
 * @param slot_end One past the last slot in the search range.
 * @param dest The accumulated destination positions.
 * @param pProto The item prototype being stored.
 * @param count The remaining quantity to place.
 * @param merge True to search existing stacks; false to search empty slots.
 * @param pSrcItem The source item being moved.
 * @param skip_bag A bag identifier to skip.
 * @param skip_slot A slot identifier to skip.
 * @return The inventory result for the slot-range search.
 */
/**
 * @brief Searches a range of inventory slots for valid storage positions.
 *
 * @param slot_begin The first slot in the search range.
 * @param slot_end One past the last slot in the search range.
 * @param dest The accumulated destination positions.
 * @param pProto The item prototype being stored.
 * @param count The remaining quantity to place.
 * @param merge True to search existing stacks; false to search empty slots.
 * @param pSrcItem The source item being moved.
 * @param skip_bag A bag identifier to skip.
 * @param skip_slot A slot identifier to skip.
 * @return The inventory result for the slot-range search.
 */
InventoryResult Inventory::FitsInRun(uint8 slot_begin, uint8 slot_end, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool merge, Item* pSrcItem, uint8 skip_bag, uint8 skip_slot) const
{
    for (uint32 j = slot_begin; j < slot_end; ++j)
    {
        // skip specific slot already processed in first called _CanStoreItem_InSpecificSlot
        if (INVENTORY_SLOT_BAG_0 == skip_bag && j == skip_slot)
        {
            continue;
        }

        Item* pItem2 = At(INVENTORY_SLOT_BAG_0, j);

        // ignore move item (this slot will be empty at move)
        if (pItem2 == pSrcItem)
        {
            pItem2 = nullptr;
        }

        // if merge skip empty, if !merge skip non-empty
        if ((pItem2 != nullptr) != merge)
        {
            continue;
        }

        uint32 need_space = pProto->GetMaxStackSize();

        if (pItem2)
        {
            // can be merged at least partly
            uint8 res  = pItem2->CanBeMergedPartlyWith(pProto);
            if (res != EQUIP_ERR_OK)
            {
                continue;
            }

            // descrease at current stacksize
            need_space -= pItem2->GetCount();
        }

        if (need_space > count)
        {
            need_space = count;
        }

        ItemPosCount newPosition = ItemPosCount((INVENTORY_SLOT_BAG_0 << 8) | j, need_space);
        if (!newPosition.isContainedIn(dest))
        {
            dest.push_back(newPosition);
            count -= need_space;

            if (count == 0)
            {
                return EQUIP_ERR_OK;
            }
        }
    }
    return EQUIP_ERR_OK;
}

/**
 * @brief Computes valid destinations for storing an item stack in inventory.
 *
 * @param bag The preferred destination bag, or NULL_BAG for auto-placement.
 * @param slot The preferred destination slot, or NULL_SLOT for auto-placement.
 * @param dest The accumulated destination positions.
 * @param entry The item entry being stored.
 * @param count The quantity to store.
 * @param pItem The source item being moved.
 * @param swap True to allow swapping with occupied slots.
 * @param no_space_count Optional output for the quantity that could not be placed.
 * @return The inventory result for the storage search.
 */
/**
 * @brief Computes valid destinations for storing an item stack in inventory.
 *
 * @param bag The preferred destination bag, or NULL_BAG for auto-placement.
 * @param slot The preferred destination slot, or NULL_SLOT for auto-placement.
 * @param dest The accumulated destination positions.
 * @param entry The item entry being stored.
 * @param count The quantity to store.
 * @param pItem The source item being moved.
 * @param swap True to allow swapping with occupied slots.
 * @param no_space_count Optional output for the quantity that could not be placed.
 * @return The inventory result for the storage search.
 */
InventoryResult Inventory::PlanToStore(uint8 bag, uint8 slot, ItemPosCountVec& dest, uint32 entry, uint32 count, Item* pItem, bool swap, uint32* no_space_count) const
{
    DEBUG_LOG("STORAGE: CanStoreItem bag = %u, slot = %u, item = %u, count = %u", bag, slot, entry, count);

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(entry);
    if (!pProto)
    {
        if (no_space_count)
        {
            *no_space_count = count;
        }
        return swap ? EQUIP_ERR_ITEMS_CANT_BE_SWAPPED : EQUIP_ERR_ITEM_NOT_FOUND;
    }

    if (pItem)
    {
        // item used
        if (pItem->HasTemporaryLoot())
        {
            if (no_space_count)
            {
                *no_space_count = count;
            }
            return EQUIP_ERR_ALREADY_LOOTED;
        }

        if (pItem->IsBindedNotWith(&m_owner))
        {
            if (no_space_count)
            {
                *no_space_count = count;
            }
            return EQUIP_ERR_DONT_OWN_THAT_ITEM;
        }
    }

    // check count of items (skip for auto move for same player from bank)
    uint32 no_similar_count = 0;                            // can't store this amount similar items
    InventoryResult res = RoomForMore(entry, count, pItem, &no_similar_count);
    if (res != EQUIP_ERR_OK)
    {
        if (count == no_similar_count)
        {
            if (no_space_count)
            {
                *no_space_count = no_similar_count;
            }
            return res;
        }
        count -= no_similar_count;
    }

    // in specific slot
    if (bag != NULL_BAG && slot != NULL_SLOT)
    {
        res = FitsHere(bag, slot, dest, pProto, count, swap, pItem);
        if (res != EQUIP_ERR_OK)
        {
            if (no_space_count)
            {
                *no_space_count = count + no_similar_count;
            }
            return res;
        }

        if (count == 0)
        {
            if (no_similar_count == 0)
            {
                return EQUIP_ERR_OK;
            }

            if (no_space_count)
            {
                *no_space_count = count + no_similar_count;
            }
            return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
        }
    }

    // not specific slot or have space for partly store only in specific slot

    // in specific bag
    if (bag != NULL_BAG)
    {
        // search stack in bag for merge to
        if (pProto->Stackable > 1)
        {
            if (bag == INVENTORY_SLOT_BAG_0)               // inventory
            {
                res = FitsInRun(KEYRING_SLOT_START, KEYRING_SLOT_END, dest, pProto, count, true, pItem, bag, slot);
                if (res != EQUIP_ERR_OK)
                {
                    if (no_space_count)
                    {
                        *no_space_count = count + no_similar_count;
                    }
                    return res;
                }

                if (count == 0)
                {
                    if (no_similar_count == 0)
                    {
                        return EQUIP_ERR_OK;
                    }

                    if (no_space_count)
                    {
                        *no_space_count = count + no_similar_count;
                    }
                    return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
                }

                res = FitsInRun(INVENTORY_SLOT_ITEM_START, INVENTORY_SLOT_ITEM_END, dest, pProto, count, true, pItem, bag, slot);
                if (res != EQUIP_ERR_OK)
                {
                    if (no_space_count)
                    {
                        *no_space_count = count + no_similar_count;
                    }
                    return res;
                }

                if (count == 0)
                {
                    if (no_similar_count == 0)
                    {
                        return EQUIP_ERR_OK;
                    }

                    if (no_space_count)
                    {
                        *no_space_count = count + no_similar_count;
                    }
                    return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
                }
            }
            else                                            // equipped bag
            {
                // we need check 2 time (specialized/non_specialized), use NULL_BAG to prevent skipping bag
                res = FitsInBag(bag, dest, pProto, count, true, false, pItem, NULL_BAG, slot);
                if (res != EQUIP_ERR_OK)
                {
                    res = FitsInBag(bag, dest, pProto, count, true, true, pItem, NULL_BAG, slot);
                }

                if (res != EQUIP_ERR_OK)
                {
                    if (no_space_count)
                    {
                        *no_space_count = count + no_similar_count;
                    }
                    return res;
                }

                if (count == 0)
                {
                    if (no_similar_count == 0)
                    {
                        return EQUIP_ERR_OK;
                    }

                    if (no_space_count)
                    {
                        *no_space_count = count + no_similar_count;
                    }
                    return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
                }
            }
        }

        // search free slot in bag for place to
        if (bag == INVENTORY_SLOT_BAG_0)                    // inventory
        {
            // search free slot - keyring case
            if (pProto->BagFamily == BAG_FAMILY_KEYS)
            {
                uint32 keyringSize = MaxKeyring();
                res = FitsInRun(KEYRING_SLOT_START, KEYRING_SLOT_START + keyringSize, dest, pProto, count, false, pItem, bag, slot);
                if (res != EQUIP_ERR_OK)
                {
                    if (no_space_count)
                    {
                        *no_space_count = count + no_similar_count;
                    }
                    return res;
                }

                if (count == 0)
                {
                    if (no_similar_count == 0)
                    {
                        return EQUIP_ERR_OK;
                    }

                    if (no_space_count)
                    {
                        *no_space_count = count + no_similar_count;
                    }
                    return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
                }
            }

            res = FitsInRun(INVENTORY_SLOT_ITEM_START, INVENTORY_SLOT_ITEM_END, dest, pProto, count, false, pItem, bag, slot);
            if (res != EQUIP_ERR_OK)
            {
                if (no_space_count)
                {
                    *no_space_count = count + no_similar_count;
                }
                return res;
            }

            if (count == 0)
            {
                if (no_similar_count == 0)
                {
                    return EQUIP_ERR_OK;
                }

                if (no_space_count)
                {
                    *no_space_count = count + no_similar_count;
                }
                return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
            }
        }
        else                                                // equipped bag
        {
            res = FitsInBag(bag, dest, pProto, count, false, false, pItem, NULL_BAG, slot);
            if (res != EQUIP_ERR_OK)
            {
                res = FitsInBag(bag, dest, pProto, count, false, true, pItem, NULL_BAG, slot);
            }

            if (res != EQUIP_ERR_OK)
            {
                if (no_space_count)
                {
                    *no_space_count = count + no_similar_count;
                }
                return res;
            }

            if (count == 0)
            {
                if (no_similar_count == 0)
                {
                    return EQUIP_ERR_OK;
                }

                if (no_space_count)
                {
                    *no_space_count = count + no_similar_count;
                }
                return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
            }
        }
    }

    // not specific bag or have space for partly store only in specific bag

    // search stack for merge to
    if (pProto->Stackable > 1)
    {
        res = FitsInRun(KEYRING_SLOT_START, KEYRING_SLOT_END, dest, pProto, count, true, pItem, bag, slot);
        if (res != EQUIP_ERR_OK)
        {
            if (no_space_count)
            {
                *no_space_count = count + no_similar_count;
            }
            return res;
        }

        if (count == 0)
        {
            if (no_similar_count == 0)
            {
                return EQUIP_ERR_OK;
            }

            if (no_space_count)
            {
                *no_space_count = count + no_similar_count;
            }
            return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
        }

        res = FitsInRun(INVENTORY_SLOT_ITEM_START, INVENTORY_SLOT_ITEM_END, dest, pProto, count, true, pItem, bag, slot);
        if (res != EQUIP_ERR_OK)
        {
            if (no_space_count)
            {
                *no_space_count = count + no_similar_count;
            }
            return res;
        }

        if (count == 0)
        {
            if (no_similar_count == 0)
            {
                return EQUIP_ERR_OK;
            }

            if (no_space_count)
            {
                *no_space_count = count + no_similar_count;
            }
            return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
        }

        if (pProto->BagFamily)
        {
            for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                res = FitsInBag(i, dest, pProto, count, true, false, pItem, bag, slot);
                if (res != EQUIP_ERR_OK)
                {
                    continue;
                }

                if (count == 0)
                {
                    if (no_similar_count == 0)
                    {
                        return EQUIP_ERR_OK;
                    }

                    if (no_space_count)
                    {
                        *no_space_count = count + no_similar_count;
                    }
                    return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
                }
            }
        }

        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            res = FitsInBag(i, dest, pProto, count, true, true, pItem, bag, slot);
            if (res != EQUIP_ERR_OK)
            {
                continue;
            }

            if (count == 0)
            {
                if (no_similar_count == 0)
                {
                    return EQUIP_ERR_OK;
                }

                if (no_space_count)
                {
                    *no_space_count = count + no_similar_count;
                }
                return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
            }
        }
    }

    // search free slot - special bag case
    if (pProto->BagFamily)
    {
        if (pProto->BagFamily == BAG_FAMILY_KEYS)
        {
            uint32 keyringSize = MaxKeyring();
            res = FitsInRun(KEYRING_SLOT_START, KEYRING_SLOT_START + keyringSize, dest, pProto, count, false, pItem, bag, slot);
            if (res != EQUIP_ERR_OK)
            {
                if (no_space_count)
                {
                    *no_space_count = count + no_similar_count;
                }
                return res;
            }

            if (count == 0)
            {
                if (no_similar_count == 0)
                {
                    return EQUIP_ERR_OK;
                }

                if (no_space_count)
                {
                    *no_space_count = count + no_similar_count;
                }
                return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
            }
        }

        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            res = FitsInBag(i, dest, pProto, count, false, false, pItem, bag, slot);
            if (res != EQUIP_ERR_OK)
            {
                continue;
            }

            if (count == 0)
            {
                if (no_similar_count == 0)
                {
                    return EQUIP_ERR_OK;
                }

                if (no_space_count)
                {
                    *no_space_count = count + no_similar_count;
                }
                return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
            }
        }
    }

    // Normally it would be impossible to autostore not empty bags
    if (pItem && pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
    {
        return EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG;
    }

    // search free slot
    res = FitsInRun(INVENTORY_SLOT_ITEM_START, INVENTORY_SLOT_ITEM_END, dest, pProto, count, false, pItem, bag, slot);
    if (res != EQUIP_ERR_OK)
    {
        if (no_space_count)
        {
            *no_space_count = count + no_similar_count;
        }
        return res;
    }

    if (count == 0)
    {
        if (no_similar_count == 0)
        {
            return EQUIP_ERR_OK;
        }

        if (no_space_count)
        {
            *no_space_count = count + no_similar_count;
        }
        return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        res = FitsInBag(i, dest, pProto, count, false, true, pItem, bag, slot);
        if (res != EQUIP_ERR_OK)
        {
            continue;
        }

        if (count == 0)
        {
            if (no_similar_count == 0)
            {
                return EQUIP_ERR_OK;
            }

            if (no_space_count)
            {
                *no_space_count = count + no_similar_count;
            }
            return EQUIP_ERR_CANT_CARRY_MORE_OF_THIS;
        }
    }

    if (no_space_count)
    {
        *no_space_count = count + no_similar_count;
    }

    return EQUIP_ERR_INVENTORY_FULL;
}

//////////////////////////////////////////////////////////////////////////
InventoryResult Inventory::PlanForAll(Item** pItems, int count) const
{
    Item*    pItem2;

    // fill space table
    int inv_slot_items[INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START];
    int inv_bags[INVENTORY_SLOT_BAG_END - INVENTORY_SLOT_BAG_START][MAX_BAG_SIZE];
    int inv_keys[KEYRING_SLOT_END - KEYRING_SLOT_START];

    memset(inv_slot_items, 0, sizeof(int) * (INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START));
    memset(inv_bags, 0, sizeof(int) * (INVENTORY_SLOT_BAG_END - INVENTORY_SLOT_BAG_START)*MAX_BAG_SIZE);
    memset(inv_keys, 0, sizeof(int) * (KEYRING_SLOT_END - KEYRING_SLOT_START));

    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        pItem2 = At(INVENTORY_SLOT_BAG_0, i);

        if (pItem2 && !pItem2->IsInTrade())
        {
            inv_slot_items[i - INVENTORY_SLOT_ITEM_START] = pItem2->GetCount();
        }
    }

    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        pItem2 = At(INVENTORY_SLOT_BAG_0, i);

        if (pItem2 && !pItem2->IsInTrade())
        {
            inv_keys[i - KEYRING_SLOT_START] = pItem2->GetCount();
        }
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)At(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                pItem2 = At(i, j);
                if (pItem2 && !pItem2->IsInTrade())
                {
                    inv_bags[i - INVENTORY_SLOT_BAG_START][j] = pItem2->GetCount();
                }
            }
        }
    }

    // check free space for all items
    for (int k = 0; k < count; ++k)
    {
        Item*  pItem = pItems[k];

        // no item
        if (!pItem)
        {
            continue;
        }

        DEBUG_LOG("STORAGE: CanStoreItems %i. item = %u, count = %u", k + 1, pItem->GetEntry(), pItem->GetCount());
        ItemPrototype const* pProto = pItem->GetProto();

        // strange item
        if (!pProto)
        {
            return EQUIP_ERR_ITEM_NOT_FOUND;
        }

        // item used
        if (pItem->HasTemporaryLoot())
        {
            return EQUIP_ERR_ALREADY_LOOTED;
        }

        // item it 'bind'
        if (pItem->IsBindedNotWith(&m_owner))
        {
            return EQUIP_ERR_DONT_OWN_THAT_ITEM;
        }

        Bag* pBag;
        ItemPrototype const* pBagProto;

        // item is 'one item only'
        InventoryResult res = RoomForMore(pItem->GetEntry(), pItem->GetCount(), pItem);
        if (res != EQUIP_ERR_OK)
        {
            return res;
        }

        // search stack for merge to
        if (pProto->Stackable > 1)
        {
            bool b_found = false;

            for (int t = KEYRING_SLOT_START; t < KEYRING_SLOT_END; ++t)
            {
                pItem2 = At(INVENTORY_SLOT_BAG_0, t);
                if (pItem2 && pItem2->CanBeMergedPartlyWith(pProto) == EQUIP_ERR_OK && inv_keys[t - KEYRING_SLOT_START] + pItem->GetCount() <= pProto->GetMaxStackSize())
                {
                    inv_keys[t - KEYRING_SLOT_START] += pItem->GetCount();
                    b_found = true;
                    break;
                }
            }
            if (b_found)
            {
                continue;
            }

            for (int t = INVENTORY_SLOT_ITEM_START; t < INVENTORY_SLOT_ITEM_END; ++t)
            {
                pItem2 = At(INVENTORY_SLOT_BAG_0, t);
                if (pItem2 && pItem2->CanBeMergedPartlyWith(pProto) == EQUIP_ERR_OK && inv_slot_items[t - INVENTORY_SLOT_ITEM_START] + pItem->GetCount() <= pProto->GetMaxStackSize())
                {
                    inv_slot_items[t - INVENTORY_SLOT_ITEM_START] += pItem->GetCount();
                    b_found = true;
                    break;
                }
            }
            if (b_found)
            {
                continue;
            }

            for (int t = INVENTORY_SLOT_BAG_START; !b_found && t < INVENTORY_SLOT_BAG_END; ++t)
            {
                pBag = (Bag*)At(INVENTORY_SLOT_BAG_0, t);
                if (pBag)
                {
                    for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    {
                        pItem2 = At(t, j);
                        if (pItem2 && pItem2->CanBeMergedPartlyWith(pProto) == EQUIP_ERR_OK && inv_bags[t - INVENTORY_SLOT_BAG_START][j] + pItem->GetCount() <= pProto->GetMaxStackSize())
                        {
                            inv_bags[t - INVENTORY_SLOT_BAG_START][j] += pItem->GetCount();
                            b_found = true;
                            break;
                        }
                    }
                }
            }
            if (b_found)
            {
                continue;
            }
        }

        // special bag case
        if (pProto->BagFamily)
        {
            bool b_found = false;
            if (pProto->BagFamily == BAG_FAMILY_KEYS)
            {
                uint32 keyringSize = MaxKeyring();
                for (uint32 t = KEYRING_SLOT_START; t < KEYRING_SLOT_START + keyringSize; ++t)
                {
                    if (inv_keys[t - KEYRING_SLOT_START] == 0)
                    {
                        inv_keys[t - KEYRING_SLOT_START] = 1;
                        b_found = true;
                        break;
                    }
                }
            }

            if (b_found)
            {
                continue;
            }

            for (int t = INVENTORY_SLOT_BAG_START; !b_found && t < INVENTORY_SLOT_BAG_END; ++t)
            {
                pBag = (Bag*)At(INVENTORY_SLOT_BAG_0, t);
                if (pBag)
                {
                    pBagProto = pBag->GetProto();

                    // not plain container check
                    if (pBagProto && (pBagProto->Class != ITEM_CLASS_CONTAINER || pBagProto->SubClass != ITEM_SUBCLASS_CONTAINER) &&
                        ItemCanGoIntoBag(pProto, pBagProto))
                    {
                        for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                        {
                            if (inv_bags[t - INVENTORY_SLOT_BAG_START][j] == 0)
                            {
                                inv_bags[t - INVENTORY_SLOT_BAG_START][j] = 1;
                                b_found = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (b_found)
            {
                continue;
            }
        }

        // search free slot
        bool b_found = false;
        for (int t = INVENTORY_SLOT_ITEM_START; t < INVENTORY_SLOT_ITEM_END; ++t)
        {
            if (inv_slot_items[t - INVENTORY_SLOT_ITEM_START] == 0)
            {
                inv_slot_items[t - INVENTORY_SLOT_ITEM_START] = 1;
                b_found = true;
                break;
            }
        }
        if (b_found)
        {
            continue;
        }

        // search free slot in bags
        for (int t = INVENTORY_SLOT_BAG_START; !b_found && t < INVENTORY_SLOT_BAG_END; ++t)
        {
            pBag = (Bag*)At(INVENTORY_SLOT_BAG_0, t);
            if (pBag)
            {
                pBagProto = pBag->GetProto();

                // special bag already checked
                if (pBagProto && (pBagProto->Class != ITEM_CLASS_CONTAINER || pBagProto->SubClass != ITEM_SUBCLASS_CONTAINER))
                {
                    continue;
                }

                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    if (inv_bags[t - INVENTORY_SLOT_BAG_START][j] == 0)
                    {
                        inv_bags[t - INVENTORY_SLOT_BAG_START][j] = 1;
                        b_found = true;
                        break;
                    }
                }
            }
        }

        // no free slot found?
        if (!b_found)
        {
            return EQUIP_ERR_BAG_FULL;
        }
    }

    return EQUIP_ERR_OK;
}

//////////////////////////////////////////////////////////////////////////
/**
 * @brief Checks whether an equipped or banked item can be unequipped.
 *
 * @param pos The packed item position.
 * @param swap True if the item is being swapped rather than simply removed.
 * @return The inventory result for the unequip check.
 */
InventoryResult Inventory::CanTakeOff(uint16 pos, bool swap) const
{
    // Applied only to equipped items and bank bags
    if (!Inventory::IsWorn(pos) && !Inventory::HoldsBag(pos))
    {
        return EQUIP_ERR_OK;
    }

    Item* pItem = At(pos);

    // Applied only to existing equipped item
    if (!pItem)
    {
        return EQUIP_ERR_OK;
    }

    DEBUG_LOG("STORAGE: CanUnequipItem slot = %u, item = %u, count = %u", pos, pItem->GetEntry(), pItem->GetCount());

    ItemPrototype const* pProto = pItem->GetProto();
    if (!pProto)
    {
        return EQUIP_ERR_ITEM_NOT_FOUND;
    }

    // item used
    if (pItem->HasTemporaryLoot())
    {
        return EQUIP_ERR_ALREADY_LOOTED;
    }

    // do not allow unequipping gear except weapons, offhands, projectiles, relics in
    // - combat
    if (!pProto->CanChangeEquipStateInCombat())
    {
        if (m_owner.IsInCombat())
        {
            return EQUIP_ERR_NOT_IN_COMBAT;
        }
    }

    // prevent unequip item in process logout
    if (m_owner.GetSession()->isLogingOut())
    {
        return EQUIP_ERR_YOU_ARE_STUNNED;
    }

    if (!swap && pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
    {
        return EQUIP_ERR_CAN_ONLY_DO_WITH_EMPTY_BAGS;
    }

    return EQUIP_ERR_OK;
}

/**
 * @brief Checks whether an item can be stored in the bank and resolves destinations.
 *
 * @param bag The preferred destination bag, or NULL_BAG for auto-placement.
 * @param slot The preferred destination slot, or NULL_SLOT for auto-placement.
 * @param dest The accumulated destination positions.
 * @param pItem The item to bank.
 * @param swap True to allow swapping with occupied slots.
 * @param not_loading True when validating an active player action instead of load-time state.
 * @return The inventory result for the bank storage check.
 */
/**
 * @brief Checks whether an item can be stored in the bank and resolves destinations.
 *
 * @param bag The preferred destination bag, or NULL_BAG for auto-placement.
 * @param slot The preferred destination slot, or NULL_SLOT for auto-placement.
 * @param dest The accumulated destination positions.
 * @param pItem The item to bank.
 * @param swap True to allow swapping with occupied slots.
 * @param not_loading True when validating an active player action instead of load-time state.
 * @return The inventory result for the bank storage check.
 */
InventoryResult Inventory::PlanToBank(uint8 bag, uint8 slot, ItemPosCountVec& dest, Item* pItem, bool swap, bool not_loading) const
{
    if (!pItem)
    {
        return swap ? EQUIP_ERR_ITEMS_CANT_BE_SWAPPED : EQUIP_ERR_ITEM_NOT_FOUND;
    }

    uint32 count = pItem->GetCount();

    DEBUG_LOG("STORAGE: CanBankItem bag = %u, slot = %u, item = %u, count = %u", bag, slot, pItem->GetEntry(), pItem->GetCount());
    ItemPrototype const* pProto = pItem->GetProto();
    if (!pProto)
    {
        return swap ? EQUIP_ERR_ITEMS_CANT_BE_SWAPPED : EQUIP_ERR_ITEM_NOT_FOUND;
    }

    // item used
    if (pItem->HasTemporaryLoot())
    {
        return EQUIP_ERR_ALREADY_LOOTED;
    }

    if (pItem->IsBindedNotWith(&m_owner))
    {
        return EQUIP_ERR_DONT_OWN_THAT_ITEM;
    }

    // check count of items (skip for auto move for same player from bank)
    InventoryResult res = RoomForMore(pItem->GetEntry(), pItem->GetCount(), pItem);
    if (res != EQUIP_ERR_OK)
    {
        return res;
    }

    // in specific slot
    if (bag != NULL_BAG && slot != NULL_SLOT)
    {
        if (slot >= BANK_SLOT_BAG_START && slot < BANK_SLOT_BAG_END)
        {
            if (!pItem->IsBag())
            {
                return EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT;
            }

            if (slot - BANK_SLOT_BAG_START >= m_owner.GetBankBagSlotCount())
            {
                return EQUIP_ERR_MUST_PURCHASE_THAT_BAG_SLOT;
            }

            res = m_owner.CanUseItem(pItem, not_loading);
            if (res != EQUIP_ERR_OK)
            {
                return res;
            }
        }

        res = FitsHere(bag, slot, dest, pProto, count, swap, pItem);
        if (res != EQUIP_ERR_OK)
        {
            return res;
        }

        if (count == 0)
        {
            return EQUIP_ERR_OK;
        }
    }

    // not specific slot or have space for partly store only in specific slot

    // in specific bag
    if (bag != NULL_BAG)
    {
        if (pProto->InventoryType == INVTYPE_BAG)
        {
            Bag* pBag = (Bag*)pItem;
            if (pBag && !pBag->IsEmpty())
            {
                return EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG;
            }
        }

        // search stack in bag for merge to
        if (pProto->Stackable > 1)
        {
            if (bag == INVENTORY_SLOT_BAG_0)
            {
                res = FitsInRun(BANK_SLOT_ITEM_START, BANK_SLOT_ITEM_END, dest, pProto, count, true, pItem, bag, slot);
                if (res != EQUIP_ERR_OK)
                {
                    return res;
                }

                if (count == 0)
                {
                    return EQUIP_ERR_OK;
                }
            }
            else
            {
                res = FitsInBag(bag, dest, pProto, count, true, false, pItem, NULL_BAG, slot);
                if (res != EQUIP_ERR_OK)
                {
                    res = FitsInBag(bag, dest, pProto, count, true, true, pItem, NULL_BAG, slot);
                }

                if (res != EQUIP_ERR_OK)
                {
                    return res;
                }

                if (count == 0)
                {
                    return EQUIP_ERR_OK;
                }
            }
        }

        // search free slot in bag
        if (bag == INVENTORY_SLOT_BAG_0)
        {
            res = FitsInRun(BANK_SLOT_ITEM_START, BANK_SLOT_ITEM_END, dest, pProto, count, false, pItem, bag, slot);
            if (res != EQUIP_ERR_OK)
            {
                return res;
            }

            if (count == 0)
            {
                return EQUIP_ERR_OK;
            }
        }
        else
        {
            res = FitsInBag(bag, dest, pProto, count, false, false, pItem, NULL_BAG, slot);
            if (res != EQUIP_ERR_OK)
            {
                res = FitsInBag(bag, dest, pProto, count, false, true, pItem, NULL_BAG, slot);
            }

            if (res != EQUIP_ERR_OK)
            {
                return res;
            }

            if (count == 0)
            {
                return EQUIP_ERR_OK;
            }
        }
    }

    // not specific bag or have space for partly store only in specific bag

    // search stack for merge to
    if (pProto->Stackable > 1)
    {
        // in slots
        res = FitsInRun(BANK_SLOT_ITEM_START, BANK_SLOT_ITEM_END, dest, pProto, count, true, pItem, bag, slot);
        if (res != EQUIP_ERR_OK)
        {
            return res;
        }

        if (count == 0)
        {
            return EQUIP_ERR_OK;
        }

        // in special bags
        if (pProto->BagFamily)
        {
            for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
            {
                res = FitsInBag(i, dest, pProto, count, true, false, pItem, bag, slot);
                if (res != EQUIP_ERR_OK)
                {
                    continue;
                }

                if (count == 0)
                {
                    return EQUIP_ERR_OK;
                }
            }
        }

        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            res = FitsInBag(i, dest, pProto, count, true, true, pItem, bag, slot);
            if (res != EQUIP_ERR_OK)
            {
                continue;
            }

            if (count == 0)
            {
                return EQUIP_ERR_OK;
            }
        }
    }

    // search free place in special bag
    if (pProto->BagFamily)
    {
        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            res = FitsInBag(i, dest, pProto, count, false, false, pItem, bag, slot);
            if (res != EQUIP_ERR_OK)
            {
                continue;
            }

            if (count == 0)
            {
                return EQUIP_ERR_OK;
            }
        }
    }

    // search free space
    res = FitsInRun(BANK_SLOT_ITEM_START, BANK_SLOT_ITEM_END, dest, pProto, count, false, pItem, bag, slot);
    if (res != EQUIP_ERR_OK)
    {
        return res;
    }

    if (count == 0)
    {
        return EQUIP_ERR_OK;
    }

    for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
    {
        res = FitsInBag(i, dest, pProto, count, false, true, pItem, bag, slot);
        if (res != EQUIP_ERR_OK)
        {
            continue;
        }

        if (count == 0)
        {
            return EQUIP_ERR_OK;
        }
    }
    return EQUIP_ERR_BANK_FULL;
}

/**
 * @brief Checks whether a specific item instance can currently be used or equipped.
 *
 * @param pItem The item instance to validate.
 * @param direct_action True if the check is for an immediate player action.
 * @return The inventory result for the use check.
 */
