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
#include "Player.h"
#include "Unit.h"

Inventory::Inventory(Player& owner)
    : m_owner(owner), m_place{}, m_nextBuyback(BUYBACK_SLOT_START)
{
}

bool Inventory::IsCarried(uint8 bag, uint8 slot)
{
    if (IsHisOwn(bag))
    {
        return slot == NULL_SLOT
            || (slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END)
            || (slot >= KEYRING_SLOT_START && slot < KEYRING_SLOT_END);
    }

    return bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END;
}

bool Inventory::IsWorn(uint8 bag, uint8 slot)
{
    return IsHisOwn(bag)
        && (slot < EQUIPMENT_SLOT_END
            || (slot >= INVENTORY_SLOT_BAG_START && slot < INVENTORY_SLOT_BAG_END));
}

bool Inventory::IsBanked(uint8 bag, uint8 slot)
{
    if (IsHisOwn(bag))
    {
        return (slot >= BANK_SLOT_ITEM_START && slot < BANK_SLOT_ITEM_END)
            || (slot >= BANK_SLOT_BAG_START && slot < BANK_SLOT_BAG_END);
    }

    return bag >= BANK_SLOT_BAG_START && bag < BANK_SLOT_BAG_END;
}

bool Inventory::HoldsBag(uint16 place)
{
    if (!IsHisOwn(Container(place)))
    {
        return false;
    }

    uint8 const slot = Slot(place);
    return (slot >= INVENTORY_SLOT_BAG_START && slot < INVENTORY_SLOT_BAG_END)
        || (slot >= BANK_SLOT_BAG_START && slot < BANK_SLOT_BAG_END);
}

uint32 Inventory::AttackFrom(uint8 slot)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_MAINHAND: return BASE_ATTACK;
        case EQUIPMENT_SLOT_OFFHAND:  return OFF_ATTACK;
        case EQUIPMENT_SLOT_RANGED:   return RANGED_ATTACK;
        default:                      return MAX_ATTACK;
    }
}

Item* Inventory::At(uint8 bag, uint8 slot) const
{
    if (IsHisOwn(bag))
    {
        return Own(slot);
    }

    if (Bag* holder = BagAt(bag))
    {
        return holder->GetItemByPos(slot);
    }

    return nullptr;
}

/**
 * The client reads one guid per place out of the character's own fields, in the
 * same order as the places themselves, so the field is the place said again on
 * the wire. Writing both here is what keeps them from ever disagreeing.
 */
void Inventory::Own(uint8 slot, Item* item)
{
    MANGOS_ASSERT(slot < PLAYER_SLOTS_COUNT);

    m_place[slot] = item;
    m_owner.SetGuidValue(uint16(PLAYER_FIELD_INV_SLOT_HEAD + slot * 2),
                         item ? item->GetObjectGuid() : ObjectGuid());

    if (slot < EQUIPMENT_SLOT_END)
    {
        Shows(slot, item);
    }
}

void Inventory::Shows(uint8 slot, Item const* item)
{
    uint16 const face = uint16(PLAYER_VISIBLE_ITEM_1_CREATOR + slot * MAX_VISIBLE_ITEM_OFFSET);
    uint16 const piece = uint16(PLAYER_VISIBLE_ITEM_1_0 + slot * MAX_VISIBLE_ITEM_OFFSET);
    uint16 const suffix = uint16(PLAYER_VISIBLE_ITEM_1_PROPERTIES + slot * MAX_VISIBLE_ITEM_OFFSET);

    m_owner.SetGuidValue(face, item ? item->GetCreatorGuid() : ObjectGuid());
    m_owner.SetUInt32Value(piece, item ? item->GetEntry() : 0);

    for (uint32 which = 0; which < MAX_INSPECTED_ENCHANTMENT_SLOT; ++which)
    {
        m_owner.SetUInt32Value(uint16(piece + 1 + which),
                               item ? item->GetEnchantmentId(EnchantmentSlot(which)) : 0);
    }

    if (item)
    {
        // Signed, and set as a short so that a negative suffix does not fill the
        // high half with ones.
        m_owner.SetInt16Value(suffix, 0, int16(item->GetItemRandomPropertyId()));
        m_owner.SetUInt32Value(uint16(suffix + 1), item->GetItemSuffixFactor());
    }
    else
    {
        m_owner.SetUInt32Value(suffix, 0);
        m_owner.SetUInt32Value(uint16(suffix + 1), 0);
    }
}

void Inventory::ShowsEnchant(Item const* item, uint32 which, uint32 enchantId)
{
    if (!item || which >= MAX_INSPECTED_ENCHANTMENT_SLOT)
    {
        return;
    }

    uint8 const slot = item->GetSlot();
    if (slot >= EQUIPMENT_SLOT_END || Own(slot) != item)
    {
        return;
    }

    m_owner.SetUInt32Value(uint16(PLAYER_VISIBLE_ITEM_1_0 + slot * MAX_VISIBLE_ITEM_OFFSET + 1 + which),
                           enchantId);
}

void Inventory::Arrived(Item* item, bool tell)
{
    if (!item || !tell || !m_owner.IsInWorld())
    {
        return;
    }

    item->AddToWorld();
    item->SendCreateUpdateToPlayer(&m_owner);
}

