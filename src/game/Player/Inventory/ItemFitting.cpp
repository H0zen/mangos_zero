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

bool Inventory::HasTotem(uint32 ) const
{

    return false;
}

InventoryResult Inventory::FitsHere(uint8 bag, uint8 slot, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool swap, Item* pSrcItem) const
{
    Item* pItem2 = At(bag, slot);

    if (pItem2 == pSrcItem)
    {
        pItem2 = nullptr;
    }

    uint32 need_space;

    if (!pItem2 || swap)
    {
        if (bag == INVENTORY_SLOT_BAG_0)
        {

            if (slot >= KEYRING_SLOT_START && slot < KEYRING_SLOT_START + MaxKeyring() && !(pProto->BagFamily == BAG_FAMILY_KEYS))
            {
                return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
            }

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

        need_space = pProto->Stackable;
    }

    else
    {

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

InventoryResult Inventory::FitsInBag(uint8 bag, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool merge, bool non_specialized, Item* pSrcItem, uint8 skip_bag, uint8 skip_slot) const
{

    if (bag == skip_bag)
    {
        return EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG;
    }

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

        if (j == skip_slot)
        {
            continue;
        }

        Item* pItem2 = At(bag, j);

        if (pItem2 == pSrcItem)
        {
            pItem2 = nullptr;
        }

        if ((pItem2 != nullptr) != merge)
        {
            continue;
        }

        uint32 need_space = pProto->GetMaxStackSize();

        if (pItem2)
        {

            uint8 res  = pItem2->CanBeMergedPartlyWith(pProto);
            if (res != EQUIP_ERR_OK)
            {
                continue;
            }

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

InventoryResult Inventory::FitsInRun(uint8 slot_begin, uint8 slot_end, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool merge, Item* pSrcItem, uint8 skip_bag, uint8 skip_slot) const
{
    for (uint32 j = slot_begin; j < slot_end; ++j)
    {

        if (INVENTORY_SLOT_BAG_0 == skip_bag && j == skip_slot)
        {
            continue;
        }

        Item* pItem2 = At(INVENTORY_SLOT_BAG_0, j);

        if (pItem2 == pSrcItem)
        {
            pItem2 = nullptr;
        }

        if ((pItem2 != nullptr) != merge)
        {
            continue;
        }

        uint32 need_space = pProto->GetMaxStackSize();

        if (pItem2)
        {

            uint8 res  = pItem2->CanBeMergedPartlyWith(pProto);
            if (res != EQUIP_ERR_OK)
            {
                continue;
            }

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

    uint32 no_similar_count = 0;
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

    if (bag != NULL_BAG)
    {

        if (pProto->Stackable > 1)
        {
            if (bag == INVENTORY_SLOT_BAG_0)
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
            else
            {

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

        if (bag == INVENTORY_SLOT_BAG_0)
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
        else
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

    if (pItem && pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
    {
        return EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG;
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

InventoryResult Inventory::PlanForAll(Item** pItems, int count) const
{
    Item*    pItem2;

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

    for (int k = 0; k < count; ++k)
    {
        Item*  pItem = pItems[k];

        if (!pItem)
        {
            continue;
        }

        DEBUG_LOG("STORAGE: CanStoreItems %i. item = %u, count = %u", k + 1, pItem->GetEntry(), pItem->GetCount());
        ItemPrototype const* pProto = pItem->GetProto();

        if (!pProto)
        {
            return EQUIP_ERR_ITEM_NOT_FOUND;
        }

        if (pItem->HasTemporaryLoot())
        {
            return EQUIP_ERR_ALREADY_LOOTED;
        }

        if (pItem->IsBindedNotWith(&m_owner))
        {
            return EQUIP_ERR_DONT_OWN_THAT_ITEM;
        }

        Bag* pBag;
        ItemPrototype const* pBagProto;

        InventoryResult res = RoomForMore(pItem->GetEntry(), pItem->GetCount(), pItem);
        if (res != EQUIP_ERR_OK)
        {
            return res;
        }

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

        for (int t = INVENTORY_SLOT_BAG_START; !b_found && t < INVENTORY_SLOT_BAG_END; ++t)
        {
            pBag = (Bag*)At(INVENTORY_SLOT_BAG_0, t);
            if (pBag)
            {
                pBagProto = pBag->GetProto();

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

        if (!b_found)
        {
            return EQUIP_ERR_BAG_FULL;
        }
    }

    return EQUIP_ERR_OK;
}

InventoryResult Inventory::CanTakeOff(uint16 pos, bool swap) const
{

    if (!Inventory::IsWorn(pos) && !Inventory::HoldsBag(pos))
    {
        return EQUIP_ERR_OK;
    }

    Item* pItem = At(pos);

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

    if (pItem->HasTemporaryLoot())
    {
        return EQUIP_ERR_ALREADY_LOOTED;
    }

    if (!pProto->CanChangeEquipStateInCombat())
    {
        if (m_owner.IsInCombat())
        {
            return EQUIP_ERR_NOT_IN_COMBAT;
        }
    }

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

    if (pItem->HasTemporaryLoot())
    {
        return EQUIP_ERR_ALREADY_LOOTED;
    }

    if (pItem->IsBindedNotWith(&m_owner))
    {
        return EQUIP_ERR_DONT_OWN_THAT_ITEM;
    }

    InventoryResult res = RoomForMore(pItem->GetEntry(), pItem->GetCount(), pItem);
    if (res != EQUIP_ERR_OK)
    {
        return res;
    }

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

    if (pProto->Stackable > 1)
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
