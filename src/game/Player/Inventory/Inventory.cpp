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

void Inventory::StartClocks(Item* item)
{
    if (!item)
    {
        return;
    }

    StartEnchantClocks(item);

    if (item->GetUInt32Value(ITEM_FIELD_DURATION))
    {
        m_running.push_back(item);
        item->SendTimeUpdate(&m_owner);
    }
}

void Inventory::StartEnchantClocks(Item* item)
{
    if (!item)
    {
        return;
    }

    for (uint32 which = 0; which < MAX_ENCHANTMENT_SLOT; ++which)
    {
        EnchantmentSlot const slot = EnchantmentSlot(which);
        if (!item->GetEnchantmentId(slot))
        {
            continue;
        }

        uint32 const duration = item->GetEnchantmentDuration(slot);
        if (duration > 0)
        {
            StartEnchantClock(item, slot, duration);
        }
    }
}

void Inventory::StopClocks(Item* item)
{
    if (!item)
    {
        return;
    }

    // The time left goes back into the item, so that it takes up where it left
    // off if he picks the thing up again.
    for (auto itr = m_runningEnchants.begin(); itr != m_runningEnchants.end();)
    {
        if (itr->item != item)
        {
            ++itr;
            continue;
        }

        item->SetEnchantmentDuration(itr->slot, itr->leftduration);
        itr = m_runningEnchants.erase(itr);
    }

    m_running.remove(item);
}

void Inventory::StartEnchantClock(Item* item, EnchantmentSlot which, uint32 duration)
{
    if (!item || which >= MAX_ENCHANTMENT_SLOT)
    {
        return;
    }

    for (auto itr = m_runningEnchants.begin(); itr != m_runningEnchants.end(); ++itr)
    {
        if (itr->item == item && itr->slot == which)
        {
            itr->item->SetEnchantmentDuration(itr->slot, itr->leftduration);
            m_runningEnchants.erase(itr);
            break;
        }
    }

    if (duration == 0)
    {
        return;
    }

    m_owner.GetSession()->SendItemEnchantTimeUpdate(m_owner.GetObjectGuid(), item->GetObjectGuid(),
                                                    which, duration / IN_MILLISECONDS);
    m_runningEnchants.emplace_back(item, which, duration);
}

void Inventory::RunClocks(uint32 elapsed, bool realTimeOnly)
{
    // An item counts itself down and may drop out of the list while doing it, so
    // the next one is taken before the current one is asked.
    for (auto itr = m_running.begin(); itr != m_running.end();)
    {
        Item* item = *itr;
        ++itr;

        if (!realTimeOnly || (item->GetProto()->ExtraFlags & ITEM_EXTRA_REAL_TIME_DURATION))
        {
            item->UpdateDuration(&m_owner, elapsed);
        }
    }
}

void Inventory::RunEnchantClocks(uint32 elapsed)
{
    for (auto itr = m_runningEnchants.begin(); itr != m_runningEnchants.end();)
    {
        // Gone from the item by some other route, so there is nothing to count.
        if (!itr->item->GetEnchantmentId(itr->slot))
        {
            itr = m_runningEnchants.erase(itr);
        }
        else if (itr->leftduration > elapsed)
        {
            itr->leftduration -= elapsed;
            ++itr;
        }
        else
        {
            m_owner.ApplyEnchantment(itr->item, itr->slot, false, false);
            itr->item->ClearEnchantment(itr->slot);
            itr = m_runningEnchants.erase(itr);
        }
    }
}

void Inventory::SendClocks()
{
    for (auto const& running : m_runningEnchants)
    {
        m_owner.GetSession()->SendItemEnchantTimeUpdate(m_owner.GetObjectGuid(),
                                                        running.item->GetObjectGuid(),
                                                        running.slot,
                                                        running.leftduration / IN_MILLISECONDS);
    }

    for (auto* item : m_running)
    {
        item->SendTimeUpdate(&m_owner);
    }
}

void Inventory::SettleClocks()
{
    for (auto const& running : m_runningEnchants)
    {
        running.item->SetEnchantmentDuration(running.slot, running.leftduration);
    }
}

bool Inventory::BindsOnArrival(ItemPrototype const& proto, uint16 place)
{
    return proto.Bonding == BIND_WHEN_PICKED_UP
        || proto.Bonding == BIND_QUEST_ITEM
        || (proto.Bonding == BIND_WHEN_EQUIPPED && HoldsBag(place));
}

