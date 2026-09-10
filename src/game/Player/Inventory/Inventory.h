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

#pragma once

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "Inventory/ItemSlots.h"
#include "Item.h"
#include "ItemSaveQueue.h"

#include <list>

class Bag;
class Item;
class Player;

struct EnchantClock
{
    EnchantClock(Item* onItem, EnchantmentSlot which, uint32 left)
        : item(onItem), slot(which), leftduration(left) {}

    Item* item;
    EnchantmentSlot slot;
    uint32 leftduration;
};

enum SearchScope : uint32
{

    SCOPE_ON_HIM        = 0x01,
    SCOPE_KEYRING       = 0x02,
    SCOPE_IN_BAGS       = 0x04,

    SCOPE_BANK          = 0x08,
    SCOPE_IN_BANK_BAGS  = 0x10,

    SCOPE_TO_HAND       = SCOPE_ON_HIM | SCOPE_KEYRING | SCOPE_IN_BAGS,
    SCOPE_IN_THE_BANK   = SCOPE_BANK | SCOPE_IN_BANK_BAGS,
    SCOPE_EVERYWHERE    = SCOPE_TO_HAND | SCOPE_IN_THE_BANK,
};

class Inventory
{
    public:
        explicit Inventory(Player& owner);

        static bool IsHisOwn(uint8 bag) { return bag == INVENTORY_SLOT_BAG_0; }

        static bool IsCarried(uint8 bag, uint8 slot);
        static bool IsCarried(uint16 place) { return IsCarried(Container(place), Slot(place)); }

        static bool IsWorn(uint8 bag, uint8 slot);
        static bool IsWorn(uint16 place) { return IsWorn(Container(place), Slot(place)); }

        static bool IsBanked(uint8 bag, uint8 slot);
        static bool IsBanked(uint16 place) { return IsBanked(Container(place), Slot(place)); }

        static bool HoldsBag(uint16 place);

        static uint32 AttackFrom(uint8 slot);

        static uint16 Packed(uint8 bag, uint8 slot) { return uint16(uint16(bag) << 8) | slot; }
        static uint8 Container(uint16 place) { return uint8(place >> 8); }
        static uint8 Slot(uint16 place) { return uint8(place & 255); }

        Item* At(uint8 bag, uint8 slot) const;
        Item* At(uint16 place) const { return At(Container(place), Slot(place)); }

        Item* Own(uint8 slot) const { return slot < PLAYER_SLOTS_COUNT ? m_place[slot] : nullptr; }
        void Own(uint8 slot, Item* item);

        Bag* BagAt(uint8 slot) const;

        bool Exists(uint8 bag, uint8 slot, bool exact) const;

        uint32 Count(uint32 entry, uint32 scope, Item const* except = nullptr) const;

        bool Holds(uint32 entry, uint32 count, uint32 scope) const;

        Item* ByGuid(ObjectGuid guid) const;

        Item* ByEntry(uint32 entry) const;

        uint32 NextBuyback() const { return m_nextBuyback; }
        void NextBuyback(uint32 slot) { m_nextBuyback = slot; }

        void Arrived(Item* item, bool tell);
        void Changed(Item* item, bool tell);
        void Gone(Item* item, bool tell);

        void ShowsEnchant(Item const* item, uint32 which, uint32 enchantId);

        void StartClocks(Item* item);
        void StopClocks(Item* item);

        void StartEnchantClocks(Item* item);

        void StartEnchantClock(Item* item, EnchantmentSlot which, uint32 duration);

        void RunClocks(uint32 elapsed, bool realTimeOnly);
        void RunEnchantClocks(uint32 elapsed);

        void SendClocks();

        void SettleClocks();

        ItemSaveQueue& Saves() { return m_saves; }
        ItemSaveQueue const& Saves() const { return m_saves; }

        Item* Store(ItemPosCountVec const& plan, Item* item, bool tell);

        void Wear(uint8 slot, Item* item);

        void QuickWear(uint16 place, Item* item);

        void Take(uint8 bag, uint8 slot, bool tell);

        void Destroy(uint8 bag, uint8 slot, bool tell);

        void Merge(Item* into, Item* from, uint32 count, bool tell);

        void ToBuyback(Item* item);
        Item* InBuyback(uint32 slot) const;
        void ClearBuyback(uint32 slot, bool destroy);

        static uint32 MaxKeyring() { return KEYRING_SLOT_END - KEYRING_SLOT_START; }

        InventoryResult PlanToStore(uint8 bag, uint8 slot, ItemPosCountVec& plan,
                                    uint32 entry, uint32 count, Item* item = nullptr,
                                    bool swap = false, uint32* shortBy = nullptr) const;

        InventoryResult PlanForAll(Item** items, int count) const;

        InventoryResult PlanToBank(uint8 bag, uint8 slot, ItemPosCountVec& plan,
                                   Item* item, bool swap, bool weighing = true) const;

        InventoryResult CanTakeOff(uint16 place, bool swap) const;

        InventoryResult RoomForMore(uint32 entry, uint32 count, Item* except,
                                    uint32* shortBy = nullptr) const;

        bool HasTotem(uint32 totemCategory) const;

    private:

        template <typename Visit>
        void Walk(uint32 scope, Visit visit) const;

        void Shows(uint8 slot, Item const* item);

        Item* Put(uint16 place, Item* item, uint32 count, bool clone, bool tell);

        static bool BindsOnArrival(ItemPrototype const& proto, uint16 place);

        InventoryResult FitsHere(uint8 bag, uint8 slot, ItemPosCountVec& plan,
                                 ItemPrototype const* proto, uint32& count,
                                 bool swap, Item* source) const;
        InventoryResult FitsInBag(uint8 bag, ItemPosCountVec& plan,
                                  ItemPrototype const* proto, uint32& count,
                                  bool merge, bool nonSpecialised, Item* source,
                                  uint8 skipBag, uint8 skipSlot) const;
        InventoryResult FitsInRun(uint8 from, uint8 to, ItemPosCountVec& plan,
                                  ItemPrototype const* proto, uint32& count,
                                  bool merge, Item* source,
                                  uint8 skipBag, uint8 skipSlot) const;

        Player& m_owner;
        Item* m_place[PLAYER_SLOTS_COUNT];
        uint32 m_nextBuyback;

        ItemSaveQueue m_saves;

        std::list<Item*> m_running;

        std::list<EnchantClock> m_runningEnchants;
};