void Inventory::Changed(Item* item, bool tell)
{
    if (!item || !tell || !m_owner.IsInWorld())
    {
        return;
    }

    item->SendCreateUpdateToPlayer(&m_owner);
}

void Inventory::Gone(Item* item, bool tell)
{
    if (!item || !tell || !m_owner.IsInWorld())
    {
        return;
    }

    item->RemoveFromWorld();
    item->DestroyForPlayer(&m_owner);
}

Bag* Inventory::BagAt(uint8 slot) const
{
    Item* item = Own(slot);
    return item && item->IsBag() ? static_cast<Bag*>(item) : nullptr;
}

bool Inventory::Exists(uint8 bag, uint8 slot, bool exact) const
{
    // "Wherever it fits" is a place only when the caller allows one.
    if (bag == NULL_BAG)
    {
        return !exact;
    }

    if (IsHisOwn(bag))
    {
        if (slot == NULL_SLOT)
        {
            return !exact;
        }

        return slot < EQUIPMENT_SLOT_END
            || (slot >= INVENTORY_SLOT_BAG_START && slot < INVENTORY_SLOT_BAG_END)
            || (slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END)
            || (slot >= KEYRING_SLOT_START && slot < KEYRING_SLOT_END)
            || (slot >= BANK_SLOT_ITEM_START && slot < BANK_SLOT_ITEM_END)
            || (slot >= BANK_SLOT_BAG_START && slot < BANK_SLOT_BAG_END);
    }

    bool const isBagSlot = (bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END)
        || (bag >= BANK_SLOT_BAG_START && bag < BANK_SLOT_BAG_END);
    if (!isBagSlot)
    {
        return false;
    }

    // A bag's places exist only as far as the bag is deep, so an empty bag slot
    // has none at all.
    Bag* holder = BagAt(bag);
    if (!holder)
    {
        return false;
    }

    return slot == NULL_SLOT ? !exact : slot < holder->GetBagSize();
}

/**
 * Hands every item in the wanted regions to the visitor, in the order the
 * regions are listed, and stops as soon as the visitor says it has seen enough.
 *
 * All four searches below are this walk with a different question, which is why
 * the walk is written once. What they genuinely differ in is how far they look,
 * and that is the scope each of them passes.
 */
template <typename Visit>
void Inventory::Walk(uint32 scope, Visit visit) const
{
    if (scope & SCOPE_ON_HIM)
    {
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            if (Item* item = Own(slot))
            {
                if (!visit(item))
                {
                    return;
                }
            }
        }
    }

    if (scope & SCOPE_KEYRING)
    {
        for (uint8 slot = KEYRING_SLOT_START; slot < KEYRING_SLOT_END; ++slot)
        {
            if (Item* item = Own(slot))
            {
                if (!visit(item))
                {
                    return;
                }
            }
        }
    }

    if (scope & SCOPE_IN_BAGS)
    {
        for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
        {
            Bag* holder = BagAt(slot);
            if (!holder)
            {
                continue;
            }

            for (uint32 inside = 0; inside < holder->GetBagSize(); ++inside)
            {
                if (Item* item = holder->GetItemByPos(uint8(inside)))
                {
                    if (!visit(item))
                    {
                        return;
                    }
                }
            }
        }
    }

    if (scope & SCOPE_BANK)
    {
        for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
        {
            if (Item* item = Own(slot))
            {
                if (!visit(item))
                {
                    return;
                }
            }
        }
    }

    if (scope & SCOPE_IN_BANK_BAGS)
    {
        for (uint8 slot = BANK_SLOT_BAG_START; slot < BANK_SLOT_BAG_END; ++slot)
        {
            Bag* holder = BagAt(slot);
            if (!holder)
            {
                continue;
            }

            for (uint32 inside = 0; inside < holder->GetBagSize(); ++inside)
            {
                if (Item* item = holder->GetItemByPos(uint8(inside)))
                {
                    if (!visit(item))
                    {
                        return;
                    }
                }
            }
        }
    }
}

uint32 Inventory::Count(uint32 entry, uint32 scope, Item const* except) const
{
    uint32 found = 0;

    Walk(scope, [&](Item* item)
    {
        if (item != except && item->GetEntry() == entry)
        {
            found += item->GetCount();
        }
        return true;
    });

    return found;
}

bool Inventory::Holds(uint32 entry, uint32 count, uint32 scope) const
{
    uint32 found = 0;

    Walk(scope, [&](Item* item)
    {
        if (item->GetEntry() == entry && !item->IsInTrade())
        {
            found += item->GetCount();
        }
        return found < count;
    });

    return found >= count;
}

Item* Inventory::ByGuid(ObjectGuid guid) const
{
    Item* found = nullptr;

    Walk(SCOPE_EVERYWHERE, [&](Item* item)
    {
        if (item->GetObjectGuid() != guid)
        {
            return true;
        }
        found = item;
        return false;
    });

    return found;
}

Item* Inventory::ByEntry(uint32 entry) const
{
    Item* found = nullptr;

    Walk(SCOPE_ON_HIM | SCOPE_IN_BAGS, [&](Item* item)
    {
        if (item->GetEntry() != entry)
        {
            return true;
        }
        found = item;
        return false;
    });

    return found;
}