Item* Inventory::Store(ItemPosCountVec const& plan, Item* item, bool tell)
{
    if (!item)
    {
        return nullptr;
    }

    Item* last = item;

    for (auto step = plan.begin(); step != plan.end();)
    {
        uint16 const place = step->pos;
        uint32 const count = step->count;
        ++step;

        // The item itself goes in the last place; the others get copies.
        bool const isLast = step == plan.end();
        last = Put(place, item, count, !isLast, tell);

        if (isLast)
        {
            break;
        }
    }

    return last;
}

Item* Inventory::Put(uint16 place, Item* item, uint32 count, bool clone, bool tell)
{
    if (!item)
    {
        return nullptr;
    }

    uint8 const bag = Container(place);
    uint8 const slot = Slot(place);

    DEBUG_LOG("STORAGE: StoreItem bag = %u, slot = %u, item = %u, count = %u",
              bag, slot, item->GetEntry(), count);

    if (Item* sitting = At(bag, slot))
    {
        // The same thing is already there, so the two stacks become one and the
        // item that arrived is destroyed.
        if (BindsOnArrival(*sitting->GetProto(), place))
        {
            sitting->SetBinding(true);
        }

        if (clone)
        {
            // The stack it came from is going to other places too, so only the
            // count crosses over.
            sitting->SetCount(sitting->GetCount() + count);
            Changed(sitting, tell);
        }
        else
        {
            Merge(sitting, item, count, tell);
        }

        // Its own life is already being counted; only the enchantments are new.
        StartEnchantClocks(sitting);
        sitting->SetState(ITEM_CHANGED, &m_owner);

        return sitting;
    }

    item = clone ? item->CloneItem(count, &m_owner) : (item->SetCount(count), item);
    if (!item)
    {
        return nullptr;
    }

    if (BindsOnArrival(*item->GetProto(), place))
    {
        item->SetBinding(true);
    }

    if (IsHisOwn(bag))
    {
        Own(slot, item);
        item->SetGuidValue(ITEM_FIELD_CONTAINED, m_owner.GetObjectGuid());
        item->SetGuidValue(ITEM_FIELD_OWNER, m_owner.GetObjectGuid());
        item->SetSlot(slot);
        item->SetContainer(nullptr);

        Arrived(item, tell);
        item->SetState(ITEM_CHANGED, &m_owner);
    }
    else if (Bag* holder = BagAt(bag))
    {
        holder->StoreItem(slot, item);
        Arrived(item, tell);
        item->SetState(ITEM_CHANGED, &m_owner);
        holder->SetState(ITEM_CHANGED, &m_owner);
    }

    StartClocks(item);

    return item;
}

void Inventory::Wear(uint8 slot, Item* item)
{
    if (!item)
    {
        return;
    }

    // A piece put on by a command was never picked up, so this is the first
    // chance it has to be bound.
    ItemPrototype const& proto = *item->GetProto();
    if (proto.Bonding == BIND_WHEN_EQUIPPED || proto.Bonding == BIND_WHEN_PICKED_UP
        || proto.Bonding == BIND_QUEST_ITEM)
    {
        item->SetBinding(true);
    }

    DEBUG_LOG("STORAGE: EquipItem slot = %u, item = %u", slot, item->GetEntry());

    Own(slot, item);
    item->SetGuidValue(ITEM_FIELD_CONTAINED, m_owner.GetObjectGuid());
    item->SetGuidValue(ITEM_FIELD_OWNER, m_owner.GetObjectGuid());
    item->SetSlot(slot);
    item->SetContainer(nullptr);

    item->SetState(ITEM_CHANGED, &m_owner);
}

void Inventory::QuickWear(uint16 place, Item* item)
{
    if (!item)
    {
        return;
    }

    StartClocks(item);
    Wear(Slot(place), item);
    Arrived(item, true);
}

void Inventory::Take(uint8 bag, uint8 slot, bool tell)
{
    Item* item = At(bag, slot);
    if (!item)
    {
        return;
    }

    DEBUG_LOG("STORAGE: RemoveItem bag = %u, slot = %u, item = %u", bag, slot, item->GetEntry());

    StopClocks(item);

    if (IsHisOwn(bag))
    {
        Own(slot, nullptr);
    }
    else if (Bag* holder = BagAt(bag))
    {
        holder->RemoveItem(slot);
    }

    item->SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());
    // The owner is left alone: it is set again at the next store, and mail and
    // auction read it while the item is in neither place.
    item->SetSlot(NULL_SLOT);

    Changed(item, tell);
}

void Inventory::Destroy(uint8 bag, uint8 slot, bool tell)
{
    Item* item = At(bag, slot);
    if (!item)
    {
        return;
    }

    DEBUG_LOG("STORAGE: DestroyItem bag = %u, slot = %u, item = %u", bag, slot, item->GetEntry());

    StopClocks(item);

    if (IsHisOwn(bag))
    {
        Own(slot, nullptr);
    }
    else if (Bag* holder = BagAt(bag))
    {
        holder->RemoveItem(slot);
    }

    Gone(item, tell);

    item->SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());
    item->SetSlot(NULL_SLOT);
    item->SetState(ITEM_REMOVED, &m_owner);
}

void Inventory::Merge(Item* into, Item* from, uint32 count, bool tell)
{
    if (!into || !from)
    {
        return;
    }

    into->SetCount(into->GetCount() + count);
    Changed(into, tell);

    Gone(from, tell);
    StopClocks(from);

    // Named as his before the state changes, or a trade, a mail or a purchase
    // would be writing an item with no owner.
    from->SetOwnerGuid(m_owner.GetObjectGuid());
    from->SetState(ITEM_REMOVED, &m_owner);
}

void Inventory::ToBuyback(Item* item)
{
    if (!item)
    {
        return;
    }

    uint32 slot = NextBuyback();
    if (Own(uint8(slot)))
    {
        uint32 oldestAt = m_owner.GetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1);
        slot = BUYBACK_SLOT_START;

        for (uint32 place = BUYBACK_SLOT_START + 1; place < BUYBACK_SLOT_END; ++place)
        {
            if (!Own(uint8(place)))
            {
                slot = place;
                break;
            }

            uint32 const at = m_owner.GetUInt32Value(
                uint16(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + place - BUYBACK_SLOT_START));
            if (oldestAt > at)
            {
                oldestAt = at;
                slot = place;
            }
        }
    }

    ClearBuyback(slot, true);
    DEBUG_LOG("STORAGE: AddItemToBuyBackSlot item = %u, slot = %u", item->GetEntry(), slot);

    Own(uint8(slot), item);

    // The hour is written as seconds since he logged in, pushed ahead by a fixed
    // amount that the client subtracts back off. What that amount stands for is
    // not established here; it is kept as it is sent.
    uint32 const BUYBACK_STAMP_AHEAD = 30 * HOUR;

    uint16 const row = uint16(slot - BUYBACK_SLOT_START);
    uint32 const sold = uint32(time(nullptr) - m_owner.LoginTime() + BUYBACK_STAMP_AHEAD);

    m_owner.SetGuidValue(uint16(PLAYER_FIELD_VENDORBUYBACK_SLOT_1 + row * 2), item->GetObjectGuid());
    ItemPrototype const* proto = item->GetProto();
    m_owner.SetUInt32Value(uint16(PLAYER_FIELD_BUYBACK_PRICE_1 + row),
                           proto ? proto->SellPrice * item->GetCount() : 0);
    m_owner.SetUInt32Value(uint16(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + row), sold);

    // The next sale takes the place after this one while the row is filling up.
    if (NextBuyback() < BUYBACK_SLOT_END - 1)
    {
        NextBuyback(NextBuyback() + 1);
    }
}

Item* Inventory::InBuyback(uint32 slot) const
{
    DEBUG_LOG("STORAGE: GetItemFromBuyBackSlot slot = %u", slot);

    if (slot < BUYBACK_SLOT_START || slot >= BUYBACK_SLOT_END)
    {
        return nullptr;
    }

    return Own(uint8(slot));
}

void Inventory::ClearBuyback(uint32 slot, bool destroy)
{
    DEBUG_LOG("STORAGE: RemoveItemFromBuyBackSlot slot = %u", slot);

    if (slot < BUYBACK_SLOT_START || slot >= BUYBACK_SLOT_END)
    {
        return;
    }

    if (Item* item = Own(uint8(slot)))
    {
        item->RemoveFromWorld();
        if (destroy)
        {
            item->SetState(ITEM_REMOVED, &m_owner);
        }
    }

    Own(uint8(slot), nullptr);

    uint16 const row = uint16(slot - BUYBACK_SLOT_START);
    m_owner.SetGuidValue(uint16(PLAYER_FIELD_VENDORBUYBACK_SLOT_1 + row * 2), ObjectGuid());
    m_owner.SetUInt32Value(uint16(PLAYER_FIELD_BUYBACK_PRICE_1 + row), 0);
    m_owner.SetUInt32Value(uint16(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + row), 0);

    // The place just emptied is worth taking next only while the one already
    // chosen is occupied.
    if (Own(uint8(NextBuyback())))
    {
        NextBuyback(slot);
    }
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
